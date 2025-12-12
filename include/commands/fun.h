/*
 * Himiko Discord Bot (C Edition) - Fun Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_FUN_H
#define HIMIKO_COMMANDS_FUN_H

#include <concord/discord.h>
#include "bot.h"

void register_fun_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_8ball(struct discord *client, const struct discord_interaction *interaction);
void cmd_8ball_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_dice(struct discord *client, const struct discord_interaction *interaction);
void cmd_dice_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_coinflip(struct discord *client, const struct discord_interaction *interaction);
void cmd_coinflip_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_rps(struct discord *client, const struct discord_interaction *interaction);
void cmd_rps_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_rate(struct discord *client, const struct discord_interaction *interaction);
void cmd_rate_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_choose(struct discord *client, const struct discord_interaction *interaction);
void cmd_choose_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_FUN_H */
