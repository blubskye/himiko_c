/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Database schema is EXACTLY compatible with Himiko Go version.
 * The same himiko.db file can be used by both Go and C versions.
 */

#ifndef HIMIKO_DATABASE_H
#define HIMIKO_DATABASE_H

#include <stdint.h>
#include <sqlite3.h>
#include <time.h>

#define MAX_REASON_LEN 512
#define MAX_CONTENT_LEN 2048
#define MAX_PATTERN_LEN 256
#define MAX_MESSAGE_LEN 2000

/* Guild settings */
typedef struct {
    char guild_id[32];
    char prefix[16];
    char mod_log_channel[32];
    char welcome_channel[32];
    char welcome_message[MAX_CONTENT_LEN];
    char join_dm_title[256];
    char join_dm_message[MAX_CONTENT_LEN];
} guild_settings_t;

/* Custom command */
typedef struct {
    int64_t id;
    char guild_id[32];
    char name[64];
    char response[MAX_CONTENT_LEN];
    char created_by[32];
    int use_count;
} custom_command_t;

/* Warning */
typedef struct {
    int64_t id;
    char guild_id[32];
    char user_id[32];
    char moderator_id[32];
    char reason[MAX_REASON_LEN];
    time_t created_at;
} warning_t;

/* Deleted message (for snipe) */
typedef struct {
    int64_t id;
    char guild_id[32];
    char channel_id[32];
    char user_id[32];
    char content[MAX_CONTENT_LEN];
    time_t deleted_at;
} deleted_message_t;

/* User XP */
typedef struct {
    char guild_id[32];
    char user_id[32];
    int64_t xp;
    int level;
    time_t updated_at;
} user_xp_t;

/* Level rank (role reward) */
typedef struct {
    int64_t id;
    char guild_id[32];
    char role_id[32];
    int level;
} level_rank_t;

/* Moderation action */
typedef struct {
    int64_t id;
    char guild_id[32];
    char moderator_id[32];
    char target_id[32];
    char action[32];
    char reason[MAX_REASON_LEN];
    int64_t timestamp;
} mod_action_t;

/* Bot ban */
typedef struct {
    int64_t id;
    char target_id[32];
    char ban_type[16];
    char reason[MAX_REASON_LEN];
    char banned_by[32];
    time_t created_at;
} bot_ban_t;

/* AFK status */
typedef struct {
    char user_id[32];
    char message[MAX_MESSAGE_LEN];
    time_t set_at;
} afk_status_t;

/* Reminder */
typedef struct {
    int64_t id;
    char user_id[32];
    char channel_id[32];
    char message[MAX_MESSAGE_LEN];
    time_t remind_at;
    int completed;
} reminder_t;

/* Anti-raid config */
typedef struct {
    char guild_id[32];
    int enabled;
    int raid_time;
    int raid_size;
    int auto_silence;
    int lockdown_duration;
    char silent_role_id[32];
    char alert_role_id[32];
    char log_channel_id[32];
    char action[32];
} antiraid_config_t;

/* Anti-spam config (pressure system) */
typedef struct {
    char guild_id[32];
    int enabled;
    double base_pressure;
    double image_pressure;
    double link_pressure;
    double ping_pressure;
    double length_pressure;
    double line_pressure;
    double repeat_pressure;
    double max_pressure;
    double pressure_decay;
    char action[32];
    char silent_role_id[32];
} antispam_config_t;

/* Logging config */
typedef struct {
    char guild_id[32];
    char log_channel_id[32];
    int enabled;
    int message_delete;
    int message_edit;
    int voice_join;
    int voice_leave;
    int nickname_change;
    int avatar_change;
    int presence_change;
    int presence_batch_mins;
} logging_config_t;

/* Spam filter config */
typedef struct {
    char guild_id[32];
    int enabled;
    int max_mentions;
    int max_links;
    int max_emojis;
    char action[32];
} spam_filter_config_t;

/* Database handle */
typedef struct {
    sqlite3 *db;
} himiko_database_t;

/* Database lifecycle */
int db_open(himiko_database_t *database, const char *path);
void db_close(himiko_database_t *database);
int db_migrate(himiko_database_t *database);

/* Guild settings */
int db_get_guild_settings(himiko_database_t *db, const char *guild_id, guild_settings_t *settings);
int db_set_guild_settings(himiko_database_t *db, const guild_settings_t *settings);
int db_get_prefix(himiko_database_t *db, const char *guild_id, const char *default_prefix, char *out_prefix, size_t out_len);
int db_set_prefix(himiko_database_t *db, const char *guild_id, const char *prefix);

/* Custom commands */
int db_get_custom_command(himiko_database_t *db, const char *guild_id, const char *name, custom_command_t *cmd);
int db_create_custom_command(himiko_database_t *db, const char *guild_id, const char *name, const char *response, const char *created_by);
int db_delete_custom_command(himiko_database_t *db, const char *guild_id, const char *name);
int db_list_custom_commands(himiko_database_t *db, const char *guild_id, custom_command_t *cmds, int max_cmds, int *count);
int db_increment_command_use(himiko_database_t *db, const char *guild_id, const char *name);

/* Command history */
int db_log_command(himiko_database_t *db, const char *guild_id, const char *channel_id, const char *user_id, const char *command, const char *args);

/* Warnings */
int db_add_warning(himiko_database_t *db, const char *guild_id, const char *user_id, const char *moderator_id, const char *reason);
int db_get_warnings(himiko_database_t *db, const char *guild_id, const char *user_id, warning_t *warnings, int max_warnings, int *count);
int db_clear_warnings(himiko_database_t *db, const char *guild_id, const char *user_id);
int db_delete_warning(himiko_database_t *db, int64_t id);

/* Deleted messages (snipe) */
int db_log_deleted_message(himiko_database_t *db, const char *guild_id, const char *channel_id, const char *user_id, const char *content);
int db_get_deleted_messages(himiko_database_t *db, const char *channel_id, deleted_message_t *messages, int max_messages, int *count);
int db_clean_old_deleted_messages(himiko_database_t *db, int older_than_hours);

/* XP/Leveling */
int db_get_user_xp(himiko_database_t *db, const char *guild_id, const char *user_id, user_xp_t *xp);
int db_set_user_xp(himiko_database_t *db, const char *guild_id, const char *user_id, int64_t xp, int level);
int db_add_user_xp(himiko_database_t *db, const char *guild_id, const char *user_id, int64_t amount, user_xp_t *result);
int db_get_leaderboard(himiko_database_t *db, const char *guild_id, user_xp_t *results, int max_results, int *count);
int db_get_user_rank(himiko_database_t *db, const char *guild_id, const char *user_id, int *rank);

/* Level ranks */
int db_add_level_rank(himiko_database_t *db, const char *guild_id, const char *role_id, int level);
int db_remove_level_rank(himiko_database_t *db, const char *guild_id, const char *role_id);
int db_get_level_ranks(himiko_database_t *db, const char *guild_id, level_rank_t *ranks, int max_ranks, int *count);
int db_get_ranks_for_level(himiko_database_t *db, const char *guild_id, int level, level_rank_t *ranks, int max_ranks, int *count);

/* Mod actions */
int db_add_mod_action(himiko_database_t *db, const mod_action_t *action);
int db_get_mod_actions_count(himiko_database_t *db, const char *guild_id, int *count);

/* Bot bans */
int db_add_bot_ban(himiko_database_t *db, const char *target_id, const char *ban_type, const char *reason, const char *banned_by);
int db_remove_bot_ban(himiko_database_t *db, const char *target_id);
int db_is_bot_banned(himiko_database_t *db, const char *target_id);
int db_get_bot_bans(himiko_database_t *db, const char *ban_type, bot_ban_t *bans, int max_bans, int *count);

/* AFK */
int db_set_afk(himiko_database_t *db, const char *user_id, const char *message);
int db_get_afk(himiko_database_t *db, const char *user_id, afk_status_t *afk);
int db_remove_afk(himiko_database_t *db, const char *user_id);

/* Reminders */
int db_add_reminder(himiko_database_t *db, const char *user_id, const char *channel_id, const char *message, time_t remind_at);
int db_get_pending_reminders(himiko_database_t *db, reminder_t *reminders, int max_reminders, int *count);
int db_mark_reminder_completed(himiko_database_t *db, int64_t id);

/* Anti-raid */
int db_get_antiraid_config(himiko_database_t *db, const char *guild_id, antiraid_config_t *config);
int db_set_antiraid_config(himiko_database_t *db, const antiraid_config_t *config);
int db_record_member_join(himiko_database_t *db, const char *guild_id, const char *user_id, int64_t joined_at, int64_t account_created_at);
int db_count_recent_joins(himiko_database_t *db, const char *guild_id, int64_t since_timestamp, int *count);

/* Anti-spam */
int db_get_antispam_config(himiko_database_t *db, const char *guild_id, antispam_config_t *config);
int db_set_antispam_config(himiko_database_t *db, const antispam_config_t *config);

/* Logging */
int db_get_logging_config(himiko_database_t *db, const char *guild_id, logging_config_t *config);
int db_set_logging_config(himiko_database_t *db, const logging_config_t *config);
int db_set_log_channel(himiko_database_t *db, const char *guild_id, const char *channel_id);
int db_is_log_channel_disabled(himiko_database_t *db, const char *guild_id, const char *channel_id, int *disabled);
int db_add_disabled_log_channel(himiko_database_t *db, const char *guild_id, const char *channel_id);
int db_remove_disabled_log_channel(himiko_database_t *db, const char *guild_id, const char *channel_id);

/* Spam filter */
int db_get_spam_filter_config(himiko_database_t *db, const char *guild_id, spam_filter_config_t *config);
int db_set_spam_filter_config(himiko_database_t *db, const spam_filter_config_t *config);

/* XP calculation helpers */
int calculate_level(int64_t xp);
int64_t xp_for_level(int level);

#endif /* HIMIKO_DATABASE_H */
