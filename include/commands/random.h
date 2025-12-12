/*
 * Himiko Discord Bot (C Edition) - Random Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_RANDOM_H
#define HIMIKO_COMMANDS_RANDOM_H

#include <concord/discord.h>
#include "bot.h"

void register_random_commands(himiko_bot_t *bot);

/* Command handlers (prefix-only to stay under 100 slash commands) */
void cmd_advice_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_quote_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_fact_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_dadjoke_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_password_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_RANDOM_H */
