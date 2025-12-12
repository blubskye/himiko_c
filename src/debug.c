/*
 * Himiko Discord Bot (C Edition) - Debug Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <execinfo.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#define MAX_STACK_FRAMES 64
#define CALLER_BUF_SIZE 256

static int debug_enabled = 0;
static char caller_buf[CALLER_BUF_SIZE];

static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

void debug_init(const himiko_config_t *config) {
    debug_enabled = config->features.debug_mode;
    if (debug_enabled) {
        fprintf(stderr, "[DEBUG] Debug mode enabled\n");
    }
}

int debug_is_enabled(void) {
    return debug_enabled;
}

void debug_log(const char *format, ...) {
    if (!debug_enabled) return;

    char time_buf[32];
    get_timestamp(time_buf, sizeof(time_buf));

    fprintf(stderr, "[DEBUG %s] ", time_buf);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void debug_log_with_caller(const char *file, int line, const char *func, const char *format, ...) {
    if (!debug_enabled) return;

    char time_buf[32];
    get_timestamp(time_buf, sizeof(time_buf));

    /* Extract just the filename */
    const char *filename = file;
    const char *last_slash = strrchr(file, '/');
    if (last_slash) filename = last_slash + 1;

    fprintf(stderr, "[DEBUG %s] [%s:%d %s] ", time_buf, filename, line, func);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void debug_error(const char *format, ...) {
    char time_buf[32];
    get_timestamp(time_buf, sizeof(time_buf));

    fprintf(stderr, "[ERROR %s] ", time_buf);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");

    if (debug_enabled) {
        debug_print_stack_trace();
    }
}

void debug_error_context(const char *context, const char *format, ...) {
    char time_buf[32];
    get_timestamp(time_buf, sizeof(time_buf));

    fprintf(stderr, "[ERROR %s] %s: ", time_buf, context);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");

    if (debug_enabled) {
        debug_print_stack_trace();
    }
}

void debug_print_stack_trace(void) {
#ifdef __linux__
    void *buffer[MAX_STACK_FRAMES];
    int nptrs = backtrace(buffer, MAX_STACK_FRAMES);
    char **strings = backtrace_symbols(buffer, nptrs);

    if (strings == NULL) {
        fprintf(stderr, "  (stack trace unavailable)\n");
        return;
    }

    fprintf(stderr, "[STACK TRACE]\n");
    for (int i = 1; i < nptrs; i++) { /* Skip frame 0 (this function) */
        fprintf(stderr, "  %d: %s\n", i, strings[i]);
    }

    free(strings);
#else
    fprintf(stderr, "  (stack trace not available on this platform)\n");
#endif
}

char *debug_get_stack_trace(void) {
#ifdef __linux__
    void *buffer[MAX_STACK_FRAMES];
    int nptrs = backtrace(buffer, MAX_STACK_FRAMES);
    char **strings = backtrace_symbols(buffer, nptrs);

    if (strings == NULL) {
        return strdup("(stack trace unavailable)");
    }

    /* Calculate total size needed */
    size_t total_len = 0;
    for (int i = 1; i < nptrs; i++) {
        total_len += strlen(strings[i]) + 16; /* Extra for formatting */
    }

    char *result = malloc(total_len + 1);
    if (!result) {
        free(strings);
        return strdup("(out of memory)");
    }

    result[0] = '\0';
    for (int i = 1; i < nptrs; i++) {
        char line[512];
        snprintf(line, sizeof(line), "  %d: %s\n", i, strings[i]);
        strcat(result, line);
    }

    free(strings);
    return result;
#else
    return strdup("(stack trace not available on this platform)");
#endif
}

void debug_print_mem_stats(void) {
    if (!debug_enabled) return;

#ifdef __linux__
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        fprintf(stderr, "[DEBUG] Memory Stats:\n");
        fprintf(stderr, "  Max RSS: %ld KB\n", usage.ru_maxrss);
        fprintf(stderr, "  Shared Memory: %ld KB\n", usage.ru_ixrss);
        fprintf(stderr, "  Unshared Data: %ld KB\n", usage.ru_idrss);
        fprintf(stderr, "  Unshared Stack: %ld KB\n", usage.ru_isrss);
        fprintf(stderr, "  Page Faults (minor): %ld\n", usage.ru_minflt);
        fprintf(stderr, "  Page Faults (major): %ld\n", usage.ru_majflt);
        fprintf(stderr, "  Context Switches (voluntary): %ld\n", usage.ru_nvcsw);
        fprintf(stderr, "  Context Switches (involuntary): %ld\n", usage.ru_nivcsw);
    }

    /* Also try to read /proc/self/status for more details */
    FILE *f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        fprintf(stderr, "  Process Memory:\n");
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmSize:", 7) == 0 ||
                strncmp(line, "VmRSS:", 6) == 0 ||
                strncmp(line, "VmData:", 7) == 0 ||
                strncmp(line, "VmStk:", 6) == 0 ||
                strncmp(line, "Threads:", 8) == 0) {
                fprintf(stderr, "    %s", line);
            }
        }
        fclose(f);
    }
#else
    fprintf(stderr, "[DEBUG] Memory stats not available on this platform\n");
#endif
}

const char *debug_get_caller_info(int skip) {
#ifdef __linux__
    void *buffer[MAX_STACK_FRAMES];
    int nptrs = backtrace(buffer, MAX_STACK_FRAMES);
    char **strings = backtrace_symbols(buffer, nptrs);

    if (strings == NULL || skip + 1 >= nptrs) {
        snprintf(caller_buf, sizeof(caller_buf), "unknown");
        if (strings) free(strings);
        return caller_buf;
    }

    /* Parse the symbol string to extract function info */
    /* Format is usually: ./binary(function+offset) [address] */
    const char *symbol = strings[skip + 1];
    const char *paren_open = strchr(symbol, '(');
    const char *paren_close = strchr(symbol, ')');

    if (paren_open && paren_close && paren_close > paren_open) {
        size_t len = paren_close - paren_open - 1;
        if (len >= sizeof(caller_buf)) len = sizeof(caller_buf) - 1;
        strncpy(caller_buf, paren_open + 1, len);
        caller_buf[len] = '\0';

        /* Remove the +offset part if present */
        char *plus = strchr(caller_buf, '+');
        if (plus) *plus = '\0';
    } else {
        snprintf(caller_buf, sizeof(caller_buf), "%s", symbol);
    }

    free(strings);
    return caller_buf;
#else
    (void)skip;
    snprintf(caller_buf, sizeof(caller_buf), "unknown");
    return caller_buf;
#endif
}
