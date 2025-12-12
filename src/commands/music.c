/*
 * Himiko Discord Bot (C Edition) - Music Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/music.h"
#include "audio/discord_voice_internal.h"
#include "bot.h"
#include "database.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#ifdef HAVE_OPUS
#include <opus/opus.h>
#endif

/* Global music state */
music_state_t g_music = {0};

/* Helper to format duration as MM:SS or HH:MM:SS */
static void format_duration(int seconds, char *buf, size_t buf_size) {
    if (seconds < 0) seconds = 0;
    int hours = seconds / 3600;
    int mins = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (hours > 0) {
        snprintf(buf, buf_size, "%d:%02d:%02d", hours, mins, secs);
    } else {
        snprintf(buf, buf_size, "%d:%02d", mins, secs);
    }
}

/* Initialize music system */
int music_init(void) {
    if (g_music.initialized) return 0;

    pthread_mutex_init(&g_music.lock, NULL);
    g_music.players = NULL;
    g_music.initialized = true;

    DEBUG_LOG("Music system initialized");
    return 0;
}

/* Cleanup music system */
void music_cleanup(void) {
    if (!g_music.initialized) return;

    pthread_mutex_lock(&g_music.lock);

    /* Destroy all players */
    music_player_t *player = g_music.players;
    while (player) {
        music_player_t *next = (music_player_t *)player->next;
        music_destroy_player(player);
        player = next;
    }
    g_music.players = NULL;

    pthread_mutex_unlock(&g_music.lock);
    pthread_mutex_destroy(&g_music.lock);

    g_music.initialized = false;
    DEBUG_LOG("Music system cleaned up");
}

/* Get player for guild */
music_player_t *music_get_player(u64snowflake guild_id) {
    pthread_mutex_lock(&g_music.lock);

    music_player_t *player = g_music.players;
    while (player) {
        if (player->guild_id == guild_id) {
            pthread_mutex_unlock(&g_music.lock);
            return player;
        }
        player = (music_player_t *)player->next;
    }

    pthread_mutex_unlock(&g_music.lock);
    return NULL;
}

/* Create new player for guild */
music_player_t *music_create_player(u64snowflake guild_id) {
    music_player_t *existing = music_get_player(guild_id);
    if (existing) return existing;

    music_player_t *player = calloc(1, sizeof(music_player_t));
    if (!player) return NULL;

    player->guild_id = guild_id;
    player->volume = 100;
    player->state = PLAYER_STATE_IDLE;
    player->voice_state = VOICE_STATE_DISCONNECTED;

    pthread_mutex_init(&player->lock, NULL);

    /* Initialize voice UDP */
    voice_udp_init(&player->udp);

    /* Initialize audio stream */
    if (audio_stream_init(&player->audio) != 0) {
        DEBUG_LOG("Failed to initialize audio stream");
        pthread_mutex_destroy(&player->lock);
        free(player);
        return NULL;
    }

    /* Set volume on audio stream */
    audio_stream_set_volume(&player->audio, player->volume);

    /* Add to global list */
    pthread_mutex_lock(&g_music.lock);
    player->next = g_music.players;
    g_music.players = player;
    pthread_mutex_unlock(&g_music.lock);

    DEBUG_LOG("Created music player for guild %"PRIu64, guild_id);
    return player;
}

/* Destroy player */
void music_destroy_player(music_player_t *player) {
    if (!player) return;

    /* Stop audio streaming */
    audio_stream_stop(&player->audio);

    /* Cleanup audio stream */
    audio_stream_cleanup(&player->audio);

    /* Close UDP connection */
    voice_udp_close(&player->udp);

    pthread_mutex_lock(&player->lock);

    /* Free current track */
    if (player->current_track) {
        free(player->current_track);
        player->current_track = NULL;
    }

    pthread_mutex_unlock(&player->lock);
    pthread_mutex_destroy(&player->lock);

    /* Remove from global list */
    pthread_mutex_lock(&g_music.lock);
    if (g_music.players == player) {
        g_music.players = (music_player_t *)player->next;
    } else {
        music_player_t *prev = g_music.players;
        while (prev && prev->next != player) {
            prev = (music_player_t *)prev->next;
        }
        if (prev) {
            prev->next = player->next;
        }
    }
    pthread_mutex_unlock(&g_music.lock);

    DEBUG_LOG("Destroyed music player for guild %"PRIu64, player->guild_id);
    free(player);
}

/* Voice join */
int music_voice_join(struct discord *client, u64snowflake guild_id,
                     u64snowflake channel_id, u64snowflake text_channel_id) {
    music_player_t *player = music_get_player(guild_id);
    if (!player) {
        player = music_create_player(guild_id);
        if (!player) return -1;
    }

    pthread_mutex_lock(&player->lock);
    player->voice_channel_id = channel_id;
    player->text_channel_id = text_channel_id;
    player->voice_state = VOICE_STATE_CONNECTING;
    pthread_mutex_unlock(&player->lock);

    /* Use Concord's voice join */
    enum discord_voice_status status = discord_voice_join(client, guild_id,
                                                           channel_id, false, false);

    if (status != DISCORD_VOICE_JOINED && status != DISCORD_VOICE_ALREADY_JOINED) {
        DEBUG_LOG("Failed to join voice channel: status=%d", status);
        pthread_mutex_lock(&player->lock);
        player->voice_state = VOICE_STATE_DISCONNECTED;
        pthread_mutex_unlock(&player->lock);
        return -1;
    }

    return 0;
}

/* Voice leave */
int music_voice_leave(struct discord *client, u64snowflake guild_id) {
    music_player_t *player = music_get_player(guild_id);
    if (!player) return -1;

    /* Stop playback first */
    music_stop(player);

    /* Update voice state to leave */
    struct discord_update_voice_state params = {
        .guild_id = guild_id,
        .channel_id = 0,  /* NULL to disconnect */
        .self_mute = false,
        .self_deaf = false
    };
    discord_update_voice_state(client, &params);

    pthread_mutex_lock(&player->lock);
    player->voice_state = VOICE_STATE_DISCONNECTED;
    player->voice_channel_id = 0;
    pthread_mutex_unlock(&player->lock);

    return 0;
}

/* Handle voice state update events */
void music_on_voice_state_update(struct discord *client,
                                  const struct discord_voice_state *vs) {
    (void)client;

    if (!vs || !vs->guild_id) return;

    music_player_t *player = music_get_player(vs->guild_id);
    if (!player) return;

    /* Check if it's our bot's voice state */
    const struct discord_user *self = discord_get_self(g_bot->client);
    if (self && vs->user_id == self->id) {
        pthread_mutex_lock(&player->lock);

        if (vs->session_id) {
            strncpy(player->voice_info.session_id, vs->session_id,
                    sizeof(player->voice_info.session_id) - 1);
        }

        if (vs->channel_id) {
            player->voice_channel_id = vs->channel_id;
            player->voice_info.channel_id = vs->channel_id;
        } else {
            /* Disconnected */
            player->voice_state = VOICE_STATE_DISCONNECTED;
            player->voice_channel_id = 0;
        }

        pthread_mutex_unlock(&player->lock);
    }
}

/* Handle voice server update events */
void music_on_voice_server_update(struct discord *client,
                                   const struct discord_voice_server_update *vsu) {
    (void)client;

    if (!vsu || !vsu->guild_id) return;

    music_player_t *player = music_get_player(vsu->guild_id);
    if (!player) return;

    pthread_mutex_lock(&player->lock);

    if (vsu->token) {
        strncpy(player->voice_info.token, vsu->token,
                sizeof(player->voice_info.token) - 1);
    }

    if (vsu->endpoint) {
        strncpy(player->voice_info.endpoint, vsu->endpoint,
                sizeof(player->voice_info.endpoint) - 1);
    }

    player->voice_info.guild_id = vsu->guild_id;
    player->voice_state = VOICE_STATE_CONNECTED;

    pthread_mutex_unlock(&player->lock);

    DEBUG_LOG("Voice server update for guild %"PRIu64": endpoint=%s",
              vsu->guild_id, vsu->endpoint ? vsu->endpoint : "null");
}

/* Concord voice ready callback - called when voice WebSocket is ready */
void music_on_voice_ready(struct discord_voice *vc) {
    if (!vc) return;

#ifdef CCORD_VOICE
    /* Access internal structure to get UDP service info */
    struct discord_voice_internal *vci = voice_get_internal(vc);

    DEBUG_LOG("Voice ready callback: guild=%"PRIu64" ssrc=%d ip=%s port=%d",
              vci->guild_id, vci->udp_service.ssrc,
              vci->udp_service.server_ip, vci->udp_service.server_port);

    /* Find the player for this guild */
    music_player_t *player = music_get_player(vci->guild_id);
    if (!player) {
        DEBUG_LOG("No player found for guild %"PRIu64, vci->guild_id);
        return;
    }

    pthread_mutex_lock(&player->lock);

    /* Store voice connection pointer */
    player->voice_connection = vc;

    /* Connect UDP to voice server */
    if (vci->udp_service.server_ip[0] && vci->udp_service.server_port > 0) {
        int ret = voice_udp_connect(&player->udp,
                                    vci->udp_service.server_ip,
                                    (uint16_t)vci->udp_service.server_port,
                                    (uint32_t)vci->udp_service.ssrc);
        if (ret == 0) {
            /* Perform IP discovery */
            if (voice_udp_discover_ip(&player->udp) == 0) {
                DEBUG_LOG("IP discovery successful: %s:%u",
                          voice_udp_get_local_ip(&player->udp),
                          voice_udp_get_local_port(&player->udp));
            } else {
                DEBUG_LOG("IP discovery failed");
            }
        } else {
            DEBUG_LOG("UDP connect failed");
        }
    }

    player->voice_state = VOICE_STATE_READY;

    /* Link audio stream to voice connection */
    audio_stream_set_voice(&player->audio, vc);

    pthread_mutex_unlock(&player->lock);

    DEBUG_LOG("Voice connection ready for guild %"PRIu64, vci->guild_id);
#else
    (void)vc;
#endif
}

/* Concord voice session descriptor callback - called when encryption key is received */
void music_on_voice_session_descriptor(struct discord_voice *vc) {
    if (!vc) return;

#ifdef CCORD_VOICE
    /* Access internal structure to get encryption key */
    struct discord_voice_internal *vci = voice_get_internal(vc);

    DEBUG_LOG("Voice session descriptor callback: guild=%"PRIu64, vci->guild_id);

    /* Find the player for this guild */
    music_player_t *player = music_get_player(vci->guild_id);
    if (!player) {
        DEBUG_LOG("No player found for guild %"PRIu64, vci->guild_id);
        return;
    }

    pthread_mutex_lock(&player->lock);

    /* Set the encryption key from unique_key (first 32 bytes) */
    voice_udp_set_secret_key(&player->udp, (const uint8_t *)vci->udp_service.unique_key);

    DEBUG_LOG("Encryption key set for guild %"PRIu64, vci->guild_id);

    /* Link UDP to audio stream */
    audio_stream_set_udp(&player->audio, &player->udp);

    /* Check if we should start playback */
    bool should_start = (player->state == PLAYER_STATE_LOADING && player->current_track);

    pthread_mutex_unlock(&player->lock);

    /* Start playback if we were waiting */
    if (should_start) {
        DEBUG_LOG("Starting playback after session ready");
        music_start_playback(player);
    }

    DEBUG_LOG("Voice session ready for guild %"PRIu64, vci->guild_id);
#else
    (void)vc;
#endif
}

/* Track end callback */
static void on_track_end(void *user_data) {
    music_player_t *player = (music_player_t *)user_data;
    if (!player) return;

    DEBUG_LOG("Track ended for guild %"PRIu64, player->guild_id);

    /* Free current track */
    pthread_mutex_lock(&player->lock);
    if (player->current_track) {
        free(player->current_track);
        player->current_track = NULL;
    }
    player->state = PLAYER_STATE_IDLE;
    pthread_mutex_unlock(&player->lock);

    /* Check for loop or next track */
    if (player->loop_track && player->current_track) {
        /* Loop current track */
        music_start_playback(player);
    } else {
        /* Try to play next track */
        char guild_str[32];
        snprintf(guild_str, sizeof(guild_str), "%"PRIu64, player->guild_id);

        music_track_t *next = music_queue_next(guild_str);
        if (next) {
            pthread_mutex_lock(&player->lock);
            player->current_track = next;
            pthread_mutex_unlock(&player->lock);
            music_start_playback(player);
        } else if (player->loop_queue) {
            /* TODO: Reload queue for loop */
            DEBUG_LOG("Queue ended, loop_queue not fully implemented");
        }
    }
}

/* Start playback of current track */
int music_start_playback(music_player_t *player) {
    if (!player || !player->current_track) return -1;

    /* Get stream URL */
    char *stream_url = music_get_stream_url(player->current_track);
    if (!stream_url) {
        DEBUG_LOG("Failed to get stream URL for track");
        return -1;
    }

    /* Set callback for track end */
    audio_stream_set_callback(&player->audio, on_track_end, player);

    /* Start audio stream */
    int ret = audio_stream_play(&player->audio, stream_url);
    free(stream_url);

    if (ret != 0) {
        DEBUG_LOG("Failed to start audio playback");
        return -1;
    }

    pthread_mutex_lock(&player->lock);
    player->state = PLAYER_STATE_PLAYING;
    pthread_mutex_unlock(&player->lock);

    DEBUG_LOG("Started playback: %s", player->current_track->title);
    return 0;
}

/* Queue management - add track */
int music_queue_add(const char *guild_id, const music_track_t *track) {
    if (!g_bot || !g_bot->database.db || !guild_id || !track) return -1;

    const char *sql =
        "INSERT INTO music_queue (guild_id, channel_id, user_id, title, url, "
        "duration, thumbnail, is_local, position, added_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, "
        "(SELECT COALESCE(MAX(position), 0) + 1 FROM music_queue WHERE guild_id = ?), "
        "datetime('now'))";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, track->channel_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, track->user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, track->title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, track->url, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, track->duration);
    sqlite3_bind_text(stmt, 7, track->thumbnail, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 8, track->is_local ? 1 : 0);
    sqlite3_bind_text(stmt, 9, guild_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Queue management - remove track */
int music_queue_remove(const char *guild_id, int position) {
    if (!g_bot || !g_bot->database.db || !guild_id) return -1;

    const char *sql = "DELETE FROM music_queue WHERE guild_id = ? AND position = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, position);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -1;

    /* Reorder positions */
    const char *reorder =
        "UPDATE music_queue SET position = position - 1 "
        "WHERE guild_id = ? AND position > ?";

    if (sqlite3_prepare_v2(g_bot->database.db, reorder, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, position);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return 0;
}

/* Queue management - clear */
int music_queue_clear(const char *guild_id) {
    if (!g_bot || !g_bot->database.db || !guild_id) return -1;

    const char *sql = "DELETE FROM music_queue WHERE guild_id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Queue management - get queue */
music_track_t *music_queue_get(const char *guild_id, int *count) {
    if (!g_bot || !g_bot->database.db || !guild_id) return NULL;

    *count = 0;

    /* First get count */
    const char *count_sql = "SELECT COUNT(*) FROM music_queue WHERE guild_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, count_sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (total == 0) return NULL;

    /* Get tracks */
    music_track_t *tracks = calloc(total, sizeof(music_track_t));
    if (!tracks) return NULL;

    const char *sql =
        "SELECT id, guild_id, channel_id, user_id, title, url, duration, "
        "thumbnail, is_local, position, added_at FROM music_queue "
        "WHERE guild_id = ? ORDER BY position ASC";

    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(tracks);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < total) {
        tracks[i].id = sqlite3_column_int(stmt, 0);

        const char *gid = (const char *)sqlite3_column_text(stmt, 1);
        if (gid) strncpy(tracks[i].guild_id, gid, sizeof(tracks[i].guild_id) - 1);

        const char *cid = (const char *)sqlite3_column_text(stmt, 2);
        if (cid) strncpy(tracks[i].channel_id, cid, sizeof(tracks[i].channel_id) - 1);

        const char *uid = (const char *)sqlite3_column_text(stmt, 3);
        if (uid) strncpy(tracks[i].user_id, uid, sizeof(tracks[i].user_id) - 1);

        const char *title = (const char *)sqlite3_column_text(stmt, 4);
        if (title) strncpy(tracks[i].title, title, sizeof(tracks[i].title) - 1);

        const char *url = (const char *)sqlite3_column_text(stmt, 5);
        if (url) strncpy(tracks[i].url, url, sizeof(tracks[i].url) - 1);

        tracks[i].duration = sqlite3_column_int(stmt, 6);

        const char *thumb = (const char *)sqlite3_column_text(stmt, 7);
        if (thumb) strncpy(tracks[i].thumbnail, thumb, sizeof(tracks[i].thumbnail) - 1);

        tracks[i].is_local = sqlite3_column_int(stmt, 8) != 0;
        tracks[i].position = sqlite3_column_int(stmt, 9);

        i++;
    }
    sqlite3_finalize(stmt);

    *count = i;
    return tracks;
}

/* Queue management - get next track */
music_track_t *music_queue_next(const char *guild_id) {
    if (!g_bot || !g_bot->database.db || !guild_id) return NULL;

    const char *sql =
        "SELECT id, guild_id, channel_id, user_id, title, url, duration, "
        "thumbnail, is_local, position FROM music_queue "
        "WHERE guild_id = ? ORDER BY position ASC LIMIT 1";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    music_track_t *track = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        track = calloc(1, sizeof(music_track_t));
        if (track) {
            track->id = sqlite3_column_int(stmt, 0);

            const char *gid = (const char *)sqlite3_column_text(stmt, 1);
            if (gid) strncpy(track->guild_id, gid, sizeof(track->guild_id) - 1);

            const char *cid = (const char *)sqlite3_column_text(stmt, 2);
            if (cid) strncpy(track->channel_id, cid, sizeof(track->channel_id) - 1);

            const char *uid = (const char *)sqlite3_column_text(stmt, 3);
            if (uid) strncpy(track->user_id, uid, sizeof(track->user_id) - 1);

            const char *title = (const char *)sqlite3_column_text(stmt, 4);
            if (title) strncpy(track->title, title, sizeof(track->title) - 1);

            const char *url = (const char *)sqlite3_column_text(stmt, 5);
            if (url) strncpy(track->url, url, sizeof(track->url) - 1);

            track->duration = sqlite3_column_int(stmt, 6);

            const char *thumb = (const char *)sqlite3_column_text(stmt, 7);
            if (thumb) strncpy(track->thumbnail, thumb, sizeof(track->thumbnail) - 1);

            track->is_local = sqlite3_column_int(stmt, 8) != 0;
            track->position = sqlite3_column_int(stmt, 9);
        }
    }
    sqlite3_finalize(stmt);

    return track;
}

/* Queue shuffle */
int music_queue_shuffle(const char *guild_id) {
    if (!g_bot || !g_bot->database.db || !guild_id) return -1;

    /* Get all track IDs */
    const char *sql = "SELECT id FROM music_queue WHERE guild_id = ? ORDER BY position";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    int ids[MUSIC_MAX_QUEUE_SIZE];
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < MUSIC_MAX_QUEUE_SIZE) {
        ids[count++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count < 2) return 0;  /* Nothing to shuffle */

    /* Fisher-Yates shuffle */
    srand((unsigned int)time(NULL));
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = ids[i];
        ids[i] = ids[j];
        ids[j] = tmp;
    }

    /* Update positions */
    const char *update = "UPDATE music_queue SET position = ? WHERE id = ?";
    for (int i = 0; i < count; i++) {
        if (sqlite3_prepare_v2(g_bot->database.db, update, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, i + 1);
            sqlite3_bind_int(stmt, 2, ids[i]);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    return 0;
}

/* Resolve track from query (URL or search) */
int music_resolve_track(const char *query, music_track_t *track) {
    if (!query || !track) return -1;

    memset(track, 0, sizeof(music_track_t));

    /* Check if it's a URL */
    if (strstr(query, "youtube.com") || strstr(query, "youtu.be")) {
        track->source = TRACK_SOURCE_YOUTUBE;
        strncpy(track->url, query, sizeof(track->url) - 1);
    } else if (strstr(query, "soundcloud.com")) {
        track->source = TRACK_SOURCE_SOUNDCLOUD;
        strncpy(track->url, query, sizeof(track->url) - 1);
    } else if (strncmp(query, "http://", 7) == 0 || strncmp(query, "https://", 8) == 0) {
        track->source = TRACK_SOURCE_DIRECT_URL;
        strncpy(track->url, query, sizeof(track->url) - 1);
    } else {
        /* Search query */
        track->source = TRACK_SOURCE_SEARCH;
        snprintf(track->url, sizeof(track->url), "ytsearch:%s", query);
    }

    /* Use yt-dlp to get metadata */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "yt-dlp --no-download --print title --print duration --print thumbnail "
             "--print webpage_url \"%s\" 2>/dev/null | head -4",
             track->url);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        strncpy(track->title, query, sizeof(track->title) - 1);
        return 0;
    }

    char line[512];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp) && line_num < 4) {
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        switch (line_num) {
            case 0:  /* Title */
                strncpy(track->title, line, sizeof(track->title) - 1);
                break;
            case 1:  /* Duration */
                track->duration = atoi(line);
                break;
            case 2:  /* Thumbnail */
                strncpy(track->thumbnail, line, sizeof(track->thumbnail) - 1);
                break;
            case 3:  /* URL */
                strncpy(track->url, line, sizeof(track->url) - 1);
                break;
        }
        line_num++;
    }

    pclose(fp);

    /* Fallback title */
    if (track->title[0] == '\0') {
        strncpy(track->title, query, sizeof(track->title) - 1);
    }

    return 0;
}

/* Get stream URL using yt-dlp */
char *music_get_stream_url(const music_track_t *track) {
    if (!track) return NULL;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "yt-dlp -f bestaudio --get-url \"%s\" 2>/dev/null | head -1",
             track->url);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char *url = malloc(2048);
    if (!url) {
        pclose(fp);
        return NULL;
    }

    if (!fgets(url, 2048, fp)) {
        free(url);
        pclose(fp);
        return NULL;
    }

    /* Remove newline */
    size_t len = strlen(url);
    if (len > 0 && url[len-1] == '\n') url[len-1] = '\0';

    pclose(fp);

    return url;
}

/* Playback control - play */
int music_play(music_player_t *player, const char *query, u64snowflake user_id) {
    if (!player || !query) return -1;

    /* Resolve track */
    music_track_t track;
    if (music_resolve_track(query, &track) != 0) {
        return -1;
    }

    /* Set user info */
    char guild_str[32], user_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, player->guild_id);
    snprintf(user_str, sizeof(user_str), "%"PRIu64, user_id);

    strncpy(track.guild_id, guild_str, sizeof(track.guild_id) - 1);
    strncpy(track.user_id, user_str, sizeof(track.user_id) - 1);

    /* Add to queue */
    if (music_queue_add(guild_str, &track) != 0) {
        return -1;
    }

    /* If not playing, start playback */
    pthread_mutex_lock(&player->lock);
    if (player->state == PLAYER_STATE_IDLE && player->udp.ready) {
        /* Get next track from queue */
        music_track_t *next = music_queue_next(guild_str);
        if (next) {
            player->current_track = next;
            player->state = PLAYER_STATE_LOADING;
            pthread_mutex_unlock(&player->lock);
            music_start_playback(player);
        } else {
            pthread_mutex_unlock(&player->lock);
        }
    } else if (player->state == PLAYER_STATE_IDLE) {
        /* Not ready yet, mark as loading so it starts when ready */
        player->state = PLAYER_STATE_LOADING;
        pthread_mutex_unlock(&player->lock);
    } else {
        pthread_mutex_unlock(&player->lock);
    }

    return 0;
}

/* Playback control - skip */
int music_skip(music_player_t *player) {
    if (!player) return -1;

    /* Stop current audio stream - this will trigger on_track_end callback */
    audio_stream_stop(&player->audio);

    /* Remove current track from queue */
    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, player->guild_id);

    pthread_mutex_lock(&player->lock);
    if (player->current_track) {
        music_queue_remove(guild_str, player->current_track->position);
        free(player->current_track);
        player->current_track = NULL;
    }
    player->state = PLAYER_STATE_IDLE;
    pthread_mutex_unlock(&player->lock);

    /* Start next track */
    music_track_t *next = music_queue_next(guild_str);
    if (next && player->udp.ready) {
        pthread_mutex_lock(&player->lock);
        player->current_track = next;
        pthread_mutex_unlock(&player->lock);
        music_start_playback(player);
    }

    return 0;
}

/* Playback control - stop */
int music_stop(music_player_t *player) {
    if (!player) return -1;

    /* Stop audio stream */
    audio_stream_stop(&player->audio);

    pthread_mutex_lock(&player->lock);

    /* Free current track */
    if (player->current_track) {
        free(player->current_track);
        player->current_track = NULL;
    }

    player->state = PLAYER_STATE_IDLE;

    pthread_mutex_unlock(&player->lock);

    /* Clear queue */
    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, player->guild_id);
    music_queue_clear(guild_str);

    return 0;
}

/* Playback control - pause */
int music_pause(music_player_t *player) {
    if (!player) return -1;

    audio_stream_pause(&player->audio);

    pthread_mutex_lock(&player->lock);
    if (player->state == PLAYER_STATE_PLAYING) {
        player->state = PLAYER_STATE_PAUSED;
    }
    pthread_mutex_unlock(&player->lock);

    return 0;
}

/* Playback control - resume */
int music_resume(music_player_t *player) {
    if (!player) return -1;

    audio_stream_resume(&player->audio);

    pthread_mutex_lock(&player->lock);
    if (player->state == PLAYER_STATE_PAUSED) {
        player->state = PLAYER_STATE_PLAYING;
    }
    pthread_mutex_unlock(&player->lock);

    return 0;
}

/* Playback control - set volume */
int music_set_volume(music_player_t *player, int volume) {
    if (!player) return -1;

    if (volume < 0) volume = 0;
    if (volume > 200) volume = 200;

    audio_stream_set_volume(&player->audio, volume);

    pthread_mutex_lock(&player->lock);
    player->volume = volume;
    pthread_mutex_unlock(&player->lock);

    return 0;
}

/* Playback control - seek (stub) */
int music_seek(music_player_t *player, int position) {
    (void)player;
    (void)position;
    /* Not implemented - requires FFmpeg seek support */
    return -1;
}

/* Queue management - move track */
int music_queue_move(const char *guild_id, int from, int to) {
    (void)guild_id;
    (void)from;
    (void)to;
    /* Not implemented */
    return -1;
}

/* Search YouTube (stub) */
int music_search_youtube(const char *query, music_track_t *results, int max_results) {
    (void)query;
    (void)results;
    (void)max_results;
    /* Not implemented - use music_resolve_track with ytsearch: prefix instead */
    return 0;
}

/* Get music settings */
int music_get_settings(const char *guild_id, music_settings_t *settings) {
    if (!g_bot || !g_bot->database.db || !guild_id || !settings) return -1;

    memset(settings, 0, sizeof(music_settings_t));
    strncpy(settings->guild_id, guild_id, sizeof(settings->guild_id) - 1);
    settings->default_volume = 100;

    const char *sql =
        "SELECT dj_role_id, mod_role_id, volume, music_folder FROM music_settings "
        "WHERE guild_id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *dj = (const char *)sqlite3_column_text(stmt, 0);
        if (dj) strncpy(settings->dj_role_id, dj, sizeof(settings->dj_role_id) - 1);

        const char *mod = (const char *)sqlite3_column_text(stmt, 1);
        if (mod) strncpy(settings->mod_role_id, mod, sizeof(settings->mod_role_id) - 1);

        settings->default_volume = sqlite3_column_int(stmt, 2);

        const char *folder = (const char *)sqlite3_column_text(stmt, 3);
        if (folder) strncpy(settings->music_folder, folder, sizeof(settings->music_folder) - 1);
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Set music settings */
int music_set_settings(const char *guild_id, const music_settings_t *settings) {
    if (!g_bot || !g_bot->database.db || !guild_id || !settings) return -1;

    const char *sql =
        "INSERT OR REPLACE INTO music_settings "
        "(guild_id, dj_role_id, mod_role_id, volume, music_folder, updated_at) "
        "VALUES (?, ?, ?, ?, ?, datetime('now'))";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, settings->dj_role_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, settings->mod_role_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, settings->default_volume);
    sqlite3_bind_text(stmt, 5, settings->music_folder, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Check if user has DJ role */
bool music_has_dj_role(struct discord *client, u64snowflake guild_id,
                       const struct discord_guild_member *member) {
    (void)client;

    if (!member || !member->roles) return false;

    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, guild_id);

    music_settings_t settings;
    if (music_get_settings(guild_str, &settings) != 0) return false;
    if (settings.dj_role_id[0] == '\0') return true;  /* No DJ role set = everyone can use */

    u64snowflake dj_role = string_to_snowflake(settings.dj_role_id);

    for (int i = 0; i < member->roles->size; i++) {
        if (member->roles->array[i] == dj_role) return true;
    }

    return false;
}

/* Check if bot is alone in voice channel (stub) */
bool music_is_alone_in_voice(struct discord *client, u64snowflake guild_id,
                              u64snowflake channel_id) {
    (void)client;
    (void)guild_id;
    (void)channel_id;
    /* Would require querying voice states - not implemented */
    return false;
}

/* Add track to history */
int music_add_to_history(const char *guild_id, const char *user_id,
                         const music_track_t *track) {
    if (!g_bot || !g_bot->database.db || !guild_id || !user_id || !track) return -1;

    const char *sql =
        "INSERT INTO music_history (guild_id, user_id, title, url, played_at) "
        "VALUES (?, ?, ?, ?, datetime('now'))";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_bot->database.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, guild_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, track->title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, track->url, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ========== Command Handlers ========== */

void cmd_play(struct discord *client, const struct discord_interaction *interaction) {
    if (!interaction->data || !interaction->data->options) {
        respond_ephemeral(client, interaction, "Please provide a song to play!");
        return;
    }

    const char *query = NULL;
    for (int i = 0; i < interaction->data->options->size; i++) {
        if (strcmp(interaction->data->options->array[i].name, "query") == 0) {
            query = interaction->data->options->array[i].value;
            break;
        }
    }

    if (!query || !*query) {
        respond_ephemeral(client, interaction, "Please provide a song to play!");
        return;
    }

    /* Get or create player */
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player) {
        player = music_create_player(interaction->guild_id);
    }

    if (!player) {
        respond_ephemeral(client, interaction, "Failed to create music player!");
        return;
    }

    /* Check if in voice channel */
    if (player->voice_state == VOICE_STATE_DISCONNECTED) {
        respond_ephemeral(client, interaction,
            "I'm not in a voice channel! Use `/join` first.");
        return;
    }

    /* Resolve and queue track */
    music_track_t track;
    if (music_resolve_track(query, &track) != 0) {
        respond_ephemeral(client, interaction, "Failed to resolve track!");
        return;
    }

    char guild_str[32], user_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, interaction->guild_id);
    snprintf(user_str, sizeof(user_str), "%"PRIu64, interaction->member->user->id);

    strncpy(track.guild_id, guild_str, sizeof(track.guild_id) - 1);
    strncpy(track.user_id, user_str, sizeof(track.user_id) - 1);

    if (music_queue_add(guild_str, &track) != 0) {
        respond_ephemeral(client, interaction, "Failed to add track to queue!");
        return;
    }

    char duration_str[16];
    format_duration(track.duration, duration_str, sizeof(duration_str));

    char response[512];
    snprintf(response, sizeof(response),
             ":musical_note: **Added to queue:**\n"
             "**%s**\n"
             "Duration: %s",
             track.title, duration_str);

    respond_message(client, interaction, response);
}

void cmd_play_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = {
            .content = "Please provide a song to play! Usage: play <query or URL>"
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Get or create player */
    music_player_t *player = music_get_player(msg->guild_id);
    if (!player) {
        player = music_create_player(msg->guild_id);
    }

    if (!player) {
        struct discord_create_message params = { .content = "Failed to create music player!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (player->voice_state == VOICE_STATE_DISCONNECTED) {
        struct discord_create_message params = {
            .content = "I'm not in a voice channel! Use `join` first."
        };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    /* Resolve and queue */
    music_track_t track;
    if (music_resolve_track(args, &track) != 0) {
        struct discord_create_message params = { .content = "Failed to resolve track!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_str[32], user_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, msg->guild_id);
    snprintf(user_str, sizeof(user_str), "%"PRIu64, msg->author->id);

    strncpy(track.guild_id, guild_str, sizeof(track.guild_id) - 1);
    strncpy(track.user_id, user_str, sizeof(track.user_id) - 1);

    if (music_queue_add(guild_str, &track) != 0) {
        struct discord_create_message params = { .content = "Failed to add track to queue!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char duration_str[16];
    format_duration(track.duration, duration_str, sizeof(duration_str));

    char response[512];
    snprintf(response, sizeof(response),
             ":musical_note: **Added to queue:**\n"
             "**%s**\n"
             "Duration: %s",
             track.title, duration_str);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_skip(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player || player->state == PLAYER_STATE_IDLE) {
        respond_ephemeral(client, interaction, "Nothing is playing!");
        return;
    }

    music_skip(player);
    respond_message(client, interaction, ":fast_forward: Skipped!");
}

void cmd_skip_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player || player->state == PLAYER_STATE_IDLE) {
        struct discord_create_message params = { .content = "Nothing is playing!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    music_skip(player);

    struct discord_create_message params = { .content = ":fast_forward: Skipped!" };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_stop(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player) {
        respond_ephemeral(client, interaction, "No music player active!");
        return;
    }

    music_stop(player);
    respond_message(client, interaction, ":stop_button: Stopped and cleared queue!");
}

void cmd_stop_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player) {
        struct discord_create_message params = { .content = "No music player active!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    music_stop(player);

    struct discord_create_message params = { .content = ":stop_button: Stopped and cleared queue!" };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_pause(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player || player->state != PLAYER_STATE_PLAYING) {
        respond_ephemeral(client, interaction, "Nothing is playing!");
        return;
    }

    music_pause(player);
    respond_message(client, interaction, ":pause_button: Paused!");
}

void cmd_pause_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player || player->state != PLAYER_STATE_PLAYING) {
        struct discord_create_message params = { .content = "Nothing is playing!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    music_pause(player);

    struct discord_create_message params = { .content = ":pause_button: Paused!" };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_resume(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player || player->state != PLAYER_STATE_PAUSED) {
        respond_ephemeral(client, interaction, "Nothing is paused!");
        return;
    }

    music_resume(player);
    respond_message(client, interaction, ":arrow_forward: Resumed!");
}

void cmd_resume_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player || player->state != PLAYER_STATE_PAUSED) {
        struct discord_create_message params = { .content = "Nothing is paused!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    music_resume(player);

    struct discord_create_message params = { .content = ":arrow_forward: Resumed!" };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_queue(struct discord *client, const struct discord_interaction *interaction) {
    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, interaction->guild_id);

    int count = 0;
    music_track_t *tracks = music_queue_get(guild_str, &count);

    if (!tracks || count == 0) {
        respond_ephemeral(client, interaction, "The queue is empty!");
        if (tracks) free(tracks);
        return;
    }

    char response[2000];
    int offset = snprintf(response, sizeof(response), ":musical_note: **Queue (%d tracks):**\n\n", count);

    int max_show = count > 10 ? 10 : count;
    for (int i = 0; i < max_show && offset < (int)sizeof(response) - 100; i++) {
        char duration_str[16];
        format_duration(tracks[i].duration, duration_str, sizeof(duration_str));

        offset += snprintf(response + offset, sizeof(response) - offset,
                          "**%d.** %s [%s]\n",
                          i + 1, tracks[i].title, duration_str);
    }

    if (count > 10) {
        snprintf(response + offset, sizeof(response) - offset,
                "\n*...and %d more tracks*", count - 10);
    }

    respond_message(client, interaction, response);
    free(tracks);
}

void cmd_queue_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, msg->guild_id);

    int count = 0;
    music_track_t *tracks = music_queue_get(guild_str, &count);

    if (!tracks || count == 0) {
        struct discord_create_message params = { .content = "The queue is empty!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        if (tracks) free(tracks);
        return;
    }

    char response[2000];
    int offset = snprintf(response, sizeof(response), ":musical_note: **Queue (%d tracks):**\n\n", count);

    int max_show = count > 10 ? 10 : count;
    for (int i = 0; i < max_show && offset < (int)sizeof(response) - 100; i++) {
        char duration_str[16];
        format_duration(tracks[i].duration, duration_str, sizeof(duration_str));

        offset += snprintf(response + offset, sizeof(response) - offset,
                          "**%d.** %s [%s]\n",
                          i + 1, tracks[i].title, duration_str);
    }

    if (count > 10) {
        snprintf(response + offset, sizeof(response) - offset,
                "\n*...and %d more tracks*", count - 10);
    }

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
    free(tracks);
}

void cmd_nowplaying(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player || !player->current_track || player->state == PLAYER_STATE_IDLE) {
        respond_ephemeral(client, interaction, "Nothing is currently playing!");
        return;
    }

    char duration_str[16];
    format_duration(player->current_track->duration, duration_str, sizeof(duration_str));

    char response[512];
    snprintf(response, sizeof(response),
             ":musical_note: **Now Playing:**\n"
             "**%s**\n"
             "Duration: %s | Volume: %d%%",
             player->current_track->title, duration_str, player->volume);

    respond_message(client, interaction, response);
}

void cmd_nowplaying_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player || !player->current_track || player->state == PLAYER_STATE_IDLE) {
        struct discord_create_message params = { .content = "Nothing is currently playing!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char duration_str[16];
    format_duration(player->current_track->duration, duration_str, sizeof(duration_str));

    char response[512];
    snprintf(response, sizeof(response),
             ":musical_note: **Now Playing:**\n"
             "**%s**\n"
             "Duration: %s | Volume: %d%%",
             player->current_track->title, duration_str, player->volume);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_volume(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player) {
        respond_ephemeral(client, interaction, "No music player active!");
        return;
    }

    int volume = 100;
    if (interaction->data && interaction->data->options) {
        for (int i = 0; i < interaction->data->options->size; i++) {
            if (strcmp(interaction->data->options->array[i].name, "level") == 0) {
                volume = (int)strtol(interaction->data->options->array[i].value, NULL, 10);
                break;
            }
        }
    }

    music_set_volume(player, volume);

    char response[128];
    snprintf(response, sizeof(response), ":loud_sound: Volume set to **%d%%**", player->volume);
    respond_message(client, interaction, response);
}

void cmd_volume_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    music_player_t *player = music_get_player(msg->guild_id);
    if (!player) {
        struct discord_create_message params = { .content = "No music player active!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    int volume = 100;
    if (args && *args) {
        volume = atoi(args);
    }

    music_set_volume(player, volume);

    char response[128];
    snprintf(response, sizeof(response), ":loud_sound: Volume set to **%d%%**", player->volume);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_join(struct discord *client, const struct discord_interaction *interaction) {
    /* TODO: Get user's current voice channel */
    /* For now, just respond with instructions */
    respond_ephemeral(client, interaction,
        "Voice join requires knowing your current voice channel. "
        "This feature requires additional Discord gateway events to be implemented.");
}

void cmd_join_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    struct discord_create_message params = {
        .content = "Voice join requires knowing your current voice channel. "
                   "This feature requires additional Discord gateway events to be implemented."
    };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_leave(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player || player->voice_state == VOICE_STATE_DISCONNECTED) {
        respond_ephemeral(client, interaction, "I'm not in a voice channel!");
        return;
    }

    music_voice_leave(client, interaction->guild_id);
    respond_message(client, interaction, ":wave: Left the voice channel!");
}

void cmd_leave_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player || player->voice_state == VOICE_STATE_DISCONNECTED) {
        struct discord_create_message params = { .content = "I'm not in a voice channel!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    music_voice_leave(client, msg->guild_id);

    struct discord_create_message params = { .content = ":wave: Left the voice channel!" };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_shuffle(struct discord *client, const struct discord_interaction *interaction) {
    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, interaction->guild_id);

    if (music_queue_shuffle(guild_str) == 0) {
        respond_message(client, interaction, ":twisted_rightwards_arrows: Queue shuffled!");
    } else {
        respond_ephemeral(client, interaction, "Failed to shuffle queue!");
    }
}

void cmd_shuffle_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, msg->guild_id);

    if (music_queue_shuffle(guild_str) == 0) {
        struct discord_create_message params = { .content = ":twisted_rightwards_arrows: Queue shuffled!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
    } else {
        struct discord_create_message params = { .content = "Failed to shuffle queue!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void cmd_loop(struct discord *client, const struct discord_interaction *interaction) {
    music_player_t *player = music_get_player(interaction->guild_id);
    if (!player) {
        respond_ephemeral(client, interaction, "No music player active!");
        return;
    }

    pthread_mutex_lock(&player->lock);
    player->loop_track = !player->loop_track;
    bool looping = player->loop_track;
    pthread_mutex_unlock(&player->lock);

    char response[128];
    snprintf(response, sizeof(response),
             looping ? ":repeat_one: Loop enabled!" : ":repeat_one: Loop disabled!");
    respond_message(client, interaction, response);
}

void cmd_loop_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    music_player_t *player = music_get_player(msg->guild_id);
    if (!player) {
        struct discord_create_message params = { .content = "No music player active!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    pthread_mutex_lock(&player->lock);
    player->loop_track = !player->loop_track;
    bool looping = player->loop_track;
    pthread_mutex_unlock(&player->lock);

    char response[128];
    snprintf(response, sizeof(response),
             looping ? ":repeat_one: Loop enabled!" : ":repeat_one: Loop disabled!");

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_remove(struct discord *client, const struct discord_interaction *interaction) {
    int position = 1;
    if (interaction->data && interaction->data->options) {
        for (int i = 0; i < interaction->data->options->size; i++) {
            if (strcmp(interaction->data->options->array[i].name, "position") == 0) {
                position = (int)strtol(interaction->data->options->array[i].value, NULL, 10);
                break;
            }
        }
    }

    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, interaction->guild_id);

    if (music_queue_remove(guild_str, position) == 0) {
        char response[128];
        snprintf(response, sizeof(response), ":wastebasket: Removed track #%d from queue!", position);
        respond_message(client, interaction, response);
    } else {
        respond_ephemeral(client, interaction, "Failed to remove track!");
    }
}

void cmd_remove_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    int position = 1;
    if (args && *args) {
        position = atoi(args);
    }

    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, msg->guild_id);

    if (music_queue_remove(guild_str, position) == 0) {
        char response[128];
        snprintf(response, sizeof(response), ":wastebasket: Removed track #%d from queue!", position);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
    } else {
        struct discord_create_message params = { .content = "Failed to remove track!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void cmd_clear(struct discord *client, const struct discord_interaction *interaction) {
    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, interaction->guild_id);

    if (music_queue_clear(guild_str) == 0) {
        respond_message(client, interaction, ":wastebasket: Queue cleared!");
    } else {
        respond_ephemeral(client, interaction, "Failed to clear queue!");
    }
}

void cmd_clear_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_str[32];
    snprintf(guild_str, sizeof(guild_str), "%"PRIu64, msg->guild_id);

    if (music_queue_clear(guild_str) == 0) {
        struct discord_create_message params = { .content = ":wastebasket: Queue cleared!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
    } else {
        struct discord_create_message params = { .content = "Failed to clear queue!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void cmd_seek(struct discord *client, const struct discord_interaction *interaction) {
    (void)client;
    (void)interaction;
    respond_ephemeral(client, interaction, "Seek is not yet implemented.");
}

void cmd_seek_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;
    struct discord_create_message params = { .content = "Seek is not yet implemented." };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_musicsetup(struct discord *client, const struct discord_interaction *interaction) {
    (void)client;
    (void)interaction;
    respond_ephemeral(client, interaction, "Music setup is not yet implemented.");
}

void cmd_musicsetup_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;
    struct discord_create_message params = { .content = "Music setup is not yet implemented." };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

/* Register music commands */
void register_music_commands(himiko_bot_t *bot) {
    /* Initialize music system */
    music_init();

#ifdef CCORD_VOICE
    /* Register voice callbacks with Concord */
    if (bot && bot->client) {
        struct discord_voice_evcallbacks voice_cbs = {
            .on_ready = music_on_voice_ready,
            .on_session_descriptor = music_on_voice_session_descriptor,
        };
        discord_set_voice_cbs(bot->client, &voice_cbs);
        DEBUG_LOG("Voice callbacks registered");
    }
#endif

    /* Music commands are prefix-only to save slash command slots */
    static const himiko_command_t music_cmds[] = {
        {"play", "Play a song", "Music", cmd_play, cmd_play_prefix, 0, 1},
        {"skip", "Skip the current song", "Music", cmd_skip, cmd_skip_prefix, 0, 1},
        {"stop", "Stop playback and clear queue", "Music", cmd_stop, cmd_stop_prefix, 0, 1},
        {"pause", "Pause playback", "Music", cmd_pause, cmd_pause_prefix, 0, 1},
        {"resume", "Resume playback", "Music", cmd_resume, cmd_resume_prefix, 0, 1},
        {"queue", "Show the queue", "Music", cmd_queue, cmd_queue_prefix, 0, 1},
        {"np", "Show now playing", "Music", cmd_nowplaying, cmd_nowplaying_prefix, 0, 1},
        {"nowplaying", "Show now playing", "Music", cmd_nowplaying, cmd_nowplaying_prefix, 0, 1},
        {"volume", "Set volume (0-200)", "Music", cmd_volume, cmd_volume_prefix, 0, 1},
        {"join", "Join your voice channel", "Music", cmd_join, cmd_join_prefix, 0, 1},
        {"leave", "Leave the voice channel", "Music", cmd_leave, cmd_leave_prefix, 0, 1},
        {"shuffle", "Shuffle the queue", "Music", cmd_shuffle, cmd_shuffle_prefix, 0, 1},
        {"loop", "Toggle loop mode", "Music", cmd_loop, cmd_loop_prefix, 0, 1},
        {"remove", "Remove a track from queue", "Music", cmd_remove, cmd_remove_prefix, 0, 1},
        {"clearqueue", "Clear the queue", "Music", cmd_clear, cmd_clear_prefix, 0, 1},
        {"seek", "Seek to a position", "Music", cmd_seek, cmd_seek_prefix, 0, 1},
        {"musicsetup", "Configure music settings", "Music", cmd_musicsetup, cmd_musicsetup_prefix, 0, 1},
    };

    for (size_t i = 0; i < sizeof(music_cmds) / sizeof(music_cmds[0]); i++) {
        bot_register_command(bot, &music_cmds[i]);
    }
}
