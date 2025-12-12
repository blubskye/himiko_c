/*
 * Himiko Discord Bot (C Edition) - Utility Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/utility.h"
#include "bot.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

/* Bot start time for uptime calculation */
static time_t bot_start_time = 0;

/* Parse duration string like "1h30m", "2d", "30m" into seconds */
time_t parse_duration(const char *str) {
    if (!str || !*str) return 0;

    time_t total = 0;
    long value = 0;

    while (*str) {
        if (isdigit(*str)) {
            value = value * 10 + (*str - '0');
        } else {
            switch (*str) {
                case 'd': case 'D':
                    total += value * 86400; /* days */
                    value = 0;
                    break;
                case 'h': case 'H':
                    total += value * 3600; /* hours */
                    value = 0;
                    break;
                case 'm': case 'M':
                    total += value * 60; /* minutes */
                    value = 0;
                    break;
                case 's': case 'S':
                    total += value; /* seconds */
                    value = 0;
                    break;
                default:
                    break;
            }
        }
        str++;
    }

    /* If there's a trailing number without unit, treat as minutes */
    if (value > 0) {
        total += value * 60;
    }

    return total;
}

/* Simple math evaluation */
static int evaluate_math(const char *expr, double *result) {
    double a, b;
    char op;

    /* Try to parse as single number */
    if (sscanf(expr, "%lf", &a) == 1 && strchr(expr, '+') == NULL &&
        strchr(expr, '-') == NULL && strchr(expr, '*') == NULL &&
        strchr(expr, '/') == NULL && strchr(expr, '^') == NULL &&
        strchr(expr, '%') == NULL) {
        *result = a;
        return 0;
    }

    /* Try basic operations */
    if (sscanf(expr, "%lf+%lf", &a, &b) == 2) {
        *result = a + b;
        return 0;
    }
    if (sscanf(expr, "%lf-%lf", &a, &b) == 2) {
        *result = a - b;
        return 0;
    }
    if (sscanf(expr, "%lf*%lf", &a, &b) == 2) {
        *result = a * b;
        return 0;
    }
    if (sscanf(expr, "%lf/%lf", &a, &b) == 2) {
        if (b == 0) return -1; /* Division by zero */
        *result = a / b;
        return 0;
    }
    if (sscanf(expr, "%lf^%lf", &a, &b) == 2) {
        *result = pow(a, b);
        return 0;
    }
    if (sscanf(expr, "%lf%%%lf", &a, &b) == 2) {
        *result = fmod(a, b);
        return 0;
    }

    return -1;
}

void cmd_ping(struct discord *client, const struct discord_interaction *interaction) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Send initial response */
    struct discord_interaction_response response = {
        .type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE,
        .data = &(struct discord_interaction_callback_data){
            .content = "Pinging..."
        }
    };
    discord_create_interaction_response(client, interaction->id, interaction->token, &response, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    long latency_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

    char response_text[256];
    snprintf(response_text, sizeof(response_text),
        "**Pong!**\n"
        "API Latency: %ldms",
        latency_ms);

    struct discord_edit_original_interaction_response edit = {
        .content = response_text
    };
    discord_edit_original_interaction_response(client, g_bot->config.app_id, interaction->token, &edit, NULL);
}

void cmd_ping_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    struct discord_create_message params = { .content = "Pinging..." };
    discord_create_message(client, msg->channel_id, &params, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    long latency_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

    char response[256];
    snprintf(response, sizeof(response),
        "**Pong!**\n"
        "API Latency: %ldms",
        latency_ms);

    /* Send as a second message since we can't easily edit without message ID */
    struct discord_create_message reply = { .content = response };
    discord_create_message(client, msg->channel_id, &reply, NULL);
}

void cmd_snipe(struct discord *client, const struct discord_interaction *interaction) {
    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)interaction->channel_id);

    deleted_message_t messages[5];
    int count;
    db_get_deleted_messages(&g_bot->database, channel_id_str, messages, 5, &count);

    if (count == 0) {
        respond_ephemeral(client, interaction, "No deleted messages found in this channel.");
        return;
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response), "**Sniped Messages**\n\n");

    for (int i = 0; i < count; i++) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**<@%s>**: %s\n",
            messages[i].user_id, messages[i].content);
    }

    respond_message(client, interaction, response);
}

void cmd_snipe_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)msg->channel_id);

    deleted_message_t messages[5];
    int count;
    db_get_deleted_messages(&g_bot->database, channel_id_str, messages, 5, &count);

    if (count == 0) {
        struct discord_create_message params = { .content = "No deleted messages found in this channel." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response), "**Sniped Messages**\n\n");

    for (int i = 0; i < count; i++) {
        offset += snprintf(response + offset, sizeof(response) - offset,
            "**<@%s>**: %s\n",
            messages[i].user_id, messages[i].content);
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_afk(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *message = "AFK";

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "message") == 0) {
                message = opts->array[i].value;
            }
        }
    }

    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)interaction->member->user->id);

    db_set_afk(&g_bot->database, user_id_str, message);

    char response[512];
    snprintf(response, sizeof(response), "You are now AFK: %s", message);
    respond_message(client, interaction, response);
}

void cmd_afk_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    const char *message = (args && *args) ? args : "AFK";

    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);

    db_set_afk(&g_bot->database, user_id_str, message);

    char response[512];
    snprintf(response, sizeof(response), "You are now AFK: %s", message);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_remind(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *time_str = NULL;
    const char *message = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "time") == 0) {
                time_str = opts->array[i].value;
            } else if (strcmp(opts->array[i].name, "message") == 0) {
                message = opts->array[i].value;
            }
        }
    }

    if (!time_str || !message) {
        respond_ephemeral(client, interaction, "Please specify a time and message.");
        return;
    }

    time_t duration = parse_duration(time_str);
    if (duration <= 0) {
        respond_ephemeral(client, interaction, "Invalid time format. Use format like: 1h30m, 2d, 30m");
        return;
    }

    time_t remind_at = time(NULL) + duration;

    char user_id_str[32], channel_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)interaction->member->user->id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)interaction->channel_id);

    db_add_reminder(&g_bot->database, user_id_str, channel_id_str, message, remind_at);

    char response[512];
    snprintf(response, sizeof(response),
        "**Reminder Set!**\n\n"
        "I'll remind you <t:%ld:R>\n"
        "**Message:** %s",
        (long)remind_at, message);

    respond_message(client, interaction, response);
}

void cmd_remind_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: remind <time> <message>\nExample: remind 1h30m Take a break" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char time_str[32];
    if (sscanf(args, "%31s", time_str) != 1) {
        struct discord_create_message params = { .content = "Invalid format." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    const char *message = args + strlen(time_str);
    while (*message == ' ') message++;

    if (!*message) {
        struct discord_create_message params = { .content = "Please provide a message." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    time_t duration = parse_duration(time_str);
    if (duration <= 0) {
        struct discord_create_message params = { .content = "Invalid time format. Use format like: 1h30m, 2d, 30m" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    time_t remind_at = time(NULL) + duration;

    char user_id_str[32], channel_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)msg->channel_id);

    db_add_reminder(&g_bot->database, user_id_str, channel_id_str, message, remind_at);

    char response[512];
    snprintf(response, sizeof(response),
        "**Reminder Set!**\n\n"
        "I'll remind you <t:%ld:R>\n"
        "**Message:** %s",
        (long)remind_at, message);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_uptime(struct discord *client, const struct discord_interaction *interaction) {
    if (bot_start_time == 0) {
        bot_start_time = time(NULL);
    }

    time_t uptime = time(NULL) - bot_start_time;

    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;

    char uptime_str[128];
    if (days > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%dd %dh %dm %ds", days, hours, minutes, seconds);
    } else if (hours > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%dh %dm %ds", hours, minutes, seconds);
    } else if (minutes > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%dm %ds", minutes, seconds);
    } else {
        snprintf(uptime_str, sizeof(uptime_str), "%ds", seconds);
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Bot Uptime**\n\n"
        "%s\n"
        "**Started:** <t:%ld:F>",
        uptime_str, (long)bot_start_time);

    respond_message(client, interaction, response);
}

void cmd_uptime_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    if (bot_start_time == 0) {
        bot_start_time = time(NULL);
    }

    time_t uptime = time(NULL) - bot_start_time;

    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;

    char uptime_str[128];
    if (days > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%dd %dh %dm %ds", days, hours, minutes, seconds);
    } else if (hours > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%dh %dm %ds", hours, minutes, seconds);
    } else if (minutes > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%dm %ds", minutes, seconds);
    } else {
        snprintf(uptime_str, sizeof(uptime_str), "%ds", seconds);
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Bot Uptime**\n\n"
        "%s\n"
        "**Started:** <t:%ld:F>",
        uptime_str, (long)bot_start_time);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_poll(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *question = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "question") == 0) {
                question = opts->array[i].value;
            }
        }
    }

    if (!question) {
        respond_ephemeral(client, interaction, "Please provide a question.");
        return;
    }

    char response[512];
    snprintf(response, sizeof(response), "**Poll**\n\n%s\n\nReact with your vote!", question);

    respond_message(client, interaction, response);

    /* Note: Adding reactions requires getting the message ID after sending
       which is more complex with interaction responses. For now, users can
       add reactions manually or we can implement this with follow-up messages. */
}

void cmd_poll_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: poll <question>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[512];
    snprintf(response, sizeof(response), "**Poll**\n\n%s\n\nReact with your vote!", args);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
    /* Note: Adding reactions would require callbacks to get the sent message ID */
}

void cmd_say(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *message = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "message") == 0) {
                message = opts->array[i].value;
            }
        }
    }

    if (!message) {
        respond_ephemeral(client, interaction, "Please provide a message.");
        return;
    }

    struct discord_create_message params = { .content = (char *)message };
    discord_create_message(client, interaction->channel_id, &params, NULL);

    respond_ephemeral(client, interaction, "Message sent!");
}

void cmd_say_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: say <message>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Delete the command message */
    struct discord_delete_message del_params = { .reason = NULL };
    discord_delete_message(client, msg->channel_id, msg->id, &del_params, NULL);

    /* Send the message */
    struct discord_create_message params = { .content = (char *)args };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_math(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *expression = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "expression") == 0) {
                expression = opts->array[i].value;
            }
        }
    }

    if (!expression) {
        respond_ephemeral(client, interaction, "Please provide an expression.");
        return;
    }

    double result;
    if (evaluate_math(expression, &result) != 0) {
        respond_ephemeral(client, interaction, "Invalid expression or division by zero.");
        return;
    }

    char response[256];
    if (result == (double)(int)result) {
        snprintf(response, sizeof(response), "**Math Result**\n\n`%s` = `%.0f`", expression, result);
    } else {
        snprintf(response, sizeof(response), "**Math Result**\n\n`%s` = `%.4f`", expression, result);
    }

    respond_message(client, interaction, response);
}

void cmd_math_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: math <expression>\nExample: math 2+2" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    double result;
    if (evaluate_math(args, &result) != 0) {
        struct discord_create_message params = { .content = "Invalid expression or division by zero." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[256];
    if (result == (double)(int)result) {
        snprintf(response, sizeof(response), "**Math Result**\n\n`%s` = `%.0f`", args, result);
    } else {
        snprintf(response, sizeof(response), "**Math Result**\n\n`%s` = `%.4f`", args, result);
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

/* Initialize bot start time */
void utility_init_start_time(void) {
    if (bot_start_time == 0) {
        bot_start_time = time(NULL);
    }
}

void register_utility_commands(himiko_bot_t *bot) {
    utility_init_start_time();

    himiko_command_t cmds[] = {
        { "ping", "Check bot latency", "Utility", cmd_ping, cmd_ping_prefix, 0, 0 },
        { "snipe", "Retrieve recently deleted messages", "Utility", cmd_snipe, cmd_snipe_prefix, 0, 0 },
        { "afk", "Set your AFK status", "Utility", cmd_afk, cmd_afk_prefix, 0, 0 },
        { "remind", "Set a reminder", "Utility", cmd_remind, cmd_remind_prefix, 0, 0 },
        { "uptime", "Check bot uptime", "Utility", cmd_uptime, cmd_uptime_prefix, 0, 0 },
        { "poll", "Create a poll", "Utility", cmd_poll, cmd_poll_prefix, 0, 0 },
        { "say", "Make the bot say something", "Utility", cmd_say, cmd_say_prefix, 0, 0 },
        { "math", "Simple math evaluation", "Utility", cmd_math, cmd_math_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
