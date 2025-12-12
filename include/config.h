/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef HIMIKO_CONFIG_H
#define HIMIKO_CONFIG_H

#include <stdint.h>

#define MAX_TOKEN_LEN 256
#define MAX_PREFIX_LEN 16
#define MAX_PATH_LEN 256
#define MAX_URL_LEN 512
#define MAX_MODEL_LEN 64
#define MAX_OWNER_IDS 10
#define MAX_SNOWFLAKE_LEN 32

/*
 * Config structure matching Himiko Go's config.json EXACTLY
 * for full compatibility between Go and C versions.
 */
typedef struct {
    char token[MAX_TOKEN_LEN];
    char prefix[MAX_PREFIX_LEN];
    char database_path[MAX_PATH_LEN];
    uint64_t app_id; /* Application ID for interaction responses */

    /* Single owner (backwards compatible with old configs) */
    char owner_id[MAX_SNOWFLAKE_LEN];

    /* Multiple owners support */
    char owner_ids[MAX_OWNER_IDS][MAX_SNOWFLAKE_LEN];
    int owner_ids_count;

    /* API keys - matches Go's APIs struct */
    struct {
        char weather_api_key[128];
        char google_api_key[128];
        char spotify_client_id[128];
        char spotify_client_secret[128];
        char openai_api_key[256];
        char openai_base_url[MAX_URL_LEN];
        char openai_model[MAX_MODEL_LEN];
        char youtube_api_key[128];
        char soundcloud_auth_token[256];
    } apis;

    /* Feature flags - matches Go's Features struct */
    struct {
        int dm_logging;
        int command_history;
        int delete_timer;
        int webhook_notify;
        char webhook_url[MAX_URL_LEN];
        int auto_update;
        int auto_update_apply;
        int update_check_hours;
        char update_notify_channel[MAX_SNOWFLAKE_LEN];
        int debug_mode;
    } features;

} himiko_config_t;

/* Initialize config with default values */
void config_init_defaults(himiko_config_t *config);

/* Load config from JSON file - returns 0 on success, -1 on error */
int config_load(himiko_config_t *config, const char *path);

/* Load config from environment variables (fallback) */
int config_load_from_env(himiko_config_t *config);

/* Check if a user ID is an owner (checks both owner_id and owner_ids) */
int config_is_owner(const himiko_config_t *config, const char *user_id);

#endif /* HIMIKO_CONFIG_H */
