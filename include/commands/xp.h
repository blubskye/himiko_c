/*
 * Himiko Discord Bot (C Edition) - XP/Leveling Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_XP_H
#define HIMIKO_COMMANDS_XP_H

#include <concord/discord.h>
#include "bot.h"

void register_xp_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_xp(struct discord *client, const struct discord_interaction *interaction);
void cmd_xp_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_rank(struct discord *client, const struct discord_interaction *interaction);
void cmd_rank_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_leaderboard(struct discord *client, const struct discord_interaction *interaction);
void cmd_leaderboard_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_setxp(struct discord *client, const struct discord_interaction *interaction);
void cmd_setxp_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_addxp(struct discord *client, const struct discord_interaction *interaction);
void cmd_addxp_prefix(struct discord *client, const struct discord_message *msg, const char *args);

/* XP calculation helpers are declared in database.h */

#endif /* HIMIKO_COMMANDS_XP_H */
