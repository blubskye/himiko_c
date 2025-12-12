/*
 * Himiko Discord Bot (C Edition) - Mention Response Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_MENTION_RESPONSE_H
#define HIMIKO_MODULES_MENTION_RESPONSE_H

#include <concord/discord.h>
#include "bot.h"
#include <time.h>

/* Mention response structure */
typedef struct {
    int id;
    char guild_id[32];
    char trigger_text[256];
    char response[2048];
    char image_url[512];
    char created_by[32];
    time_t created_at;
} mention_response_t;

/* Module lifecycle */
void mention_response_init(himiko_bot_t *bot);
void mention_response_cleanup(himiko_bot_t *bot);

/* Check message for mention triggers - returns 1 if handled */
int mention_response_check(struct discord *client, const struct discord_message *msg);

/* Commands */
void cmd_mention(struct discord *client, const struct discord_interaction *interaction);
void cmd_mention_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void register_mention_response_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_MENTION_RESPONSE_H */
