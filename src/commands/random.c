/*
 * Himiko Discord Bot (C Edition) - Random Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/random.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>

/* CURL response buffer */
typedef struct {
    char *data;
    size_t size;
} curl_buffer_t;

static size_t write_callback_fn(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    curl_buffer_t *buf = (curl_buffer_t *)userp;

    char *ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr) return 0;

    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

static char *http_get_with_headers(const char *url, const char *accept_header) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    curl_buffer_t buf = { .data = malloc(1), .size = 0 };
    if (!buf.data) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    buf.data[0] = '\0';

    struct curl_slist *headers = NULL;
    if (accept_header) {
        char header[128];
        snprintf(header, sizeof(header), "Accept: %s", accept_header);
        headers = curl_slist_append(headers, header);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Himiko-Bot/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buf.data);
        return NULL;
    }

    return buf.data;
}

static char *http_get(const char *url) {
    return http_get_with_headers(url, NULL);
}

/* Simple JSON string extraction */
static const char *json_get_string(const char *json, const char *key, char *out, size_t out_len) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char *pos = strstr(json, search);
    if (!pos) return NULL;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;

    if (*pos == '"') {
        pos++;
        size_t i = 0;
        while (*pos && *pos != '"' && i < out_len - 1) {
            if (*pos == '\\' && *(pos + 1)) {
                pos++;
                if (*pos == 'n') out[i++] = '\n';
                else if (*pos == 't') out[i++] = '\t';
                else if (*pos == 'r') out[i++] = '\r';
                else out[i++] = *pos;
            } else {
                out[i++] = *pos;
            }
            pos++;
        }
        out[i] = '\0';
        return out;
    }
    return NULL;
}

/* Random seed initialization */
static int random_initialized = 0;
static void init_random(void) {
    if (!random_initialized) {
        srand((unsigned int)time(NULL));
        random_initialized = 1;
    }
}

void cmd_advice_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char *response_json = http_get("https://api.adviceslip.com/advice");
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch advice." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char advice[512] = "";
    json_get_string(response_json, "advice", advice, sizeof(advice));
    free(response_json);

    if (!advice[0]) {
        struct discord_create_message params = { .content = "No advice found." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[600];
    snprintf(response, sizeof(response), ":bulb: **Advice:** %s", advice);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_quote_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char *response_json = http_get("https://zenquotes.io/api/random");
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch quote." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* zenquotes returns an array, look for "q" and "a" */
    char quote[512] = "";
    char author[128] = "";
    json_get_string(response_json, "q", quote, sizeof(quote));
    json_get_string(response_json, "a", author, sizeof(author));
    free(response_json);

    if (!quote[0]) {
        struct discord_create_message params = { .content = "No quote found." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[800];
    snprintf(response, sizeof(response),
        ":scroll: *\"%s\"*\n\n- **%s**",
        quote, author[0] ? author : "Unknown");

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_fact_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char *response_json = http_get("https://uselessfacts.jsph.pl/api/v2/facts/random");
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch fact." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char fact[1024] = "";
    json_get_string(response_json, "text", fact, sizeof(fact));
    free(response_json);

    if (!fact[0]) {
        struct discord_create_message params = { .content = "No fact found." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[1100];
    snprintf(response, sizeof(response), ":brain: **Random Fact:** %s", fact);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_dadjoke_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char *response_json = http_get_with_headers("https://icanhazdadjoke.com/", "application/json");
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch dad joke." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char joke[1024] = "";
    json_get_string(response_json, "joke", joke, sizeof(joke));
    free(response_json);

    if (!joke[0]) {
        struct discord_create_message params = { .content = "No joke found." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[1100];
    snprintf(response, sizeof(response), ":laughing: %s", joke);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_password_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    init_random();

    int length = 16;
    if (args && *args) {
        length = atoi(args);
        if (length < 8) length = 8;
        if (length > 64) length = 64;
    }

    /* Character sets */
    const char lowercase[] = "abcdefghijklmnopqrstuvwxyz";
    const char uppercase[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char digits[] = "0123456789";
    const char symbols[] = "!@#$%^&*()-_=+[]{}|;:,.<>?";

    char all_chars[128];
    snprintf(all_chars, sizeof(all_chars), "%s%s%s%s", lowercase, uppercase, digits, symbols);
    size_t all_len = strlen(all_chars);

    char password[65];
    for (int i = 0; i < length; i++) {
        password[i] = all_chars[rand() % all_len];
    }
    password[length] = '\0';

    char response[256];
    snprintf(response, sizeof(response),
        ":key: **Generated Password** (%d chars):\n||`%s`||",
        length, password);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_random_commands(himiko_bot_t *bot) {
    /* Random commands are prefix-only to stay under Discord's 100 slash command limit */
    himiko_command_t cmds[] = {
        { "advice", "Get random advice", "Random", NULL, cmd_advice_prefix, 0, 0 },
        { "quote", "Get an inspirational quote", "Random", NULL, cmd_quote_prefix, 0, 0 },
        { "fact", "Get a random fact", "Random", NULL, cmd_fact_prefix, 0, 0 },
        { "dadjoke", "Get a random dad joke", "Random", NULL, cmd_dadjoke_prefix, 0, 0 },
        { "password", "Generate a secure password", "Random", NULL, cmd_password_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
