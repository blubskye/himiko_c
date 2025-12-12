/*
 * Himiko Discord Bot (C Edition) - Settings Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/settings.h"
#include "bot.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cmd_setprefix(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *new_prefix = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "prefix") == 0) {
                new_prefix = opts->array[i].value;
            }
        }
    }

    if (!new_prefix || !*new_prefix) {
        respond_ephemeral(client, interaction, "Please specify a prefix.");
        return;
    }

    if (strlen(new_prefix) > 10) {
        respond_ephemeral(client, interaction, "Prefix must be 10 characters or less.");
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    db_set_prefix(&g_bot->database, guild_id_str, new_prefix);

    char response[256];
    snprintf(response, sizeof(response), "Prefix changed to `%s`", new_prefix);
    respond_message(client, interaction, response);
}

void cmd_setprefix_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setprefix <prefix>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strlen(args) > 10) {
        struct discord_create_message params = { .content = "Prefix must be 10 characters or less." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    db_set_prefix(&g_bot->database, guild_id_str, args);

    char response[256];
    snprintf(response, sizeof(response), "Prefix changed to `%s`", args);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_setmodlog(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake channel_id = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "channel") == 0) {
                channel_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (channel_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a channel.");
        return;
    }

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    /* Get current settings and update mod log channel */
    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.guild_id, guild_id_str, sizeof(settings.guild_id) - 1);
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }
    strncpy(settings.mod_log_channel, channel_id_str, sizeof(settings.mod_log_channel) - 1);
    db_set_guild_settings(&g_bot->database, &settings);

    char response[256];
    snprintf(response, sizeof(response), "Mod log channel set to <#%lu>", (unsigned long)channel_id);
    respond_message(client, interaction, response);
}

void cmd_setmodlog_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setmodlog <#channel>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake channel_id = parse_channel_mention(args);
    if (channel_id == 0) {
        struct discord_create_message params = { .content = "Please mention a valid channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.guild_id, guild_id_str, sizeof(settings.guild_id) - 1);
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }
    strncpy(settings.mod_log_channel, channel_id_str, sizeof(settings.mod_log_channel) - 1);
    db_set_guild_settings(&g_bot->database, &settings);

    char response[256];
    snprintf(response, sizeof(response), "Mod log channel set to <#%lu>", (unsigned long)channel_id);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_setwelcome(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake channel_id = 0;
    const char *message = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "channel") == 0) {
                channel_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "message") == 0) {
                message = opts->array[i].value;
            }
        }
    }

    if (channel_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a channel.");
        return;
    }

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.guild_id, guild_id_str, sizeof(settings.guild_id) - 1);
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }
    strncpy(settings.welcome_channel, channel_id_str, sizeof(settings.welcome_channel) - 1);
    if (message && *message) {
        strncpy(settings.welcome_message, message, sizeof(settings.welcome_message) - 1);
    } else {
        strncpy(settings.welcome_message, "Welcome to the server, {user}!", sizeof(settings.welcome_message) - 1);
    }
    db_set_guild_settings(&g_bot->database, &settings);

    char response[512];
    snprintf(response, sizeof(response),
        "Welcome channel set to <#%lu>\nMessage: %s",
        (unsigned long)channel_id, settings.welcome_message);
    respond_message(client, interaction, response);
}

void cmd_setwelcome_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setwelcome <#channel> [message]\nUse {user} for the user mention." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse channel from args */
    char channel_str[64];
    const char *message = NULL;
    if (sscanf(args, "%63s", channel_str) == 1) {
        message = args + strlen(channel_str);
        while (*message == ' ') message++;
        if (!*message) message = NULL;
    }

    u64snowflake channel_id = parse_channel_mention(channel_str);
    if (channel_id == 0) {
        struct discord_create_message params = { .content = "Please mention a valid channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.guild_id, guild_id_str, sizeof(settings.guild_id) - 1);
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }
    strncpy(settings.welcome_channel, channel_id_str, sizeof(settings.welcome_channel) - 1);
    if (message && *message) {
        strncpy(settings.welcome_message, message, sizeof(settings.welcome_message) - 1);
    } else {
        strncpy(settings.welcome_message, "Welcome to the server, {user}!", sizeof(settings.welcome_message) - 1);
    }
    db_set_guild_settings(&g_bot->database, &settings);

    char response[512];
    snprintf(response, sizeof(response),
        "Welcome channel set to <#%lu>\nMessage: %s",
        (unsigned long)channel_id, settings.welcome_message);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_disablewelcome(struct discord *client, const struct discord_interaction *interaction) {
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        respond_message(client, interaction, "Welcome messages are not configured.");
        return;
    }

    settings.welcome_channel[0] = '\0';
    settings.welcome_message[0] = '\0';
    db_set_guild_settings(&g_bot->database, &settings);

    respond_message(client, interaction, "Welcome messages disabled.");
}

void cmd_disablewelcome_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        struct discord_create_message params = { .content = "Welcome messages are not configured." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    settings.welcome_channel[0] = '\0';
    settings.welcome_message[0] = '\0';
    db_set_guild_settings(&g_bot->database, &settings);

    struct discord_create_message params = { .content = "Welcome messages disabled." };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_setjoindm(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *title = NULL;
    const char *message = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "title") == 0) {
                title = opts->array[i].value;
            } else if (strcmp(opts->array[i].name, "message") == 0) {
                message = opts->array[i].value;
            }
        }
    }

    if (!message || !*message) {
        respond_ephemeral(client, interaction, "Please specify a message.");
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.guild_id, guild_id_str, sizeof(settings.guild_id) - 1);
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }

    if (title && *title) {
        strncpy(settings.join_dm_title, title, sizeof(settings.join_dm_title) - 1);
    } else {
        strncpy(settings.join_dm_title, "Welcome!", sizeof(settings.join_dm_title) - 1);
    }
    strncpy(settings.join_dm_message, message, sizeof(settings.join_dm_message) - 1);
    db_set_guild_settings(&g_bot->database, &settings);

    char response[512];
    snprintf(response, sizeof(response),
        "Join DM configured!\n**Title:** %s\n**Message:** %s",
        settings.join_dm_title, settings.join_dm_message);
    respond_message(client, interaction, response);
}

void cmd_setjoindm_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setjoindm <message>\nUse {user} for the username, {server} for the server name." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.guild_id, guild_id_str, sizeof(settings.guild_id) - 1);
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }

    strncpy(settings.join_dm_title, "Welcome!", sizeof(settings.join_dm_title) - 1);
    strncpy(settings.join_dm_message, args, sizeof(settings.join_dm_message) - 1);
    db_set_guild_settings(&g_bot->database, &settings);

    char response[512];
    snprintf(response, sizeof(response),
        "Join DM configured!\n**Title:** %s\n**Message:** %s",
        settings.join_dm_title, settings.join_dm_message);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_disablejoindm(struct discord *client, const struct discord_interaction *interaction) {
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        respond_message(client, interaction, "Join DMs are not configured.");
        return;
    }

    settings.join_dm_title[0] = '\0';
    settings.join_dm_message[0] = '\0';
    db_set_guild_settings(&g_bot->database, &settings);

    respond_message(client, interaction, "Join DMs disabled.");
}

void cmd_disablejoindm_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        struct discord_create_message params = { .content = "Join DMs are not configured." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    settings.join_dm_title[0] = '\0';
    settings.join_dm_message[0] = '\0';
    db_set_guild_settings(&g_bot->database, &settings);

    struct discord_create_message params = { .content = "Join DMs disabled." };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_settings(struct discord *client, const struct discord_interaction *interaction) {
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response),
        "**Server Settings**\n\n"
        "**Prefix:** `%s`\n",
        settings.prefix[0] ? settings.prefix : g_bot->config.prefix);

    if (settings.mod_log_channel[0]) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Mod Log:** <#%s>\n", settings.mod_log_channel);
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Mod Log:** Not set\n");
    }

    if (settings.welcome_channel[0]) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Welcome Channel:** <#%s>\n"
            "**Welcome Message:** %s\n",
            settings.welcome_channel,
            settings.welcome_message[0] ? settings.welcome_message : "Not set");
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Welcome:** Disabled\n");
    }

    if (settings.join_dm_message[0]) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Join DM:** Enabled\n"
            "**DM Title:** %s\n",
            settings.join_dm_title[0] ? settings.join_dm_title : "Welcome!");
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Join DM:** Disabled\n");
    }

    respond_message(client, interaction, response);
}

void cmd_settings_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    guild_settings_t settings;
    if (db_get_guild_settings(&g_bot->database, guild_id_str, &settings) != 0) {
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.prefix, g_bot->config.prefix, sizeof(settings.prefix) - 1);
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response),
        "**Server Settings**\n\n"
        "**Prefix:** `%s`\n",
        settings.prefix[0] ? settings.prefix : g_bot->config.prefix);

    if (settings.mod_log_channel[0]) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Mod Log:** <#%s>\n", settings.mod_log_channel);
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Mod Log:** Not set\n");
    }

    if (settings.welcome_channel[0]) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Welcome Channel:** <#%s>\n"
            "**Welcome Message:** %s\n",
            settings.welcome_channel,
            settings.welcome_message[0] ? settings.welcome_message : "Not set");
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Welcome:** Disabled\n");
    }

    if (settings.join_dm_message[0]) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Join DM:** Enabled\n"
            "**DM Title:** %s\n",
            settings.join_dm_title[0] ? settings.join_dm_title : "Welcome!");
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**Join DM:** Disabled\n");
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_settings_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "setprefix", "Set the bot prefix for this server", "Settings", cmd_setprefix, cmd_setprefix_prefix, 0, 0 },
        { "setmodlog", "Set the moderation log channel", "Settings", cmd_setmodlog, cmd_setmodlog_prefix, 0, 0 },
        { "setwelcome", "Set the welcome channel and message", "Settings", cmd_setwelcome, cmd_setwelcome_prefix, 0, 0 },
        { "disablewelcome", "Disable welcome messages", "Settings", cmd_disablewelcome, cmd_disablewelcome_prefix, 0, 0 },
        { "setjoindm", "Set the join DM message", "Settings", cmd_setjoindm, cmd_setjoindm_prefix, 0, 0 },
        { "disablejoindm", "Disable join DMs", "Settings", cmd_disablejoindm, cmd_disablejoindm_prefix, 0, 0 },
        { "settings", "View server settings", "Settings", cmd_settings, cmd_settings_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
