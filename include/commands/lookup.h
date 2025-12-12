/*
 * Himiko Discord Bot (C Edition) - Lookup Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_LOOKUP_H
#define HIMIKO_COMMANDS_LOOKUP_H

#include <concord/discord.h>
#include "bot.h"

void register_lookup_commands(himiko_bot_t *bot);

/* Command handlers (prefix-only to stay under 100 slash commands) */
void cmd_urban_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_wiki_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_ip_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_color_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_LOOKUP_H */
