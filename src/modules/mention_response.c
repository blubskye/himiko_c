/*
 * Himiko Discord Bot (C Edition) - Mention Response Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/mention_response.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sqlite3.h>

void mention_response_init(himiko_bot_t *bot) {
    (void)bot;
}

void mention_response_cleanup(himiko_bot_t *bot) {
    (void)bot;
}

/* Database helpers */
static int add_mention_response(himiko_database_t *db, const char *guild_id, const char *trigger,
                                const char *response, const char *image_url, const char *created_by) {
    if (!db || !db->db || !guild_id || !trigger || !response) return -1;

    const char *sql = "INSERT INTO mention_responses (guild_id, trigger_text, response, image_url, created_by, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, trigger, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, response, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, image_url && image_url[0] ? image_url : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, created_by, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)time(NULL));

    int result = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
    return result;
}

static int remove_mention_response(himiko_database_t *db, const char *guild_id, const char *trigger) {
    if (!db || !db->db || !guild_id || !trigger) return -1;

    const char *sql = "DELETE FROM mention_responses WHERE guild_id = ? AND trigger_text = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, trigger, -1, SQLITE_STATIC);

    int result = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db->db) > 0) ? 0 : -1;
    sqlite3_finalize(stmt);
    return result;
}

static int get_mention_responses(himiko_database_t *db, const char *guild_id,
                                 mention_response_t *responses, int max_count, int *out_count) {
    if (!db || !db->db || !guild_id || !responses || !out_count) return -1;
    *out_count = 0;

    const char *sql = "SELECT id, guild_id, trigger_text, response, image_url, created_by, created_at "
                      "FROM mention_responses WHERE guild_id = ? ORDER BY created_at DESC";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_count) {
        mention_response_t *r = &responses[*out_count];
        memset(r, 0, sizeof(mention_response_t));

        r->id = sqlite3_column_int(stmt, 0);
        const char *gid = (const char *)sqlite3_column_text(stmt, 1);
        if (gid) strncpy(r->guild_id, gid, sizeof(r->guild_id) - 1);
        const char *trig = (const char *)sqlite3_column_text(stmt, 2);
        if (trig) strncpy(r->trigger_text, trig, sizeof(r->trigger_text) - 1);
        const char *resp = (const char *)sqlite3_column_text(stmt, 3);
        if (resp) strncpy(r->response, resp, sizeof(r->response) - 1);
        const char *img = (const char *)sqlite3_column_text(stmt, 4);
        if (img) strncpy(r->image_url, img, sizeof(r->image_url) - 1);
        const char *created = (const char *)sqlite3_column_text(stmt, 5);
        if (created) strncpy(r->created_by, created, sizeof(r->created_by) - 1);
        r->created_at = sqlite3_column_int64(stmt, 6);

        (*out_count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

static int find_mention_response(himiko_database_t *db, const char *guild_id, const char *trigger,
                                 mention_response_t *response) {
    if (!db || !db->db || !guild_id || !trigger || !response) return -1;
    memset(response, 0, sizeof(mention_response_t));

    const char *sql = "SELECT id, guild_id, trigger_text, response, image_url, created_by, created_at "
                      "FROM mention_responses WHERE guild_id = ? AND trigger_text = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, trigger, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        response->id = sqlite3_column_int(stmt, 0);
        const char *gid = (const char *)sqlite3_column_text(stmt, 1);
        if (gid) strncpy(response->guild_id, gid, sizeof(response->guild_id) - 1);
        const char *trig = (const char *)sqlite3_column_text(stmt, 2);
        if (trig) strncpy(response->trigger_text, trig, sizeof(response->trigger_text) - 1);
        const char *resp = (const char *)sqlite3_column_text(stmt, 3);
        if (resp) strncpy(response->response, resp, sizeof(response->response) - 1);
        const char *img = (const char *)sqlite3_column_text(stmt, 4);
        if (img) strncpy(response->image_url, img, sizeof(response->image_url) - 1);
        const char *created = (const char *)sqlite3_column_text(stmt, 5);
        if (created) strncpy(response->created_by, created, sizeof(response->created_by) - 1);
        response->created_at = sqlite3_column_int64(stmt, 6);
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return -1;
}

/* Convert string to lowercase */
static void str_to_lower(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

/* Truncate string with ellipsis */
static void truncate_str(const char *src, char *dst, size_t max_len) {
    if (strlen(src) <= max_len) {
        strcpy(dst, src);
    } else {
        strncpy(dst, src, max_len - 3);
        dst[max_len - 3] = '\0';
        strcat(dst, "...");
    }
}

/* Check message for mention triggers */
int mention_response_check(struct discord *client, const struct discord_message *msg) {
    if (!client || !msg || !msg->content || msg->guild_id == 0) return 0;
    if (msg->author && msg->author->bot) return 0;

    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return 0;

    /* Check if message mentions the bot */
    int mentions_bot = 0;
    if (msg->mentions && msg->mentions->size > 0) {
        struct discord_user *self;
        discord_get_current_user(client, &(struct discord_ret_user){ .sync = self });

        for (int i = 0; i < msg->mentions->size; i++) {
            if (msg->mentions->array[i].id == self->id) {
                mentions_bot = 1;
                break;
            }
        }
    }

    if (!mentions_bot) return 0;

    /* Get the message content after the mention */
    char content_lower[2048];
    strncpy(content_lower, msg->content, sizeof(content_lower) - 1);
    content_lower[sizeof(content_lower) - 1] = '\0';
    str_to_lower(content_lower);

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    /* Get all responses for this guild */
    mention_response_t responses[50];
    int count = 0;
    if (get_mention_responses(&bot->database, guild_id_str, responses, 50, &count) != 0 || count == 0) {
        return 0;
    }

    /* Check each trigger */
    for (int i = 0; i < count; i++) {
        char trigger_lower[256];
        strncpy(trigger_lower, responses[i].trigger_text, sizeof(trigger_lower) - 1);
        trigger_lower[sizeof(trigger_lower) - 1] = '\0';
        str_to_lower(trigger_lower);

        if (strstr(content_lower, trigger_lower)) {
            /* Found a match - send response */
            struct discord_embed embed = {0};
            int use_embed = responses[i].image_url[0] != '\0';

            if (use_embed) {
                embed.description = responses[i].response;
                embed.color = 0xFF69B4;
                embed.image = &(struct discord_embed_image){ .url = responses[i].image_url };

                struct discord_create_message params = {
                    .embeds = &(struct discord_embeds){
                        .size = 1,
                        .array = &embed
                    }
                };
                discord_create_message(client, msg->channel_id, &params, NULL);
            } else {
                struct discord_create_message params = {
                    .content = responses[i].response
                };
                discord_create_message(client, msg->channel_id, &params, NULL);
            }

            return 1;
        }
    }

    return 0;
}

/* Check if user is admin */
static int is_admin(const struct discord_guild_member *member) {
    if (!member) return 0;
    /* permissions is already a uint64_t bitmask */
    if (member->permissions & 0x8) return 1; /* ADMINISTRATOR */
    return 0;
}

/* /mention command handler */
void cmd_mention(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Admin only */
    if (!is_admin(interaction->member)) {
        respond_ephemeral(client, interaction, "You need administrator permission to manage mention responses.");
        return;
    }

    if (!interaction->data->options || interaction->data->options->size == 0) {
        respond_ephemeral(client, interaction, "Please specify a subcommand: add, remove, or list");
        return;
    }

    const char *subcommand = interaction->data->options->array[0].name;
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    if (strcmp(subcommand, "add") == 0) {
        /* Get options */
        struct discord_application_command_interaction_data_options *opts = interaction->data->options->array[0].options;
        if (!opts) {
            respond_ephemeral(client, interaction, "Missing options.");
            return;
        }

        const char *trigger = NULL;
        const char *response = NULL;
        const char *image_url = NULL;

        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "trigger") == 0) {
                trigger = opts->array[i].value;
            } else if (strcmp(opts->array[i].name, "response") == 0) {
                response = opts->array[i].value;
            } else if (strcmp(opts->array[i].name, "image") == 0) {
                image_url = opts->array[i].value;
            }
        }

        if (!trigger || !response) {
            respond_ephemeral(client, interaction, "Please provide both trigger and response.");
            return;
        }

        /* Convert trigger to lowercase */
        char trigger_lower[256];
        strncpy(trigger_lower, trigger, sizeof(trigger_lower) - 1);
        trigger_lower[sizeof(trigger_lower) - 1] = '\0';
        str_to_lower(trigger_lower);

        char user_id_str[32];
        snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)interaction->member->user->id);

        if (add_mention_response(&bot->database, guild_id_str, trigger_lower, response,
                                  image_url ? image_url : "", user_id_str) != 0) {
            respond_ephemeral(client, interaction, "Failed to add mention response. It may already exist.");
            return;
        }

        char msg[512];
        char truncated_response[100];
        truncate_str(response, truncated_response, 100);
        snprintf(msg, sizeof(msg),
            "**Mention Response Added**\n"
            "**Trigger:** %s\n"
            "**Response:** %s%s",
            trigger_lower, truncated_response,
            image_url ? "\n**Image:** Attached" : "");
        respond_message(client, interaction, msg);

    } else if (strcmp(subcommand, "remove") == 0) {
        struct discord_application_command_interaction_data_options *opts = interaction->data->options->array[0].options;
        if (!opts || opts->size == 0) {
            respond_ephemeral(client, interaction, "Please provide a trigger to remove.");
            return;
        }

        const char *trigger = NULL;
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "trigger") == 0) {
                trigger = opts->array[i].value;
                break;
            }
        }

        if (!trigger) {
            respond_ephemeral(client, interaction, "Please provide a trigger to remove.");
            return;
        }

        char trigger_lower[256];
        strncpy(trigger_lower, trigger, sizeof(trigger_lower) - 1);
        trigger_lower[sizeof(trigger_lower) - 1] = '\0';
        str_to_lower(trigger_lower);

        if (remove_mention_response(&bot->database, guild_id_str, trigger_lower) != 0) {
            respond_ephemeral(client, interaction, "Failed to remove mention response or it doesn't exist.");
            return;
        }

        char msg[256];
        snprintf(msg, sizeof(msg), "**Mention Response Removed**\nRemoved response for trigger: **%s**", trigger_lower);
        respond_message(client, interaction, msg);

    } else if (strcmp(subcommand, "list") == 0) {
        mention_response_t responses[50];
        int count = 0;

        if (get_mention_responses(&bot->database, guild_id_str, responses, 50, &count) != 0 || count == 0) {
            respond_ephemeral(client, interaction, "No custom mention responses configured.");
            return;
        }

        char response_text[2000];
        response_text[0] = '\0';
        strcat(response_text, "**Custom Mention Responses**\n\n");

        int shown = 0;
        for (int i = 0; i < count && shown < 15; i++) {
            char truncated[50];
            truncate_str(responses[i].response, truncated, 50);
            char line[256];
            snprintf(line, sizeof(line), "**%s**%s\n└ %s\n\n",
                responses[i].trigger_text,
                responses[i].image_url[0] ? " [IMG]" : "",
                truncated);

            if (strlen(response_text) + strlen(line) < sizeof(response_text) - 50) {
                strcat(response_text, line);
                shown++;
            } else {
                break;
            }
        }

        if (count > shown) {
            char footer[64];
            snprintf(footer, sizeof(footer), "... and %d more", count - shown);
            strcat(response_text, footer);
        }

        char total[32];
        snprintf(total, sizeof(total), "\n\n_%d responses configured_", count);
        strcat(response_text, total);

        respond_message(client, interaction, response_text);

    } else {
        respond_ephemeral(client, interaction, "Unknown subcommand.");
    }
}

void cmd_mention_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    if (!args || !*args) {
        /* Show list */
        mention_response_t responses[50];
        int count = 0;

        if (get_mention_responses(&bot->database, guild_id_str, responses, 50, &count) != 0 || count == 0) {
            struct discord_create_message params = {
                .content = "No mention responses configured. Use `mention add <trigger> | <response>` to add one."
            };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response_text[2000];
        snprintf(response_text, sizeof(response_text), "**Mention Responses** (%d total)\n", count);

        for (int i = 0; i < count && i < 10; i++) {
            char truncated[40];
            truncate_str(responses[i].response, truncated, 40);
            char line[128];
            snprintf(line, sizeof(line), "• **%s** → %s\n", responses[i].trigger_text, truncated);
            if (strlen(response_text) + strlen(line) < sizeof(response_text) - 20) {
                strcat(response_text, line);
            }
        }

        struct discord_create_message params = { .content = response_text };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse command */
    char cmd[32], rest[2048];
    if (sscanf(args, "%31s %2047[^\n]", cmd, rest) < 1) {
        struct discord_create_message params = {
            .content = "Usage:\n`mention add <trigger> | <response>` - Add a response\n"
                       "`mention remove <trigger>` - Remove a response\n"
                       "`mention` - List all responses"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strcmp(cmd, "add") == 0) {
        /* Format: add trigger | response */
        char *pipe = strchr(rest, '|');
        if (!pipe) {
            struct discord_create_message params = {
                .content = "Usage: mention add <trigger> | <response>"
            };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        *pipe = '\0';
        char *trigger = rest;
        char *response = pipe + 1;

        /* Trim whitespace */
        while (*trigger && isspace(*trigger)) trigger++;
        while (*response && isspace(*response)) response++;
        char *end = trigger + strlen(trigger) - 1;
        while (end > trigger && isspace(*end)) *end-- = '\0';
        end = response + strlen(response) - 1;
        while (end > response && isspace(*end)) *end-- = '\0';

        if (!*trigger || !*response) {
            struct discord_create_message params = {
                .content = "Please provide both a trigger and response."
            };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char trigger_lower[256];
        strncpy(trigger_lower, trigger, sizeof(trigger_lower) - 1);
        trigger_lower[sizeof(trigger_lower) - 1] = '\0';
        str_to_lower(trigger_lower);

        char user_id_str[32];
        snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);

        if (add_mention_response(&bot->database, guild_id_str, trigger_lower, response, "", user_id_str) != 0) {
            struct discord_create_message params = {
                .content = "Failed to add mention response. It may already exist."
            };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char resp[256];
        snprintf(resp, sizeof(resp), "Added mention response for **%s**", trigger_lower);
        struct discord_create_message params = { .content = resp };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else if (strcmp(cmd, "remove") == 0 || strcmp(cmd, "delete") == 0) {
        /* Trim whitespace */
        char *trigger = rest;
        while (*trigger && isspace(*trigger)) trigger++;
        char *end = trigger + strlen(trigger) - 1;
        while (end > trigger && isspace(*end)) *end-- = '\0';

        if (!*trigger) {
            struct discord_create_message params = {
                .content = "Usage: mention remove <trigger>"
            };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char trigger_lower[256];
        strncpy(trigger_lower, trigger, sizeof(trigger_lower) - 1);
        trigger_lower[sizeof(trigger_lower) - 1] = '\0';
        str_to_lower(trigger_lower);

        if (remove_mention_response(&bot->database, guild_id_str, trigger_lower) != 0) {
            struct discord_create_message params = {
                .content = "Failed to remove or response doesn't exist."
            };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char resp[128];
        snprintf(resp, sizeof(resp), "Removed mention response for **%s**", trigger_lower);
        struct discord_create_message params = { .content = resp };
        discord_create_message(client, msg->channel_id, &params, NULL);

    } else {
        struct discord_create_message params = {
            .content = "Usage: mention [add|remove|list]"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void register_mention_response_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "mention", "Manage custom mention responses", "Configuration", cmd_mention, cmd_mention_prefix, 0, DISCORD_PERM_ADMINISTRATOR },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
