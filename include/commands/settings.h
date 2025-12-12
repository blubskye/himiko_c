/*
 * Himiko Discord Bot (C Edition) - Settings Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_SETTINGS_H
#define HIMIKO_COMMANDS_SETTINGS_H

#include <concord/discord.h>
#include "bot.h"

void register_settings_commands(himiko_bot_t *bot);

/* Command handlers */
void cmd_setprefix(struct discord *client, const struct discord_interaction *interaction);
void cmd_setprefix_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_setmodlog(struct discord *client, const struct discord_interaction *interaction);
void cmd_setmodlog_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_setwelcome(struct discord *client, const struct discord_interaction *interaction);
void cmd_setwelcome_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_disablewelcome(struct discord *client, const struct discord_interaction *interaction);
void cmd_disablewelcome_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_setjoindm(struct discord *client, const struct discord_interaction *interaction);
void cmd_setjoindm_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_disablejoindm(struct discord *client, const struct discord_interaction *interaction);
void cmd_disablejoindm_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_settings(struct discord *client, const struct discord_interaction *interaction);
void cmd_settings_prefix(struct discord *client, const struct discord_message *msg, const char *args);

#endif /* HIMIKO_COMMANDS_SETTINGS_H */
