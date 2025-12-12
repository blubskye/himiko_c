/*
 * Himiko Discord Bot (C Edition) - Info Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/info.h"
#include "bot.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/utsname.h>

/* Convert snowflake to Unix timestamp */
static time_t snowflake_to_timestamp(u64snowflake id) {
    /* Discord epoch: Jan 1, 2015 */
    return ((id >> 22) + 1420070400000ULL) / 1000;
}

void cmd_userinfo(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = interaction->member->user->id;
    struct discord_user *target_user = interaction->member->user;

    /* Check if a specific user was mentioned */
    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    /* Get user info */
    struct discord_user user = {0};
    struct discord_ret_user ret = { .sync = &user };
    discord_get_user(client, user_id, &ret);

    /* Calculate account creation date from snowflake */
    time_t created_at = snowflake_to_timestamp(user_id);

    char avatar_url[256] = "No avatar";
    if (user.avatar && user.avatar[0]) {
        snprintf(avatar_url, sizeof(avatar_url),
            "https://cdn.discordapp.com/avatars/%lu/%s.%s",
            (unsigned long)user_id, user.avatar,
            (user.avatar[0] == 'a' && user.avatar[1] == '_') ? "gif" : "png");
    }

    char response[2048];
    snprintf(response, sizeof(response),
        "**User Information**\n\n"
        "**Username:** %s\n"
        "**ID:** %lu\n"
        "**Created:** <t:%ld:F> (<t:%ld:R>)\n"
        "**Bot:** %s\n"
        "**Avatar:** [Link](%s)",
        user.username ? user.username : "Unknown",
        (unsigned long)user_id,
        (long)created_at, (long)created_at,
        user.bot ? "Yes" : "No",
        avatar_url);

    respond_message(client, interaction, response);
}

void cmd_userinfo_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    u64snowflake user_id = msg->author->id;

    /* Parse user mention if provided */
    if (args && *args) {
        user_id = parse_user_mention(args);
        if (user_id == 0) {
            user_id = msg->author->id;
        }
    }

    /* Get user info */
    struct discord_user user = {0};
    struct discord_ret_user ret = { .sync = &user };
    discord_get_user(client, user_id, &ret);

    time_t created_at = snowflake_to_timestamp(user_id);

    char avatar_url[256] = "No avatar";
    if (user.avatar && user.avatar[0]) {
        snprintf(avatar_url, sizeof(avatar_url),
            "https://cdn.discordapp.com/avatars/%lu/%s.%s",
            (unsigned long)user_id, user.avatar,
            (user.avatar[0] == 'a' && user.avatar[1] == '_') ? "gif" : "png");
    }

    char response[2048];
    snprintf(response, sizeof(response),
        "**User Information**\n\n"
        "**Username:** %s\n"
        "**ID:** %lu\n"
        "**Created:** <t:%ld:F> (<t:%ld:R>)\n"
        "**Bot:** %s\n"
        "**Avatar:** [Link](%s)",
        user.username ? user.username : "Unknown",
        (unsigned long)user_id,
        (long)created_at, (long)created_at,
        user.bot ? "Yes" : "No",
        avatar_url);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_serverinfo(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_guild guild = {0};
    struct discord_ret_guild ret = { .sync = &guild };
    discord_get_guild(client, interaction->guild_id, &ret);

    time_t created_at = snowflake_to_timestamp(interaction->guild_id);

    char icon_url[256] = "No icon";
    if (guild.icon && guild.icon[0]) {
        snprintf(icon_url, sizeof(icon_url),
            "https://cdn.discordapp.com/icons/%lu/%s.%s",
            (unsigned long)interaction->guild_id, guild.icon,
            (guild.icon[0] == 'a' && guild.icon[1] == '_') ? "gif" : "png");
    }

    char response[2048];
    snprintf(response, sizeof(response),
        "**Server Information**\n\n"
        "**Name:** %s\n"
        "**ID:** %lu\n"
        "**Owner:** <@%lu>\n"
        "**Created:** <t:%ld:F> (<t:%ld:R>)\n"
        "**Members:** %d\n"
        "**Channels:** %d\n"
        "**Roles:** %d\n"
        "**Boost Level:** %d\n"
        "**Boost Count:** %d\n"
        "**Icon:** [Link](%s)",
        guild.name ? guild.name : "Unknown",
        (unsigned long)interaction->guild_id,
        (unsigned long)guild.owner_id,
        (long)created_at, (long)created_at,
        guild.member_count,
        guild.channels ? guild.channels->size : 0,
        guild.roles ? guild.roles->size : 0,
        guild.premium_tier,
        guild.premium_subscription_count,
        icon_url);

    respond_message(client, interaction, response);
}

void cmd_serverinfo_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    struct discord_guild guild = {0};
    struct discord_ret_guild ret = { .sync = &guild };
    discord_get_guild(client, msg->guild_id, &ret);

    time_t created_at = snowflake_to_timestamp(msg->guild_id);

    char icon_url[256] = "No icon";
    if (guild.icon && guild.icon[0]) {
        snprintf(icon_url, sizeof(icon_url),
            "https://cdn.discordapp.com/icons/%lu/%s.%s",
            (unsigned long)msg->guild_id, guild.icon,
            (guild.icon[0] == 'a' && guild.icon[1] == '_') ? "gif" : "png");
    }

    char response[2048];
    snprintf(response, sizeof(response),
        "**Server Information**\n\n"
        "**Name:** %s\n"
        "**ID:** %lu\n"
        "**Owner:** <@%lu>\n"
        "**Created:** <t:%ld:F> (<t:%ld:R>)\n"
        "**Members:** %d\n"
        "**Channels:** %d\n"
        "**Roles:** %d\n"
        "**Boost Level:** %d\n"
        "**Boost Count:** %d\n"
        "**Icon:** [Link](%s)",
        guild.name ? guild.name : "Unknown",
        (unsigned long)msg->guild_id,
        (unsigned long)guild.owner_id,
        (long)created_at, (long)created_at,
        guild.member_count,
        guild.channels ? guild.channels->size : 0,
        guild.roles ? guild.roles->size : 0,
        guild.premium_tier,
        guild.premium_subscription_count,
        icon_url);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_botinfo(struct discord *client, const struct discord_interaction *interaction) {
    struct utsname sys_info;
    uname(&sys_info);

    char response[2048];
    snprintf(response, sizeof(response),
        "**Himiko Bot Information**\n\n"
        "**Version:** %s (C Edition)\n"
        "**Library:** Concord (Discord C Library)\n"
        "**Language:** C11\n"
        "**Platform:** %s %s\n"
        "**Commands:** %d registered\n"
        "**Prefix:** `%s`\n\n"
        "**Links:**\n"
        "[GitHub](https://github.com/blubskye/himiko_c) |"
        "[Support Server](https://discord.gg/himiko)",
        HIMIKO_VERSION,
        sys_info.sysname, sys_info.release,
        g_bot->command_count,
        g_bot->config.prefix);

    respond_message(client, interaction, response);
}

void cmd_botinfo_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    struct utsname sys_info;
    uname(&sys_info);

    char response[2048];
    snprintf(response, sizeof(response),
        "**Himiko Bot Information**\n\n"
        "**Version:** %s (C Edition)\n"
        "**Library:** Concord (Discord C Library)\n"
        "**Language:** C11\n"
        "**Platform:** %s %s\n"
        "**Commands:** %d registered\n"
        "**Prefix:** `%s`\n\n"
        "**Links:**\n"
        "[GitHub](https://github.com/blubskye/himiko_c) |"
        "[Support Server](https://discord.gg/himiko)",
        HIMIKO_VERSION,
        sys_info.sysname, sys_info.release,
        g_bot->command_count,
        g_bot->config.prefix);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_avatar(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = interaction->member->user->id;
    const char *avatar_hash = interaction->member->user->avatar;

    /* Check if a specific user was mentioned */
    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
                /* Need to fetch the user to get their avatar */
                struct discord_user user = {0};
                struct discord_ret_user ret = { .sync = &user };
                discord_get_user(client, user_id, &ret);
                avatar_hash = user.avatar;
            }
        }
    }

    if (!avatar_hash || !avatar_hash[0]) {
        respond_ephemeral(client, interaction, "User has no avatar.");
        return;
    }

    char avatar_url[512];
    snprintf(avatar_url, sizeof(avatar_url),
        "https://cdn.discordapp.com/avatars/%lu/%s.%s?size=1024",
        (unsigned long)user_id, avatar_hash,
        (avatar_hash[0] == 'a' && avatar_hash[1] == '_') ? "gif" : "png");

    char response[1024];
    snprintf(response, sizeof(response),
        "**Avatar for <@%lu>**\n\n[PNG](%s) | [WEBP](%s)",
        (unsigned long)user_id, avatar_url, avatar_url);

    respond_message(client, interaction, response);
}

void cmd_avatar_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    u64snowflake user_id = msg->author->id;
    const char *avatar_hash = msg->author->avatar;

    /* Parse user mention if provided */
    if (args && *args) {
        user_id = parse_user_mention(args);
        if (user_id == 0) {
            user_id = msg->author->id;
        } else {
            struct discord_user user = {0};
            struct discord_ret_user ret = { .sync = &user };
            discord_get_user(client, user_id, &ret);
            avatar_hash = user.avatar;
        }
    }

    if (!avatar_hash || !avatar_hash[0]) {
        struct discord_create_message params = { .content = "User has no avatar." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char avatar_url[512];
    snprintf(avatar_url, sizeof(avatar_url),
        "https://cdn.discordapp.com/avatars/%lu/%s.%s?size=1024",
        (unsigned long)user_id, avatar_hash,
        (avatar_hash[0] == 'a' && avatar_hash[1] == '_') ? "gif" : "png");

    char response[1024];
    snprintf(response, sizeof(response),
        "**Avatar for <@%lu>**\n\n%s",
        (unsigned long)user_id, avatar_url);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_membercount(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_guild guild = {0};
    struct discord_ret_guild ret = { .sync = &guild };
    discord_get_guild(client, interaction->guild_id, &ret);

    char response[256];
    snprintf(response, sizeof(response),
        "**Member Count**\n\n"
        "**Total:** %d members",
        guild.member_count);

    respond_message(client, interaction, response);
}

void cmd_membercount_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    struct discord_guild guild = {0};
    struct discord_ret_guild ret = { .sync = &guild };
    discord_get_guild(client, msg->guild_id, &ret);

    char response[256];
    snprintf(response, sizeof(response),
        "**Member Count**\n\n"
        "**Total:** %d members",
        guild.member_count);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_info_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "userinfo", "Get information about a user", "Info", cmd_userinfo, cmd_userinfo_prefix, 0, 0 },
        { "serverinfo", "Get information about the server", "Info", cmd_serverinfo, cmd_serverinfo_prefix, 0, 0 },
        { "botinfo", "Get information about the bot", "Info", cmd_botinfo, cmd_botinfo_prefix, 0, 0 },
        { "avatar", "Get a user's avatar", "Info", cmd_avatar, cmd_avatar_prefix, 0, 0 },
        { "membercount", "Get server member count", "Info", cmd_membercount, cmd_membercount_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
