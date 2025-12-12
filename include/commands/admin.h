/*
 * Himiko Discord Bot (C Edition) - Admin Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef HIMIKO_COMMANDS_ADMIN_H
#define HIMIKO_COMMANDS_ADMIN_H

#include <concord/discord.h>
#include "bot.h"

void register_admin_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_kick(struct discord *client, const struct discord_interaction *interaction);
void cmd_kick_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_ban(struct discord *client, const struct discord_interaction *interaction);
void cmd_ban_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_unban(struct discord *client, const struct discord_interaction *interaction);
void cmd_unban_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_timeout(struct discord *client, const struct discord_interaction *interaction);
void cmd_timeout_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_warn(struct discord *client, const struct discord_interaction *interaction);
void cmd_warn_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_warnings(struct discord *client, const struct discord_interaction *interaction);
void cmd_warnings_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_ADMIN_H */
