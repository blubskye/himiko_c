/*
 * Himiko Discord Bot (C Edition) - Info Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_INFO_H
#define HIMIKO_COMMANDS_INFO_H

#include <concord/discord.h>
#include "bot.h"

void register_info_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_userinfo(struct discord *client, const struct discord_interaction *interaction);
void cmd_userinfo_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_serverinfo(struct discord *client, const struct discord_interaction *interaction);
void cmd_serverinfo_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_botinfo(struct discord *client, const struct discord_interaction *interaction);
void cmd_botinfo_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_avatar(struct discord *client, const struct discord_interaction *interaction);
void cmd_avatar_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_membercount(struct discord *client, const struct discord_interaction *interaction);
void cmd_membercount_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_INFO_H */
