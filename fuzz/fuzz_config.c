/*
 * Himiko Discord Bot (C Edition) - Config Parser Fuzzer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AFL++ fuzzing harness for JSON config parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

/* Wrapper to parse config from buffer instead of file */
static int config_parse_buffer(himiko_config_t *config, const char *buf, size_t len) {
    /* Write buffer to temp file and parse it */
    char tmpfile[] = "/tmp/fuzz_config_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) return -1;

    write(fd, buf, len);
    close(fd);

    int result = config_load(config, tmpfile);
    unlink(tmpfile);

    return result;
}

int main(int argc, char **argv) {
    himiko_config_t config;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
        if (len == 0) continue;

        config_init_defaults(&config);
        config_parse_buffer(&config, (const char *)buf, len);
    }
#else
    /* Non-AFL mode: read from stdin or file */
    char *buf = NULL;
    size_t len = 0;

    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) return 1;
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc(len + 1);
        fread(buf, 1, len, f);
        buf[len] = '\0';
        fclose(f);
    } else {
        /* Read from stdin */
        buf = malloc(65536);
        len = fread(buf, 1, 65535, stdin);
        buf[len] = '\0';
    }

    config_init_defaults(&config);
    int result = config_parse_buffer(&config, buf, len);
    free(buf);

    printf("Parse result: %d\n", result);
    if (result == 0) {
        printf("Token: %s\n", config.token[0] ? "(set)" : "(empty)");
        printf("Prefix: %s\n", config.prefix);
        printf("DB Path: %s\n", config.database_path);
    }
#endif

    return 0;
}
