/*
 * Himiko Discord Bot (C Edition) - Lookup Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/lookup.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>

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

static char *http_get(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    curl_buffer_t buf = { .data = malloc(1), .size = 0 };
    if (!buf.data) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Himiko-Bot/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buf.data);
        return NULL;
    }

    return buf.data;
}

/* URL encode a string */
static char *url_encode(const char *str) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char *encoded = curl_easy_escape(curl, str, 0);
    char *result = encoded ? strdup(encoded) : NULL;
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

/* Simple JSON string extraction - finds "key": "value" and returns value */
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

void cmd_urban_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: urban <term>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char *encoded = url_encode(args);
    if (!encoded) {
        struct discord_create_message params = { .content = "Failed to encode search term." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char url[512];
    snprintf(url, sizeof(url), "https://api.urbandictionary.com/v0/define?term=%s", encoded);
    free(encoded);

    char *response_json = http_get(url);
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch definition." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse the response - look for first definition */
    char word[256] = "";
    char definition[1024] = "";
    char example[512] = "";

    json_get_string(response_json, "word", word, sizeof(word));
    json_get_string(response_json, "definition", definition, sizeof(definition));
    json_get_string(response_json, "example", example, sizeof(example));

    free(response_json);

    if (!definition[0]) {
        struct discord_create_message params = { .content = "No definition found." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Truncate if too long */
    if (strlen(definition) > 500) {
        definition[497] = '.';
        definition[498] = '.';
        definition[499] = '.';
        definition[500] = '\0';
    }
    if (strlen(example) > 200) {
        example[197] = '.';
        example[198] = '.';
        example[199] = '.';
        example[200] = '\0';
    }

    char response[2000];
    if (example[0]) {
        snprintf(response, sizeof(response),
            "**%s** (Urban Dictionary)\n\n"
            "%s\n\n"
            "*Example:* %s",
            word[0] ? word : args, definition, example);
    } else {
        snprintf(response, sizeof(response),
            "**%s** (Urban Dictionary)\n\n%s",
            word[0] ? word : args, definition);
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_wiki_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: wiki <search term>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char *encoded = url_encode(args);
    if (!encoded) {
        struct discord_create_message params = { .content = "Failed to encode search term." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Wikipedia API - get extract */
    char url[512];
    snprintf(url, sizeof(url),
        "https://en.wikipedia.org/api/rest_v1/page/summary/%s",
        encoded);
    free(encoded);

    char *response_json = http_get(url);
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch Wikipedia article." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char title[256] = "";
    char extract[1024] = "";
    char page_url[512] = "";

    json_get_string(response_json, "title", title, sizeof(title));
    json_get_string(response_json, "extract", extract, sizeof(extract));
    json_get_string(response_json, "content_urls", page_url, sizeof(page_url)); /* Won't work perfectly but gives indication */

    free(response_json);

    if (!extract[0]) {
        struct discord_create_message params = { .content = "No Wikipedia article found." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Truncate if too long */
    if (strlen(extract) > 800) {
        extract[797] = '.';
        extract[798] = '.';
        extract[799] = '.';
        extract[800] = '\0';
    }

    char response[2000];
    snprintf(response, sizeof(response),
        "**%s** (Wikipedia)\n\n%s\n\n[Read more](https://en.wikipedia.org/wiki/%s)",
        title[0] ? title : args, extract, title[0] ? title : args);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_ip_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: ip <ip address>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Validate IP format (basic check) */
    int dots = 0;
    for (const char *p = args; *p; p++) {
        if (*p == '.') dots++;
        else if (!isdigit(*p)) break;
    }
    if (dots != 3) {
        struct discord_create_message params = { .content = "Please provide a valid IPv4 address." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://ip-api.com/json/%s", args);

    char *response_json = http_get(url);
    if (!response_json) {
        struct discord_create_message params = { .content = "Failed to fetch IP information." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char status[32] = "";
    char country[128] = "";
    char region[128] = "";
    char city[128] = "";
    char isp[256] = "";
    char timezone_str[64] = "";

    json_get_string(response_json, "status", status, sizeof(status));
    json_get_string(response_json, "country", country, sizeof(country));
    json_get_string(response_json, "regionName", region, sizeof(region));
    json_get_string(response_json, "city", city, sizeof(city));
    json_get_string(response_json, "isp", isp, sizeof(isp));
    json_get_string(response_json, "timezone", timezone_str, sizeof(timezone_str));

    free(response_json);

    if (strcmp(status, "fail") == 0) {
        struct discord_create_message params = { .content = "Failed to lookup that IP address." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[1024];
    snprintf(response, sizeof(response),
        "**IP Lookup:** `%s`\n\n"
        "**Country:** %s\n"
        "**Region:** %s\n"
        "**City:** %s\n"
        "**ISP:** %s\n"
        "**Timezone:** %s",
        args,
        country[0] ? country : "Unknown",
        region[0] ? region : "Unknown",
        city[0] ? city : "Unknown",
        isp[0] ? isp : "Unknown",
        timezone_str[0] ? timezone_str : "Unknown");

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_color_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: color <hex color>\nExample: color #ff5500 or color ff5500" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Parse hex color */
    const char *hex = args;
    if (*hex == '#') hex++;

    if (strlen(hex) != 6) {
        struct discord_create_message params = { .content = "Please provide a valid 6-digit hex color." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Validate hex characters */
    for (int i = 0; i < 6; i++) {
        if (!isxdigit(hex[i])) {
            struct discord_create_message params = { .content = "Please provide a valid hex color." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }
    }

    /* Parse RGB values */
    unsigned int r, g, b;
    sscanf(hex, "%2x%2x%2x", &r, &g, &b);

    /* Calculate complementary color */
    unsigned int comp_r = 255 - r;
    unsigned int comp_g = 255 - g;
    unsigned int comp_b = 255 - b;

    /* Convert to HSL (simplified) */
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max_c = rf > gf ? (rf > bf ? rf : bf) : (gf > bf ? gf : bf);
    float min_c = rf < gf ? (rf < bf ? rf : bf) : (gf < bf ? gf : bf);
    float l = (max_c + min_c) / 2.0f;

    char response[1024];
    snprintf(response, sizeof(response),
        "**Color Info:** #%s\n\n"
        "**RGB:** %u, %u, %u\n"
        "**Decimal:** %u\n"
        "**Lightness:** %.0f%%\n"
        "**Complementary:** #%02X%02X%02X\n\n"
        "[Preview](https://singlecolorimage.com/get/%s/100x100)",
        hex, r, g, b,
        (r << 16) | (g << 8) | b,
        l * 100,
        comp_r, comp_g, comp_b,
        hex);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_lookup_commands(himiko_bot_t *bot) {
    /* Lookup commands are prefix-only to stay under Discord's 100 slash command limit */
    himiko_command_t cmds[] = {
        { "urban", "Look up a term on Urban Dictionary", "Lookup", NULL, cmd_urban_prefix, 0, 0 },
        { "wiki", "Search Wikipedia", "Lookup", NULL, cmd_wiki_prefix, 0, 0 },
        { "ip", "Look up IP address information", "Lookup", NULL, cmd_ip_prefix, 0, 0 },
        { "color", "Get information about a hex color", "Lookup", NULL, cmd_color_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
