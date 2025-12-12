/*
 * Himiko Discord Bot (C Edition) - Logging Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_LOGGING_H
#define HIMIKO_MODULES_LOGGING_H

#include <concord/discord.h>
#include "bot.h"

/* Module lifecycle */
void logging_init(himiko_bot_t *bot);
void logging_cleanup(himiko_bot_t *bot);

/* Event handlers */
void logging_on_message_delete(struct discord *client, u64snowflake guild_id, u64snowflake channel_id, const struct discord_message *msg);
void logging_on_message_update(struct discord *client, const struct discord_message *old_msg, const struct discord_message *new_msg);
void logging_on_voice_state_update(struct discord *client, const struct discord_voice_state *old_state, const struct discord_voice_state *new_state);
void logging_on_guild_member_update(struct discord *client, const struct discord_guild_member *old_member, const struct discord_guild_member *new_member);

/* Commands */
void cmd_setlogchannel(struct discord *client, const struct discord_interaction *interaction);
void cmd_setlogchannel_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_togglelogging(struct discord *client, const struct discord_interaction *interaction);
void cmd_togglelogging_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_logconfig(struct discord *client, const struct discord_interaction *interaction);
void cmd_logconfig_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_logstatus(struct discord *client, const struct discord_interaction *interaction);
void cmd_logstatus_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_disablechannellog(struct discord *client, const struct discord_interaction *interaction);
void cmd_disablechannellog_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_enablechannellog(struct discord *client, const struct discord_interaction *interaction);
void cmd_enablechannellog_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void register_logging_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_LOGGING_H */
