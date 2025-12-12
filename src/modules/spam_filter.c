/*
 * Himiko Discord Bot (C Edition) - Spam Filter Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/spam_filter.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void spam_filter_init(himiko_bot_t *bot) {
    (void)bot;
}

void spam_filter_cleanup(himiko_bot_t *bot) {
    (void)bot;
}

/* Count mentions in message content */
static int count_mentions(const struct discord_message *msg) {
    int count = 0;

    /* Count user mentions from the mentions array */
    if (msg->mentions) {
        count += msg->mentions->size;
    }

    /* Count @everyone and @here */
    if (msg->mention_everyone) {
        count += 10; /* Heavy penalty for @everyone */
    }

    /* Count role mentions */
    if (msg->mention_roles) {
        count += msg->mention_roles->size;
    }

    return count;
}

/* Count URLs in message content */
static int count_links(const char *content) {
    if (!content) return 0;

    int count = 0;
    const char *p = content;

    while ((p = strstr(p, "http")) != NULL) {
        if (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0) {
            count++;
        }
        p++;
    }

    return count;
}

/* Count emojis in message content */
static int count_emojis(const char *content) {
    if (!content) return 0;

    int count = 0;
    const char *p = content;

    /* Count custom Discord emojis <:name:id> and <a:name:id> */
    while ((p = strchr(p, '<')) != NULL) {
        if ((p[1] == ':' || (p[1] == 'a' && p[2] == ':')) && strchr(p, '>')) {
            count++;
        }
        p++;
    }

    /* Count Unicode emojis (basic detection) */
    /* This is a simplified check for common emoji codepoints */
    p = content;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        /* Check for emoji-like UTF-8 sequences (F0 9F range) */
        if (c == 0xF0 && p[1]) {
            unsigned char c2 = (unsigned char)p[1];
            if (c2 == 0x9F) {
                count++;
                p += 4; /* Skip 4-byte UTF-8 sequence */
                continue;
            }
        }
        /* Check for other emoji ranges */
        if (c >= 0xE2 && c <= 0xEF && p[1] && p[2]) {
            /* Potentially an emoji in 3-byte range */
            unsigned char c2 = (unsigned char)p[1];
            if (c2 >= 0x80 && c2 <= 0xBF) {
                /* Simple heuristic - count sequences that look like symbols */
                p += 3;
                continue;
            }
        }
        p++;
    }

    return count;
}

/* Take action on spam */
static void take_action(struct discord *client, const struct discord_message *msg, spam_filter_config_t *cfg, const char *reason) {
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    if (strcmp(cfg->action, "delete") == 0) {
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);

    } else if (strcmp(cfg->action, "warn") == 0) {
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        /* Could send DM warning here */

    } else if (strcmp(cfg->action, "kick") == 0) {
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        discord_remove_guild_member(client, msg->guild_id, msg->author->id, NULL, NULL);

    } else if (strcmp(cfg->action, "ban") == 0) {
        discord_delete_message(client, msg->channel_id, msg->id, NULL, NULL);
        struct discord_create_guild_ban params = { .delete_message_days = 1, .reason = (char *)reason };
        discord_create_guild_ban(client, msg->guild_id, msg->author->id, &params, NULL);
    }
}

/* Main filter check - returns 1 if blocked */
int spam_filter_check(struct discord *client, const struct discord_message *msg) {
    if (!msg || !msg->author) return 0;

    /* Ignore bots */
    if (msg->author->bot) return 0;

    /* Ignore DMs */
    if (msg->guild_id == 0) return 0;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return 0;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Get config */
    spam_filter_config_t cfg;
    if (db_get_spam_filter_config(&bot->database, guild_id_str, &cfg) != 0 || !cfg.enabled) {
        return 0;
    }

    /* Check mentions */
    if (cfg.max_mentions > 0) {
        int mentions = count_mentions(msg);
        if (mentions > cfg.max_mentions) {
            take_action(client, msg, &cfg, "Too many mentions");
            return 1;
        }
    }

    /* Check links */
    if (cfg.max_links > 0) {
        int links = count_links(msg->content);
        if (links > cfg.max_links) {
            take_action(client, msg, &cfg, "Too many links");
            return 1;
        }
    }

    /* Check emojis */
    if (cfg.max_emojis > 0) {
        int emojis = count_emojis(msg->content);
        if (emojis > cfg.max_emojis) {
            take_action(client, msg, &cfg, "Too many emojis");
            return 1;
        }
    }

    return 0;
}

/* Command: spamfilter enable/disable/set/status */
void cmd_spamfilter(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;

    if (!opts || opts->size == 0) {
        /* Show status */
        spam_filter_config_t cfg;
        if (db_get_spam_filter_config(&bot->database, guild_id_str, &cfg) != 0) {
            respond_message(client, interaction, "Failed to get spam filter config.");
            return;
        }

        char response[512];
        snprintf(response, sizeof(response),
            "**Spam Filter Status**\n"
            "Enabled: %s\n"
            "Max Mentions: %d %s\n"
            "Max Links: %d %s\n"
            "Max Emojis: %d %s\n"
            "Action: %s",
            cfg.enabled ? "Yes" : "No",
            cfg.max_mentions, cfg.max_mentions <= 0 ? "(disabled)" : "",
            cfg.max_links, cfg.max_links <= 0 ? "(disabled)" : "",
            cfg.max_emojis, cfg.max_emojis <= 0 ? "(disabled)" : "",
            cfg.action[0] ? cfg.action : "delete");

        respond_message(client, interaction, response);
        return;
    }

    const char *subcommand = opts->array[0].name;

    if (strcmp(subcommand, "enable") == 0) {
        spam_filter_config_t cfg;
        db_get_spam_filter_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 1;
        /* Set defaults if not configured */
        if (cfg.max_mentions <= 0) cfg.max_mentions = 10;
        if (cfg.max_links <= 0) cfg.max_links = 5;
        if (cfg.max_emojis <= 0) cfg.max_emojis = 20;
        if (!cfg.action[0]) strcpy(cfg.action, "delete");
        db_set_spam_filter_config(&bot->database, &cfg);
        respond_message(client, interaction, "Spam filter **enabled**.");

    } else if (strcmp(subcommand, "disable") == 0) {
        spam_filter_config_t cfg;
        db_get_spam_filter_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 0;
        db_set_spam_filter_config(&bot->database, &cfg);
        respond_message(client, interaction, "Spam filter **disabled**.");

    } else if (strcmp(subcommand, "set") == 0) {
        struct discord_application_command_interaction_data_options *sub_opts = opts->array[0].options;
        if (!sub_opts || sub_opts->size < 2) {
            respond_message(client, interaction, "Usage: /spamfilter set <setting> <value>");
            return;
        }

        spam_filter_config_t cfg;
        db_get_spam_filter_config(&bot->database, guild_id_str, &cfg);
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

        char response[128];
        int ival = atoi(value);

        if (strcmp(setting, "mentions") == 0) {
            cfg.max_mentions = ival;
            snprintf(response, sizeof(response), "Max mentions set to **%d**", ival);
        } else if (strcmp(setting, "links") == 0) {
            cfg.max_links = ival;
            snprintf(response, sizeof(response), "Max links set to **%d**", ival);
        } else if (strcmp(setting, "emojis") == 0) {
            cfg.max_emojis = ival;
            snprintf(response, sizeof(response), "Max emojis set to **%d**", ival);
        } else if (strcmp(setting, "action") == 0) {
            strncpy(cfg.action, value, sizeof(cfg.action) - 1);
            snprintf(response, sizeof(response), "Action set to **%s**", value);
        } else {
            respond_message(client, interaction, "Unknown setting. Valid: mentions, links, emojis, action");
            return;
        }

        db_set_spam_filter_config(&bot->database, &cfg);
        respond_message(client, interaction, response);
    }
}

/* Prefix command handler */
void cmd_spamfilter_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    if (!args || !*args) {
        /* Show status */
        spam_filter_config_t cfg;
        if (db_get_spam_filter_config(&bot->database, guild_id_str, &cfg) != 0) {
            struct discord_create_message params = { .content = "Failed to get config." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response[512];
        snprintf(response, sizeof(response),
            "**Spam Filter Status**\n"
            "Enabled: %s | Action: %s\n"
            "Max Mentions: %d | Max Links: %d | Max Emojis: %d",
            cfg.enabled ? "Yes" : "No",
            cfg.action[0] ? cfg.action : "delete",
            cfg.max_mentions, cfg.max_links, cfg.max_emojis);

        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse command */
    char cmd[32], setting[32], value[32];
    int parsed = sscanf(args, "%31s %31s %31s", cmd, setting, value);

    if (strcmp(cmd, "enable") == 0) {
        spam_filter_config_t cfg;
        db_get_spam_filter_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 1;
        if (cfg.max_mentions <= 0) cfg.max_mentions = 10;
        if (cfg.max_links <= 0) cfg.max_links = 5;
        if (cfg.max_emojis <= 0) cfg.max_emojis = 20;
        if (!cfg.action[0]) strcpy(cfg.action, "delete");
        db_set_spam_filter_config(&bot->database, &cfg);

        struct discord_create_message params = { .content = "Spam filter **enabled**." };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "disable") == 0) {
        spam_filter_config_t cfg;
        db_get_spam_filter_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        cfg.enabled = 0;
        db_set_spam_filter_config(&bot->database, &cfg);

        struct discord_create_message params = { .content = "Spam filter **disabled**." };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "set") == 0 && parsed >= 3) {
        spam_filter_config_t cfg;
        db_get_spam_filter_config(&bot->database, guild_id_str, &cfg);
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);

        char response[128];
        int ival = atoi(value);

        if (strcmp(setting, "mentions") == 0) {
            cfg.max_mentions = ival;
            snprintf(response, sizeof(response), "Max mentions set to **%d**", ival);
        } else if (strcmp(setting, "links") == 0) {
            cfg.max_links = ival;
            snprintf(response, sizeof(response), "Max links set to **%d**", ival);
        } else if (strcmp(setting, "emojis") == 0) {
            cfg.max_emojis = ival;
            snprintf(response, sizeof(response), "Max emojis set to **%d**", ival);
        } else if (strcmp(setting, "action") == 0) {
            strncpy(cfg.action, value, sizeof(cfg.action) - 1);
            snprintf(response, sizeof(response), "Action set to **%s**", value);
        } else {
            snprintf(response, sizeof(response), "Unknown setting: %s", setting);
        }

        db_set_spam_filter_config(&bot->database, &cfg);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else {
        struct discord_create_message params = {
            .content = "Usage: spamfilter [enable|disable|set <setting> <value>]\n"
                       "Settings: mentions, links, emojis, action"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void register_spam_filter_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "spamfilter", "Configure the spam filter", "Moderation", cmd_spamfilter, cmd_spamfilter_prefix, 1, DISCORD_PERM_MANAGE_GUILD },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
