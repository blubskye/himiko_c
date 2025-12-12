/*
 * Himiko Discord Bot (C Edition) - Tools Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_TOOLS_H
#define HIMIKO_COMMANDS_TOOLS_H

#include <concord/discord.h>
#include "bot.h"

void register_tools_commands(himiko_bot_t *bot);

/* Command handlers (prefix-only to stay under 100 slash commands) */
void cmd_timestamp_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_snowflake_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_charcount_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_permissions_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_servers_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_TOOLS_H */
