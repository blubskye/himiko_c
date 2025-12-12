/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "bot.h"

static himiko_bot_t bot;

void signal_handler(int sig) {
    (void)sig;
    printf("\nShutting down...\n");
    bot_stop(&bot);
}

int main(int argc, char *argv[]) {
    const char *config_path = "config.json";

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[++i];
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Himiko v%s (C Edition)\n", HIMIKO_VERSION);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Himiko Discord Bot (C Edition) v%s\n", HIMIKO_VERSION);
            printf("\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("\n");
            printf("Options:\n");
            printf("  -c, --config <path>  Path to config.json (default: config.json)\n");
            printf("  -v, --version        Show version information\n");
            printf("  -h, --help           Show this help message\n");
            printf("\n");
            printf("For more information, visit: https://github.com/blubskye/himiko_c\n");
            return 0;
        }
    }

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize bot */
    if (bot_init(&bot, config_path) != 0) {
        fprintf(stderr, "Failed to initialize bot\n");
        return 1;
    }

    /* Run bot */
    int result = bot_run(&bot);

    /* Cleanup */
    bot_cleanup(&bot);

    return result;
}
