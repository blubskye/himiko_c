/*
 * Himiko Discord Bot (C Edition) - XP/Leveling Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/xp.h"
#include "bot.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * XP formula (same as Go version):
 * level = floor((sqrt(1 + 8*xp/50) - 1) / 2)
 *
 * To get XP needed for a level:
 * xp = 50 * level * (level + 1) / 2
 *
 * Note: calculate_level() and xp_for_level() are defined in database.c
 */

/* Generate a simple text progress bar */
static void progress_bar(int current, int max, char *buf, size_t buf_size) {
    int filled = (max > 0) ? (current * 10 / max) : 0;
    if (filled > 10) filled = 10;

    int i = 0;
    buf[i++] = '[';
    for (int j = 0; j < 10; j++) {
        buf[i++] = (j < filled) ? '#' : '-';
    }
    buf[i++] = ']';
    buf[i] = '\0';
}

void cmd_xp(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = interaction->member->user->id;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    user_xp_t xp_data;
    if (db_get_user_xp(&g_bot->database, guild_id_str, user_id_str, &xp_data) != 0) {
        xp_data.xp = 0;
        xp_data.level = 0;
    }

    int level = calculate_level(xp_data.xp);
    int64_t current_level_xp = xp_for_level(level);
    int64_t next_level_xp = xp_for_level(level + 1);
    int64_t xp_in_level = xp_data.xp - current_level_xp;
    int64_t xp_needed = next_level_xp - current_level_xp;

    char bar[16];
    progress_bar((int)xp_in_level, (int)xp_needed, bar, sizeof(bar));

    char response[512];
    snprintf(response, sizeof(response),
        "**XP Stats for <@%lu>**\n\n"
        "**Level:** %d\n"
        "**Total XP:** %ld\n"
        "**Progress:** %s %ld/%ld XP\n"
        "**Next Level:** %ld XP needed",
        (unsigned long)user_id,
        level,
        (long)xp_data.xp,
        bar, (long)xp_in_level, (long)xp_needed,
        (long)(xp_needed - xp_in_level));

    respond_message(client, interaction, response);
}

void cmd_xp_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    u64snowflake user_id = msg->author->id;

    if (args && *args) {
        u64snowflake parsed = parse_user_mention(args);
        if (parsed != 0) user_id = parsed;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    user_xp_t xp_data;
    if (db_get_user_xp(&g_bot->database, guild_id_str, user_id_str, &xp_data) != 0) {
        xp_data.xp = 0;
        xp_data.level = 0;
    }

    int level = calculate_level(xp_data.xp);
    int64_t current_level_xp = xp_for_level(level);
    int64_t next_level_xp = xp_for_level(level + 1);
    int64_t xp_in_level = xp_data.xp - current_level_xp;
    int64_t xp_needed = next_level_xp - current_level_xp;

    char bar[16];
    progress_bar((int)xp_in_level, (int)xp_needed, bar, sizeof(bar));

    char response[512];
    snprintf(response, sizeof(response),
        "**XP Stats for <@%lu>**\n\n"
        "**Level:** %d\n"
        "**Total XP:** %ld\n"
        "**Progress:** %s %ld/%ld XP\n"
        "**Next Level:** %ld XP needed",
        (unsigned long)user_id,
        level,
        (long)xp_data.xp,
        bar, (long)xp_in_level, (long)xp_needed,
        (long)(xp_needed - xp_in_level));

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_rank(struct discord *client, const struct discord_interaction *interaction) {
    /* Rank is an alias for xp with position info */
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = interaction->member->user->id;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    user_xp_t xp_data;
    if (db_get_user_xp(&g_bot->database, guild_id_str, user_id_str, &xp_data) != 0) {
        xp_data.xp = 0;
        xp_data.level = 0;
    }

    int level = calculate_level(xp_data.xp);

    /* Get leaderboard position */
    user_xp_t leaderboard[100];
    int count;
    db_get_leaderboard(&g_bot->database, guild_id_str, leaderboard, 100, &count);

    int position = count + 1; /* Default to last if not found */
    for (int i = 0; i < count; i++) {
        if (strcmp(leaderboard[i].user_id, user_id_str) == 0) {
            position = i + 1;
            break;
        }
    }

    char response[512];
    snprintf(response, sizeof(response),
        "**Rank for <@%lu>**\n\n"
        "**Position:** #%d\n"
        "**Level:** %d\n"
        "**Total XP:** %ld",
        (unsigned long)user_id,
        position,
        level,
        (long)xp_data.xp);

    respond_message(client, interaction, response);
}

void cmd_rank_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    u64snowflake user_id = msg->author->id;

    if (args && *args) {
        u64snowflake parsed = parse_user_mention(args);
        if (parsed != 0) user_id = parsed;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    user_xp_t xp_data;
    if (db_get_user_xp(&g_bot->database, guild_id_str, user_id_str, &xp_data) != 0) {
        xp_data.xp = 0;
        xp_data.level = 0;
    }

    int level = calculate_level(xp_data.xp);

    user_xp_t leaderboard[100];
    int count;
    db_get_leaderboard(&g_bot->database, guild_id_str, leaderboard, 100, &count);

    int position = count + 1;
    for (int i = 0; i < count; i++) {
        if (strcmp(leaderboard[i].user_id, user_id_str) == 0) {
            position = i + 1;
            break;
        }
    }

    char response[512];
    snprintf(response, sizeof(response),
        "**Rank for <@%lu>**\n\n"
        "**Position:** #%d\n"
        "**Level:** %d\n"
        "**Total XP:** %ld",
        (unsigned long)user_id,
        position,
        level,
        (long)xp_data.xp);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_leaderboard(struct discord *client, const struct discord_interaction *interaction) {
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    user_xp_t leaderboard[10];
    int count;
    db_get_leaderboard(&g_bot->database, guild_id_str, leaderboard, 10, &count);

    if (count == 0) {
        respond_message(client, interaction, "No XP data yet! Start chatting to earn XP.");
        return;
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response), "**XP Leaderboard**\n\n");

    for (int i = 0; i < count; i++) {
        int level = calculate_level(leaderboard[i].xp);
        const char *medal = "";
        if (i == 0) medal = ":first_place: ";
        else if (i == 1) medal = ":second_place: ";
        else if (i == 2) medal = ":third_place: ";

        offset += snprintf(response + offset, sizeof(response) - offset,
            "%s**#%d** <@%s> - Level %d (%ld XP)\n",
            medal, i + 1, leaderboard[i].user_id, level, (long)leaderboard[i].xp);
    }

    respond_message(client, interaction, response);
}

void cmd_leaderboard_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    user_xp_t leaderboard[10];
    int count;
    db_get_leaderboard(&g_bot->database, guild_id_str, leaderboard, 10, &count);

    if (count == 0) {
        struct discord_create_message params = { .content = "No XP data yet! Start chatting to earn XP." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[2048];
    int offset = snprintf(response, sizeof(response), "**XP Leaderboard**\n\n");

    for (int i = 0; i < count; i++) {
        int level = calculate_level(leaderboard[i].xp);
        const char *medal = "";
        if (i == 0) medal = ":first_place: ";
        else if (i == 1) medal = ":second_place: ";
        else if (i == 2) medal = ":third_place: ";

        offset += snprintf(response + offset, sizeof(response) - offset,
            "%s**#%d** <@%s> - Level %d (%ld XP)\n",
            medal, i + 1, leaderboard[i].user_id, level, (long)leaderboard[i].xp);
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_setxp(struct discord *client, const struct discord_interaction *interaction) {
    /* Admin only - set user's XP */
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;
    int xp = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "xp") == 0) {
                xp = (int)strtol(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user.");
        return;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    int level = calculate_level(xp);
    db_set_user_xp(&g_bot->database, guild_id_str, user_id_str, xp, level);

    char response[256];
    snprintf(response, sizeof(response),
        "Set <@%lu>'s XP to **%d** (Level %d)",
        (unsigned long)user_id, xp, level);

    respond_message(client, interaction, response);
}

void cmd_setxp_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: setxp <user> <amount>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char user_mention[64];
    int xp;
    if (sscanf(args, "%63s %d", user_mention, &xp) != 2) {
        struct discord_create_message params = { .content = "Usage: setxp <user> <amount>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake user_id = parse_user_mention(user_mention);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    int level = calculate_level(xp);
    db_set_user_xp(&g_bot->database, guild_id_str, user_id_str, xp, level);

    char response[256];
    snprintf(response, sizeof(response),
        "Set <@%lu>'s XP to **%d** (Level %d)",
        (unsigned long)user_id, xp, level);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_addxp(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;
    int amount = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            } else if (strcmp(opts->array[i].name, "amount") == 0) {
                amount = (int)strtol(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user.");
        return;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    user_xp_t xp_data;
    db_add_user_xp(&g_bot->database, guild_id_str, user_id_str, amount, &xp_data);

    char response[256];
    snprintf(response, sizeof(response),
        "Added **%d** XP to <@%lu> (Now: %ld XP, Level %d)",
        amount, (unsigned long)user_id, (long)xp_data.xp, calculate_level(xp_data.xp));

    respond_message(client, interaction, response);
}

void cmd_addxp_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: addxp <user> <amount>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char user_mention[64];
    int amount;
    if (sscanf(args, "%63s %d", user_mention, &amount) != 2) {
        struct discord_create_message params = { .content = "Usage: addxp <user> <amount>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake user_id = parse_user_mention(user_mention);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Could not find that user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user_id);

    user_xp_t xp_data;
    db_add_user_xp(&g_bot->database, guild_id_str, user_id_str, amount, &xp_data);

    char response[256];
    snprintf(response, sizeof(response),
        "Added **%d** XP to <@%lu> (Now: %ld XP, Level %d)",
        amount, (unsigned long)user_id, (long)xp_data.xp, calculate_level(xp_data.xp));

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_xp_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "xp", "Check XP and level", "XP", cmd_xp, cmd_xp_prefix, 0, 0 },
        { "rank", "Check your rank on the leaderboard", "XP", cmd_rank, cmd_rank_prefix, 0, 0 },
        { "leaderboard", "View the XP leaderboard", "XP", cmd_leaderboard, cmd_leaderboard_prefix, 0, 0 },
        { "setxp", "Set a user's XP (Admin)", "XP", cmd_setxp, cmd_setxp_prefix, 0, 0 },
        { "addxp", "Add XP to a user (Admin)", "XP", cmd_addxp, cmd_addxp_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
