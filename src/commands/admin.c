/*
 * Himiko Discord Bot (C Edition) - Admin Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/admin.h"
#include "bot.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void cmd_kick(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;
    const char *reason = "No reason provided";

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "reason") == 0) {
                reason = opts->array[i].value;
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user to kick.");
        return;
    }

    struct discord_remove_guild_member kick_params = { .reason = (char *)reason };
    discord_remove_guild_member(client, interaction->guild_id, user_id, &kick_params, NULL);

    /* Log the action */
    mod_action_t action = {0};
    snprintf(action.guild_id, sizeof(action.guild_id), "%lu", (unsigned long)interaction->guild_id);
    snprintf(action.moderator_id, sizeof(action.moderator_id), "%lu", (unsigned long)interaction->member->user->id);
    snprintf(action.target_id, sizeof(action.target_id), "%lu", (unsigned long)user_id);
    strcpy(action.action, "kick");
    strncpy(action.reason, reason, sizeof(action.reason) - 1);
    action.timestamp = time(NULL);
    db_add_mod_action(&g_bot->database, &action);

    char response[512];
    snprintf(response, sizeof(response),
        "**Kicked!**\n\n"
        "**User:** <@%lu>\n**Moderator:** <@%lu>\n**Reason:** %s",
        (unsigned long)user_id, (unsigned long)interaction->member->user->id, reason);

    respond_message(client, interaction, response);
}

void cmd_kick_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    char user_mention[64];
    char reason[512] = "No reason provided";
    u64snowflake user_id;

    if (!args || strlen(args) == 0) {
        struct discord_create_message params = { .content = "Usage: kick <user> [reason]" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (sscanf(args, "%63s", user_mention) != 1) {
        struct discord_create_message params = { .content = "Please specify a user to kick." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    user_id = parse_user_mention(user_mention);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Get reason if provided */
    const char *reason_start = args + strlen(user_mention);
    while (*reason_start == ' ') reason_start++;
    if (*reason_start) {
        strncpy(reason, reason_start, sizeof(reason) - 1);
    }

    struct discord_remove_guild_member kick_params = { .reason = reason };
    discord_remove_guild_member(client, msg->guild_id, user_id, &kick_params, NULL);

    /* Log the action */
    mod_action_t action = {0};
    snprintf(action.guild_id, sizeof(action.guild_id), "%lu", (unsigned long)msg->guild_id);
    snprintf(action.moderator_id, sizeof(action.moderator_id), "%lu", (unsigned long)msg->author->id);
    snprintf(action.target_id, sizeof(action.target_id), "%lu", (unsigned long)user_id);
    strcpy(action.action, "kick");
    strncpy(action.reason, reason, sizeof(action.reason) - 1);
    action.timestamp = time(NULL);
    db_add_mod_action(&g_bot->database, &action);

    char response[512];
    snprintf(response, sizeof(response),
        "**Kicked!**\n\n"
        "**User:** <@%lu>\n**Moderator:** <@%lu>\n**Reason:** %s",
        (unsigned long)user_id, (unsigned long)msg->author->id, reason);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_ban(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;
    const char *reason = "No reason provided";

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "reason") == 0) {
                reason = opts->array[i].value;
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user to ban.");
        return;
    }

    struct discord_create_guild_ban params = { .delete_message_days = 0 };
    discord_create_guild_ban(client, interaction->guild_id, user_id, &params, NULL);

    /* Log the action */
    mod_action_t action = {0};
    snprintf(action.guild_id, sizeof(action.guild_id), "%lu", (unsigned long)interaction->guild_id);
    snprintf(action.moderator_id, sizeof(action.moderator_id), "%lu", (unsigned long)interaction->member->user->id);
    snprintf(action.target_id, sizeof(action.target_id), "%lu", (unsigned long)user_id);
    strcpy(action.action, "ban");
    strncpy(action.reason, reason, sizeof(action.reason) - 1);
    action.timestamp = time(NULL);
    db_add_mod_action(&g_bot->database, &action);

    char response[512];
    snprintf(response, sizeof(response),
        "**Banned!**\n\n"
        "**User:** <@%lu>\n**Moderator:** <@%lu>\n**Reason:** %s",
        (unsigned long)user_id, (unsigned long)interaction->member->user->id, reason);

    respond_message(client, interaction, response);
}

void cmd_ban_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    char user_mention[64];
    char reason[512] = "No reason provided";
    u64snowflake user_id;

    if (!args || strlen(args) == 0) {
        struct discord_create_message params = { .content = "Usage: ban <user> [reason]" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (sscanf(args, "%63s", user_mention) != 1) {
        struct discord_create_message params = { .content = "Please specify a user to ban." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    user_id = parse_user_mention(user_mention);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    const char *reason_start = args + strlen(user_mention);
    while (*reason_start == ' ') reason_start++;
    if (*reason_start) {
        strncpy(reason, reason_start, sizeof(reason) - 1);
    }

    struct discord_create_guild_ban ban_params = { .delete_message_days = 0 };
    discord_create_guild_ban(client, msg->guild_id, user_id, &ban_params, NULL);

    /* Log the action */
    mod_action_t action = {0};
    snprintf(action.guild_id, sizeof(action.guild_id), "%lu", (unsigned long)msg->guild_id);
    snprintf(action.moderator_id, sizeof(action.moderator_id), "%lu", (unsigned long)msg->author->id);
    snprintf(action.target_id, sizeof(action.target_id), "%lu", (unsigned long)user_id);
    strcpy(action.action, "ban");
    strncpy(action.reason, reason, sizeof(action.reason) - 1);
    action.timestamp = time(NULL);
    db_add_mod_action(&g_bot->database, &action);

    char response[512];
    snprintf(response, sizeof(response),
        "**Banned!**\n\n"
        "**User:** <@%lu>\n**Moderator:** <@%lu>\n**Reason:** %s",
        (unsigned long)user_id, (unsigned long)msg->author->id, reason);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_unban(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user_id") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user ID to unban.");
        return;
    }

    struct discord_remove_guild_ban unban_params = { .reason = NULL };
    discord_remove_guild_ban(client, interaction->guild_id, user_id, &unban_params, NULL);

    char response[256];
    snprintf(response, sizeof(response), "Unbanned <@%lu>", (unsigned long)user_id);
    respond_message(client, interaction, response);
}

void cmd_unban_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || strlen(args) == 0) {
        struct discord_create_message params = { .content = "Usage: unban <user_id>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake user_id = strtoull(args, NULL, 10);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Invalid user ID." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    struct discord_remove_guild_ban unban_params = { .reason = NULL };
    discord_remove_guild_ban(client, msg->guild_id, user_id, &unban_params, NULL);

    char response[256];
    snprintf(response, sizeof(response), "Unbanned <@%lu>", (unsigned long)user_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_timeout(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;
    int64_t minutes = 5;
    const char *reason = "No reason provided";

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "minutes") == 0) {
                minutes = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "reason") == 0) {
                reason = opts->array[i].value;
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user to timeout.");
        return;
    }

    /* Calculate timeout timestamp in milliseconds */
    u64unix_ms timeout_until = (u64unix_ms)(time(NULL) + (minutes * 60)) * 1000;

    struct discord_modify_guild_member params = {
        .communication_disabled_until = timeout_until
    };
    discord_modify_guild_member(client, interaction->guild_id, user_id, &params, NULL);

    /* Log the action */
    mod_action_t action = {0};
    snprintf(action.guild_id, sizeof(action.guild_id), "%lu", (unsigned long)interaction->guild_id);
    snprintf(action.moderator_id, sizeof(action.moderator_id), "%lu", (unsigned long)interaction->member->user->id);
    snprintf(action.target_id, sizeof(action.target_id), "%lu", (unsigned long)user_id);
    strcpy(action.action, "timeout");
    snprintf(action.reason, sizeof(action.reason), "%s (%ld minutes)", reason, (long)minutes);
    action.timestamp = time(NULL);
    db_add_mod_action(&g_bot->database, &action);

    char response[512];
    snprintf(response, sizeof(response),
        "**Timed Out!**\n\n"
        "**User:** <@%lu>\n**Duration:** %ld minutes\n**Moderator:** <@%lu>\n**Reason:** %s",
        (unsigned long)user_id, (long)minutes, (unsigned long)interaction->member->user->id, reason);

    respond_message(client, interaction, response);
}

void cmd_timeout_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    char user_mention[64];
    char minutes_str[16];
    u64snowflake user_id;
    int64_t minutes;

    if (!args || strlen(args) == 0) {
        struct discord_create_message params = { .content = "Usage: timeout <user> <minutes> [reason]" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (sscanf(args, "%63s %15s", user_mention, minutes_str) < 2) {
        struct discord_create_message params = { .content = "Usage: timeout <user> <minutes> [reason]" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    user_id = parse_user_mention(user_mention);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    minutes = strtoll(minutes_str, NULL, 10);
    if (minutes <= 0) {
        struct discord_create_message params = { .content = "Invalid duration." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Calculate timeout timestamp in milliseconds */
    u64unix_ms timeout_until = (u64unix_ms)(time(NULL) + (minutes * 60)) * 1000;

    struct discord_modify_guild_member timeout_params = {
        .communication_disabled_until = timeout_until
    };
    discord_modify_guild_member(client, msg->guild_id, user_id, &timeout_params, NULL);

    char response[512];
    snprintf(response, sizeof(response),
        "**Timed Out!**\n\n"
        "**User:** <@%lu>\n**Duration:** %ld minutes\n**Moderator:** <@%lu>",
        (unsigned long)user_id, (long)minutes, (unsigned long)msg->author->id);

    struct discord_create_message msg_params = { .content = response };
    discord_create_message(client, msg->channel_id, &msg_params, NULL);
}

void cmd_warn(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;
    const char *reason = "No reason provided";

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "reason") == 0) {
                reason = opts->array[i].value;
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user to warn.");
        return;
    }

    char guild_id_str[32], user_id_str[32], mod_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);
    snprintf(mod_id_str, sizeof(mod_id_str), "%lu", (unsigned long)interaction->member->user->id);

    db_add_warning(&g_bot->database, guild_id_str, user_id_str, mod_id_str, reason);

    /* Get warning count */
    warning_t warnings[100];
    int count;
    db_get_warnings(&g_bot->database, guild_id_str, user_id_str, warnings, 100, &count);

    char response[512];
    snprintf(response, sizeof(response),
        "**Warning Issued!**\n\n"
        "**User:** <@%lu>\n**Reason:** %s\n**Total Warnings:** %d",
        (unsigned long)user_id, reason, count);

    respond_message(client, interaction, response);
}

void cmd_warn_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    char user_mention[64];
    char reason[512] = "No reason provided";
    u64snowflake user_id;

    if (!args || strlen(args) == 0) {
        struct discord_create_message params = { .content = "Usage: warn <user> [reason]" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (sscanf(args, "%63s", user_mention) != 1) {
        struct discord_create_message params = { .content = "Please specify a user to warn." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    user_id = parse_user_mention(user_mention);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    const char *reason_start = args + strlen(user_mention);
    while (*reason_start == ' ') reason_start++;
    if (*reason_start) {
        strncpy(reason, reason_start, sizeof(reason) - 1);
    }

    char guild_id_str[32], user_id_str[32], mod_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);
    snprintf(mod_id_str, sizeof(mod_id_str), "%lu", (unsigned long)msg->author->id);

    db_add_warning(&g_bot->database, guild_id_str, user_id_str, mod_id_str, reason);

    /* Get warning count */
    warning_t warnings[100];
    int count;
    db_get_warnings(&g_bot->database, guild_id_str, user_id_str, warnings, 100, &count);

    char response[512];
    snprintf(response, sizeof(response),
        "**Warning Issued!**\n\n"
        "**User:** <@%lu>\n**Reason:** %s\n**Total Warnings:** %d",
        (unsigned long)user_id, reason, count);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_warnings(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user.");
        return;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    warning_t warnings[10];
    int count;
    db_get_warnings(&g_bot->database, guild_id_str, user_id_str, warnings, 10, &count);

    if (count == 0) {
        char response[128];
        snprintf(response, sizeof(response), "<@%lu> has no warnings.", (unsigned long)user_id);
        respond_message(client, interaction, response);
        return;
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response), "**Warnings for <@%lu>** (%d total)\n\n", (unsigned long)user_id, count);

    for (int i = 0; i < count && i < 10; i++) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "%d. %s (by <@%s>)\n", i + 1, warnings[i].reason, warnings[i].moderator_id);
    }

    respond_message(client, interaction, response);
}

void cmd_warnings_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || strlen(args) == 0) {
        struct discord_create_message params = { .content = "Usage: warnings <user>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake user_id = parse_user_mention(args);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    warning_t warnings[10];
    int count;
    db_get_warnings(&g_bot->database, guild_id_str, user_id_str, warnings, 10, &count);

    if (count == 0) {
        char response[128];
        snprintf(response, sizeof(response), "<@%lu> has no warnings.", (unsigned long)user_id);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response), "**Warnings for <@%lu>** (%d total)\n\n", (unsigned long)user_id, count);

    for (int i = 0; i < count && i < 10; i++) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "%d. %s (by <@%s>)\n", i + 1, warnings[i].reason, warnings[i].moderator_id);
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_admin_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "kick", "Kick a member from the server", "Administration", cmd_kick, cmd_kick_prefix, 0, 0 },
        { "ban", "Ban a member from the server", "Administration", cmd_ban, cmd_ban_prefix, 0, 0 },
        { "unban", "Unban a user from the server", "Administration", cmd_unban, cmd_unban_prefix, 0, 0 },
        { "timeout", "Timeout a member", "Administration", cmd_timeout, cmd_timeout_prefix, 0, 0 },
        { "warn", "Warn a member", "Administration", cmd_warn, cmd_warn_prefix, 0, 0 },
        { "warnings", "View a member's warnings", "Administration", cmd_warnings, cmd_warnings_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
