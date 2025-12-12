/*
 * Himiko Discord Bot (C Edition) - Debug Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_DEBUG_H
#define HIMIKO_DEBUG_H

#include "config.h"

/* Initialize debug module */
void debug_init(const himiko_config_t *config);

/* Check if debug mode is enabled */
int debug_is_enabled(void);

/* Log a debug message (only if debug mode enabled) */
void debug_log(const char *format, ...);

/* Log with caller info (only if debug mode enabled) */
void debug_log_with_caller(const char *file, int line, const char *func, const char *format, ...);

/* Log an error (always logged, stack trace if debug enabled) */
void debug_error(const char *format, ...);

/* Log an error with context */
void debug_error_context(const char *context, const char *format, ...);

/* Print stack trace */
void debug_print_stack_trace(void);

/* Get stack trace as string (caller must free) */
char *debug_get_stack_trace(void);

/* Print memory statistics */
void debug_print_mem_stats(void);

/* Get caller info as string */
const char *debug_get_caller_info(int skip);

/* Convenience macros */
#define DEBUG_LOG(...) debug_log_with_caller(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_ERROR(ctx, ...) debug_error_context(ctx, __VA_ARGS__)

#endif /* HIMIKO_DEBUG_H */
