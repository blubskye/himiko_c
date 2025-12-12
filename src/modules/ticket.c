/*
 * Himiko Discord Bot (C Edition) - Ticket System Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/ticket.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

void ticket_init(himiko_bot_t *bot) {
    (void)bot;
}

void ticket_cleanup(himiko_bot_t *bot) {
    (void)bot;
}

/* Database helpers - direct SQL since not all functions may be in database.h */
static int get_ticket_config(himiko_database_t *db, const char *guild_id, ticket_config_t *config) {
    if (!db || !db->db || !guild_id || !config) return -1;
    memset(config, 0, sizeof(ticket_config_t));

    const char *sql = "SELECT guild_id, channel_id, enabled, created_at FROM ticket_config WHERE guild_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(config->guild_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(config->guild_id) - 1);
        const char *channel = (const char *)sqlite3_column_text(stmt, 1);
        if (channel) strncpy(config->channel_id, channel, sizeof(config->channel_id) - 1);
        config->enabled = sqlite3_column_int(stmt, 2);
        config->created_at = sqlite3_column_int64(stmt, 3);
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return -1;
}

static int set_ticket_config(himiko_database_t *db, const char *guild_id, const char *channel_id, int enabled) {
    if (!db || !db->db || !guild_id) return -1;

    const char *sql = "INSERT INTO ticket_config (guild_id, channel_id, enabled, created_at) "
                      "VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(guild_id) DO UPDATE SET channel_id = excluded.channel_id, enabled = excluded.enabled";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, enabled);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)time(NULL));

    int result = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
    return result;
}

static int delete_ticket_config(himiko_database_t *db, const char *guild_id) {
    if (!db || !db->db || !guild_id) return -1;

    const char *sql = "DELETE FROM ticket_config WHERE guild_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    int result = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
    return result;
}

/* Check if user is admin */
static int is_admin(const struct discord_guild_member *member) {
    if (!member) return 0;

    /* Check ADMINISTRATOR permission (0x8) */
    if (member->permissions & 0x8) return 1;

    return 0;
}

/* /setticket command handler */
void cmd_setticket(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Admin only */
    if (!is_admin(interaction->member)) {
        respond_ephemeral(client, interaction, "You need administrator permission to configure tickets.");
        return;
    }

    /* Get channel option */
    if (!interaction->data->options || interaction->data->options->size == 0) {
        respond_ephemeral(client, interaction, "Please specify a channel.");
        return;
    }

    u64snowflake channel_id = 0;
    for (int i = 0; i < interaction->data->options->size; i++) {
        if (strcmp(interaction->data->options->array[i].name, "channel") == 0) {
            channel_id = strtoll(interaction->data->options->array[i].value, NULL, 10);
            break;
        }
    }

    if (channel_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a valid channel.");
        return;
    }

    char guild_id_str[32], channel_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    if (set_ticket_config(&bot->database, guild_id_str, channel_id_str, 1) != 0) {
        respond_ephemeral(client, interaction, "Failed to set ticket channel.");
        return;
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Ticket System Enabled**\n\nTickets will be forwarded to <#%s>\n\nUsers can now use `/ticket` to submit issues.",
        channel_id_str);
    respond_message(client, interaction, response);
}

void cmd_setticket_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Parse channel mention or ID */
    if (!args || !*args) {
        struct discord_create_message params = {
            .content = "Usage: setticket <#channel>"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake channel_id = parse_channel_mention(args);
    if (channel_id == 0) {
        channel_id = strtoull(args, NULL, 10);
    }

    if (channel_id == 0) {
        struct discord_create_message params = {
            .content = "Invalid channel. Please mention a channel or provide a channel ID."
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)channel_id);

    if (set_ticket_config(&bot->database, guild_id_str, channel_id_str, 1) != 0) {
        struct discord_create_message params = { .content = "Failed to set ticket channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Ticket System Enabled**\nTickets will be forwarded to <#%s>",
        channel_id_str);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

/* /disableticket command handler */
void cmd_disableticket(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Admin only */
    if (!is_admin(interaction->member)) {
        respond_ephemeral(client, interaction, "You need administrator permission to configure tickets.");
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    if (delete_ticket_config(&bot->database, guild_id_str) != 0) {
        respond_ephemeral(client, interaction, "Failed to disable ticket system.");
        return;
    }

    respond_message(client, interaction, "**Ticket System Disabled**\n\nThe ticket system has been disabled for this server.");
}

void cmd_disableticket_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    if (delete_ticket_config(&bot->database, guild_id_str) != 0) {
        struct discord_create_message params = { .content = "Failed to disable ticket system." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    struct discord_create_message params = { .content = "**Ticket System Disabled**" };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

/* /ticketstatus command handler */
void cmd_ticketstatus(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    ticket_config_t config;
    if (get_ticket_config(&bot->database, guild_id_str, &config) != 0) {
        respond_ephemeral(client, interaction, "The ticket system is not configured for this server.");
        return;
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Ticket System Status**\n"
        "Status: %s\n"
        "Channel: <#%s>\n\n"
        "Users can use `/ticket` to submit issues.",
        config.enabled ? "Enabled" : "Disabled",
        config.channel_id);
    respond_message(client, interaction, response);
}

void cmd_ticketstatus_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    ticket_config_t config;
    if (get_ticket_config(&bot->database, guild_id_str, &config) != 0) {
        struct discord_create_message params = {
            .content = "The ticket system is not configured for this server."
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Ticket System Status**\nStatus: %s | Channel: <#%s>",
        config.enabled ? "Enabled" : "Disabled",
        config.channel_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

/* /ticket command handler - submit a ticket */
void cmd_ticket(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    /* Check if ticket system is enabled */
    ticket_config_t config;
    if (get_ticket_config(&bot->database, guild_id_str, &config) != 0 || !config.enabled) {
        respond_ephemeral(client, interaction, "The ticket system is not enabled on this server.");
        return;
    }

    /* Get issue text */
    const char *issue = NULL;
    if (interaction->data->options && interaction->data->options->size > 0) {
        for (int i = 0; i < interaction->data->options->size; i++) {
            if (strcmp(interaction->data->options->array[i].name, "issue") == 0) {
                issue = interaction->data->options->array[i].value;
                break;
            }
        }
    }

    if (!issue || !*issue) {
        respond_ephemeral(client, interaction, "Please describe your issue.");
        return;
    }

    /* Create ticket embed for staff channel */
    struct discord_embed embed = {0};
    embed.title = "New Ticket";
    embed.color = 0xFF69B4;

    char from_value[256];
    snprintf(from_value, sizeof(from_value), "%s (<@%lu>)",
        interaction->member->user->username,
        (unsigned long)interaction->member->user->id);

    char user_id_value[32];
    snprintf(user_id_value, sizeof(user_id_value), "%lu", (unsigned long)interaction->member->user->id);

    char channel_footer[64];
    snprintf(channel_footer, sizeof(channel_footer), "Submitted from channel %lu", (unsigned long)interaction->channel_id);

    struct discord_embed_field fields[3] = {
        { .name = "From", .value = from_value, .Inline = 1 },
        { .name = "User ID", .value = user_id_value, .Inline = 1 },
        { .name = "Issue", .value = (char *)issue, .Inline = 0 }
    };

    struct discord_embed_fields fields_list = {
        .size = 3,
        .array = fields
    };
    embed.fields = &fields_list;

    /* Add user avatar as thumbnail */
    char avatar_url[256] = {0};
    if (interaction->member->user->avatar) {
        snprintf(avatar_url, sizeof(avatar_url),
            "https://cdn.discordapp.com/avatars/%lu/%s.png",
            (unsigned long)interaction->member->user->id,
            interaction->member->user->avatar);
        embed.thumbnail = &(struct discord_embed_thumbnail){ .url = avatar_url };
    }

    embed.footer = &(struct discord_embed_footer){ .text = channel_footer };

    /* Send to ticket channel */
    u64snowflake ticket_channel = strtoull(config.channel_id, NULL, 10);
    struct discord_create_message msg_params = {
        .embeds = &(struct discord_embeds){
            .size = 1,
            .array = &embed
        }
    };

    struct discord_ret_message ret = {0};
    CCORDcode code = discord_create_message(client, ticket_channel, &msg_params, &ret);

    if (code != CCORD_OK) {
        respond_ephemeral(client, interaction, "Failed to submit ticket. Please try again later.");
        return;
    }

    /* Respond to user (ephemeral) */
    respond_ephemeral(client, interaction, "**Ticket Submitted**\n\nYour ticket has been submitted to the server staff. They will review it shortly.");
}

void cmd_ticket_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Check if ticket system is enabled */
    ticket_config_t config;
    if (get_ticket_config(&bot->database, guild_id_str, &config) != 0 || !config.enabled) {
        struct discord_create_message params = {
            .content = "The ticket system is not enabled on this server."
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (!args || !*args) {
        struct discord_create_message params = {
            .content = "Usage: ticket <description of your issue>"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Create ticket embed */
    struct discord_embed embed = {0};
    embed.title = "New Ticket";
    embed.color = 0xFF69B4;

    char from_value[256];
    snprintf(from_value, sizeof(from_value), "%s (<@%lu>)",
        msg->author->username,
        (unsigned long)msg->author->id);

    char user_id_value[32];
    snprintf(user_id_value, sizeof(user_id_value), "%lu", (unsigned long)msg->author->id);

    char channel_footer[64];
    snprintf(channel_footer, sizeof(channel_footer), "Submitted from channel %lu", (unsigned long)msg->channel_id);

    struct discord_embed_field fields[3] = {
        { .name = "From", .value = from_value, .Inline = 1 },
        { .name = "User ID", .value = user_id_value, .Inline = 0 },
        { .name = "Issue", .value = (char *)args, .Inline = 0 }
    };

    struct discord_embed_fields fields_list2 = {
        .size = 3,
        .array = fields
    };
    embed.fields = &fields_list2;

    char avatar_url[256] = {0};
    if (msg->author->avatar) {
        snprintf(avatar_url, sizeof(avatar_url),
            "https://cdn.discordapp.com/avatars/%lu/%s.png",
            (unsigned long)msg->author->id,
            msg->author->avatar);
        embed.thumbnail = &(struct discord_embed_thumbnail){ .url = avatar_url };
    }

    embed.footer = &(struct discord_embed_footer){ .text = channel_footer };

    /* Send to ticket channel */
    u64snowflake ticket_channel = strtoull(config.channel_id, NULL, 10);
    struct discord_create_message msg_params = {
        .embeds = &(struct discord_embeds){
            .size = 1,
            .array = &embed
        }
    };

    CCORDcode code = discord_create_message(client, ticket_channel, &msg_params, NULL);

    if (code != CCORD_OK) {
        struct discord_create_message err_params = {
            .content = "Failed to submit ticket. Please try again later."
        };
        discord_create_message(client, msg->channel_id, &err_params, NULL);
        return;
    }

    /* Confirm to user */
    struct discord_create_message params = {
        .content = "**Ticket Submitted**\nYour ticket has been sent to the server staff."
    };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_ticket_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "setticket", "Set the channel where tickets will be forwarded", "Ticket", cmd_setticket, cmd_setticket_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
        { "disableticket", "Disable the ticket system", "Ticket", cmd_disableticket, cmd_disableticket_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
        { "ticketstatus", "View ticket system status", "Ticket", cmd_ticketstatus, cmd_ticketstatus_prefix, 0, 0 },
        { "ticket", "Submit a ticket/issue to the server staff", "Ticket", cmd_ticket, cmd_ticket_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
