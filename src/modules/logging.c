/*
 * Himiko Discord Bot (C Edition) - Logging Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/logging.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void logging_init(himiko_bot_t *bot) {
    (void)bot;
}

void logging_cleanup(himiko_bot_t *bot) {
    (void)bot;
}

/* Check if logging is enabled for a guild and channel */
static int should_log(himiko_database_t *db, const char *guild_id, const char *channel_id, logging_config_t *cfg) {
    if (db_get_logging_config(db, guild_id, cfg) != 0) return 0;
    if (!cfg->enabled) return 0;
    if (!cfg->log_channel_id[0]) return 0;

    /* Check if channel is disabled */
    int disabled = 0;
    db_is_log_channel_disabled(db, guild_id, channel_id, &disabled);
    return !disabled;
}

/* Send a log embed to the log channel */
static void send_log_embed(struct discord *client, const char *channel_id, const char *title, const char *description, int color) {
    u64snowflake log_channel = strtoull(channel_id, NULL, 10);

    struct discord_embed embed = {
        .title = (char *)title,
        .description = (char *)description,
        .color = color
    };

    struct discord_create_message params = {
        .embeds = &(struct discord_embeds){ .size = 1, .array = &embed }
    };

    discord_create_message(client, log_channel, &params, NULL);
}

/* Log message delete */
void logging_on_message_delete(struct discord *client, u64snowflake guild_id, u64snowflake channel_id, const struct discord_message *msg) {
    if (!msg) return;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    logging_config_t cfg;
    if (!should_log(&bot->database, guild_id_str, channel_id_str, &cfg)) return;
    if (!cfg.message_delete) return;

    /* Build description */
    char description[1024];
    if (msg->author) {
        char user_id_str[32];
        snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);
        snprintf(description, sizeof(description),
            "**Author:** <@%s> (%s)\n"
            "**Channel:** <#%s>\n"
            "**Content:** %s",
            user_id_str,
            msg->author->username ? msg->author->username : "Unknown",
            channel_id_str,
            msg->content ? msg->content : "*No content*");
    } else {
        snprintf(description, sizeof(description),
            "**Channel:** <#%s>\n"
            "**Content:** %s",
            channel_id_str,
            msg->content ? msg->content : "*No content*");
    }

    send_log_embed(client, cfg.log_channel_id, "Message Deleted", description, 0xFF0000);
}

/* Log message edit */
void logging_on_message_update(struct discord *client, const struct discord_message *old_msg, const struct discord_message *new_msg) {
    if (!new_msg || !new_msg->guild_id) return;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)new_msg->guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)new_msg->channel_id);

    logging_config_t cfg;
    if (!should_log(&bot->database, guild_id_str, channel_id_str, &cfg)) return;
    if (!cfg.message_edit) return;

    /* Build description */
    char description[2048];
    char user_id_str[32] = "Unknown";
    if (new_msg->author) {
        snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)new_msg->author->id);
    }

    snprintf(description, sizeof(description),
        "**Author:** <@%s>\n"
        "**Channel:** <#%s>\n"
        "**Before:** %s\n"
        "**After:** %s",
        user_id_str,
        channel_id_str,
        old_msg && old_msg->content ? old_msg->content : "*Unknown*",
        new_msg->content ? new_msg->content : "*No content*");

    send_log_embed(client, cfg.log_channel_id, "Message Edited", description, 0xFFA500);
}

/* Log voice state changes */
void logging_on_voice_state_update(struct discord *client, const struct discord_voice_state *old_state, const struct discord_voice_state *new_state) {
    if (!new_state || !new_state->guild_id) return;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)new_state->guild_id);

    logging_config_t cfg;
    if (db_get_logging_config(&bot->database, guild_id_str, &cfg) != 0) return;
    if (!cfg.enabled || !cfg.log_channel_id[0]) return;

    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)new_state->user_id);

    /* Determine what changed */
    u64snowflake old_channel = old_state ? old_state->channel_id : 0;
    u64snowflake new_channel = new_state->channel_id;

    char description[512];

    if (old_channel == 0 && new_channel != 0 && cfg.voice_join) {
        /* User joined */
        snprintf(description, sizeof(description),
            "<@%s> joined voice channel <#%lu>",
            user_id_str, (unsigned long)new_channel);
        send_log_embed(client, cfg.log_channel_id, "Voice Join", description, 0x00FF00);

    } else if (old_channel != 0 && new_channel == 0 && cfg.voice_leave) {
        /* User left */
        snprintf(description, sizeof(description),
            "<@%s> left voice channel <#%lu>",
            user_id_str, (unsigned long)old_channel);
        send_log_embed(client, cfg.log_channel_id, "Voice Leave", description, 0xFF0000);

    } else if (old_channel != 0 && new_channel != 0 && old_channel != new_channel) {
        /* User moved channels */
        if (cfg.voice_leave) {
            snprintf(description, sizeof(description),
                "<@%s> moved from <#%lu> to <#%lu>",
                user_id_str, (unsigned long)old_channel, (unsigned long)new_channel);
            send_log_embed(client, cfg.log_channel_id, "Voice Move", description, 0xFFA500);
        }
    }
}

/* Log guild member updates (nickname changes) */
void logging_on_guild_member_update(struct discord *client, const struct discord_guild_member *old_member, const struct discord_guild_member *new_member) {
    if (!new_member || !new_member->user) return;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Need guild_id from somewhere - this may need adjustment based on how events are dispatched */
    /* For now, this is a stub that would need the guild_id passed in */
    (void)old_member;
}

/* Command handlers */
void cmd_setlogchannel(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size == 0) {
        respond_message(client, interaction, "Please specify a channel.");
        return;
    }

    const char *channel_id = opts->array[0].value;

    if (db_set_log_channel(&bot->database, guild_id_str, channel_id) != 0) {
        respond_message(client, interaction, "Failed to set log channel.");
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Log channel set to <#%s>", channel_id);
    respond_message(client, interaction, response);
}

void cmd_setlogchannel_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setlogchannel <#channel>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Parse channel mention or ID */
    char channel_id[32] = {0};
    if (args[0] == '<' && args[1] == '#') {
        /* Channel mention <#123456> */
        const char *start = args + 2;
        const char *end = strchr(start, '>');
        if (end) {
            size_t len = end - start;
            if (len < sizeof(channel_id)) {
                strncpy(channel_id, start, len);
            }
        }
    } else {
        strncpy(channel_id, args, sizeof(channel_id) - 1);
    }

    if (!channel_id[0]) {
        struct discord_create_message params = { .content = "Invalid channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (db_set_log_channel(&bot->database, guild_id_str, channel_id) != 0) {
        struct discord_create_message params = { .content = "Failed to set log channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Log channel set to <#%s>", channel_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_togglelogging(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    int enabled = 1;
    if (opts && opts->size > 0) {
        enabled = strcmp(opts->array[0].value, "true") == 0;
    }

    logging_config_t cfg;
    db_get_logging_config(&bot->database, guild_id_str, &cfg);
    strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
    cfg.enabled = enabled;
    db_set_logging_config(&bot->database, &cfg);

    char response[64];
    snprintf(response, sizeof(response), "Logging has been **%s**", enabled ? "enabled" : "disabled");
    respond_message(client, interaction, response);
}

void cmd_togglelogging_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    int enabled = 1;
    if (args && *args) {
        if (strcmp(args, "off") == 0 || strcmp(args, "false") == 0 || strcmp(args, "0") == 0) {
            enabled = 0;
        }
    }

    logging_config_t cfg;
    db_get_logging_config(&bot->database, guild_id_str, &cfg);
    strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
    cfg.enabled = enabled;
    db_set_logging_config(&bot->database, &cfg);

    char response[64];
    snprintf(response, sizeof(response), "Logging has been **%s**", enabled ? "enabled" : "disabled");
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_logconfig(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size < 2) {
        respond_message(client, interaction, "Usage: /logconfig <type> <enabled>");
        return;
    }

    const char *log_type = NULL;
    int enabled = 0;

    for (int i = 0; i < opts->size; i++) {
        if (strcmp(opts->array[i].name, "type") == 0) {
            log_type = opts->array[i].value;
        } else if (strcmp(opts->array[i].name, "enabled") == 0) {
            enabled = strcmp(opts->array[i].value, "true") == 0;
        }
    }

    if (!log_type) {
        respond_message(client, interaction, "Please specify a log type.");
        return;
    }

    logging_config_t cfg;
    db_get_logging_config(&bot->database, guild_id_str, &cfg);
    strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);

    const char *type_name = log_type;
    if (strcmp(log_type, "message_delete") == 0) {
        cfg.message_delete = enabled;
        type_name = "Message Delete";
    } else if (strcmp(log_type, "message_edit") == 0) {
        cfg.message_edit = enabled;
        type_name = "Message Edit";
    } else if (strcmp(log_type, "voice_join") == 0) {
        cfg.voice_join = enabled;
        type_name = "Voice Join";
    } else if (strcmp(log_type, "voice_leave") == 0) {
        cfg.voice_leave = enabled;
        type_name = "Voice Leave";
    } else if (strcmp(log_type, "nickname") == 0) {
        cfg.nickname_change = enabled;
        type_name = "Nickname Change";
    } else if (strcmp(log_type, "avatar") == 0) {
        cfg.avatar_change = enabled;
        type_name = "Avatar Change";
    } else if (strcmp(log_type, "presence") == 0) {
        cfg.presence_change = enabled;
        type_name = "Presence Change";
    } else {
        respond_message(client, interaction, "Unknown log type.");
        return;
    }

    db_set_logging_config(&bot->database, &cfg);

    char response[128];
    snprintf(response, sizeof(response), "**%s** logging has been **%s**", type_name, enabled ? "enabled" : "disabled");
    respond_message(client, interaction, response);
}

void cmd_logconfig_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    if (!args || !*args) {
        struct discord_create_message params = {
            .content = "Usage: logconfig <type> <on|off>\n"
                       "Types: message_delete, message_edit, voice_join, voice_leave, nickname, avatar, presence"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    char log_type[32], enabled_str[16];
    if (sscanf(args, "%31s %15s", log_type, enabled_str) != 2) {
        struct discord_create_message params = { .content = "Usage: logconfig <type> <on|off>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    int enabled = strcmp(enabled_str, "on") == 0 || strcmp(enabled_str, "true") == 0 || strcmp(enabled_str, "1") == 0;

    logging_config_t cfg;
    db_get_logging_config(&bot->database, guild_id_str, &cfg);
    strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);

    const char *type_name = log_type;
    if (strcmp(log_type, "message_delete") == 0) {
        cfg.message_delete = enabled;
        type_name = "Message Delete";
    } else if (strcmp(log_type, "message_edit") == 0) {
        cfg.message_edit = enabled;
        type_name = "Message Edit";
    } else if (strcmp(log_type, "voice_join") == 0) {
        cfg.voice_join = enabled;
        type_name = "Voice Join";
    } else if (strcmp(log_type, "voice_leave") == 0) {
        cfg.voice_leave = enabled;
        type_name = "Voice Leave";
    } else if (strcmp(log_type, "nickname") == 0) {
        cfg.nickname_change = enabled;
        type_name = "Nickname Change";
    } else if (strcmp(log_type, "avatar") == 0) {
        cfg.avatar_change = enabled;
        type_name = "Avatar Change";
    } else if (strcmp(log_type, "presence") == 0) {
        cfg.presence_change = enabled;
        type_name = "Presence Change";
    } else {
        struct discord_create_message params = { .content = "Unknown log type." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    db_set_logging_config(&bot->database, &cfg);

    char response[128];
    snprintf(response, sizeof(response), "**%s** logging has been **%s**", type_name, enabled ? "enabled" : "disabled");
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_logstatus(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    logging_config_t cfg;
    if (db_get_logging_config(&bot->database, guild_id_str, &cfg) != 0) {
        respond_message(client, interaction, "Failed to get logging config.");
        return;
    }

    char response[1024];
    snprintf(response, sizeof(response),
        "**Logging Configuration**\n"
        "Log Channel: %s\n"
        "Enabled: %s\n\n"
        "**Event Types:**\n"
        "Message Delete: %s\n"
        "Message Edit: %s\n"
        "Voice Join: %s\n"
        "Voice Leave: %s\n"
        "Nickname Change: %s\n"
        "Avatar Change: %s\n"
        "Presence Change: %s",
        cfg.log_channel_id[0] ? cfg.log_channel_id : "Not set",
        cfg.enabled ? "Yes" : "No",
        cfg.message_delete ? "Yes" : "No",
        cfg.message_edit ? "Yes" : "No",
        cfg.voice_join ? "Yes" : "No",
        cfg.voice_leave ? "Yes" : "No",
        cfg.nickname_change ? "Yes" : "No",
        cfg.avatar_change ? "Yes" : "No",
        cfg.presence_change ? "Yes" : "No");

    respond_message(client, interaction, response);
}

void cmd_logstatus_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    logging_config_t cfg;
    if (db_get_logging_config(&bot->database, guild_id_str, &cfg) != 0) {
        struct discord_create_message params = { .content = "Failed to get logging config." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char log_channel_display[64];
    if (cfg.log_channel_id[0]) {
        snprintf(log_channel_display, sizeof(log_channel_display), "<#%s>", cfg.log_channel_id);
    } else {
        strcpy(log_channel_display, "Not set");
    }

    char response[1024];
    snprintf(response, sizeof(response),
        "**Logging Configuration**\n"
        "Log Channel: %s\n"
        "Enabled: %s\n\n"
        "**Event Types:**\n"
        "Message Delete: %s | Message Edit: %s\n"
        "Voice Join: %s | Voice Leave: %s\n"
        "Nickname: %s | Avatar: %s | Presence: %s",
        log_channel_display,
        cfg.enabled ? "Yes" : "No",
        cfg.message_delete ? "Yes" : "No",
        cfg.message_edit ? "Yes" : "No",
        cfg.voice_join ? "Yes" : "No",
        cfg.voice_leave ? "Yes" : "No",
        cfg.nickname_change ? "Yes" : "No",
        cfg.avatar_change ? "Yes" : "No",
        cfg.presence_change ? "Yes" : "No");

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_disablechannellog(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size == 0) {
        respond_message(client, interaction, "Please specify a channel.");
        return;
    }

    const char *channel_id = opts->array[0].value;

    if (db_add_disabled_log_channel(&bot->database, guild_id_str, channel_id) != 0) {
        respond_message(client, interaction, "Failed to disable logging for channel.");
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Logging disabled for <#%s>", channel_id);
    respond_message(client, interaction, response);
}

void cmd_disablechannellog_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: disablechannellog <#channel>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Parse channel mention or ID */
    char channel_id[32] = {0};
    if (args[0] == '<' && args[1] == '#') {
        const char *start = args + 2;
        const char *end = strchr(start, '>');
        if (end) {
            size_t len = end - start;
            if (len < sizeof(channel_id)) {
                strncpy(channel_id, start, len);
            }
        }
    } else {
        strncpy(channel_id, args, sizeof(channel_id) - 1);
    }

    if (!channel_id[0]) {
        struct discord_create_message params = { .content = "Invalid channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (db_add_disabled_log_channel(&bot->database, guild_id_str, channel_id) != 0) {
        struct discord_create_message params = { .content = "Failed to disable logging for channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Logging disabled for <#%s>", channel_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_enablechannellog(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size == 0) {
        respond_message(client, interaction, "Please specify a channel.");
        return;
    }

    const char *channel_id = opts->array[0].value;

    if (db_remove_disabled_log_channel(&bot->database, guild_id_str, channel_id) != 0) {
        respond_message(client, interaction, "Failed to enable logging for channel.");
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Logging re-enabled for <#%s>", channel_id);
    respond_message(client, interaction, response);
}

void cmd_enablechannellog_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: enablechannellog <#channel>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Parse channel mention or ID */
    char channel_id[32] = {0};
    if (args[0] == '<' && args[1] == '#') {
        const char *start = args + 2;
        const char *end = strchr(start, '>');
        if (end) {
            size_t len = end - start;
            if (len < sizeof(channel_id)) {
                strncpy(channel_id, start, len);
            }
        }
    } else {
        strncpy(channel_id, args, sizeof(channel_id) - 1);
    }

    if (!channel_id[0]) {
        struct discord_create_message params = { .content = "Invalid channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (db_remove_disabled_log_channel(&bot->database, guild_id_str, channel_id) != 0) {
        struct discord_create_message params = { .content = "Failed to enable logging for channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Logging re-enabled for <#%s>", channel_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_logging_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "setlogchannel", "Set the channel for server logs", "Logging", cmd_setlogchannel, cmd_setlogchannel_prefix, 1, DISCORD_PERM_ADMINISTRATOR },
        { "togglelogging", "Enable or disable logging", "Logging", cmd_togglelogging, cmd_togglelogging_prefix, 1, DISCORD_PERM_ADMINISTRATOR },
        { "logconfig", "Configure which events to log", "Logging", cmd_logconfig, cmd_logconfig_prefix, 1, DISCORD_PERM_ADMINISTRATOR },
        { "logstatus", "View current logging configuration", "Logging", cmd_logstatus, cmd_logstatus_prefix, 1, 0 },
        { "disablechannellog", "Disable logging for a channel", "Logging", cmd_disablechannellog, cmd_disablechannellog_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
        { "enablechannellog", "Re-enable logging for a channel", "Logging", cmd_enablechannellog, cmd_enablechannellog_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
