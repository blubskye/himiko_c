/*
 * Himiko Discord Bot (C Edition) - Anti-Raid Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_ANTIRAID_H
#define HIMIKO_MODULES_ANTIRAID_H

#include <concord/discord.h>
#include "bot.h"
#include <time.h>
#include <pthread.h>

/* Raid tracker state per guild */
typedef struct {
    char guild_id[32];
    time_t last_raid_alert;
    time_t lockdown_start;
    int in_lockdown;
} raid_guild_state_t;

/* Global raid tracker */
typedef struct {
    raid_guild_state_t guilds[100];
    int guild_count;
    pthread_mutex_t mutex;
} raid_tracker_t;

/* Module lifecycle */
void antiraid_init(himiko_bot_t *bot);
void antiraid_cleanup(himiko_bot_t *bot);

/* Event handler */
void antiraid_on_member_join(struct discord *client, u64snowflake guild_id, const struct discord_user *user);

/* Commands */
void cmd_antiraid(struct discord *client, const struct discord_interaction *interaction);
void cmd_antiraid_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_silence(struct discord *client, const struct discord_interaction *interaction);
void cmd_silence_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_unsilence(struct discord *client, const struct discord_interaction *interaction);
void cmd_unsilence_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_getraid(struct discord *client, const struct discord_interaction *interaction);
void cmd_getraid_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_banraid(struct discord *client, const struct discord_interaction *interaction);
void cmd_banraid_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_lockdown(struct discord *client, const struct discord_interaction *interaction);
void cmd_lockdown_prefix(struct discord *client, const struct discord_message *msg, const char *args);

/* Helper functions */
void antiraid_check_lockdown_expiry(struct discord *client);
int64_t snowflake_to_timestamp_ms(u64snowflake id);
void register_antiraid_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_ANTIRAID_H */
