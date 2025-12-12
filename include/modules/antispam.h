/*
 * Himiko Discord Bot (C Edition) - Anti-Spam Pressure Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_ANTISPAM_H
#define HIMIKO_MODULES_ANTISPAM_H

#include <concord/discord.h>
#include "bot.h"
#include <time.h>
#include <pthread.h>

/* User pressure tracking */
typedef struct {
    char guild_user_key[96]; /* guild_id:user_id */
    double pressure;
    char last_message[512];
    time_t last_update;
} user_pressure_t;

/* Global spam tracker */
typedef struct {
    user_pressure_t users[1000];
    int user_count;
    pthread_mutex_t mutex;
} spam_tracker_t;

/* Module lifecycle */
void antispam_init(himiko_bot_t *bot);
void antispam_cleanup(himiko_bot_t *bot);

/* Message handler - returns 1 if action taken, 0 otherwise */
int antispam_check(himiko_bot_t *bot, struct discord *client, const struct discord_message *msg);

/* Reset a user's pressure */
void antispam_reset_pressure(const char *guild_id, const char *user_id);

/* Commands */
void cmd_antispam(struct discord *client, const struct discord_interaction *interaction);
void cmd_antispam_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void register_antispam_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_ANTISPAM_H */
