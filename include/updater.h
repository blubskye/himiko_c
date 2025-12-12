/*
 * Himiko Discord Bot (C Edition) - Updater Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_UPDATER_H
#define HIMIKO_UPDATER_H

#include <concord/discord.h>

/* Forward declare */
struct himiko_bot;

#define GITHUB_REPO "blubskye/himiko"
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_REPO "/releases/latest"

/* Update info structure */
typedef struct {
    char current_version[32];
    char new_version[32];
    char download_url[512];
    char asset_name[128];
    char release_notes[4096];
    long size;
    int has_update;
} update_info_t;

/* Progress callback for downloads */
typedef void (*download_progress_fn)(long downloaded, long total, void *user_data);

/* Check for updates on GitHub */
int updater_check(const char *current_version, update_info_t *info);

/* Download the update to a temp file, returns path (caller must free) */
char *updater_download(const update_info_t *info, download_progress_fn progress_fn, void *user_data);

/* Apply the downloaded update (replaces binary) */
int updater_apply(const char *zip_path);

/* Send update notification to channel */
void updater_notify_channel(struct discord *client, const char *channel_id, const update_info_t *info, int applied);

/* Send update notification via DM to owners */
void updater_notify_owners(struct discord *client, struct himiko_bot *bot, const update_info_t *info, int applied);

/* Get current version string */
const char *updater_get_version(void);

/* Version comparison helpers */
int updater_is_newer_version(const char *current, const char *newer);
void updater_parse_version(const char *v, int parts[3]);

/* Slash commands */
void cmd_update(struct discord *client, const struct discord_interaction *interaction);
void cmd_update_prefix(struct discord *client, const struct discord_message *msg, const char *args);

void register_update_commands(struct himiko_bot *bot);

#endif /* HIMIKO_UPDATER_H */
