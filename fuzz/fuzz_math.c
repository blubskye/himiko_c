/*
 * Himiko Discord Bot (C Edition) - Math Expression Fuzzer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AFL++ fuzzing harness for math expression evaluation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

/* Copy of evaluate_math from utility.c for standalone fuzzing */
static int evaluate_math(const char *expr, double *result) {
    double a, b;

    /* Try to parse as single number */
    if (sscanf(expr, "%lf", &a) == 1 && strchr(expr, '+') == NULL &&
        strchr(expr, '-') == NULL && strchr(expr, '*') == NULL &&
        strchr(expr, '/') == NULL && strchr(expr, '^') == NULL &&
        strchr(expr, '%') == NULL) {
        *result = a;
        return 0;
    }

    /* Try basic operations */
    if (sscanf(expr, "%lf+%lf", &a, &b) == 2) {
        *result = a + b;
        return 0;
    }
    if (sscanf(expr, "%lf-%lf", &a, &b) == 2) {
        *result = a - b;
        return 0;
    }
    if (sscanf(expr, "%lf*%lf", &a, &b) == 2) {
        *result = a * b;
        return 0;
    }
    if (sscanf(expr, "%lf/%lf", &a, &b) == 2) {
        if (b == 0) return -1; /* Division by zero */
        *result = a / b;
        return 0;
    }
    if (sscanf(expr, "%lf^%lf", &a, &b) == 2) {
        *result = pow(a, b);
        return 0;
    }
    if (sscanf(expr, "%lf%%%lf", &a, &b) == 2) {
        *result = fmod(a, b);
        return 0;
    }

    return -1;
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

        double result;
        evaluate_math(input, &result);

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

    double result;
    int ret = evaluate_math(buf, &result);
    printf("Input: '%s'\n", buf);
    if (ret == 0) {
        printf("Result: %g\n", result);
    } else {
        printf("Parse error\n");
    }
#endif

    return 0;
}
