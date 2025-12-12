/*
 * Himiko Discord Bot (C Edition)
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Database schema is EXACTLY compatible with Himiko Go version.
 * This uses the same CREATE TABLE statements from database.go.
 */

#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int db_open(himiko_database_t *database, const char *path) {
    char uri[512];
    snprintf(uri, sizeof(uri), "%s?_foreign_keys=on", path);

    int rc = sqlite3_open(uri, &database->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(database->db));
        return -1;
    }

    return db_migrate(database);
}

void db_close(himiko_database_t *database) {
    if (database->db) {
        sqlite3_close(database->db);
        database->db = NULL;
    }
}

/*
 * Schema migration - EXACT copy from Himiko Go database.go
 * This ensures both versions can use the same database file.
 */
int db_migrate(himiko_database_t *database) {
    const char *schema =
        /* Guild settings */
        "CREATE TABLE IF NOT EXISTS guild_settings ("
        "    guild_id TEXT PRIMARY KEY,"
        "    prefix TEXT DEFAULT '/',"
        "    mod_log_channel TEXT,"
        "    welcome_channel TEXT,"
        "    welcome_message TEXT,"
        "    join_dm_title TEXT,"
        "    join_dm_message TEXT,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Custom commands */
        "CREATE TABLE IF NOT EXISTS custom_commands ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    name TEXT NOT NULL,"
        "    response TEXT NOT NULL,"
        "    created_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    use_count INTEGER DEFAULT 0,"
        "    UNIQUE(guild_id, name)"
        ");"

        /* Command history */
        "CREATE TABLE IF NOT EXISTS command_history ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT,"
        "    channel_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    command TEXT NOT NULL,"
        "    args TEXT,"
        "    executed_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Warnings */
        "CREATE TABLE IF NOT EXISTS warnings ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    moderator_id TEXT NOT NULL,"
        "    reason TEXT,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Deleted messages log (for snipe command) */
        "CREATE TABLE IF NOT EXISTS deleted_messages ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT,"
        "    channel_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    content TEXT NOT NULL,"
        "    deleted_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* User profiles/notes */
        "CREATE TABLE IF NOT EXISTS user_notes ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    note TEXT NOT NULL,"
        "    created_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(guild_id, user_id)"
        ");"

        /* Scheduled messages */
        "CREATE TABLE IF NOT EXISTS scheduled_messages ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT,"
        "    channel_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    message TEXT NOT NULL,"
        "    scheduled_for DATETIME NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    executed INTEGER DEFAULT 0"
        ");"

        /* AFK status */
        "CREATE TABLE IF NOT EXISTS afk_status ("
        "    user_id TEXT PRIMARY KEY,"
        "    message TEXT,"
        "    set_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Reminders */
        "CREATE TABLE IF NOT EXISTS reminders ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    user_id TEXT NOT NULL,"
        "    channel_id TEXT NOT NULL,"
        "    message TEXT NOT NULL,"
        "    remind_at DATETIME NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    completed INTEGER DEFAULT 0"
        ");"

        /* Tags/snippets */
        "CREATE TABLE IF NOT EXISTS tags ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    name TEXT NOT NULL,"
        "    content TEXT NOT NULL,"
        "    created_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    use_count INTEGER DEFAULT 0,"
        "    UNIQUE(guild_id, name)"
        ");"

        /* Keyword notifications */
        "CREATE TABLE IF NOT EXISTS keyword_notifications ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    user_id TEXT NOT NULL,"
        "    guild_id TEXT,"
        "    keyword TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(user_id, keyword)"
        ");"

        /* Indexes for basic tables */
        "CREATE INDEX IF NOT EXISTS idx_custom_commands_guild ON custom_commands(guild_id);"
        "CREATE INDEX IF NOT EXISTS idx_warnings_guild_user ON warnings(guild_id, user_id);"
        "CREATE INDEX IF NOT EXISTS idx_deleted_messages_channel ON deleted_messages(channel_id);"
        "CREATE INDEX IF NOT EXISTS idx_scheduled_messages_time ON scheduled_messages(scheduled_for);"
        "CREATE INDEX IF NOT EXISTS idx_reminders_time ON reminders(remind_at);"

        /* XP/Leveling system */
        "CREATE TABLE IF NOT EXISTS user_xp ("
        "    guild_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    xp INTEGER DEFAULT 0,"
        "    level INTEGER DEFAULT 0,"
        "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    PRIMARY KEY (guild_id, user_id)"
        ");"

        /* Regex filters for auto-moderation */
        "CREATE TABLE IF NOT EXISTS regex_filters ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    pattern TEXT NOT NULL,"
        "    action TEXT NOT NULL,"
        "    reason TEXT,"
        "    created_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Auto-clean channels */
        "CREATE TABLE IF NOT EXISTS autoclean_channels ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    channel_id TEXT NOT NULL,"
        "    interval_hours INTEGER DEFAULT 24,"
        "    warning_minutes INTEGER DEFAULT 5,"
        "    next_run DATETIME,"
        "    clean_message INTEGER DEFAULT 1,"
        "    clean_image INTEGER DEFAULT 1,"
        "    created_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(guild_id, channel_id)"
        ");"

        /* Logging configuration */
        "CREATE TABLE IF NOT EXISTS logging_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    log_channel_id TEXT,"
        "    enabled INTEGER DEFAULT 0,"
        "    message_delete INTEGER DEFAULT 1,"
        "    message_edit INTEGER DEFAULT 1,"
        "    voice_join INTEGER DEFAULT 1,"
        "    voice_leave INTEGER DEFAULT 1,"
        "    nickname_change INTEGER DEFAULT 1,"
        "    avatar_change INTEGER DEFAULT 0,"
        "    presence_change INTEGER DEFAULT 0,"
        "    presence_batch_mins INTEGER DEFAULT 5"
        ");"

        /* Disabled log channels */
        "CREATE TABLE IF NOT EXISTS disabled_log_channels ("
        "    guild_id TEXT NOT NULL,"
        "    channel_id TEXT NOT NULL,"
        "    PRIMARY KEY (guild_id, channel_id)"
        ");"

        /* Voice XP configuration */
        "CREATE TABLE IF NOT EXISTS voice_xp_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    enabled INTEGER DEFAULT 0,"
        "    xp_rate INTEGER DEFAULT 10,"
        "    interval_mins INTEGER DEFAULT 5,"
        "    ignore_afk INTEGER DEFAULT 1"
        ");"

        /* Level ranks (role rewards) */
        "CREATE TABLE IF NOT EXISTS level_ranks ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    role_id TEXT NOT NULL,"
        "    level INTEGER NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(guild_id, role_id)"
        ");"

        /* DM forwarding configuration */
        "CREATE TABLE IF NOT EXISTS dm_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    channel_id TEXT NOT NULL,"
        "    enabled INTEGER DEFAULT 1"
        ");"

        /* Bot-level bans */
        "CREATE TABLE IF NOT EXISTS bot_bans ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    target_id TEXT NOT NULL UNIQUE,"
        "    ban_type TEXT NOT NULL,"
        "    reason TEXT,"
        "    banned_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Moderation actions tracking */
        "CREATE TABLE IF NOT EXISTS mod_actions ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    moderator_id TEXT NOT NULL,"
        "    target_id TEXT NOT NULL,"
        "    action TEXT NOT NULL,"
        "    reason TEXT,"
        "    timestamp INTEGER NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Mention responses (custom triggers) */
        "CREATE TABLE IF NOT EXISTS mention_responses ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    trigger_text TEXT NOT NULL,"
        "    response TEXT NOT NULL,"
        "    image_url TEXT,"
        "    created_by TEXT NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(guild_id, trigger_text)"
        ");"

        /* Spam filter configuration */
        "CREATE TABLE IF NOT EXISTS spam_filter_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    enabled INTEGER DEFAULT 0,"
        "    max_mentions INTEGER DEFAULT 5,"
        "    max_links INTEGER DEFAULT 3,"
        "    max_emojis INTEGER DEFAULT 10,"
        "    action TEXT DEFAULT 'delete'"
        ");"

        /* Ticket system configuration */
        "CREATE TABLE IF NOT EXISTS ticket_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    channel_id TEXT NOT NULL,"
        "    enabled INTEGER DEFAULT 1,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Anti-raid configuration */
        "CREATE TABLE IF NOT EXISTS antiraid_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    enabled INTEGER DEFAULT 0,"
        "    raid_time INTEGER DEFAULT 300,"
        "    raid_size INTEGER DEFAULT 5,"
        "    auto_silence INTEGER DEFAULT 0,"
        "    lockdown_duration INTEGER DEFAULT 120,"
        "    silent_role_id TEXT,"
        "    alert_role_id TEXT,"
        "    log_channel_id TEXT,"
        "    action TEXT DEFAULT 'silence'"
        ");"

        /* Member join tracking for raid detection */
        "CREATE TABLE IF NOT EXISTS member_joins ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    joined_at INTEGER NOT NULL,"
        "    account_created_at INTEGER NOT NULL"
        ");"

        /* Spam pressure tracking config */
        "CREATE TABLE IF NOT EXISTS antispam_config ("
        "    guild_id TEXT PRIMARY KEY,"
        "    enabled INTEGER DEFAULT 0,"
        "    base_pressure REAL DEFAULT 10.0,"
        "    image_pressure REAL DEFAULT 8.33,"
        "    link_pressure REAL DEFAULT 8.33,"
        "    ping_pressure REAL DEFAULT 2.5,"
        "    length_pressure REAL DEFAULT 0.00625,"
        "    line_pressure REAL DEFAULT 0.71,"
        "    repeat_pressure REAL DEFAULT 10.0,"
        "    max_pressure REAL DEFAULT 60.0,"
        "    pressure_decay REAL DEFAULT 2.5,"
        "    action TEXT DEFAULT 'delete',"
        "    silent_role_id TEXT"
        ");"

        /* Scheduled events (for timed unsilence, etc) */
        "CREATE TABLE IF NOT EXISTS scheduled_events ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    event_type TEXT NOT NULL,"
        "    target_id TEXT NOT NULL,"
        "    execute_at INTEGER NOT NULL,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* User aliases (username/nickname history) */
        "CREATE TABLE IF NOT EXISTS user_aliases ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    user_id TEXT NOT NULL,"
        "    alias TEXT NOT NULL,"
        "    alias_type TEXT NOT NULL,"
        "    first_seen DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    use_count INTEGER DEFAULT 1,"
        "    UNIQUE(user_id, alias, alias_type)"
        ");"

        /* User activity tracking (per guild) */
        "CREATE TABLE IF NOT EXISTS user_activity ("
        "    guild_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    first_seen DATETIME,"
        "    first_message DATETIME,"
        "    last_seen DATETIME,"
        "    message_count INTEGER DEFAULT 0,"
        "    PRIMARY KEY (guild_id, user_id)"
        ");"

        /* User timezone settings */
        "CREATE TABLE IF NOT EXISTS user_timezones ("
        "    user_id TEXT PRIMARY KEY,"
        "    timezone TEXT NOT NULL,"
        "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Music: Guild music settings */
        "CREATE TABLE IF NOT EXISTS music_settings ("
        "    guild_id TEXT PRIMARY KEY,"
        "    dj_role_id TEXT,"
        "    mod_role_id TEXT,"
        "    volume INTEGER DEFAULT 50,"
        "    music_folder TEXT,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Music: Queue */
        "CREATE TABLE IF NOT EXISTS music_queue ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    channel_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    title TEXT NOT NULL,"
        "    url TEXT NOT NULL,"
        "    duration INTEGER DEFAULT 0,"
        "    thumbnail TEXT,"
        "    is_local INTEGER DEFAULT 0,"
        "    position INTEGER NOT NULL,"
        "    added_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Music: Playback history */
        "CREATE TABLE IF NOT EXISTS music_history ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    guild_id TEXT NOT NULL,"
        "    user_id TEXT NOT NULL,"
        "    title TEXT NOT NULL,"
        "    url TEXT NOT NULL,"
        "    played_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        /* Additional indexes */
        "CREATE INDEX IF NOT EXISTS idx_user_xp_guild ON user_xp(guild_id);"
        "CREATE INDEX IF NOT EXISTS idx_member_joins_guild ON member_joins(guild_id, joined_at);"
        "CREATE INDEX IF NOT EXISTS idx_scheduled_events_time ON scheduled_events(execute_at);"
        "CREATE INDEX IF NOT EXISTS idx_regex_filters_guild ON regex_filters(guild_id);"
        "CREATE INDEX IF NOT EXISTS idx_level_ranks_guild ON level_ranks(guild_id);"
        "CREATE INDEX IF NOT EXISTS idx_mod_actions_guild ON mod_actions(guild_id);"
        "CREATE INDEX IF NOT EXISTS idx_mod_actions_moderator ON mod_actions(guild_id, moderator_id);"
        "CREATE INDEX IF NOT EXISTS idx_mod_actions_target ON mod_actions(guild_id, target_id);"
        "CREATE INDEX IF NOT EXISTS idx_user_aliases_user ON user_aliases(user_id);"
        "CREATE INDEX IF NOT EXISTS idx_user_activity_guild ON user_activity(guild_id);"
        "CREATE INDEX IF NOT EXISTS idx_music_queue_guild ON music_queue(guild_id, position);"
        "CREATE INDEX IF NOT EXISTS idx_music_history_guild ON music_history(guild_id);";

    char *err_msg = NULL;
    int rc = sqlite3_exec(database->db, schema, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    return 0;
}

/* Guild Settings */
int db_get_guild_settings(himiko_database_t *db, const char *guild_id, guild_settings_t *settings) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT guild_id, prefix, mod_log_channel, welcome_channel, welcome_message, join_dm_title, join_dm_message "
                      "FROM guild_settings WHERE guild_id = ?";

    memset(settings, 0, sizeof(guild_settings_t));
    strncpy(settings->guild_id, guild_id, sizeof(settings->guild_id) - 1);
    strcpy(settings->prefix, "/");

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *val;
        if ((val = (const char *)sqlite3_column_text(stmt, 1)) != NULL)
            strncpy(settings->prefix, val, sizeof(settings->prefix) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 2)) != NULL)
            strncpy(settings->mod_log_channel, val, sizeof(settings->mod_log_channel) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 3)) != NULL)
            strncpy(settings->welcome_channel, val, sizeof(settings->welcome_channel) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 4)) != NULL)
            strncpy(settings->welcome_message, val, sizeof(settings->welcome_message) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 5)) != NULL)
            strncpy(settings->join_dm_title, val, sizeof(settings->join_dm_title) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 6)) != NULL)
            strncpy(settings->join_dm_message, val, sizeof(settings->join_dm_message) - 1);
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_set_guild_settings(himiko_database_t *db, const guild_settings_t *settings) {
    const char *sql = "INSERT INTO guild_settings (guild_id, prefix, mod_log_channel, welcome_channel, welcome_message, join_dm_title, join_dm_message, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
                      "ON CONFLICT(guild_id) DO UPDATE SET "
                      "prefix = excluded.prefix, mod_log_channel = excluded.mod_log_channel, "
                      "welcome_channel = excluded.welcome_channel, welcome_message = excluded.welcome_message, "
                      "join_dm_title = excluded.join_dm_title, join_dm_message = excluded.join_dm_message, "
                      "updated_at = CURRENT_TIMESTAMP";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, settings->guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, settings->prefix, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, settings->mod_log_channel[0] ? settings->mod_log_channel : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, settings->welcome_channel[0] ? settings->welcome_channel : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, settings->welcome_message[0] ? settings->welcome_message : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, settings->join_dm_title[0] ? settings->join_dm_title : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, settings->join_dm_message[0] ? settings->join_dm_message : NULL, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_prefix(himiko_database_t *db, const char *guild_id, const char *default_prefix, char *out_prefix, size_t out_len) {
    guild_settings_t settings;
    if (db_get_guild_settings(db, guild_id, &settings) == 0) {
        strncpy(out_prefix, settings.prefix[0] ? settings.prefix : default_prefix, out_len - 1);
        out_prefix[out_len - 1] = '\0';
        return 0;
    }
    strncpy(out_prefix, default_prefix, out_len - 1);
    out_prefix[out_len - 1] = '\0';
    return -1;
}

int db_set_prefix(himiko_database_t *db, const char *guild_id, const char *prefix) {
    guild_settings_t settings;
    db_get_guild_settings(db, guild_id, &settings);
    strncpy(settings.prefix, prefix, sizeof(settings.prefix) - 1);
    return db_set_guild_settings(db, &settings);
}

/* Command history */
int db_log_command(himiko_database_t *db, const char *guild_id, const char *channel_id, const char *user_id, const char *command, const char *args) {
    const char *sql = "INSERT INTO command_history (guild_id, channel_id, user_id, command, args) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, command, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, args, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Warnings */
int db_add_warning(himiko_database_t *db, const char *guild_id, const char *user_id, const char *moderator_id, const char *reason) {
    const char *sql = "INSERT INTO warnings (guild_id, user_id, moderator_id, reason) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, moderator_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, reason, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_warnings(himiko_database_t *db, const char *guild_id, const char *user_id, warning_t *warnings, int max_warnings, int *count) {
    const char *sql = "SELECT id, guild_id, user_id, moderator_id, reason, created_at FROM warnings "
                      "WHERE guild_id = ? AND user_id = ? ORDER BY created_at DESC";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_warnings) {
        warning_t *w = &warnings[*count];
        w->id = sqlite3_column_int64(stmt, 0);
        strncpy(w->guild_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(w->guild_id) - 1);
        strncpy(w->user_id, (const char *)sqlite3_column_text(stmt, 2), sizeof(w->user_id) - 1);
        strncpy(w->moderator_id, (const char *)sqlite3_column_text(stmt, 3), sizeof(w->moderator_id) - 1);
        const char *reason = (const char *)sqlite3_column_text(stmt, 4);
        if (reason) strncpy(w->reason, reason, sizeof(w->reason) - 1);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_clear_warnings(himiko_database_t *db, const char *guild_id, const char *user_id) {
    const char *sql = "DELETE FROM warnings WHERE guild_id = ? AND user_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete_warning(himiko_database_t *db, int64_t id) {
    const char *sql = "DELETE FROM warnings WHERE id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Deleted messages (snipe) */
int db_log_deleted_message(himiko_database_t *db, const char *guild_id, const char *channel_id, const char *user_id, const char *content) {
    const char *sql = "INSERT INTO deleted_messages (guild_id, channel_id, user_id, content) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, content, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_deleted_messages(himiko_database_t *db, const char *channel_id, deleted_message_t *messages, int max_messages, int *count) {
    const char *sql = "SELECT id, guild_id, channel_id, user_id, content, deleted_at FROM deleted_messages "
                      "WHERE channel_id = ? ORDER BY deleted_at DESC LIMIT ?";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, max_messages);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_messages) {
        deleted_message_t *m = &messages[*count];
        m->id = sqlite3_column_int64(stmt, 0);
        const char *val;
        if ((val = (const char *)sqlite3_column_text(stmt, 1)) != NULL)
            strncpy(m->guild_id, val, sizeof(m->guild_id) - 1);
        strncpy(m->channel_id, (const char *)sqlite3_column_text(stmt, 2), sizeof(m->channel_id) - 1);
        strncpy(m->user_id, (const char *)sqlite3_column_text(stmt, 3), sizeof(m->user_id) - 1);
        strncpy(m->content, (const char *)sqlite3_column_text(stmt, 4), sizeof(m->content) - 1);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_clean_old_deleted_messages(himiko_database_t *db, int older_than_hours) {
    const char *sql = "DELETE FROM deleted_messages WHERE deleted_at < datetime('now', ? || ' hours')";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    char hours_str[16];
    snprintf(hours_str, sizeof(hours_str), "-%d", older_than_hours);
    sqlite3_bind_text(stmt, 1, hours_str, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* XP/Leveling */
int calculate_level(int64_t xp) {
    if (xp <= 0) return 0;
    /* level = floor((sqrt(1 + 8*xp/50) - 1) / 2) */
    double val = 1.0 + (8.0 * xp / 50.0);
    int level = (int)((sqrt(val) - 1.0) / 2.0);
    return (level < 0) ? 0 : level;
}

int64_t xp_for_level(int level) {
    /* XP = 5*level^2 + 50*level + 100 */
    return (int64_t)(5 * level * level + 50 * level + 100);
}

int db_get_user_xp(himiko_database_t *db, const char *guild_id, const char *user_id, user_xp_t *xp) {
    const char *sql = "SELECT guild_id, user_id, xp, level, updated_at FROM user_xp WHERE guild_id = ? AND user_id = ?";
    sqlite3_stmt *stmt;

    memset(xp, 0, sizeof(user_xp_t));
    strncpy(xp->guild_id, guild_id, sizeof(xp->guild_id) - 1);
    strncpy(xp->user_id, user_id, sizeof(xp->user_id) - 1);

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        xp->xp = sqlite3_column_int64(stmt, 2);
        xp->level = sqlite3_column_int(stmt, 3);
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_set_user_xp(himiko_database_t *db, const char *guild_id, const char *user_id, int64_t xp, int level) {
    const char *sql = "INSERT INTO user_xp (guild_id, user_id, xp, level, updated_at) "
                      "VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP) "
                      "ON CONFLICT(guild_id, user_id) DO UPDATE SET "
                      "xp = excluded.xp, level = excluded.level, updated_at = CURRENT_TIMESTAMP";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, xp);
    sqlite3_bind_int(stmt, 4, level);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_add_user_xp(himiko_database_t *db, const char *guild_id, const char *user_id, int64_t amount, user_xp_t *result) {
    db_get_user_xp(db, guild_id, user_id, result);
    result->xp += amount;
    result->level = calculate_level(result->xp);
    return db_set_user_xp(db, guild_id, user_id, result->xp, result->level);
}

int db_get_leaderboard(himiko_database_t *db, const char *guild_id, user_xp_t *results, int max_results, int *count) {
    const char *sql = "SELECT guild_id, user_id, xp, level, updated_at FROM user_xp "
                      "WHERE guild_id = ? ORDER BY xp DESC LIMIT ?";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_results) {
        user_xp_t *ux = &results[*count];
        strncpy(ux->guild_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(ux->guild_id) - 1);
        strncpy(ux->user_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(ux->user_id) - 1);
        ux->xp = sqlite3_column_int64(stmt, 2);
        ux->level = sqlite3_column_int(stmt, 3);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_get_user_rank(himiko_database_t *db, const char *guild_id, const char *user_id, int *rank) {
    const char *sql = "SELECT COUNT(*) + 1 FROM user_xp WHERE guild_id = ? AND xp > "
                      "(SELECT COALESCE(xp, 0) FROM user_xp WHERE guild_id = ? AND user_id = ?)";
    sqlite3_stmt *stmt;
    *rank = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *rank = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Level ranks */
int db_add_level_rank(himiko_database_t *db, const char *guild_id, const char *role_id, int level) {
    const char *sql = "INSERT INTO level_ranks (guild_id, role_id, level) VALUES (?, ?, ?) "
                      "ON CONFLICT(guild_id, role_id) DO UPDATE SET level = excluded.level";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, role_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, level);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_remove_level_rank(himiko_database_t *db, const char *guild_id, const char *role_id) {
    const char *sql = "DELETE FROM level_ranks WHERE guild_id = ? AND role_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, role_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_level_ranks(himiko_database_t *db, const char *guild_id, level_rank_t *ranks, int max_ranks, int *count) {
    const char *sql = "SELECT id, guild_id, role_id, level FROM level_ranks WHERE guild_id = ? ORDER BY level ASC";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_ranks) {
        level_rank_t *r = &ranks[*count];
        r->id = sqlite3_column_int64(stmt, 0);
        strncpy(r->guild_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(r->guild_id) - 1);
        strncpy(r->role_id, (const char *)sqlite3_column_text(stmt, 2), sizeof(r->role_id) - 1);
        r->level = sqlite3_column_int(stmt, 3);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_get_ranks_for_level(himiko_database_t *db, const char *guild_id, int level, level_rank_t *ranks, int max_ranks, int *count) {
    const char *sql = "SELECT id, guild_id, role_id, level FROM level_ranks WHERE guild_id = ? AND level <= ? ORDER BY level DESC";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, level);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_ranks) {
        level_rank_t *r = &ranks[*count];
        r->id = sqlite3_column_int64(stmt, 0);
        strncpy(r->guild_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(r->guild_id) - 1);
        strncpy(r->role_id, (const char *)sqlite3_column_text(stmt, 2), sizeof(r->role_id) - 1);
        r->level = sqlite3_column_int(stmt, 3);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Bot bans */
int db_add_bot_ban(himiko_database_t *db, const char *target_id, const char *ban_type, const char *reason, const char *banned_by) {
    const char *sql = "INSERT INTO bot_bans (target_id, ban_type, reason, banned_by) "
                      "VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(target_id) DO UPDATE SET ban_type = excluded.ban_type, reason = excluded.reason";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, target_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, ban_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, reason, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, banned_by, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_remove_bot_ban(himiko_database_t *db, const char *target_id) {
    const char *sql = "DELETE FROM bot_bans WHERE target_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, target_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_is_bot_banned(himiko_database_t *db, const char *target_id) {
    const char *sql = "SELECT COUNT(*) FROM bot_bans WHERE target_id = ?";
    sqlite3_stmt *stmt;
    int banned = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, target_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        banned = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);
    return banned;
}

/* AFK */
int db_set_afk(himiko_database_t *db, const char *user_id, const char *message) {
    const char *sql = "INSERT INTO afk_status (user_id, message) VALUES (?, ?) "
                      "ON CONFLICT(user_id) DO UPDATE SET message = excluded.message, set_at = CURRENT_TIMESTAMP";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, message, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_afk(himiko_database_t *db, const char *user_id, afk_status_t *afk) {
    const char *sql = "SELECT user_id, message, set_at FROM afk_status WHERE user_id = ?";
    sqlite3_stmt *stmt;

    memset(afk, 0, sizeof(afk_status_t));

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(afk->user_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(afk->user_id) - 1);
        const char *msg = (const char *)sqlite3_column_text(stmt, 1);
        if (msg) strncpy(afk->message, msg, sizeof(afk->message) - 1);
        found = 1;
    }

    sqlite3_finalize(stmt);
    return found ? 0 : -1;
}

int db_remove_afk(himiko_database_t *db, const char *user_id) {
    const char *sql = "DELETE FROM afk_status WHERE user_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Mod actions */
int db_add_mod_action(himiko_database_t *db, const mod_action_t *action) {
    const char *sql = "INSERT INTO mod_actions (guild_id, moderator_id, target_id, action, reason, timestamp) "
                      "VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, action->guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, action->moderator_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, action->target_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, action->action, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, action->reason[0] ? action->reason : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, action->timestamp);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_mod_actions_count(himiko_database_t *db, const char *guild_id, int *count) {
    const char *sql = "SELECT COUNT(*) FROM mod_actions WHERE guild_id = ?";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Reminders */
int db_add_reminder(himiko_database_t *db, const char *user_id, const char *channel_id, const char *message, time_t remind_at) {
    const char *sql = "INSERT INTO reminders (user_id, channel_id, message, remind_at) VALUES (?, ?, ?, datetime(?, 'unixepoch'))";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, remind_at);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_pending_reminders(himiko_database_t *db, reminder_t *reminders, int max_reminders, int *count) {
    const char *sql = "SELECT id, user_id, channel_id, message, remind_at FROM reminders "
                      "WHERE completed = 0 AND remind_at <= datetime('now') ORDER BY remind_at";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_reminders) {
        reminder_t *r = &reminders[*count];
        r->id = sqlite3_column_int64(stmt, 0);
        strncpy(r->user_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(r->user_id) - 1);
        strncpy(r->channel_id, (const char *)sqlite3_column_text(stmt, 2), sizeof(r->channel_id) - 1);
        strncpy(r->message, (const char *)sqlite3_column_text(stmt, 3), sizeof(r->message) - 1);
        r->completed = 0;
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_mark_reminder_completed(himiko_database_t *db, int64_t id) {
    const char *sql = "UPDATE reminders SET completed = 1 WHERE id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Anti-raid */
int db_get_antiraid_config(himiko_database_t *db, const char *guild_id, antiraid_config_t *config) {
    const char *sql = "SELECT guild_id, enabled, raid_time, raid_size, auto_silence, lockdown_duration, "
                      "silent_role_id, alert_role_id, log_channel_id, action FROM antiraid_config WHERE guild_id = ?";
    sqlite3_stmt *stmt;

    memset(config, 0, sizeof(antiraid_config_t));
    strncpy(config->guild_id, guild_id, sizeof(config->guild_id) - 1);
    config->raid_time = 300;
    config->raid_size = 5;
    config->lockdown_duration = 120;
    strcpy(config->action, "silence");

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        config->enabled = sqlite3_column_int(stmt, 1);
        config->raid_time = sqlite3_column_int(stmt, 2);
        config->raid_size = sqlite3_column_int(stmt, 3);
        config->auto_silence = sqlite3_column_int(stmt, 4);
        config->lockdown_duration = sqlite3_column_int(stmt, 5);
        const char *val;
        if ((val = (const char *)sqlite3_column_text(stmt, 6)) != NULL)
            strncpy(config->silent_role_id, val, sizeof(config->silent_role_id) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 7)) != NULL)
            strncpy(config->alert_role_id, val, sizeof(config->alert_role_id) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 8)) != NULL)
            strncpy(config->log_channel_id, val, sizeof(config->log_channel_id) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 9)) != NULL)
            strncpy(config->action, val, sizeof(config->action) - 1);
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_record_member_join(himiko_database_t *db, const char *guild_id, const char *user_id, int64_t joined_at, int64_t account_created_at) {
    const char *sql = "INSERT INTO member_joins (guild_id, user_id, joined_at, account_created_at) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, joined_at);
    sqlite3_bind_int64(stmt, 4, account_created_at);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_count_recent_joins(himiko_database_t *db, const char *guild_id, int64_t since_timestamp, int *count) {
    const char *sql = "SELECT COUNT(*) FROM member_joins WHERE guild_id = ? AND joined_at >= ?";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, since_timestamp);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Anti-spam */
int db_get_antispam_config(himiko_database_t *db, const char *guild_id, antispam_config_t *config) {
    const char *sql = "SELECT guild_id, enabled, base_pressure, image_pressure, link_pressure, "
                      "ping_pressure, length_pressure, line_pressure, repeat_pressure, max_pressure, "
                      "pressure_decay, action, silent_role_id FROM antispam_config WHERE guild_id = ?";
    sqlite3_stmt *stmt;

    memset(config, 0, sizeof(antispam_config_t));
    strncpy(config->guild_id, guild_id, sizeof(config->guild_id) - 1);
    config->base_pressure = 10.0;
    config->image_pressure = 8.33;
    config->link_pressure = 8.33;
    config->ping_pressure = 2.5;
    config->length_pressure = 0.00625;
    config->line_pressure = 0.71;
    config->repeat_pressure = 10.0;
    config->max_pressure = 60.0;
    config->pressure_decay = 2.5;
    strcpy(config->action, "delete");

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        config->enabled = sqlite3_column_int(stmt, 1);
        config->base_pressure = sqlite3_column_double(stmt, 2);
        config->image_pressure = sqlite3_column_double(stmt, 3);
        config->link_pressure = sqlite3_column_double(stmt, 4);
        config->ping_pressure = sqlite3_column_double(stmt, 5);
        config->length_pressure = sqlite3_column_double(stmt, 6);
        config->line_pressure = sqlite3_column_double(stmt, 7);
        config->repeat_pressure = sqlite3_column_double(stmt, 8);
        config->max_pressure = sqlite3_column_double(stmt, 9);
        config->pressure_decay = sqlite3_column_double(stmt, 10);
        const char *val;
        if ((val = (const char *)sqlite3_column_text(stmt, 11)) != NULL)
            strncpy(config->action, val, sizeof(config->action) - 1);
        if ((val = (const char *)sqlite3_column_text(stmt, 12)) != NULL)
            strncpy(config->silent_role_id, val, sizeof(config->silent_role_id) - 1);
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Logging */
int db_get_logging_config(himiko_database_t *db, const char *guild_id, logging_config_t *config) {
    const char *sql = "SELECT guild_id, log_channel_id, enabled, message_delete, message_edit, "
                      "voice_join, voice_leave, nickname_change, avatar_change, presence_change, "
                      "presence_batch_mins FROM logging_config WHERE guild_id = ?";
    sqlite3_stmt *stmt;

    memset(config, 0, sizeof(logging_config_t));
    strncpy(config->guild_id, guild_id, sizeof(config->guild_id) - 1);
    config->message_delete = 1;
    config->message_edit = 1;
    config->voice_join = 1;
    config->voice_leave = 1;
    config->nickname_change = 1;
    config->presence_batch_mins = 5;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *val;
        if ((val = (const char *)sqlite3_column_text(stmt, 1)) != NULL)
            strncpy(config->log_channel_id, val, sizeof(config->log_channel_id) - 1);
        config->enabled = sqlite3_column_int(stmt, 2);
        config->message_delete = sqlite3_column_int(stmt, 3);
        config->message_edit = sqlite3_column_int(stmt, 4);
        config->voice_join = sqlite3_column_int(stmt, 5);
        config->voice_leave = sqlite3_column_int(stmt, 6);
        config->nickname_change = sqlite3_column_int(stmt, 7);
        config->avatar_change = sqlite3_column_int(stmt, 8);
        config->presence_change = sqlite3_column_int(stmt, 9);
        config->presence_batch_mins = sqlite3_column_int(stmt, 10);
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_set_log_channel(himiko_database_t *db, const char *guild_id, const char *channel_id) {
    const char *sql = "INSERT INTO logging_config (guild_id, log_channel_id, enabled) "
                      "VALUES (?, ?, 1) "
                      "ON CONFLICT(guild_id) DO UPDATE SET log_channel_id = excluded.log_channel_id, enabled = 1";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Custom commands */
int db_get_custom_command(himiko_database_t *db, const char *guild_id, const char *name, custom_command_t *cmd) {
    const char *sql = "SELECT id, guild_id, name, response, created_by, use_count FROM custom_commands "
                      "WHERE guild_id = ? AND name = ?";
    sqlite3_stmt *stmt;

    memset(cmd, 0, sizeof(custom_command_t));

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cmd->id = sqlite3_column_int64(stmt, 0);
        strncpy(cmd->guild_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(cmd->guild_id) - 1);
        strncpy(cmd->name, (const char *)sqlite3_column_text(stmt, 2), sizeof(cmd->name) - 1);
        strncpy(cmd->response, (const char *)sqlite3_column_text(stmt, 3), sizeof(cmd->response) - 1);
        strncpy(cmd->created_by, (const char *)sqlite3_column_text(stmt, 4), sizeof(cmd->created_by) - 1);
        cmd->use_count = sqlite3_column_int(stmt, 5);
        found = 1;
    }

    sqlite3_finalize(stmt);
    return found ? 0 : -1;
}

int db_create_custom_command(himiko_database_t *db, const char *guild_id, const char *name, const char *response, const char *created_by) {
    const char *sql = "INSERT INTO custom_commands (guild_id, name, response, created_by) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, response, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, created_by, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete_custom_command(himiko_database_t *db, const char *guild_id, const char *name) {
    const char *sql = "DELETE FROM custom_commands WHERE guild_id = ? AND name = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_list_custom_commands(himiko_database_t *db, const char *guild_id, custom_command_t *cmds, int max_cmds, int *count) {
    const char *sql = "SELECT id, guild_id, name, response, created_by, use_count FROM custom_commands "
                      "WHERE guild_id = ? ORDER BY name";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_cmds) {
        custom_command_t *c = &cmds[*count];
        c->id = sqlite3_column_int64(stmt, 0);
        strncpy(c->guild_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(c->guild_id) - 1);
        strncpy(c->name, (const char *)sqlite3_column_text(stmt, 2), sizeof(c->name) - 1);
        strncpy(c->response, (const char *)sqlite3_column_text(stmt, 3), sizeof(c->response) - 1);
        strncpy(c->created_by, (const char *)sqlite3_column_text(stmt, 4), sizeof(c->created_by) - 1);
        c->use_count = sqlite3_column_int(stmt, 5);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_increment_command_use(himiko_database_t *db, const char *guild_id, const char *name) {
    const char *sql = "UPDATE custom_commands SET use_count = use_count + 1 WHERE guild_id = ? AND name = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Spam filter */
int db_get_spam_filter_config(himiko_database_t *db, const char *guild_id, spam_filter_config_t *config) {
    const char *sql = "SELECT guild_id, enabled, max_mentions, max_links, max_emojis, action "
                      "FROM spam_filter_config WHERE guild_id = ?";
    sqlite3_stmt *stmt;

    memset(config, 0, sizeof(spam_filter_config_t));
    strncpy(config->guild_id, guild_id, sizeof(config->guild_id) - 1);
    config->max_mentions = 5;
    config->max_links = 3;
    config->max_emojis = 10;
    strcpy(config->action, "delete");

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        config->enabled = sqlite3_column_int(stmt, 1);
        config->max_mentions = sqlite3_column_int(stmt, 2);
        config->max_links = sqlite3_column_int(stmt, 3);
        config->max_emojis = sqlite3_column_int(stmt, 4);
        const char *val = (const char *)sqlite3_column_text(stmt, 5);
        if (val) strncpy(config->action, val, sizeof(config->action) - 1);
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_set_spam_filter_config(himiko_database_t *db, const spam_filter_config_t *config) {
    const char *sql = "INSERT INTO spam_filter_config (guild_id, enabled, max_mentions, max_links, max_emojis, action) "
                      "VALUES (?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(guild_id) DO UPDATE SET "
                      "enabled = excluded.enabled, max_mentions = excluded.max_mentions, "
                      "max_links = excluded.max_links, max_emojis = excluded.max_emojis, action = excluded.action";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, config->guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, config->enabled);
    sqlite3_bind_int(stmt, 3, config->max_mentions);
    sqlite3_bind_int(stmt, 4, config->max_links);
    sqlite3_bind_int(stmt, 5, config->max_emojis);
    sqlite3_bind_text(stmt, 6, config->action, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_set_antiraid_config(himiko_database_t *db, const antiraid_config_t *config) {
    const char *sql = "INSERT INTO antiraid_config (guild_id, enabled, raid_time, raid_size, auto_silence, "
                      "lockdown_duration, silent_role_id, alert_role_id, log_channel_id, action) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(guild_id) DO UPDATE SET "
                      "enabled = excluded.enabled, raid_time = excluded.raid_time, raid_size = excluded.raid_size, "
                      "auto_silence = excluded.auto_silence, lockdown_duration = excluded.lockdown_duration, "
                      "silent_role_id = excluded.silent_role_id, alert_role_id = excluded.alert_role_id, "
                      "log_channel_id = excluded.log_channel_id, action = excluded.action";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, config->guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, config->enabled);
    sqlite3_bind_int(stmt, 3, config->raid_time);
    sqlite3_bind_int(stmt, 4, config->raid_size);
    sqlite3_bind_int(stmt, 5, config->auto_silence);
    sqlite3_bind_int(stmt, 6, config->lockdown_duration);
    sqlite3_bind_text(stmt, 7, config->silent_role_id[0] ? config->silent_role_id : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, config->alert_role_id[0] ? config->alert_role_id : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, config->log_channel_id[0] ? config->log_channel_id : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, config->action, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_set_antispam_config(himiko_database_t *db, const antispam_config_t *config) {
    const char *sql = "INSERT INTO antispam_config (guild_id, enabled, base_pressure, image_pressure, link_pressure, "
                      "ping_pressure, length_pressure, line_pressure, repeat_pressure, max_pressure, pressure_decay, "
                      "action, silent_role_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(guild_id) DO UPDATE SET "
                      "enabled = excluded.enabled, base_pressure = excluded.base_pressure, "
                      "image_pressure = excluded.image_pressure, link_pressure = excluded.link_pressure, "
                      "ping_pressure = excluded.ping_pressure, length_pressure = excluded.length_pressure, "
                      "line_pressure = excluded.line_pressure, repeat_pressure = excluded.repeat_pressure, "
                      "max_pressure = excluded.max_pressure, pressure_decay = excluded.pressure_decay, "
                      "action = excluded.action, silent_role_id = excluded.silent_role_id";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, config->guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, config->enabled);
    sqlite3_bind_double(stmt, 3, config->base_pressure);
    sqlite3_bind_double(stmt, 4, config->image_pressure);
    sqlite3_bind_double(stmt, 5, config->link_pressure);
    sqlite3_bind_double(stmt, 6, config->ping_pressure);
    sqlite3_bind_double(stmt, 7, config->length_pressure);
    sqlite3_bind_double(stmt, 8, config->line_pressure);
    sqlite3_bind_double(stmt, 9, config->repeat_pressure);
    sqlite3_bind_double(stmt, 10, config->max_pressure);
    sqlite3_bind_double(stmt, 11, config->pressure_decay);
    sqlite3_bind_text(stmt, 12, config->action, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, config->silent_role_id[0] ? config->silent_role_id : NULL, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_set_logging_config(himiko_database_t *db, const logging_config_t *config) {
    const char *sql = "INSERT INTO logging_config (guild_id, log_channel_id, enabled, message_delete, message_edit, "
                      "voice_join, voice_leave, nickname_change, avatar_change, presence_change, presence_batch_mins) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(guild_id) DO UPDATE SET "
                      "log_channel_id = excluded.log_channel_id, enabled = excluded.enabled, "
                      "message_delete = excluded.message_delete, message_edit = excluded.message_edit, "
                      "voice_join = excluded.voice_join, voice_leave = excluded.voice_leave, "
                      "nickname_change = excluded.nickname_change, avatar_change = excluded.avatar_change, "
                      "presence_change = excluded.presence_change, presence_batch_mins = excluded.presence_batch_mins";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, config->guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, config->log_channel_id[0] ? config->log_channel_id : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, config->enabled);
    sqlite3_bind_int(stmt, 4, config->message_delete);
    sqlite3_bind_int(stmt, 5, config->message_edit);
    sqlite3_bind_int(stmt, 6, config->voice_join);
    sqlite3_bind_int(stmt, 7, config->voice_leave);
    sqlite3_bind_int(stmt, 8, config->nickname_change);
    sqlite3_bind_int(stmt, 9, config->avatar_change);
    sqlite3_bind_int(stmt, 10, config->presence_change);
    sqlite3_bind_int(stmt, 11, config->presence_batch_mins);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Disabled log channels */
int db_is_log_channel_disabled(himiko_database_t *db, const char *guild_id, const char *channel_id, int *disabled) {
    const char *sql = "SELECT COUNT(*) FROM disabled_log_channels WHERE guild_id = ? AND channel_id = ?";
    sqlite3_stmt *stmt;
    *disabled = 0;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *disabled = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_add_disabled_log_channel(himiko_database_t *db, const char *guild_id, const char *channel_id) {
    const char *sql = "INSERT OR IGNORE INTO disabled_log_channels (guild_id, channel_id) VALUES (?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_remove_disabled_log_channel(himiko_database_t *db, const char *guild_id, const char *channel_id) {
    const char *sql = "DELETE FROM disabled_log_channels WHERE guild_id = ? AND channel_id = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, channel_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_bot_bans(himiko_database_t *db, const char *ban_type, bot_ban_t *bans, int max_bans, int *count) {
    const char *sql_all = "SELECT id, target_id, ban_type, reason, banned_by, created_at FROM bot_bans ORDER BY created_at DESC";
    const char *sql_type = "SELECT id, target_id, ban_type, reason, banned_by, created_at FROM bot_bans WHERE ban_type = ? ORDER BY created_at DESC";
    sqlite3_stmt *stmt;
    *count = 0;

    const char *sql = (ban_type && ban_type[0]) ? sql_type : sql_all;

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    if (ban_type && ban_type[0]) {
        sqlite3_bind_text(stmt, 1, ban_type, -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_bans) {
        bot_ban_t *b = &bans[*count];
        b->id = sqlite3_column_int64(stmt, 0);
        strncpy(b->target_id, (const char *)sqlite3_column_text(stmt, 1), sizeof(b->target_id) - 1);
        strncpy(b->ban_type, (const char *)sqlite3_column_text(stmt, 2), sizeof(b->ban_type) - 1);
        const char *reason = (const char *)sqlite3_column_text(stmt, 3);
        if (reason) strncpy(b->reason, reason, sizeof(b->reason) - 1);
        strncpy(b->banned_by, (const char *)sqlite3_column_text(stmt, 4), sizeof(b->banned_by) - 1);
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}
