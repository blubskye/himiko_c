/*
 * Himiko Discord Bot (C Edition) - Text Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/text.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Base64 encoding table */
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Reverse a string */
static void reverse_string(const char *input, char *output, size_t max_len) {
    size_t len = strlen(input);
    if (len >= max_len) len = max_len - 1;

    for (size_t i = 0; i < len; i++) {
        output[i] = input[len - 1 - i];
    }
    output[len] = '\0';
}

/* Mock text (SpOnGeBoB mOcK) */
static void mock_text(const char *input, char *output, size_t max_len) {
    size_t len = strlen(input);
    if (len >= max_len) len = max_len - 1;

    int upper = 0;
    for (size_t i = 0; i < len; i++) {
        if (isalpha(input[i])) {
            output[i] = upper ? toupper(input[i]) : tolower(input[i]);
            upper = !upper;
        } else {
            output[i] = input[i];
        }
    }
    output[len] = '\0';
}

/* OwO-ify text */
static void owo_text(const char *input, char *output, size_t max_len) {
    size_t out_i = 0;
    size_t len = strlen(input);

    for (size_t i = 0; i < len && out_i < max_len - 10; i++) {
        char c = input[i];

        /* Replace r and l with w */
        if (c == 'r' || c == 'l') {
            output[out_i++] = 'w';
        } else if (c == 'R' || c == 'L') {
            output[out_i++] = 'W';
        }
        /* Replace n followed by vowel with ny */
        else if ((c == 'n' || c == 'N') && i + 1 < len) {
            char next = tolower(input[i + 1]);
            if (next == 'a' || next == 'e' || next == 'i' || next == 'o' || next == 'u') {
                output[out_i++] = c;
                output[out_i++] = (c == 'N') ? 'Y' : 'y';
            } else {
                output[out_i++] = c;
            }
        }
        /* Replace ove with uv */
        else if (tolower(c) == 'o' && i + 2 < len &&
                 tolower(input[i + 1]) == 'v' && tolower(input[i + 2]) == 'e') {
            output[out_i++] = isupper(c) ? 'U' : 'u';
            output[out_i++] = isupper(input[i + 1]) ? 'V' : 'v';
            i++; /* Skip the 'v', 'e' will be skipped in next iteration */
        } else {
            output[out_i++] = c;
        }
    }
    output[out_i] = '\0';

    /* Add cute ending */
    if (out_i < max_len - 10) {
        const char *endings[] = { " owo", " uwu", " >w<", " ^w^", " :3" };
        size_t idx = strlen(input) % 5;
        strcat(output, endings[idx]);
    }
}

/* Base64 encode */
static void base64_encode(const char *input, char *output, size_t max_len) {
    size_t in_len = strlen(input);
    size_t out_i = 0;

    for (size_t i = 0; i < in_len && out_i < max_len - 4; i += 3) {
        unsigned int val = (unsigned char)input[i] << 16;
        if (i + 1 < in_len) val |= (unsigned char)input[i + 1] << 8;
        if (i + 2 < in_len) val |= (unsigned char)input[i + 2];

        output[out_i++] = base64_table[(val >> 18) & 0x3F];
        output[out_i++] = base64_table[(val >> 12) & 0x3F];
        output[out_i++] = (i + 1 < in_len) ? base64_table[(val >> 6) & 0x3F] : '=';
        output[out_i++] = (i + 2 < in_len) ? base64_table[val & 0x3F] : '=';
    }
    output[out_i] = '\0';
}

/* Base64 decode */
static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static void base64_decode(const char *input, char *output, size_t max_len) {
    size_t in_len = strlen(input);
    size_t out_i = 0;

    for (size_t i = 0; i < in_len && out_i < max_len - 3; i += 4) {
        int a = base64_decode_char(input[i]);
        int b = (i + 1 < in_len) ? base64_decode_char(input[i + 1]) : 0;
        int c = (i + 2 < in_len && input[i + 2] != '=') ? base64_decode_char(input[i + 2]) : 0;
        int d = (i + 3 < in_len && input[i + 3] != '=') ? base64_decode_char(input[i + 3]) : 0;

        if (a < 0 || b < 0) break;

        unsigned int val = (a << 18) | (b << 12) | (c << 6) | d;

        output[out_i++] = (val >> 16) & 0xFF;
        if (i + 2 < in_len && input[i + 2] != '=') {
            output[out_i++] = (val >> 8) & 0xFF;
        }
        if (i + 3 < in_len && input[i + 3] != '=') {
            output[out_i++] = val & 0xFF;
        }
    }
    output[out_i] = '\0';
}

/* Hex encode */
static void hex_encode(const char *input, char *output, size_t max_len) {
    size_t len = strlen(input);
    size_t out_i = 0;

    for (size_t i = 0; i < len && out_i < max_len - 3; i++) {
        snprintf(output + out_i, 3, "%02x", (unsigned char)input[i]);
        out_i += 2;
    }
}

/* Hex decode */
static void hex_decode(const char *input, char *output, size_t max_len) {
    size_t len = strlen(input);
    size_t out_i = 0;

    for (size_t i = 0; i < len - 1 && out_i < max_len - 1; i += 2) {
        char hex[3] = { input[i], input[i + 1], '\0' };
        output[out_i++] = (char)strtol(hex, NULL, 16);
    }
    output[out_i] = '\0';
}

void cmd_reverse(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text to reverse.");
        return;
    }

    char reversed[2000];
    reverse_string(text, reversed, sizeof(reversed));

    char response[2100];
    snprintf(response, sizeof(response), "**Reversed:** %s", reversed);
    respond_message(client, interaction, response);
}

void cmd_reverse_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: reverse <text>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char reversed[2000];
    reverse_string(args, reversed, sizeof(reversed));

    char response[2100];
    snprintf(response, sizeof(response), "**Reversed:** %s", reversed);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_mock(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text to mock.");
        return;
    }

    char mocked[2000];
    mock_text(text, mocked, sizeof(mocked));

    respond_message(client, interaction, mocked);
}

void cmd_mock_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: mock <text>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char mocked[2000];
    mock_text(args, mocked, sizeof(mocked));

    struct discord_create_message params = { .content = mocked };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_owo(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text to owo-ify.");
        return;
    }

    char owoed[2000];
    owo_text(text, owoed, sizeof(owoed));

    respond_message(client, interaction, owoed);
}

void cmd_owo_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: owo <text>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char owoed[2000];
    owo_text(args, owoed, sizeof(owoed));

    struct discord_create_message params = { .content = owoed };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_upper(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text.");
        return;
    }

    char upper[2000];
    size_t len = strlen(text);
    if (len >= sizeof(upper)) len = sizeof(upper) - 1;

    for (size_t i = 0; i < len; i++) {
        upper[i] = toupper(text[i]);
    }
    upper[len] = '\0';

    respond_message(client, interaction, upper);
}

void cmd_upper_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: upper <text>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char upper[2000];
    size_t len = strlen(args);
    if (len >= sizeof(upper)) len = sizeof(upper) - 1;

    for (size_t i = 0; i < len; i++) {
        upper[i] = toupper(args[i]);
    }
    upper[len] = '\0';

    struct discord_create_message params = { .content = upper };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_lower(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text.");
        return;
    }

    char lower[2000];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;

    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower(text[i]);
    }
    lower[len] = '\0';

    respond_message(client, interaction, lower);
}

void cmd_lower_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: lower <text>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char lower[2000];
    size_t len = strlen(args);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;

    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower(args[i]);
    }
    lower[len] = '\0';

    struct discord_create_message params = { .content = lower };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_encode(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *type = "base64";
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "type") == 0) {
                type = opts->array[i].value;
            } else if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text to encode.");
        return;
    }

    char encoded[4000];

    if (strcasecmp(type, "base64") == 0) {
        base64_encode(text, encoded, sizeof(encoded));
    } else if (strcasecmp(type, "hex") == 0) {
        hex_encode(text, encoded, sizeof(encoded));
    } else {
        respond_ephemeral(client, interaction, "Unknown encoding type. Use: base64, hex");
        return;
    }

    char response[4100];
    snprintf(response, sizeof(response), "**Encoded (%s):**\n```\n%s\n```", type, encoded);
    respond_message(client, interaction, response);
}

void cmd_encode_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: encode <type> <text>\nTypes: base64, hex" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char type[32];
    if (sscanf(args, "%31s", type) != 1) {
        struct discord_create_message params = { .content = "Invalid format." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    const char *text = args + strlen(type);
    while (*text == ' ') text++;

    if (!*text) {
        struct discord_create_message params = { .content = "Please provide text to encode." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char encoded[4000];

    if (strcasecmp(type, "base64") == 0) {
        base64_encode(text, encoded, sizeof(encoded));
    } else if (strcasecmp(type, "hex") == 0) {
        hex_encode(text, encoded, sizeof(encoded));
    } else {
        struct discord_create_message params = { .content = "Unknown encoding type. Use: base64, hex" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[4100];
    snprintf(response, sizeof(response), "**Encoded (%s):**\n```\n%s\n```", type, encoded);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_decode(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *type = "base64";
    const char *text = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "type") == 0) {
                type = opts->array[i].value;
            } else if (strcmp(opts->array[i].name, "text") == 0) {
                text = opts->array[i].value;
            }
        }
    }

    if (!text || !*text) {
        respond_ephemeral(client, interaction, "Please provide text to decode.");
        return;
    }

    char decoded[4000];

    if (strcasecmp(type, "base64") == 0) {
        base64_decode(text, decoded, sizeof(decoded));
    } else if (strcasecmp(type, "hex") == 0) {
        hex_decode(text, decoded, sizeof(decoded));
    } else {
        respond_ephemeral(client, interaction, "Unknown encoding type. Use: base64, hex");
        return;
    }

    char response[4100];
    snprintf(response, sizeof(response), "**Decoded (%s):**\n```\n%s\n```", type, decoded);
    respond_message(client, interaction, response);
}

void cmd_decode_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: decode <type> <text>\nTypes: base64, hex" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char type[32];
    if (sscanf(args, "%31s", type) != 1) {
        struct discord_create_message params = { .content = "Invalid format." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    const char *text = args + strlen(type);
    while (*text == ' ') text++;

    if (!*text) {
        struct discord_create_message params = { .content = "Please provide text to decode." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char decoded[4000];

    if (strcasecmp(type, "base64") == 0) {
        base64_decode(text, decoded, sizeof(decoded));
    } else if (strcasecmp(type, "hex") == 0) {
        hex_decode(text, decoded, sizeof(decoded));
    } else {
        struct discord_create_message params = { .content = "Unknown encoding type. Use: base64, hex" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char response[4100];
    snprintf(response, sizeof(response), "**Decoded (%s):**\n```\n%s\n```", type, decoded);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_text_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "reverse", "Reverse text", "Text", cmd_reverse, cmd_reverse_prefix, 0, 0 },
        { "mock", "SpOnGeBoB mOcK text", "Text", cmd_mock, cmd_mock_prefix, 0, 0 },
        { "owo", "OwO-ify text", "Text", cmd_owo, cmd_owo_prefix, 0, 0 },
        { "upper", "UPPERCASE text", "Text", cmd_upper, cmd_upper_prefix, 0, 0 },
        { "lower", "lowercase text", "Text", cmd_lower, cmd_lower_prefix, 0, 0 },
        { "encode", "Encode text (base64, hex)", "Text", cmd_encode, cmd_encode_prefix, 0, 0 },
        { "decode", "Decode text (base64, hex)", "Text", cmd_decode, cmd_decode_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
