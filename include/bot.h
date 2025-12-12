/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef HIMIKO_BOT_H
#define HIMIKO_BOT_H

#include <concord/discord.h>
#include "config.h"
#include "database.h"

#define HIMIKO_VERSION "1.0.0"

/* Command definition */
typedef struct himiko_command {
    const char *name;
    const char *description;
    const char *category;
    void (*slash_handler)(struct discord *client, const struct discord_interaction *interaction);
    void (*prefix_handler)(struct discord *client, const struct discord_message *msg, const char *args);
    int slash_only;
    int prefix_only;
} himiko_command_t;

/* Bot state */
typedef struct himiko_bot {
    struct discord *client;
    himiko_config_t config;
    himiko_database_t database;
    himiko_command_t *commands;
    int command_count;
    int running;
} himiko_bot_t;

/* Global bot instance */
extern himiko_bot_t *g_bot;

/* Bot lifecycle */
int bot_init(himiko_bot_t *bot, const char *config_path);
void bot_cleanup(himiko_bot_t *bot);
int bot_run(himiko_bot_t *bot);
void bot_stop(himiko_bot_t *bot);

/* Command registration */
void bot_register_command(himiko_bot_t *bot, const himiko_command_t *cmd);
void bot_register_all_commands(himiko_bot_t *bot);
himiko_command_t *bot_find_command(himiko_bot_t *bot, const char *name);

/* Discord event handlers */
void on_ready(struct discord *client, const struct discord_ready *event);
void on_interaction_create(struct discord *client, const struct discord_interaction *interaction);
void on_message_create(struct discord *client, const struct discord_message *msg);
void on_message_delete(struct discord *client, const struct discord_message_delete *event);
void on_guild_member_add(struct discord *client, const struct discord_guild_member *member);

/* Utility functions */
u64snowflake parse_user_mention(const char *mention);
u64snowflake parse_channel_mention(const char *mention);
u64snowflake parse_role_mention(const char *mention);
void respond_ephemeral(struct discord *client, const struct discord_interaction *i, const char *message);
void respond_message(struct discord *client, const struct discord_interaction *i, const char *message);
char *snowflake_to_string(u64snowflake id, char *buf, size_t buf_size);
u64snowflake string_to_snowflake(const char *str);

/* Permission helpers */
int has_permission(const struct discord_guild_member *member, uint64_t permission);
int is_administrator(const struct discord_guild_member *member);

/* Embed helpers */
struct discord_embed *create_embed(const char *title, const char *description, int color);
void add_embed_field(struct discord_embed *embed, const char *name, const char *value, int is_inline);

/* Categories that are prefix-only (to stay under Discord's 100 slash command limit) */
int is_prefix_only_category(const char *category);

#endif /* HIMIKO_BOT_H */
