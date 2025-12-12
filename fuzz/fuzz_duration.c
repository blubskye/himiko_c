/*
 * Himiko Discord Bot (C Edition) - Duration Parser Fuzzer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AFL++ fuzzing harness for duration string parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

/* Copy of parse_duration from utility.c for standalone fuzzing */
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

        time_t result = parse_duration(input);
        (void)result; /* Suppress unused warning */

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

    time_t result = parse_duration(buf);
    printf("Input: '%s'\n", buf);
    printf("Result: %ld seconds\n", (long)result);
    printf("  = %ld days, %ld hours, %ld minutes, %ld seconds\n",
           result / 86400,
           (result % 86400) / 3600,
           (result % 3600) / 60,
           result % 60);
#endif

    return 0;
}
