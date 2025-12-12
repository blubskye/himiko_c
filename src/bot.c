/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

/* Forward declarations for command registration */
void register_admin_commands(himiko_bot_t *bot);
void register_fun_commands(himiko_bot_t *bot);
void register_text_commands(himiko_bot_t *bot);
void register_images_commands(himiko_bot_t *bot);
void register_utility_commands(himiko_bot_t *bot);
void register_info_commands(himiko_bot_t *bot);
void register_lookup_commands(himiko_bot_t *bot);
void register_random_commands(himiko_bot_t *bot);
void register_tools_commands(himiko_bot_t *bot);
void register_settings_commands(himiko_bot_t *bot);
void register_xp_commands(himiko_bot_t *bot);
void register_ai_commands(himiko_bot_t *bot);

/* Global bot instance */
himiko_bot_t *g_bot = NULL;

/* Categories that are prefix-only to stay under Discord's 100 slash command limit */
static const char *prefix_only_categories[] = {
    "Fun",
    "Text",
    "Random",
    "Images",
    "Lookup",
    "Tools",
    "Utility",
    NULL
};

int is_prefix_only_category(const char *category) {
    for (int i = 0; prefix_only_categories[i] != NULL; i++) {
        if (strcmp(prefix_only_categories[i], category) == 0) {
            return 1;
        }
    }
    return 0;
}

int bot_init(himiko_bot_t *bot, const char *config_path) {
    memset(bot, 0, sizeof(himiko_bot_t));
    g_bot = bot;

    /* Initialize config with defaults */
    config_init_defaults(&bot->config);

    /* Load config from file */
    if (config_load(&bot->config, config_path) != 0) {
        fprintf(stderr, "Failed to load config from %s\n", config_path);
        /* Try environment variables as fallback */
        if (config_load_from_env(&bot->config) != 0) {
            fprintf(stderr, "Failed to load config from environment\n");
            return -1;
        }
        printf("Loaded config from environment variables\n");
    } else {
        printf("Loaded config from %s\n", config_path);
    }

    /* Open database */
    if (db_open(&bot->database, bot->config.database_path) != 0) {
        fprintf(stderr, "Failed to open database: %s\n", bot->config.database_path);
        return -1;
    }
    printf("Database opened: %s\n", bot->config.database_path);

    /* Initialize Discord client */
    bot->client = discord_init(bot->config.token);
    if (!bot->client) {
        fprintf(stderr, "Failed to initialize Discord client\n");
        db_close(&bot->database);
        return -1;
    }

    /* Set up event handlers */
    discord_set_on_ready(bot->client, on_ready);
    discord_set_on_interaction_create(bot->client, on_interaction_create);
    discord_set_on_message_create(bot->client, on_message_create);
    discord_set_on_message_delete(bot->client, on_message_delete);
    discord_set_on_guild_member_add(bot->client, on_guild_member_add);

    /* Set intents */
    discord_add_intents(bot->client, DISCORD_GATEWAY_GUILDS);
    discord_add_intents(bot->client, DISCORD_GATEWAY_GUILD_MEMBERS);
    discord_add_intents(bot->client, DISCORD_GATEWAY_GUILD_MESSAGES);
    discord_add_intents(bot->client, DISCORD_GATEWAY_MESSAGE_CONTENT);
    discord_add_intents(bot->client, DISCORD_GATEWAY_DIRECT_MESSAGES);

    /* Allocate commands array */
    bot->commands = calloc(500, sizeof(himiko_command_t));
    bot->command_count = 0;

    /* Register all commands */
    bot_register_all_commands(bot);

    bot->running = 1;
    return 0;
}

void bot_cleanup(himiko_bot_t *bot) {
    if (bot->client) {
        discord_cleanup(bot->client);
        bot->client = NULL;
    }
    db_close(&bot->database);
    if (bot->commands) {
        free(bot->commands);
        bot->commands = NULL;
    }
    g_bot = NULL;
}

int bot_run(himiko_bot_t *bot) {
    printf("\n");
    printf("  ██╗  ██╗██╗███╗   ███╗██╗██╗  ██╗ ██████╗ \n");
    printf("  ██║  ██║██║████╗ ████║██║██║ ██╔╝██╔═══██╗\n");
    printf("  ███████║██║██╔████╔██║██║█████╔╝ ██║   ██║\n");
    printf("  ██╔══██║██║██║╚██╔╝██║██║██╔═██╗ ██║   ██║\n");
    printf("  ██║  ██║██║██║ ╚═╝ ██║██║██║  ██╗╚██████╔╝\n");
    printf("  ╚═╝  ╚═╝╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═╝ ╚═════╝ \n");
    printf("\n");
    printf("  Himiko v%s (C Edition)\n", HIMIKO_VERSION);
    printf("  Made with love and obsessive devotion\n");
    printf("\n");

    discord_run(bot->client);
    return 0;
}

void bot_stop(himiko_bot_t *bot) {
    bot->running = 0;
    if (bot->client) {
        discord_shutdown(bot->client);
    }
}

void bot_register_command(himiko_bot_t *bot, const himiko_command_t *cmd) {
    if (bot->command_count < 500) {
        bot->commands[bot->command_count++] = *cmd;
    }
}

himiko_command_t *bot_find_command(himiko_bot_t *bot, const char *name) {
    for (int i = 0; i < bot->command_count; i++) {
        if (strcmp(bot->commands[i].name, name) == 0) {
            return &bot->commands[i];
        }
    }
    return NULL;
}

void bot_register_all_commands(himiko_bot_t *bot) {
    register_admin_commands(bot);
    register_fun_commands(bot);
    register_text_commands(bot);
    register_images_commands(bot);
    register_utility_commands(bot);
    register_info_commands(bot);
    register_lookup_commands(bot);
    register_random_commands(bot);
    register_tools_commands(bot);
    register_settings_commands(bot);
    register_xp_commands(bot);
    register_ai_commands(bot);

    printf("Registered %d commands\n", bot->command_count);
}

/* Event Handlers */
void on_ready(struct discord *client, const struct discord_ready *event) {
    (void)client;

    /* Store application ID for interaction responses */
    g_bot->config.app_id = event->application->id;

    printf("\n");
    printf("Bot is online!\n");
    printf("  Logged in as: %s#%s\n", event->user->username, event->user->discriminator);
    printf("  Bot ID: %lu\n", (unsigned long)event->user->id);
    printf("  App ID: %lu\n", (unsigned long)event->application->id);
    printf("  Guilds: %d\n", event->guilds->size);
    printf("\n");

    /* Register slash commands globally */
    struct discord_application_commands app_cmds = {0};
    struct discord_create_global_application_command *cmds = calloc(g_bot->command_count, sizeof(struct discord_create_global_application_command));

    int slash_count = 0;
    for (int i = 0; i < g_bot->command_count; i++) {
        himiko_command_t *cmd = &g_bot->commands[i];

        /* Skip prefix-only commands */
        if (cmd->prefix_only) continue;
        if (!cmd->slash_only && is_prefix_only_category(cmd->category)) continue;

        /* Only register if we have a slash handler */
        if (cmd->slash_handler == NULL) continue;

        cmds[slash_count].name = (char *)cmd->name;
        cmds[slash_count].description = (char *)cmd->description;
        slash_count++;

        /* Discord limit is 100 global slash commands */
        if (slash_count >= 100) {
            printf("Warning: Reached Discord's 100 slash command limit\n");
            break;
        }
    }

    printf("Registering %d slash commands...\n", slash_count);

    app_cmds.size = slash_count;
    app_cmds.array = (struct discord_application_command *)cmds;

    discord_bulk_overwrite_global_application_commands(client, event->application->id, &app_cmds, NULL);

    free(cmds);

    printf("Himiko is ready!\n");
}

void on_interaction_create(struct discord *client, const struct discord_interaction *interaction) {
    if (interaction->type != DISCORD_INTERACTION_APPLICATION_COMMAND) {
        return;
    }

    /* Check if user is bot banned */
    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)interaction->member->user->id);
    if (db_is_bot_banned(&g_bot->database, user_id_str)) {
        respond_ephemeral(client, interaction, "You are banned from using this bot.");
        return;
    }

    /* Find and execute command */
    himiko_command_t *cmd = bot_find_command(g_bot, interaction->data->name);
    if (cmd && cmd->slash_handler) {
        /* Log command if enabled */
        if (g_bot->config.features.command_history) {
            char guild_id_str[32], channel_id_str[32];
            snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);
            snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)interaction->channel_id);
            db_log_command(&g_bot->database, guild_id_str, channel_id_str, user_id_str, cmd->name, "");
        }

        cmd->slash_handler(client, interaction);
    } else {
        respond_ephemeral(client, interaction, "Unknown command.");
    }
}

void on_message_create(struct discord *client, const struct discord_message *msg) {
    /* Ignore bot messages */
    if (msg->author->bot) return;

    /* Check if user is bot banned */
    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);
    if (db_is_bot_banned(&g_bot->database, user_id_str)) {
        return;
    }

    /* Get guild prefix */
    char prefix[16];
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);
    db_get_prefix(&g_bot->database, guild_id_str, g_bot->config.prefix, prefix, sizeof(prefix));

    /* Check if message starts with prefix */
    size_t prefix_len = strlen(prefix);
    if (strncmp(msg->content, prefix, prefix_len) != 0) {
        /* Check for AFK mentions, etc. */
        return;
    }

    /* Parse command and args */
    const char *after_prefix = msg->content + prefix_len;
    char cmd_name[64];
    const char *args = NULL;

    /* Extract command name */
    int i = 0;
    while (after_prefix[i] && !isspace(after_prefix[i]) && i < 63) {
        cmd_name[i] = after_prefix[i];
        i++;
    }
    cmd_name[i] = '\0';

    /* Extract args */
    while (after_prefix[i] && isspace(after_prefix[i])) i++;
    if (after_prefix[i]) {
        args = &after_prefix[i];
    }

    /* Find command */
    himiko_command_t *cmd = bot_find_command(g_bot, cmd_name);
    if (!cmd) return;

    /* Check if command is slash-only */
    if (cmd->slash_only) {
        char reply[256];
        snprintf(reply, sizeof(reply), "This command is only available as a slash command. Use `/%s`", cmd_name);
        struct discord_create_message params = { .content = reply };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Log command if enabled */
    if (g_bot->config.features.command_history) {
        char channel_id_str[32];
        snprintf(channel_id_str, sizeof(channel_id_str), "%lu", (unsigned long)msg->channel_id);
        db_log_command(&g_bot->database, guild_id_str, channel_id_str, user_id_str, cmd_name, args ? args : "");
    }

    /* Execute command */
    if (cmd->prefix_handler) {
        cmd->prefix_handler(client, msg, args);
    } else {
        char reply[256];
        snprintf(reply, sizeof(reply), "Usage: `%s%s <args>`\nUse `%shelp %s` for details.", prefix, cmd_name, prefix, cmd_name);
        struct discord_create_message params = { .content = reply };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void on_message_delete(struct discord *client, const struct discord_message_delete *event) {
    (void)client;

    /* Log for snipe command - we'd need the message content which we don't have here */
    /* This would require caching messages, which is a more complex feature */
    (void)event;
}

void on_guild_member_add(struct discord *client, const struct discord_guild_member *member) {
    (void)client;

    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)member->guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)member->user->id);

    /* Record join for anti-raid tracking */
    time_t now = time(NULL);
    db_record_member_join(&g_bot->database, guild_id_str, user_id_str, now, 0);

    /* TODO: Welcome messages, anti-raid checks */
}

/* Utility Functions */
u64snowflake parse_user_mention(const char *mention) {
    if (!mention) return 0;

    /* Format: <@!123456789> or <@123456789> */
    if (mention[0] == '<' && mention[1] == '@') {
        const char *start = mention + 2;
        if (*start == '!') start++;
        return strtoull(start, NULL, 10);
    }

    /* Try direct ID */
    return strtoull(mention, NULL, 10);
}

u64snowflake parse_channel_mention(const char *mention) {
    if (!mention) return 0;

    /* Format: <#123456789> */
    if (mention[0] == '<' && mention[1] == '#') {
        return strtoull(mention + 2, NULL, 10);
    }

    return strtoull(mention, NULL, 10);
}

u64snowflake parse_role_mention(const char *mention) {
    if (!mention) return 0;

    /* Format: <@&123456789> */
    if (mention[0] == '<' && mention[1] == '@' && mention[2] == '&') {
        return strtoull(mention + 3, NULL, 10);
    }

    return strtoull(mention, NULL, 10);
}

void respond_ephemeral(struct discord *client, const struct discord_interaction *i, const char *message) {
    struct discord_interaction_response response = {
        .type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE,
        .data = &(struct discord_interaction_callback_data){
            .content = (char *)message,
            .flags = DISCORD_MESSAGE_EPHEMERAL
        }
    };
    discord_create_interaction_response(client, i->id, i->token, &response, NULL);
}

void respond_message(struct discord *client, const struct discord_interaction *i, const char *message) {
    struct discord_interaction_response response = {
        .type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE,
        .data = &(struct discord_interaction_callback_data){
            .content = (char *)message
        }
    };
    discord_create_interaction_response(client, i->id, i->token, &response, NULL);
}

char *snowflake_to_string(u64snowflake id, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "%lu", (unsigned long)id);
    return buf;
}

u64snowflake string_to_snowflake(const char *str) {
    return strtoull(str, NULL, 10);
}
