/*
 * Himiko Discord Bot (C Edition) - Updater Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "updater.h"
#include "bot.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __linux__
#include <linux/limits.h>
#define PATH_MAX_LEN PATH_MAX
#else
#define PATH_MAX_LEN 4096
#endif

/* Curl write callback */
struct curl_mem_data {
    char *data;
    size_t size;
};

static size_t write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_mem_data *mem = (struct curl_mem_data *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/* Download progress state */
typedef struct {
    download_progress_fn callback;
    void *user_data;
    long total_size;
} progress_state_t;

static int xfer_progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    progress_state_t *state = (progress_state_t *)clientp;
    if (state && state->callback && dltotal > 0) {
        state->callback((long)dlnow, (long)dltotal, state->user_data);
    }
    return 0;
}

/* File write callback for downloads */
static size_t file_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    return fwrite(contents, size, nmemb, (FILE *)userp);
}

/* Get asset name for current platform */
static const char *get_platform_suffix(void) {
#if defined(__linux__)
    #if defined(__x86_64__)
        return "linux-amd64";
    #elif defined(__aarch64__)
        return "linux-arm64";
    #else
        return "linux-unknown";
    #endif
#elif defined(_WIN32)
    return "windows-amd64";
#elif defined(__APPLE__)
    return "darwin-amd64";
#else
    return "unknown";
#endif
}

const char *updater_get_version(void) {
    return HIMIKO_VERSION;
}

void updater_parse_version(const char *v, int parts[3]) {
    parts[0] = parts[1] = parts[2] = 0;
    const char *p = v;
    if (*p == 'v') p++;
    sscanf(p, "%d.%d.%d", &parts[0], &parts[1], &parts[2]);
}

int updater_is_newer_version(const char *current, const char *newer) {
    int cur[3], new[3];
    updater_parse_version(current, cur);
    updater_parse_version(newer, new);

    for (int i = 0; i < 3; i++) {
        if (new[i] > cur[i]) return 1;
        if (new[i] < cur[i]) return 0;
    }
    return 0;
}

int updater_check(const char *current_version, update_info_t *info) {
    if (!info) return -1;
    memset(info, 0, sizeof(update_info_t));
    strncpy(info->current_version, current_version, sizeof(info->current_version) - 1);

    CURL *curl = curl_easy_init();
    if (!curl) {
        debug_error("Failed to init curl");
        return -1;
    }

    struct curl_mem_data chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: Himiko-Bot/" HIMIKO_VERSION);

    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        debug_error("curl failed: %s", curl_easy_strerror(res));
        free(chunk.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        debug_error("GitHub API returned %ld", http_code);
        free(chunk.data);
        return -1;
    }

    /* Parse JSON response */
    struct json_object *root = json_tokener_parse(chunk.data);
    free(chunk.data);

    if (!root) {
        debug_error("Failed to parse GitHub response");
        return -1;
    }

    struct json_object *tag_name, *body, *assets;
    if (!json_object_object_get_ex(root, "tag_name", &tag_name) ||
        !json_object_object_get_ex(root, "assets", &assets)) {
        debug_error("Missing fields in GitHub response");
        json_object_put(root);
        return -1;
    }

    const char *new_version = json_object_get_string(tag_name);
    if (new_version[0] == 'v') new_version++;
    strncpy(info->new_version, new_version, sizeof(info->new_version) - 1);

    if (json_object_object_get_ex(root, "body", &body)) {
        const char *notes = json_object_get_string(body);
        if (notes) {
            strncpy(info->release_notes, notes, sizeof(info->release_notes) - 1);
        }
    }

    /* Check if newer */
    if (!updater_is_newer_version(current_version, info->new_version)) {
        info->has_update = 0;
        json_object_put(root);
        return 0;
    }

    /* Find matching asset for our platform */
    const char *platform = get_platform_suffix();
    int asset_count = json_object_array_length(assets);

    for (int i = 0; i < asset_count; i++) {
        struct json_object *asset = json_object_array_get_idx(assets, i);
        struct json_object *name_obj, *url_obj, *size_obj;

        if (!json_object_object_get_ex(asset, "name", &name_obj)) continue;
        const char *name = json_object_get_string(name_obj);

        /* Check if asset matches our platform */
        if (strstr(name, platform) && strstr(name, ".zip")) {
            if (json_object_object_get_ex(asset, "browser_download_url", &url_obj)) {
                strncpy(info->download_url, json_object_get_string(url_obj), sizeof(info->download_url) - 1);
            }
            strncpy(info->asset_name, name, sizeof(info->asset_name) - 1);
            if (json_object_object_get_ex(asset, "size", &size_obj)) {
                info->size = json_object_get_int64(size_obj);
            }
            info->has_update = 1;
            break;
        }
    }

    json_object_put(root);

    if (!info->has_update && info->download_url[0] == '\0') {
        /* Version is newer but no matching asset found */
        debug_log("Update v%s available but no asset for %s", info->new_version, platform);
    }

    return 0;
}

char *updater_download(const update_info_t *info, download_progress_fn progress_fn, void *user_data) {
    if (!info || !info->has_update || !info->download_url[0]) {
        return NULL;
    }

    /* Create temp file */
    char temp_path[PATH_MAX_LEN];
    snprintf(temp_path, sizeof(temp_path), "/tmp/himiko-update-XXXXXX.zip");
    int fd = mkstemps(temp_path, 4);
    if (fd < 0) {
        debug_error("Failed to create temp file");
        return NULL;
    }

    FILE *fp = fdopen(fd, "wb");
    if (!fp) {
        close(fd);
        unlink(temp_path);
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        unlink(temp_path);
        return NULL;
    }

    progress_state_t progress = {
        .callback = progress_fn,
        .user_data = user_data,
        .total_size = info->size
    };

    curl_easy_setopt(curl, CURLOPT_URL, info->download_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: Himiko-Bot/" HIMIKO_VERSION);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        debug_error("Download failed: %s", curl_easy_strerror(res));
        unlink(temp_path);
        return NULL;
    }

    return strdup(temp_path);
}

int updater_apply(const char *zip_path) {
    if (!zip_path) return -1;

    /* Get current executable path */
    char exec_path[PATH_MAX_LEN];
    ssize_t len = readlink("/proc/self/exe", exec_path, sizeof(exec_path) - 1);
    if (len <= 0) {
        debug_error("Failed to get executable path");
        return -1;
    }
    exec_path[len] = '\0';

    /* Extract binary name from zip based on platform */
    const char *platform = get_platform_suffix();
    char binary_name[128];
    snprintf(binary_name, sizeof(binary_name), "himiko-%s", platform);

    /* Use unzip command to extract */
    char cmd[PATH_MAX_LEN * 2];
    char temp_dir[PATH_MAX_LEN];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/himiko-update-%d", getpid());
    mkdir(temp_dir, 0755);

    snprintf(cmd, sizeof(cmd), "unzip -o -q '%s' -d '%s' 2>/dev/null", zip_path, temp_dir);
    int ret = system(cmd);
    if (ret != 0) {
        debug_error("Failed to extract update archive");
        return -1;
    }

    /* Find the binary */
    char new_binary[PATH_MAX_LEN];
    snprintf(new_binary, sizeof(new_binary), "%s/%s", temp_dir, binary_name);

    if (access(new_binary, F_OK) != 0) {
        /* Try without platform suffix */
        snprintf(new_binary, sizeof(new_binary), "%s/himiko", temp_dir);
        if (access(new_binary, F_OK) != 0) {
            debug_error("Binary not found in update archive");
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
            system(cmd);
            return -1;
        }
    }

    /* Backup old binary */
    char backup_path[PATH_MAX_LEN];
    snprintf(backup_path, sizeof(backup_path), "%s.old", exec_path);
    if (rename(exec_path, backup_path) != 0) {
        debug_error("Failed to backup old binary");
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
        system(cmd);
        return -1;
    }

    /* Copy new binary */
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s' && chmod +x '%s'", new_binary, exec_path, exec_path);
    ret = system(cmd);
    if (ret != 0) {
        debug_error("Failed to install new binary");
        /* Restore backup */
        rename(backup_path, exec_path);
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
        system(cmd);
        return -1;
    }

    /* Clean up */
    unlink(backup_path);
    unlink(zip_path);
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
    system(cmd);

    debug_log("Update applied successfully");
    return 0;
}

void updater_notify_channel(struct discord *client, const char *channel_id, const update_info_t *info, int applied) {
    if (!client || !channel_id || !channel_id[0] || !info) return;

    struct discord_embed embed = {0};

    if (applied) {
        embed.title = "Himiko Update Applied!";
        embed.color = 0x57F287;
    } else {
        embed.title = "Himiko Update Available!";
        embed.color = 0x5865F2;
    }

    char desc[1024];
    if (applied) {
        snprintf(desc, sizeof(desc),
            "Himiko has been updated from **v%s** to **v%s**.\n\n"
            "The bot will use the new version after restart.",
            info->current_version, info->new_version);
    } else {
        snprintf(desc, sizeof(desc),
            "A new version of Himiko is available!\n\n"
            "**Current:** v%s\n**New:** v%s\n\n"
            "Bot owner can use `/update apply` to install.",
            info->current_version, info->new_version);
    }
    embed.description = desc;

    /* Add release notes if available */
    struct discord_embed_field fields[1] = {0};
    struct discord_embed_fields fields_list = {0};
    if (info->release_notes[0]) {
        static char notes[512];
        strncpy(notes, info->release_notes, 500);
        notes[500] = '\0';
        if (strlen(info->release_notes) > 500) {
            strcat(notes, "...");
        }
        fields[0].name = "What's New";
        fields[0].value = notes;
        fields_list.size = 1;
        fields_list.array = fields;
        embed.fields = &fields_list;
    }

    struct discord_create_message params = {
        .embeds = &(struct discord_embeds){
            .size = 1,
            .array = &embed
        }
    };

    u64snowflake cid = strtoull(channel_id, NULL, 10);
    discord_create_message(client, cid, &params, NULL);
}

void updater_notify_owners(struct discord *client, struct himiko_bot *bot, const update_info_t *info, int applied) {
    if (!client || !bot || !info) return;

    /* Collect owner IDs */
    const char *owner_ids[11] = {0};
    int count = 0;

    if (bot->config.owner_id[0]) {
        owner_ids[count++] = bot->config.owner_id;
    }
    for (int i = 0; i < bot->config.owner_ids_count && count < 10; i++) {
        /* Check for duplicates */
        int dup = 0;
        for (int j = 0; j < count; j++) {
            if (strcmp(owner_ids[j], bot->config.owner_ids[i]) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup) {
            owner_ids[count++] = bot->config.owner_ids[i];
        }
    }

    /* Create embed */
    struct discord_embed embed = {0};
    char desc[512];

    if (applied) {
        embed.title = "Himiko Auto-Updated!";
        embed.color = 0x57F287;
        snprintf(desc, sizeof(desc),
            "Updated from v%s to v%s\n\n**Please restart the bot to complete the update.**",
            info->current_version, info->new_version);
    } else {
        embed.title = "Himiko Update Available!";
        embed.color = 0x5865F2;
        snprintf(desc, sizeof(desc),
            "A new version is available: **v%s** (current: v%s)\n\nUse `/update apply` to download and install.",
            info->new_version, info->current_version);
    }
    embed.description = desc;

    /* Send to each owner */
    for (int i = 0; i < count; i++) {
        u64snowflake uid = strtoull(owner_ids[i], NULL, 10);
        struct discord_channel dm_channel;

        struct discord_create_dm params = { .recipient_id = uid };
        struct discord_ret_channel ret = { .sync = &dm_channel };

        if (discord_create_dm(client, &params, &ret) == CCORD_OK) {
            struct discord_create_message msg_params = {
                .embeds = &(struct discord_embeds){
                    .size = 1,
                    .array = &embed
                }
            };
            discord_create_message(client, dm_channel.id, &msg_params, NULL);
        }
    }
}

/* Format bytes helper */
static void format_bytes(long bytes, char *buf, size_t size) {
    const char *units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double val = bytes;
    while (val >= 1024 && unit < 3) {
        val /= 1024;
        unit++;
    }
    snprintf(buf, size, "%.1f %s", val, units[unit]);
}

/* /update command handler */
void cmd_update(struct discord *client, const struct discord_interaction *interaction) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Owner only */
    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)interaction->member->user->id);
    if (!config_is_owner(&bot->config, user_id_str)) {
        respond_ephemeral(client, interaction, "This command is only available to bot owners.");
        return;
    }

    /* Get subcommand */
    if (!interaction->data->options || interaction->data->options->size == 0) {
        respond_ephemeral(client, interaction, "Please specify a subcommand: check, apply, or version");
        return;
    }

    const char *subcommand = interaction->data->options->array[0].name;

    if (strcmp(subcommand, "version") == 0) {
        char response[256];
        snprintf(response, sizeof(response),
            "**Himiko Version Info**\n"
            "Current Version: v%s\n"
            "Auto-Update Check: %s\n"
            "Auto-Apply Updates: %s",
            HIMIKO_VERSION,
            bot->config.features.auto_update ? "Enabled" : "Disabled",
            bot->config.features.auto_update_apply ? "Enabled" : "Disabled");
        respond_message(client, interaction, response);
        return;
    }

    if (strcmp(subcommand, "check") == 0) {
        /* Defer response since this takes time */
        struct discord_interaction_response defer = {
            .type = DISCORD_INTERACTION_DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE
        };
        discord_create_interaction_response(client, interaction->id, interaction->token, &defer, NULL);

        update_info_t info;
        if (updater_check(HIMIKO_VERSION, &info) != 0) {
            struct discord_edit_original_interaction_response params = {
                .content = "Failed to check for updates."
            };
            discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &params, NULL);
            return;
        }

        if (!info.has_update) {
            char msg[128];
            snprintf(msg, sizeof(msg), "You are running the latest version (**v%s**).", info.current_version);
            struct discord_edit_original_interaction_response params = { .content = msg };
            discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &params, NULL);
            return;
        }

        char size_str[32];
        format_bytes(info.size, size_str, sizeof(size_str));

        char msg[1024];
        snprintf(msg, sizeof(msg),
            "**Update Available!**\n\n"
            "A new version is available: **v%s** (current: v%s)\n"
            "Download Size: %s\n\n"
            "Use `/update apply` to download and install.",
            info.new_version, info.current_version, size_str);

        struct discord_edit_original_interaction_response params = { .content = msg };
        discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &params, NULL);
        return;
    }

    if (strcmp(subcommand, "apply") == 0) {
        /* Defer response */
        struct discord_interaction_response defer = {
            .type = DISCORD_INTERACTION_DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE
        };
        discord_create_interaction_response(client, interaction->id, interaction->token, &defer, NULL);

        update_info_t info;
        if (updater_check(HIMIKO_VERSION, &info) != 0) {
            struct discord_edit_original_interaction_response params = {
                .content = "Failed to check for updates."
            };
            discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &params, NULL);
            return;
        }

        if (!info.has_update) {
            struct discord_edit_original_interaction_response params = {
                .content = "No updates available. You are running the latest version."
            };
            discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &params, NULL);
            return;
        }

        char size_str[32];
        format_bytes(info.size, size_str, sizeof(size_str));

        char msg[256];
        snprintf(msg, sizeof(msg), "Downloading update v%s (%s)...", info.new_version, size_str);
        struct discord_edit_original_interaction_response params = { .content = msg };
        discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &params, NULL);

        /* Download */
        char *zip_path = updater_download(&info, NULL, NULL);
        if (!zip_path) {
            struct discord_edit_original_interaction_response err_params = {
                .content = "Failed to download update."
            };
            discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &err_params, NULL);
            return;
        }

        struct discord_edit_original_interaction_response dl_params = {
            .content = "Download complete. Applying update..."
        };
        discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &dl_params, NULL);

        /* Apply */
        if (updater_apply(zip_path) != 0) {
            free(zip_path);
            struct discord_edit_original_interaction_response err_params = {
                .content = "Failed to apply update."
            };
            discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &err_params, NULL);
            return;
        }
        free(zip_path);

        snprintf(msg, sizeof(msg),
            "**Update Applied Successfully!**\n\n"
            "Updated from v%s to v%s\n\n"
            "**The bot needs to be restarted to use the new version.**",
            info.current_version, info.new_version);

        struct discord_edit_original_interaction_response done_params = { .content = msg };
        discord_edit_original_interaction_response(client, interaction->application_id, interaction->token, &done_params, NULL);
        return;
    }

    respond_ephemeral(client, interaction, "Unknown subcommand.");
}

void cmd_update_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    himiko_bot_t *bot = discord_get_data(client);
    if (!bot) return;

    /* Owner only */
    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)msg->author->id);
    if (!config_is_owner(&bot->config, user_id_str)) {
        struct discord_create_message params = {
            .content = "This command is only available to bot owners."
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (!args || !*args || strcmp(args, "version") == 0) {
        char response[256];
        snprintf(response, sizeof(response),
            "**Himiko Version Info**\n"
            "Current Version: v%s\n"
            "Auto-Update: %s | Auto-Apply: %s",
            HIMIKO_VERSION,
            bot->config.features.auto_update ? "On" : "Off",
            bot->config.features.auto_update_apply ? "On" : "Off");
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strcmp(args, "check") == 0) {
        update_info_t info;
        if (updater_check(HIMIKO_VERSION, &info) != 0) {
            struct discord_create_message params = { .content = "Failed to check for updates." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        char response[512];
        if (!info.has_update) {
            snprintf(response, sizeof(response), "You are running the latest version (**v%s**).", info.current_version);
        } else {
            char size_str[32];
            format_bytes(info.size, size_str, sizeof(size_str));
            snprintf(response, sizeof(response),
                "**Update Available!**\nNew: v%s (current: v%s)\nSize: %s\n\nUse `update apply` to install.",
                info.new_version, info.current_version, size_str);
        }

        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strcmp(args, "apply") == 0) {
        struct discord_create_message start_params = { .content = "Checking for updates..." };
        discord_create_message(client, msg->channel_id, &start_params, NULL);

        update_info_t info;
        if (updater_check(HIMIKO_VERSION, &info) != 0) {
            struct discord_create_message params = { .content = "Failed to check for updates." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        if (!info.has_update) {
            struct discord_create_message params = { .content = "No updates available." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        struct discord_create_message dl_params = { .content = "Downloading update..." };
        discord_create_message(client, msg->channel_id, &dl_params, NULL);

        char *zip_path = updater_download(&info, NULL, NULL);
        if (!zip_path) {
            struct discord_create_message params = { .content = "Failed to download update." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        struct discord_create_message apply_params = { .content = "Applying update..." };
        discord_create_message(client, msg->channel_id, &apply_params, NULL);

        if (updater_apply(zip_path) != 0) {
            free(zip_path);
            struct discord_create_message params = { .content = "Failed to apply update." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }
        free(zip_path);

        char response[256];
        snprintf(response, sizeof(response),
            "**Update applied!** v%s -> v%s\nRestart the bot to complete the update.",
            info.current_version, info.new_version);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    struct discord_create_message params = {
        .content = "Usage: update [check|apply|version]"
    };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_update_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "update", "Check for and apply bot updates", "Admin", cmd_update, cmd_update_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
