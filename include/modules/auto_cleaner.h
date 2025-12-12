/*
 * Himiko Discord Bot (C Edition) - Auto-Clean Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_AUTO_CLEANER_H
#define HIMIKO_MODULES_AUTO_CLEANER_H

#include <concord/discord.h>
#include "bot.h"
#include <time.h>

/* Auto-clean channel config */
typedef struct {
    int id;
    char guild_id[32];
    char channel_id[32];
    int interval_hours;
    int warning_minutes;
    time_t next_run;
    int clean_message;
    int clean_image;
    char created_by[32];
    time_t created_at;
} autoclean_channel_t;

/* Module lifecycle */
void auto_cleaner_init(himiko_bot_t *bot);
void auto_cleaner_tick(struct discord *client);
void auto_cleaner_cleanup(himiko_bot_t *bot);

/* Commands */
void cmd_autoclean(struct discord *client, const struct discord_interaction *interaction);
void cmd_autoclean_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_setcleanmessage(struct discord *client, const struct discord_interaction *interaction);
void cmd_setcleanmessage_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_setcleanimage(struct discord *client, const struct discord_interaction *interaction);
void cmd_setcleanimage_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void register_autoclean_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_AUTO_CLEANER_H */
