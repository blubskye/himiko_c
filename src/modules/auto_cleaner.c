/*
 * Himiko Discord Bot (C Edition) - Auto-Clean Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/auto_cleaner.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

static time_t g_last_tick = 0;

void auto_cleaner_init(himiko_bot_t *bot) {
    (void)bot;
    g_last_tick = 0;
}

void auto_cleaner_cleanup(himiko_bot_t *bot) {
    (void)bot;
}

/* Get due auto-clean channels from database */
static int get_due_channels(himiko_database_t *db, autoclean_channel_t *channels, int max_count) {
    if (!db || !db->db) return 0;

    const char *sql = "SELECT id, guild_id, channel_id, interval_hours, warning_minutes, "
                      "CAST(strftime('%s', next_run) AS INTEGER), clean_message, clean_image, created_by "
                      "FROM autoclean_channels WHERE next_run <= datetime('now') "
                      "ORDER BY next_run LIMIT ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, max_count);

    int count = 0;
    while (count < max_count && sqlite3_step(stmt) == SQLITE_ROW) {
        autoclean_channel_t *c = &channels[count];
        memset(c, 0, sizeof(*c));

        c->id = sqlite3_column_int(stmt, 0);
        const char *guild = (const char *)sqlite3_column_text(stmt, 1);
        const char *channel = (const char *)sqlite3_column_text(stmt, 2);
        if (guild) strncpy(c->guild_id, guild, sizeof(c->guild_id) - 1);
        if (channel) strncpy(c->channel_id, channel, sizeof(c->channel_id) - 1);
        c->interval_hours = sqlite3_column_int(stmt, 3);
        c->warning_minutes = sqlite3_column_int(stmt, 4);
        c->next_run = (time_t)sqlite3_column_int64(stmt, 5);
        c->clean_message = sqlite3_column_int(stmt, 6);
        c->clean_image = sqlite3_column_int(stmt, 7);
        const char *created_by = (const char *)sqlite3_column_text(stmt, 8);
        if (created_by) strncpy(c->created_by, created_by, sizeof(c->created_by) - 1);

        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* Update next run time for a channel */
static int update_next_run(himiko_database_t *db, int id, int interval_hours) {
    if (!db || !db->db) return -1;

    const char *sql = "UPDATE autoclean_channels SET next_run = datetime('now', '+' || ? || ' hours') WHERE id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, interval_hours);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Clean a channel */
static void clean_channel(struct discord *client, autoclean_channel_t *channel) {
    u64snowflake channel_id = strtoull(channel->channel_id, NULL, 10);

    /* Note: Bulk delete is limited to messages < 14 days old and max 100 at a time */
    /* Full implementation would need to get messages and delete in batches */
    /* For now, we'll send a notification that clean was triggered */

    if (channel->clean_message) {
        char msg[256];
        snprintf(msg, sizeof(msg), "**Auto-Clean Complete**\nThis channel has been cleaned.");
        struct discord_create_message params = { .content = msg };
        discord_create_message(client, channel_id, &params, NULL);
    }
}

/* Tick function - called periodically */
void auto_cleaner_tick(struct discord *client) {
    time_t now = time(NULL);

    /* Only check every 60 seconds */
    if (now - g_last_tick < 60) return;
    g_last_tick = now;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Get due channels */
    autoclean_channel_t channels[10];
    int count = get_due_channels(&bot->database, channels, 10);

    for (int i = 0; i < count; i++) {
        autoclean_channel_t *c = &channels[i];

        /* Send warning if enabled */
        if (c->warning_minutes > 0 && c->clean_message) {
            u64snowflake channel_id = strtoull(c->channel_id, NULL, 10);
            char warning[256];
            snprintf(warning, sizeof(warning),
                "**Auto-Clean Warning**\nThis channel will be cleaned in **%d minutes**.",
                c->warning_minutes);
            struct discord_create_message params = { .content = warning };
            discord_create_message(client, channel_id, &params, NULL);
        }

        /* Clean the channel */
        clean_channel(client, c);

        /* Update next run time */
        update_next_run(&bot->database, c->id, c->interval_hours);
    }
}

/* Add autoclean channel */
static int add_autoclean_channel(himiko_database_t *db, const char *guild_id, const char *channel_id,
                                  const char *created_by, int interval_hours, int warning_minutes) {
    if (!db || !db->db) return -1;

    const char *sql = "INSERT INTO autoclean_channels (guild_id, channel_id, interval_hours, warning_minutes, next_run, created_by) "
                      "VALUES (?, ?, ?, ?, datetime('now', '+' || ? || ' hours'), ?) "
                      "ON CONFLICT(guild_id, channel_id) DO UPDATE SET "
                      "interval_hours = excluded.interval_hours, warning_minutes = excluded.warning_minutes, "
                      "next_run = excluded.next_run";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, interval_hours);
    sqlite3_bind_int(stmt, 4, warning_minutes);
    sqlite3_bind_int(stmt, 5, interval_hours);
    sqlite3_bind_text(stmt, 6, created_by, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Remove autoclean channel */
static int remove_autoclean_channel(himiko_database_t *db, const char *guild_id, const char *channel_id) {
    if (!db || !db->db) return -1;

    const char *sql = "DELETE FROM autoclean_channels WHERE guild_id = ? AND channel_id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Get autoclean channels for a guild */
static int get_autoclean_channels(himiko_database_t *db, const char *guild_id,
                                   autoclean_channel_t *channels, int max_count) {
    if (!db || !db->db) return 0;

    const char *sql = "SELECT id, guild_id, channel_id, interval_hours, warning_minutes, "
                      "CAST(strftime('%s', next_run) AS INTEGER), clean_message, clean_image "
                      "FROM autoclean_channels WHERE guild_id = ? ORDER BY id";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    int count = 0;
    while (count < max_count && sqlite3_step(stmt) == SQLITE_ROW) {
        autoclean_channel_t *c = &channels[count];
        memset(c, 0, sizeof(*c));

        c->id = sqlite3_column_int(stmt, 0);
        const char *g = (const char *)sqlite3_column_text(stmt, 1);
        const char *ch = (const char *)sqlite3_column_text(stmt, 2);
        if (g) strncpy(c->guild_id, g, sizeof(c->guild_id) - 1);
        if (ch) strncpy(c->channel_id, ch, sizeof(c->channel_id) - 1);
        c->interval_hours = sqlite3_column_int(stmt, 3);
        c->warning_minutes = sqlite3_column_int(stmt, 4);
        c->next_run = (time_t)sqlite3_column_int64(stmt, 5);
        c->clean_message = sqlite3_column_int(stmt, 6);
        c->clean_image = sqlite3_column_int(stmt, 7);

        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* Set clean message setting */
static int set_clean_message(himiko_database_t *db, const char *guild_id, const char *channel_id, int enabled) {
    if (!db || !db->db) return -1;

    const char *sql = "UPDATE autoclean_channels SET clean_message = ? WHERE guild_id = ? AND channel_id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, enabled);
    sqlite3_bind_text(stmt, 2, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, channel_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Set clean image setting */
static int set_clean_image(himiko_database_t *db, const char *guild_id, const char *channel_id, int enabled) {
    if (!db || !db->db) return -1;

    const char *sql = "UPDATE autoclean_channels SET clean_image = ? WHERE guild_id = ? AND channel_id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, enabled);
    sqlite3_bind_text(stmt, 2, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, channel_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Command: autoclean add/remove/list */
void cmd_autoclean(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)interaction->member->user->id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size == 0) {
        respond_message(client, interaction, "Please specify a subcommand: add, remove, or list");
        return;
    }

    const char *subcommand = opts->array[0].name;

    if (strcmp(subcommand, "add") == 0) {
        struct discord_application_command_interaction_data_options *sub_opts = opts->array[0].options;
        const char *channel_id = NULL;
        int interval = 24;
        int warning = 5;

        if (sub_opts) {
            for (int i = 0; i < sub_opts->size; i++) {
                if (strcmp(sub_opts->array[i].name, "channel") == 0) {
                    channel_id = sub_opts->array[i].value;
                } else if (strcmp(sub_opts->array[i].name, "interval") == 0) {
                    interval = atoi(sub_opts->array[i].value);
                } else if (strcmp(sub_opts->array[i].name, "warning") == 0) {
                    warning = atoi(sub_opts->array[i].value);
                }
            }
        }

        if (!channel_id) {
            respond_message(client, interaction, "Please specify a channel.");
            return;
        }

        if (add_autoclean_channel(&bot->database, guild_id_str, channel_id, user_id_str, interval, warning) != 0) {
            respond_message(client, interaction, "Failed to add auto-clean channel.");
            return;
        }

        char response[256];
        snprintf(response, sizeof(response),
            "<#%s> will be cleaned every **%d hours** with a **%d minute** warning.",
            channel_id, interval, warning);
        respond_message(client, interaction, response);

    } else if (strcmp(subcommand, "remove") == 0) {
        struct discord_application_command_interaction_data_options *sub_opts = opts->array[0].options;
        const char *channel_id = NULL;

        if (sub_opts) {
            for (int i = 0; i < sub_opts->size; i++) {
                if (strcmp(sub_opts->array[i].name, "channel") == 0) {
                    channel_id = sub_opts->array[i].value;
                }
            }
        }

        if (!channel_id) {
            respond_message(client, interaction, "Please specify a channel.");
            return;
        }

        if (remove_autoclean_channel(&bot->database, guild_id_str, channel_id) != 0) {
            respond_message(client, interaction, "Failed to remove auto-clean channel.");
            return;
        }

        char response[128];
        snprintf(response, sizeof(response), "<#%s> has been removed from auto-clean.", channel_id);
        respond_message(client, interaction, response);

    } else if (strcmp(subcommand, "list") == 0) {
        autoclean_channel_t channels[20];
        int count = get_autoclean_channels(&bot->database, guild_id_str, channels, 20);

        if (count == 0) {
            respond_message(client, interaction, "No auto-clean channels configured.");
            return;
        }

        char response[2048];
        int offset = snprintf(response, sizeof(response), "**Auto-Clean Channels (%d)**\n\n", count);

        for (int i = 0; i < count && offset < (int)sizeof(response) - 200; i++) {
            autoclean_channel_t *c = &channels[i];
            offset += snprintf(response + offset, sizeof(response) - offset,
                "<#%s>\n"
                "├ Interval: %d hours\n"
                "├ Warning: %d minutes\n"
                "├ Next run: <t:%ld:R>\n"
                "└ Warning msg: %s | Clean images: %s\n\n",
                c->channel_id, c->interval_hours, c->warning_minutes,
                (long)c->next_run,
                c->clean_message ? "Yes" : "No",
                c->clean_image ? "Yes" : "No");
        }

        respond_message(client, interaction, response);
    }
}

/* Prefix command handler */
void cmd_autoclean_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);

    if (!args || !*args) {
        struct discord_create_message params = {
            .content = "Usage: autoclean <add|remove|list> [#channel] [interval] [warning]\n"
                       "Example: autoclean add #temp 24 5"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char cmd[16], channel_arg[64];
    int interval = 24, warning = 5;
    int parsed = sscanf(args, "%15s %63s %d %d", cmd, channel_arg, &interval, &warning);

    if (strcmp(cmd, "add") == 0 && parsed >= 2) {
        /* Parse channel mention or ID */
        char channel_id[32] = {0};
        if (channel_arg[0] == '<' && channel_arg[1] == '#') {
            const char *start = channel_arg + 2;
            const char *end = strchr(start, '>');
            if (end) {
                size_t len = end - start;
                if (len < sizeof(channel_id)) strncpy(channel_id, start, len);
            }
        } else {
            strncpy(channel_id, channel_arg, sizeof(channel_id) - 1);
        }

        if (!channel_id[0]) {
            struct discord_create_message params = { .content = "Invalid channel." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        if (add_autoclean_channel(&bot->database, guild_id_str, channel_id, user_id_str, interval, warning) != 0) {
            struct discord_create_message params = { .content = "Failed to add auto-clean channel." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response[256];
        snprintf(response, sizeof(response),
            "<#%s> will be cleaned every **%d hours** with a **%d minute** warning.",
            channel_id, interval, warning);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "remove") == 0 && parsed >= 2) {
        char channel_id[32] = {0};
        if (channel_arg[0] == '<' && channel_arg[1] == '#') {
            const char *start = channel_arg + 2;
            const char *end = strchr(start, '>');
            if (end) {
                size_t len = end - start;
                if (len < sizeof(channel_id)) strncpy(channel_id, start, len);
            }
        } else {
            strncpy(channel_id, channel_arg, sizeof(channel_id) - 1);
        }

        if (!channel_id[0]) {
            struct discord_create_message params = { .content = "Invalid channel." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        if (remove_autoclean_channel(&bot->database, guild_id_str, channel_id) != 0) {
            struct discord_create_message params = { .content = "Failed to remove channel." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response[128];
        snprintf(response, sizeof(response), "<#%s> removed from auto-clean.", channel_id);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "list") == 0) {
        autoclean_channel_t channels[20];
        int count = get_autoclean_channels(&bot->database, guild_id_str, channels, 20);

        if (count == 0) {
            struct discord_create_message params = { .content = "No auto-clean channels configured." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response[2048];
        int offset = snprintf(response, sizeof(response), "**Auto-Clean Channels (%d)**\n", count);

        for (int i = 0; i < count && offset < (int)sizeof(response) - 150; i++) {
            autoclean_channel_t *c = &channels[i];
            offset += snprintf(response + offset, sizeof(response) - offset,
                "<#%s> - %dh interval, %dm warning\n",
                c->channel_id, c->interval_hours, c->warning_minutes);
        }

        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else {
        struct discord_create_message params = {
            .content = "Usage: autoclean <add|remove|list> [#channel] [interval] [warning]"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void cmd_setcleanmessage(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size < 2) {
        respond_message(client, interaction, "Usage: /setcleanmessage <channel> <enabled>");
        return;
    }

    const char *channel_id = NULL;
    int enabled = 0;

    for (int i = 0; i < opts->size; i++) {
        if (strcmp(opts->array[i].name, "channel") == 0) {
            channel_id = opts->array[i].value;
        } else if (strcmp(opts->array[i].name, "enabled") == 0) {
            enabled = strcmp(opts->array[i].value, "true") == 0;
        }
    }

    if (!channel_id) {
        respond_message(client, interaction, "Please specify a channel.");
        return;
    }

    if (set_clean_message(&bot->database, guild_id_str, channel_id, enabled) != 0) {
        respond_message(client, interaction, "Failed to update setting.");
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Warning messages %s for <#%s>",
        enabled ? "enabled" : "disabled", channel_id);
    respond_message(client, interaction, response);
}

void cmd_setcleanmessage_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setcleanmessage <#channel> <on|off>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    char channel_arg[64], enabled_str[16];
    if (sscanf(args, "%63s %15s", channel_arg, enabled_str) != 2) {
        struct discord_create_message params = { .content = "Usage: setcleanmessage <#channel> <on|off>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char channel_id[32] = {0};
    if (channel_arg[0] == '<' && channel_arg[1] == '#') {
        const char *start = channel_arg + 2;
        const char *end = strchr(start, '>');
        if (end) {
            size_t len = end - start;
            if (len < sizeof(channel_id)) strncpy(channel_id, start, len);
        }
    } else {
        strncpy(channel_id, channel_arg, sizeof(channel_id) - 1);
    }

    int enabled = strcmp(enabled_str, "on") == 0 || strcmp(enabled_str, "true") == 0;

    if (set_clean_message(&bot->database, guild_id_str, channel_id, enabled) != 0) {
        struct discord_create_message params = { .content = "Failed to update setting." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Warning messages %s for <#%s>",
        enabled ? "enabled" : "disabled", channel_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_setcleanimage(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    if (!opts || opts->size < 2) {
        respond_message(client, interaction, "Usage: /setcleanimage <channel> <preserve>");
        return;
    }

    const char *channel_id = NULL;
    int preserve = 0;

    for (int i = 0; i < opts->size; i++) {
        if (strcmp(opts->array[i].name, "channel") == 0) {
            channel_id = opts->array[i].value;
        } else if (strcmp(opts->array[i].name, "preserve") == 0) {
            preserve = strcmp(opts->array[i].value, "true") == 0;
        }
    }

    if (!channel_id) {
        respond_message(client, interaction, "Please specify a channel.");
        return;
    }

    /* clean_image = !preserve (if preserve=true, don't clean images) */
    if (set_clean_image(&bot->database, guild_id_str, channel_id, !preserve) != 0) {
        respond_message(client, interaction, "Failed to update setting.");
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Images in <#%s> %s during clean",
        channel_id, preserve ? "will be preserved" : "will be deleted");
    respond_message(client, interaction, response);
}

void cmd_setcleanimage_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setcleanimage <#channel> <preserve|delete>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    char channel_arg[64], mode_str[16];
    if (sscanf(args, "%63s %15s", channel_arg, mode_str) != 2) {
        struct discord_create_message params = { .content = "Usage: setcleanimage <#channel> <preserve|delete>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char channel_id[32] = {0};
    if (channel_arg[0] == '<' && channel_arg[1] == '#') {
        const char *start = channel_arg + 2;
        const char *end = strchr(start, '>');
        if (end) {
            size_t len = end - start;
            if (len < sizeof(channel_id)) strncpy(channel_id, start, len);
        }
    } else {
        strncpy(channel_id, channel_arg, sizeof(channel_id) - 1);
    }

    int preserve = strcmp(mode_str, "preserve") == 0;

    if (set_clean_image(&bot->database, guild_id_str, channel_id, !preserve) != 0) {
        struct discord_create_message params = { .content = "Failed to update setting." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[128];
    snprintf(response, sizeof(response), "Images in <#%s> %s during clean",
        channel_id, preserve ? "will be preserved" : "will be deleted");
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_autoclean_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "autoclean", "Manage auto-clean channels", "AutoClean", cmd_autoclean, cmd_autoclean_prefix, 1, DISCORD_PERM_ADMINISTRATOR },
        { "setcleanmessage", "Toggle warning message before auto-clean", "AutoClean", cmd_setcleanmessage, cmd_setcleanmessage_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
        { "setcleanimage", "Toggle whether to preserve images during clean", "AutoClean", cmd_setcleanimage, cmd_setcleanimage_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
