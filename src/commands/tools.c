/*
 * Himiko Discord Bot (C Edition) - Tools Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/tools.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* Discord epoch (2015-01-01 00:00:00 UTC) in milliseconds */
#define DISCORD_EPOCH 1420070400000ULL

void cmd_timestamp_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    time_t timestamp;

    if (args && *args) {
        /* Try to parse as unix timestamp */
        timestamp = strtoll(args, NULL, 10);
        if (timestamp == 0) {
            struct discord_create_message params = { .content = "Please provide a valid unix timestamp." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }
    } else {
        /* Use current time */
        timestamp = time(NULL);
    }

    char response[1024];
    snprintf(response, sizeof(response),
        "**Timestamp Formats** (for %ld)\n\n"
        "**Short Time:** <t:%ld:t>\n"
        "**Long Time:** <t:%ld:T>\n"
        "**Short Date:** <t:%ld:d>\n"
        "**Long Date:** <t:%ld:D>\n"
        "**Short Date/Time:** <t:%ld:f>\n"
        "**Long Date/Time:** <t:%ld:F>\n"
        "**Relative:** <t:%ld:R>\n\n"
        "**Copy:** `<t:%ld:f>`",
        (long)timestamp,
        (long)timestamp, (long)timestamp, (long)timestamp,
        (long)timestamp, (long)timestamp, (long)timestamp,
        (long)timestamp, (long)timestamp);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_snowflake_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: snowflake <snowflake id>\nParses a Discord snowflake ID to show when it was created." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse snowflake */
    unsigned long long snowflake = strtoull(args, NULL, 10);
    if (snowflake == 0) {
        struct discord_create_message params = { .content = "Please provide a valid snowflake ID." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Extract timestamp from snowflake */
    unsigned long long timestamp_ms = (snowflake >> 22) + DISCORD_EPOCH;
    time_t timestamp = (time_t)(timestamp_ms / 1000);

    /* Extract other fields */
    unsigned int worker_id = (snowflake >> 17) & 0x1F;
    unsigned int process_id = (snowflake >> 12) & 0x1F;
    unsigned int increment = snowflake & 0xFFF;

    struct tm *tm_info = gmtime(&timestamp);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S UTC", tm_info);

    char response[512];
    snprintf(response, sizeof(response),
        "**Snowflake Info:** `%llu`\n\n"
        "**Created:** %s\n"
        "**Timestamp:** <t:%ld:f> (<t:%ld:R>)\n"
        "**Worker ID:** %u\n"
        "**Process ID:** %u\n"
        "**Increment:** %u",
        snowflake, date_str,
        (long)timestamp, (long)timestamp,
        worker_id, process_id, increment);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_charcount_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: charcount <text>\nCounts characters, words, and lines in the provided text." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    size_t chars = strlen(args);
    size_t chars_no_spaces = 0;
    size_t words = 0;
    size_t lines = 1;
    int in_word = 0;

    for (const char *p = args; *p; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            chars_no_spaces++;
        }

        if (isspace((unsigned char)*p)) {
            if (in_word) {
                in_word = 0;
            }
            if (*p == '\n') {
                lines++;
            }
        } else {
            if (!in_word) {
                words++;
                in_word = 1;
            }
        }
    }

    char response[512];
    snprintf(response, sizeof(response),
        "**Character Count**\n\n"
        "**Characters:** %zu\n"
        "**Characters (no spaces):** %zu\n"
        "**Words:** %zu\n"
        "**Lines:** %zu",
        chars, chars_no_spaces, words, lines);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_permissions_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    unsigned long long perms;

    if (args && *args) {
        perms = strtoull(args, NULL, 10);
    } else {
        /* Show current user's permissions if no arg */
        perms = msg->member ? msg->member->permissions : 0;
    }

    /* Discord permission flags */
    static const struct {
        unsigned long long flag;
        const char *name;
    } perm_flags[] = {
        { 0x1, "Create Invite" },
        { 0x2, "Kick Members" },
        { 0x4, "Ban Members" },
        { 0x8, "Administrator" },
        { 0x10, "Manage Channels" },
        { 0x20, "Manage Server" },
        { 0x40, "Add Reactions" },
        { 0x80, "View Audit Log" },
        { 0x100, "Priority Speaker" },
        { 0x200, "Stream" },
        { 0x400, "View Channel" },
        { 0x800, "Send Messages" },
        { 0x1000, "Send TTS Messages" },
        { 0x2000, "Manage Messages" },
        { 0x4000, "Embed Links" },
        { 0x8000, "Attach Files" },
        { 0x10000, "Read Message History" },
        { 0x20000, "Mention Everyone" },
        { 0x40000, "Use External Emojis" },
        { 0x80000, "View Server Insights" },
        { 0x100000, "Connect" },
        { 0x200000, "Speak" },
        { 0x400000, "Mute Members" },
        { 0x800000, "Deafen Members" },
        { 0x1000000, "Move Members" },
        { 0x2000000, "Use VAD" },
        { 0x4000000, "Change Nickname" },
        { 0x8000000, "Manage Nicknames" },
        { 0x10000000, "Manage Roles" },
        { 0x20000000, "Manage Webhooks" },
        { 0x40000000, "Manage Emojis" },
        { 0x80000000, "Use Application Commands" },
        { 0x100000000ULL, "Request to Speak" },
        { 0x200000000ULL, "Manage Events" },
        { 0x400000000ULL, "Manage Threads" },
        { 0x800000000ULL, "Create Public Threads" },
        { 0x1000000000ULL, "Create Private Threads" },
        { 0x2000000000ULL, "Use External Stickers" },
        { 0x4000000000ULL, "Send Messages in Threads" },
        { 0x8000000000ULL, "Use Embedded Activities" },
        { 0x10000000000ULL, "Moderate Members" },
    };
    static const int perm_count = sizeof(perm_flags) / sizeof(perm_flags[0]);

    char response[2048];
    int offset = snprintf(response, sizeof(response),
        "**Permissions** (`%llu`)\n\n", perms);

    int has_any = 0;
    for (int i = 0; i < perm_count && (size_t)offset < sizeof(response) - 50; i++) {
        if (perms & perm_flags[i].flag) {
            offset += snprintf(response + offset, sizeof(response) - offset,
                ":white_check_mark: %s\n", perm_flags[i].name);
            has_any = 1;
        }
    }

    if (!has_any) {
        snprintf(response + offset, sizeof(response) - offset, "No permissions set.");
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_servers_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;
    (void)client;
    (void)msg;

    /* This would require caching guild data, which the bot may not have access to */
    char response[256];
    snprintf(response, sizeof(response),
        "**Bot Statistics**\n\n"
        "Server count information requires additional gateway intents and caching.\n"
        "This command is not fully implemented in the C version.");

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_tools_commands(himiko_bot_t *bot) {
    /* Tools commands are prefix-only to stay under Discord's 100 slash command limit */
    himiko_command_t cmds[] = {
        { "timestamp", "Generate Discord timestamp formats", "Tools", NULL, cmd_timestamp_prefix, 0, 0 },
        { "snowflake", "Parse a Discord snowflake ID", "Tools", NULL, cmd_snowflake_prefix, 0, 0 },
        { "charcount", "Count characters, words, and lines", "Tools", NULL, cmd_charcount_prefix, 0, 0 },
        { "permissions", "Decode Discord permission flags", "Tools", NULL, cmd_permissions_prefix, 0, 0 },
        { "servers", "Show bot server statistics", "Tools", NULL, cmd_servers_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
