/*
 * Himiko Discord Bot (C Edition) - Ticket System Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_MODULES_TICKET_H
#define HIMIKO_MODULES_TICKET_H

#include <concord/discord.h>
#include "bot.h"

/* Ticket config structure */
typedef struct {
    char guild_id[32];
    char channel_id[32];
    int enabled;
    time_t created_at;
} ticket_config_t;

/* Module lifecycle */
void ticket_init(himiko_bot_t *bot);
void ticket_cleanup(himiko_bot_t *bot);

/* Commands */
void cmd_setticket(struct discord *client, const struct discord_interaction *interaction);
void cmd_setticket_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void cmd_disableticket(struct discord *client, const struct discord_interaction *interaction);
void cmd_disableticket_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void cmd_ticketstatus(struct discord *client, const struct discord_interaction *interaction);
void cmd_ticketstatus_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void cmd_ticket(struct discord *client, const struct discord_interaction *interaction);
void cmd_ticket_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void register_ticket_commands(himiko_bot_t *bot);

#endif /* HIMIKO_MODULES_TICKET_H */
