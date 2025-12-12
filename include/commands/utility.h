/*
 * Himiko Discord Bot (C Edition) - Utility Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_UTILITY_H
#define HIMIKO_COMMANDS_UTILITY_H

#include <concord/discord.h>
#include "bot.h"

void register_utility_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_ping(struct discord *client, const struct discord_interaction *interaction);
void cmd_ping_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_snipe(struct discord *client, const struct discord_interaction *interaction);
void cmd_snipe_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_afk(struct discord *client, const struct discord_interaction *interaction);
void cmd_afk_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_remind(struct discord *client, const struct discord_interaction *interaction);
void cmd_remind_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_uptime(struct discord *client, const struct discord_interaction *interaction);
void cmd_uptime_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_poll(struct discord *client, const struct discord_interaction *interaction);
void cmd_poll_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_say(struct discord *client, const struct discord_interaction *interaction);
void cmd_say_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_math(struct discord *client, const struct discord_interaction *interaction);
void cmd_math_prefix(struct discord *client, const struct discord_message *msg, const char *args);

/* Utility function */
time_t parse_duration(const char *str);

#endif /* HIMIKO_COMMANDS_UTILITY_H */
