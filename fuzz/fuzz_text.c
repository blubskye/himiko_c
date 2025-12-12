/*
 * Himiko Discord Bot (C Edition) - Text Transformation Fuzzer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AFL++ fuzzing harness for text transformation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

#define OUTPUT_SIZE 8192

/* Base64 encoding table */
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Copy of text functions from text.c for standalone fuzzing */

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

int main(int argc, char **argv) {
    char *output = malloc(OUTPUT_SIZE);
    if (!output) return 1;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
        if (len == 0) continue;

        /* Null-terminate the input */
        char *input = malloc(len + 1);
        if (!input) continue;
        memcpy(input, buf, len);
        input[len] = '\0';

        /* Test all transformations */
        reverse_string(input, output, OUTPUT_SIZE);
        mock_text(input, output, OUTPUT_SIZE);
        owo_text(input, output, OUTPUT_SIZE);
        base64_encode(input, output, OUTPUT_SIZE);
        base64_decode(input, output, OUTPUT_SIZE);

        free(input);
    }
#else
    /* Non-AFL mode: read from stdin or file */
    char buf[4096];
    size_t len;

    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { free(output); return 1; }
        len = fread(buf, 1, sizeof(buf) - 1, f);
        buf[len] = '\0';
        fclose(f);
    } else {
        len = fread(buf, 1, sizeof(buf) - 1, stdin);
        buf[len] = '\0';
    }

    /* Remove newline if present */
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';

    printf("Input: '%s'\n\n", buf);

    reverse_string(buf, output, OUTPUT_SIZE);
    printf("Reverse: '%s'\n", output);

    mock_text(buf, output, OUTPUT_SIZE);
    printf("Mock: '%s'\n", output);

    owo_text(buf, output, OUTPUT_SIZE);
    printf("OwO: '%s'\n", output);

    base64_encode(buf, output, OUTPUT_SIZE);
    printf("Base64 encode: '%s'\n", output);

    base64_decode(buf, output, OUTPUT_SIZE);
    printf("Base64 decode: '%s'\n", output);
#endif

    free(output);
    return 0;
}
