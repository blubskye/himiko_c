/*
 * Himiko Discord Bot (C Edition) - Spam Filter Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_SPAM_FILTER_H
#define HIMIKO_MODULES_SPAM_FILTER_H

#include <concord/discord.h>
#include "bot.h"

/* Module lifecycle */
void spam_filter_init(himiko_bot_t *bot);
void spam_filter_cleanup(himiko_bot_t *bot);

/* Message check - returns 1 if blocked, 0 if passed */
int spam_filter_check(struct discord *client, const struct discord_message *msg);

/* Commands */
void cmd_spamfilter(struct discord *client, const struct discord_interaction *interaction);
void cmd_spamfilter_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void register_spam_filter_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_SPAM_FILTER_H */
