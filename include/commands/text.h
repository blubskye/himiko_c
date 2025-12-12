/*
 * Himiko Discord Bot (C Edition) - Text Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_TEXT_H
#define HIMIKO_COMMANDS_TEXT_H

#include <concord/discord.h>
#include "bot.h"

void register_text_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_reverse(struct discord *client, const struct discord_interaction *interaction);
void cmd_reverse_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_mock(struct discord *client, const struct discord_interaction *interaction);
void cmd_mock_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_owo(struct discord *client, const struct discord_interaction *interaction);
void cmd_owo_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_upper(struct discord *client, const struct discord_interaction *interaction);
void cmd_upper_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_lower(struct discord *client, const struct discord_interaction *interaction);
void cmd_lower_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_encode(struct discord *client, const struct discord_interaction *interaction);
void cmd_encode_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_decode(struct discord *client, const struct discord_interaction *interaction);
void cmd_decode_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_TEXT_H */
