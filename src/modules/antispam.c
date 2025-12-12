/*
 * Himiko Discord Bot (C Edition) - Anti-Spam Pressure Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/antispam.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* Global spam tracker */
static spam_tracker_t g_spam_tracker;
static int g_spam_initialized = 0;

/* Forward declarations */
static user_pressure_t *get_or_create_user(const char *key);
static double calculate_pressure(const struct discord_message *msg, antispam_config_t *cfg, user_pressure_t *user);
static void handle_spam_action(struct discord *client, const struct discord_message *msg, antispam_config_t *cfg, double pressure);
static void log_spam_action(struct discord *client, const char *guild_id, const struct discord_user *user, const char *action, double pressure);
static int count_urls(const char *text);
static int count_lines(const char *text);

void antispam_init(himiko_bot_t *bot) {
    (void)bot;
    if (g_spam_initialized) return;

    memset(&g_spam_tracker, 0, sizeof(g_spam_tracker));
    pthread_mutex_init(&g_spam_tracker.mutex, NULL);
    g_spam_initialized = 1;
}

void antispam_cleanup(himiko_bot_t *bot) {
    (void)bot;
    if (!g_spam_initialized) return;

    pthread_mutex_destroy(&g_spam_tracker.mutex);
    g_spam_initialized = 0;
}

/* Get or create a user pressure entry */
static user_pressure_t *get_or_create_user(const char *key) {
    /* Look for existing entry */
    for (int i = 0; i < g_spam_tracker.user_count; i++) {
        if (strcmp(g_spam_tracker.users[i].guild_user_key, key) == 0) {
            return &g_spam_tracker.users[i];
        }
    }

    /* Create new entry if space available */
    if (g_spam_tracker.user_count < 1000) {
        user_pressure_t *user = &g_spam_tracker.users[g_spam_tracker.user_count];
        memset(user, 0, sizeof(*user));
        strncpy(user->guild_user_key, key, sizeof(user->guild_user_key) - 1);
        g_spam_tracker.user_count++;
        return user;
    }

    /* Evict oldest entry if full */
    time_t oldest_time = g_spam_tracker.users[0].last_update;
    int oldest_idx = 0;
    for (int i = 1; i < g_spam_tracker.user_count; i++) {
        if (g_spam_tracker.users[i].last_update < oldest_time) {
            oldest_time = g_spam_tracker.users[i].last_update;
            oldest_idx = i;
        }
    }
    user_pressure_t *user = &g_spam_tracker.users[oldest_idx];
    memset(user, 0, sizeof(*user));
    strncpy(user->guild_user_key, key, sizeof(user->guild_user_key) - 1);
    return user;
}

/* Count URLs in text using simple pattern matching */
static int count_urls(const char *text) {
    if (!text) return 0;

    int count = 0;
    const char *p = text;

    while ((p = strstr(p, "http")) != NULL) {
        if (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0) {
            count++;
            p += 4;
        } else {
            p++;
        }
    }

    return count;
}

/* Count newlines in text */
static int count_lines(const char *text) {
    if (!text) return 0;

    int count = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') count++;
    }
    return count;
}

/* Calculate pressure for a message */
static double calculate_pressure(const struct discord_message *msg, antispam_config_t *cfg, user_pressure_t *user) {
    time_t now = time(NULL);

    /* Decay pressure based on time since last message */
    if (user->last_update > 0 && cfg->pressure_decay > 0) {
        double elapsed = difftime(now, user->last_update);
        double decay = cfg->base_pressure * (elapsed / cfg->pressure_decay);
        user->pressure -= decay;
        if (user->pressure < 0) {
            user->pressure = 0;
        }
    }

    /* Start with base pressure */
    double pressure = cfg->base_pressure;

    /* Image/attachment pressure */
    if (msg->attachments) {
        pressure += msg->attachments->size * cfg->image_pressure;
    }
    if (msg->embeds) {
        pressure += msg->embeds->size * cfg->image_pressure;
    }

    /* Link pressure */
    int link_count = count_urls(msg->content);
    pressure += link_count * cfg->link_pressure;

    /* Ping pressure */
    if (msg->mentions) {
        pressure += msg->mentions->size * cfg->ping_pressure;
    }
    if (msg->mention_everyone) {
        pressure += cfg->ping_pressure * 10; /* Heavy penalty for @everyone */
    }

    /* Length pressure */
    if (msg->content) {
        pressure += strlen(msg->content) * cfg->length_pressure;
    }

    /* Line pressure */
    int line_count = count_lines(msg->content);
    pressure += line_count * cfg->line_pressure;

    /* Repeat pressure - same message as last */
    if (msg->content && user->last_message[0] &&
        strcmp(msg->content, user->last_message) == 0 &&
        msg->content[0] != '\0') {
        pressure += cfg->repeat_pressure;
    }

    /* Update user state */
    user->pressure += pressure;
    if (msg->content) {
        strncpy(user->last_message, msg->content, sizeof(user->last_message) - 1);
    }
    user->last_update = now;

    return user->pressure;
}

/* Reset a user's pressure */
void antispam_reset_pressure(const char *guild_id, const char *user_id) {
    char key[96];
    snprintf(key, sizeof(key), "%s:%s", guild_id, user_id);

    pthread_mutex_lock(&g_spam_tracker.mutex);

    for (int i = 0; i < g_spam_tracker.user_count; i++) {
        if (strcmp(g_spam_tracker.users[i].guild_user_key, key) == 0) {
            g_spam_tracker.users[i].pressure = 0;
            g_spam_tracker.users[i].last_message[0] = '\0';
            break;
        }
    }

    pthread_mutex_unlock(&g_spam_tracker.mutex);
}

/* Log spam action to mod log */
static void log_spam_action(struct discord *client, const char *guild_id, const struct discord_user *user, const char *action, double pressure) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    guild_settings_t settings;
    if (db_get_guild_settings(&bot->database, guild_id, &settings) != 0) return;
    if (!settings.mod_log_channel[0]) return;

    char description[512];
    snprintf(description, sizeof(description),
        "User <@%s> was **%s** for spam",
        user->id ? (char*)&user->id : "unknown", action);

    char pressure_str[32];
    snprintf(pressure_str, sizeof(pressure_str), "%.1f", pressure);

    struct discord_embed_field fields[] = {
        { .name = "User", .value = user->username ? user->username : "Unknown", .Inline = true },
        { .name = "Pressure", .value = pressure_str, .Inline = true }
    };

    struct discord_embed embed = {
        .title = "Anti-Spam Action",
        .description = description,
        .color = 0xFF0000,
        .fields = &(struct discord_embed_fields){ .size = 2, .array = fields }
    };

    u64snowflake channel_id = strtoull(settings.mod_log_channel, NULL, 10);
    struct discord_create_message params = { .embeds = &(struct discord_embeds){ .size = 1, .array = &embed } };
    discord_create_message(client, channel_id, &params, NULL);
}

/* Handle spam action */
static void handle_spam_action(struct discord *client, const struct discord_message *msg, antispam_config_t *cfg, double pressure) {
    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);

    if (strcmp(cfg->action, "delete") == 0) {
        /* Just delete the message */
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);

    } else if (strcmp(cfg->action, "warn") == 0) {
        /* Delete and warn */
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        /* Note: DM warning would require creating a DM channel first */

    } else if (strcmp(cfg->action, "silence") == 0) {
        /* Delete and add silent role */
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        if (cfg->silent_role_id[0]) {
            u64snowflake role_id = strtoull(cfg->silent_role_id, NULL, 10);
            discord_add_guild_member_role(client, msg->guild_id, msg->author->id, role_id, NULL, NULL);
            log_spam_action(client, guild_id_str, msg->author, "silenced", pressure);
        }

    } else if (strcmp(cfg->action, "kick") == 0) {
        /* Delete and kick */
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        discord_remove_guild_member(client, msg->guild_id, msg->author->id, NULL, NULL);
        log_spam_action(client, guild_id_str, msg->author, "kicked", pressure);

    } else if (strcmp(cfg->action, "ban") == 0) {
        /* Delete and ban */
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        struct discord_create_guild_ban params = { .delete_message_days = 1 };
        discord_create_guild_ban(client, msg->guild_id, msg->author->id, &params, NULL);
        log_spam_action(client, guild_id_str, msg->author, "banned", pressure);
    }
}

/* Main spam check function - returns 1 if action taken */
int antispam_check(himiko_bot_t *bot, struct discord *client, const struct discord_message *msg) {
    if (!msg || !msg->author) return 0;

    /* Ignore bots */
    if (msg->author->bot) return 0;

    /* Ignore DMs */
    if (msg->guild_id == 0) return 0;

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);

    /* Get config */
    antispam_config_t cfg;
    if (db_get_antispam_config(&bot->database, guild_id_str, &cfg) != 0 || !cfg.enabled) {
        return 0;
    }

    /* Build key and calculate pressure */
    char key[96];
    snprintf(key, sizeof(key), "%s:%s", guild_id_str, user_id_str);

    pthread_mutex_lock(&g_spam_tracker.mutex);
    user_pressure_t *user = get_or_create_user(key);
    double pressure = calculate_pressure(msg, &cfg, user);
    pthread_mutex_unlock(&g_spam_tracker.mutex);

    /* Check if exceeded threshold */
    if (pressure >= cfg.max_pressure) {
        handle_spam_action(client, msg, &cfg, pressure);
        antispam_reset_pressure(guild_id_str, user_id_str);
        return 1;
    }

    return 0;
}

/* Command: antispam status/enable/disable/set */
void cmd_antispam(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;

    if (!opts || opts->size == 0) {
        /* Show current status */
        antispam_config_t cfg;
        if (db_get_antispam_config(&bot->database, guild_id_str, &cfg) != 0) {
            respond_message(client, interaction, "Failed to get anti-spam config.");
            return;
        }

        char response[1024];
        snprintf(response, sizeof(response),
            "**Anti-Spam Status**\n"
            "Enabled: %s\n"
            "Base Pressure: %.1f\n"
            "Image Pressure: %.1f\n"
            "Link Pressure: %.1f\n"
            "Ping Pressure: %.1f\n"
            "Length Pressure: %.4f (per char)\n"
            "Line Pressure: %.1f\n"
            "Repeat Pressure: %.1f\n"
            "Max Pressure: %.1f\n"
            "Decay Rate: %.1f seconds\n"
            "Action: %s\n"
            "Silent Role: %s",
            cfg.enabled ? "Yes" : "No",
            cfg.base_pressure,
            cfg.image_pressure,
            cfg.link_pressure,
            cfg.ping_pressure,
            cfg.length_pressure,
            cfg.line_pressure,
            cfg.repeat_pressure,
            cfg.max_pressure,
            cfg.pressure_decay,
            cfg.action[0] ? cfg.action : "none",
            cfg.silent_role_id[0] ? cfg.silent_role_id : "Not set");

        respond_message(client, interaction, response);
        return;
    }

    const char *subcommand = opts->array[0].name;

    if (strcmp(subcommand, "enable") == 0) {
        antispam_config_t cfg;
        db_get_antispam_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 1;
        /* Set defaults if not configured */
        if (cfg.base_pressure <= 0) cfg.base_pressure = 10.0;
        if (cfg.image_pressure <= 0) cfg.image_pressure = 8.5;
        if (cfg.link_pressure <= 0) cfg.link_pressure = 10.0;
        if (cfg.ping_pressure <= 0) cfg.ping_pressure = 2.5;
        if (cfg.length_pressure <= 0) cfg.length_pressure = 0.00625;
        if (cfg.line_pressure <= 0) cfg.line_pressure = 0.714;
        if (cfg.repeat_pressure <= 0) cfg.repeat_pressure = 10.0;
        if (cfg.max_pressure <= 0) cfg.max_pressure = 60.0;
        if (cfg.pressure_decay <= 0) cfg.pressure_decay = 2.5;
        if (!cfg.action[0]) strcpy(cfg.action, "silence");
        db_set_antispam_config(&bot->database, &cfg);
        respond_message(client, interaction, "Anti-spam system **enabled**.");

    } else if (strcmp(subcommand, "disable") == 0) {
        antispam_config_t cfg;
        db_get_antispam_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 0;
        db_set_antispam_config(&bot->database, &cfg);
        respond_message(client, interaction, "Anti-spam system **disabled**.");

    } else if (strcmp(subcommand, "set") == 0) {
        struct discord_application_command_interaction_data_options *sub_opts = opts->array[0].options;
        if (!sub_opts || sub_opts->size < 2) {
            respond_message(client, interaction, "Usage: /antispam set <setting> <value>");
            return;
        }

        antispam_config_t cfg;
        db_get_antispam_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);

        const char *setting = NULL;
        const char *value = NULL;

        for (int i = 0; i < sub_opts->size; i++) {
            if (strcmp(sub_opts->array[i].name, "setting") == 0) {
                setting = sub_opts->array[i].value;
            } else if (strcmp(sub_opts->array[i].name, "value") == 0) {
                value = sub_opts->array[i].value;
            }
        }

        if (!setting || !value) {
            respond_message(client, interaction, "Please provide both setting and value.");
            return;
        }

        double dval = atof(value);
        char response[256];

        if (strcmp(setting, "action") == 0) {
            strncpy(cfg.action, value, sizeof(cfg.action) - 1);
            snprintf(response, sizeof(response), "Action set to **%s**", value);
        } else if (strcmp(setting, "silentrole") == 0) {
            strncpy(cfg.silent_role_id, value, sizeof(cfg.silent_role_id) - 1);
            snprintf(response, sizeof(response), "Silent role set to **%s**", value);
        } else if (strcmp(setting, "base") == 0) {
            cfg.base_pressure = dval;
            snprintf(response, sizeof(response), "Base pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "image") == 0) {
            cfg.image_pressure = dval;
            snprintf(response, sizeof(response), "Image pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "link") == 0) {
            cfg.link_pressure = dval;
            snprintf(response, sizeof(response), "Link pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "ping") == 0) {
            cfg.ping_pressure = dval;
            snprintf(response, sizeof(response), "Ping pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "length") == 0) {
            cfg.length_pressure = dval;
            snprintf(response, sizeof(response), "Length pressure set to **%.4f**", dval);
        } else if (strcmp(setting, "line") == 0) {
            cfg.line_pressure = dval;
            snprintf(response, sizeof(response), "Line pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "repeat") == 0) {
            cfg.repeat_pressure = dval;
            snprintf(response, sizeof(response), "Repeat pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "max") == 0) {
            cfg.max_pressure = dval;
            snprintf(response, sizeof(response), "Max pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "decay") == 0) {
            cfg.pressure_decay = dval;
            snprintf(response, sizeof(response), "Pressure decay set to **%.1f** seconds", dval);
        } else {
            respond_message(client, interaction, "Unknown setting. Valid: action, silentrole, base, image, link, ping, length, line, repeat, max, decay");
            return;
        }

        db_set_antispam_config(&bot->database, &cfg);
        respond_message(client, interaction, response);
    }
}

/* Prefix command handler */
void cmd_antispam_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    if (!args || !*args) {
        /* Show status */
        antispam_config_t cfg;
        if (db_get_antispam_config(&bot->database, guild_id_str, &cfg) != 0) {
            struct discord_create_message params = { .content = "Failed to get anti-spam config." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response[1024];
        snprintf(response, sizeof(response),
            "**Anti-Spam Status**\n"
            "Enabled: %s\n"
            "Base: %.1f | Image: %.1f | Link: %.1f | Ping: %.1f\n"
            "Length: %.4f | Line: %.1f | Repeat: %.1f\n"
            "Max: %.1f | Decay: %.1fs\n"
            "Action: %s",
            cfg.enabled ? "Yes" : "No",
            cfg.base_pressure, cfg.image_pressure, cfg.link_pressure, cfg.ping_pressure,
            cfg.length_pressure, cfg.line_pressure, cfg.repeat_pressure,
            cfg.max_pressure, cfg.pressure_decay,
            cfg.action[0] ? cfg.action : "none");

        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse command */
    char cmd[32], setting[32], value[64];
    int parsed = sscanf(args, "%31s %31s %63s", cmd, setting, value);

    if (strcmp(cmd, "enable") == 0) {
        antispam_config_t cfg;
        db_get_antispam_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 1;
        if (cfg.base_pressure <= 0) cfg.base_pressure = 10.0;
        if (cfg.image_pressure <= 0) cfg.image_pressure = 8.5;
        if (cfg.link_pressure <= 0) cfg.link_pressure = 10.0;
        if (cfg.ping_pressure <= 0) cfg.ping_pressure = 2.5;
        if (cfg.length_pressure <= 0) cfg.length_pressure = 0.00625;
        if (cfg.line_pressure <= 0) cfg.line_pressure = 0.714;
        if (cfg.repeat_pressure <= 0) cfg.repeat_pressure = 10.0;
        if (cfg.max_pressure <= 0) cfg.max_pressure = 60.0;
        if (cfg.pressure_decay <= 0) cfg.pressure_decay = 2.5;
        if (!cfg.action[0]) strcpy(cfg.action, "silence");
        db_set_antispam_config(&bot->database, &cfg);

        struct discord_create_message params = { .content = "Anti-spam system **enabled**." };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "disable") == 0) {
        antispam_config_t cfg;
        db_get_antispam_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 0;
        db_set_antispam_config(&bot->database, &cfg);

        struct discord_create_message params = { .content = "Anti-spam system **disabled**." };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "set") == 0 && parsed >= 3) {
        antispam_config_t cfg;
        db_get_antispam_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);

        double dval = atof(value);
        char response[256];

        if (strcmp(setting, "action") == 0) {
            strncpy(cfg.action, value, sizeof(cfg.action) - 1);
            snprintf(response, sizeof(response), "Action set to **%s**", value);
        } else if (strcmp(setting, "silentrole") == 0) {
            strncpy(cfg.silent_role_id, value, sizeof(cfg.silent_role_id) - 1);
            snprintf(response, sizeof(response), "Silent role set to **%s**", value);
        } else if (strcmp(setting, "base") == 0) {
            cfg.base_pressure = dval;
            snprintf(response, sizeof(response), "Base pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "max") == 0) {
            cfg.max_pressure = dval;
            snprintf(response, sizeof(response), "Max pressure set to **%.1f**", dval);
        } else if (strcmp(setting, "decay") == 0) {
            cfg.pressure_decay = dval;
            snprintf(response, sizeof(response), "Decay set to **%.1f** seconds", dval);
        } else {
            snprintf(response, sizeof(response), "Unknown setting: %s", setting);
        }

        db_set_antispam_config(&bot->database, &cfg);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else {
        struct discord_create_message params = {
            .content = "Usage: antispam [enable|disable|set <setting> <value>]\n"
                       "Settings: action, silentrole, base, max, decay"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void register_antispam_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "antispam", "Configure the anti-spam pressure system", "Moderation", cmd_antispam, cmd_antispam_prefix, 1, DISCORD_PERM_MANAGE_GUILD },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
