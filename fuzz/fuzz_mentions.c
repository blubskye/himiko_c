/*
 * Himiko Discord Bot (C Edition) - Mention Parser Fuzzer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AFL++ fuzzing harness for Discord mention parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

/* Type definition for Discord snowflake IDs */
typedef uint64_t u64snowflake;

/* Copy of mention parsers from bot.c for standalone fuzzing */
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

int main(int argc, char **argv) {
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

        /* Test all three parsers */
        volatile u64snowflake result1 = parse_user_mention(input);
        volatile u64snowflake result2 = parse_channel_mention(input);
        volatile u64snowflake result3 = parse_role_mention(input);
        (void)result1;
        (void)result2;
        (void)result3;

        free(input);
    }
#else
    /* Non-AFL mode: read from stdin or file */
    char buf[4096];
    size_t len;

    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) return 1;
        len = fread(buf, 1, sizeof(buf) - 1, f);
        buf[len] = '\0';
        fclose(f);
    } else {
        len = fread(buf, 1, sizeof(buf) - 1, stdin);
        buf[len] = '\0';
    }

    /* Remove newline if present */
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';

    printf("Input: '%s'\n", buf);
    printf("User mention:    %lu\n", parse_user_mention(buf));
    printf("Channel mention: %lu\n", parse_channel_mention(buf));
    printf("Role mention:    %lu\n", parse_role_mention(buf));
#endif

    return 0;
}
