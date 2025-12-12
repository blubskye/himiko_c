/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

void config_init_defaults(himiko_config_t *config) {
    memset(config, 0, sizeof(himiko_config_t));
    strcpy(config->prefix, "-");
    strcpy(config->database_path, "himiko.db");
    strcpy(config->apis.openai_base_url, "https://api.openai.com/v1");
    strcpy(config->apis.openai_model, "gpt-3.5-turbo");
    config->features.command_history = 1;
    config->features.auto_update = 1;
    config->features.update_check_hours = 24;
}

int config_load(himiko_config_t *config, const char *path) {
    FILE *file;
    char *buffer;
    long length;
    struct json_object *root;
    struct json_object *value;
    struct json_object *apis_obj;
    struct json_object *features_obj;

    file = fopen(path, "r");
    if (!file) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = malloc(length + 1);
    if (!buffer) {
        fclose(file);
        return -1;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    root = json_tokener_parse(buffer);
    free(buffer);

    if (!root) {
        return -1;
    }

    /* Parse token */
    if (json_object_object_get_ex(root, "token", &value)) {
        strncpy(config->token, json_object_get_string(value), MAX_TOKEN_LEN - 1);
    }

    /* Parse prefix */
    if (json_object_object_get_ex(root, "prefix", &value)) {
        strncpy(config->prefix, json_object_get_string(value), MAX_PREFIX_LEN - 1);
    }

    /* Parse database_path */
    if (json_object_object_get_ex(root, "database_path", &value)) {
        strncpy(config->database_path, json_object_get_string(value), MAX_PATH_LEN - 1);
    }

    /* Parse owner_id (single owner - backwards compatible) */
    if (json_object_object_get_ex(root, "owner_id", &value)) {
        strncpy(config->owner_id, json_object_get_string(value), MAX_SNOWFLAKE_LEN - 1);
    }

    /* Parse owner_ids array (multiple owners) */
    if (json_object_object_get_ex(root, "owner_ids", &value)) {
        if (json_object_is_type(value, json_type_array)) {
            int len = json_object_array_length(value);
            if (len > MAX_OWNER_IDS) len = MAX_OWNER_IDS;
            config->owner_ids_count = len;
            for (int i = 0; i < len; i++) {
                struct json_object *item = json_object_array_get_idx(value, i);
                strncpy(config->owner_ids[i], json_object_get_string(item), MAX_SNOWFLAKE_LEN - 1);
            }
        }
    }

    /* Parse apis object */
    if (json_object_object_get_ex(root, "apis", &apis_obj)) {
        if (json_object_object_get_ex(apis_obj, "weather_api_key", &value)) {
            strncpy(config->apis.weather_api_key, json_object_get_string(value), sizeof(config->apis.weather_api_key) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "google_api_key", &value)) {
            strncpy(config->apis.google_api_key, json_object_get_string(value), sizeof(config->apis.google_api_key) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "spotify_client_id", &value)) {
            strncpy(config->apis.spotify_client_id, json_object_get_string(value), sizeof(config->apis.spotify_client_id) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "spotify_client_secret", &value)) {
            strncpy(config->apis.spotify_client_secret, json_object_get_string(value), sizeof(config->apis.spotify_client_secret) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "openai_api_key", &value)) {
            strncpy(config->apis.openai_api_key, json_object_get_string(value), sizeof(config->apis.openai_api_key) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "openai_base_url", &value)) {
            strncpy(config->apis.openai_base_url, json_object_get_string(value), sizeof(config->apis.openai_base_url) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "openai_model", &value)) {
            strncpy(config->apis.openai_model, json_object_get_string(value), sizeof(config->apis.openai_model) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "youtube_api_key", &value)) {
            strncpy(config->apis.youtube_api_key, json_object_get_string(value), sizeof(config->apis.youtube_api_key) - 1);
        }
        if (json_object_object_get_ex(apis_obj, "soundcloud_auth_token", &value)) {
            strncpy(config->apis.soundcloud_auth_token, json_object_get_string(value), sizeof(config->apis.soundcloud_auth_token) - 1);
        }
    }

    /* Parse features object */
    if (json_object_object_get_ex(root, "features", &features_obj)) {
        if (json_object_object_get_ex(features_obj, "dm_logging", &value)) {
            config->features.dm_logging = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(features_obj, "command_history", &value)) {
            config->features.command_history = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(features_obj, "delete_timer", &value)) {
            config->features.delete_timer = json_object_get_int(value);
        }
        if (json_object_object_get_ex(features_obj, "webhook_notify", &value)) {
            config->features.webhook_notify = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(features_obj, "webhook_url", &value)) {
            strncpy(config->features.webhook_url, json_object_get_string(value), MAX_URL_LEN - 1);
        }
        if (json_object_object_get_ex(features_obj, "auto_update", &value)) {
            config->features.auto_update = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(features_obj, "auto_update_apply", &value)) {
            config->features.auto_update_apply = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(features_obj, "update_check_hours", &value)) {
            config->features.update_check_hours = json_object_get_int(value);
        }
        if (json_object_object_get_ex(features_obj, "update_notify_channel", &value)) {
            strncpy(config->features.update_notify_channel, json_object_get_string(value), MAX_SNOWFLAKE_LEN - 1);
        }
        if (json_object_object_get_ex(features_obj, "debug_mode", &value)) {
            config->features.debug_mode = json_object_get_boolean(value);
        }
    }

    json_object_put(root);
    return 0;
}

int config_load_from_env(himiko_config_t *config) {
    const char *token = getenv("DISCORD_TOKEN");
    if (token) {
        strncpy(config->token, token, MAX_TOKEN_LEN - 1);
    }

    const char *prefix = getenv("PREFIX");
    if (prefix) {
        strncpy(config->prefix, prefix, MAX_PREFIX_LEN - 1);
    }

    const char *db_path = getenv("DATABASE_PATH");
    if (db_path) {
        strncpy(config->database_path, db_path, MAX_PATH_LEN - 1);
    }

    const char *owner = getenv("OWNER_ID");
    if (owner) {
        strncpy(config->owner_id, owner, MAX_SNOWFLAKE_LEN - 1);
    }

    const char *openai_key = getenv("OPENAI_API_KEY");
    if (openai_key) {
        strncpy(config->apis.openai_api_key, openai_key, sizeof(config->apis.openai_api_key) - 1);
    }

    return (strlen(config->token) > 0) ? 0 : -1;
}

int config_is_owner(const himiko_config_t *config, const char *user_id) {
    /* Check single owner_id first (backwards compatible) */
    if (config->owner_id[0] != '\0' && strcmp(config->owner_id, user_id) == 0) {
        return 1;
    }

    /* Check owner_ids array */
    for (int i = 0; i < config->owner_ids_count; i++) {
        if (strcmp(config->owner_ids[i], user_id) == 0) {
            return 1;
        }
    }

    return 0;
}
