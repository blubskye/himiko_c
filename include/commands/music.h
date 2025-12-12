/*
 * Himiko Discord Bot (C Edition) - Music Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef HIMIKO_COMMANDS_MUSIC_H
#define HIMIKO_COMMANDS_MUSIC_H

#include "bot.h"
#include "audio/voice_udp.h"
#include "audio/audio_stream.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/* Audio constants */
#define MUSIC_SAMPLE_RATE      48000
#define MUSIC_CHANNELS         2
#define MUSIC_FRAME_SIZE       960     /* 20ms at 48kHz */
#define MUSIC_MAX_PACKET_SIZE  4000
#define MUSIC_OPUS_BITRATE     64000

/* Queue limits */
#define MUSIC_MAX_QUEUE_SIZE   500
#define MUSIC_MAX_TITLE_LEN    256
#define MUSIC_MAX_URL_LEN      512

/* Track source types */
typedef enum {
    TRACK_SOURCE_YOUTUBE,
    TRACK_SOURCE_SOUNDCLOUD,
    TRACK_SOURCE_DIRECT_URL,
    TRACK_SOURCE_LOCAL_FILE,
    TRACK_SOURCE_SEARCH
} track_source_t;

/* Track information */
typedef struct {
    int id;
    char guild_id[32];
    char channel_id[32];
    char user_id[32];
    char title[MUSIC_MAX_TITLE_LEN];
    char url[MUSIC_MAX_URL_LEN];
    char thumbnail[MUSIC_MAX_URL_LEN];
    int duration;           /* Duration in seconds */
    int position;           /* Position in queue */
    track_source_t source;
    bool is_local;
    time_t added_at;
} music_track_t;

/* Player state */
typedef enum {
    PLAYER_STATE_IDLE,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_LOADING
} player_state_t;

/* Voice connection state */
typedef enum {
    VOICE_STATE_DISCONNECTED,
    VOICE_STATE_CONNECTING,
    VOICE_STATE_CONNECTED,
    VOICE_STATE_READY
} voice_state_t;

/* Voice connection info (received from Discord) */
typedef struct {
    char session_id[128];
    char token[128];
    char endpoint[256];
    u64snowflake guild_id;
    u64snowflake channel_id;
    u64snowflake user_id;
} voice_connection_info_t;

/* Guild music player */
typedef struct music_player {
    u64snowflake guild_id;
    u64snowflake voice_channel_id;
    u64snowflake text_channel_id;

    player_state_t state;
    voice_state_t voice_state;

    voice_connection_info_t voice_info;
    voice_udp_t udp;            /* UDP voice connection */
    audio_stream_t audio;       /* Audio streaming pipeline */

    music_track_t *current_track;
    int volume;             /* 0-200, default 100 */
    bool loop_track;
    bool loop_queue;

    /* Discord voice pointer (from Concord callback) */
    struct discord_voice *voice_connection;

    pthread_mutex_t lock;

    /* Linked list for multiple guilds */
    struct music_player *next;
} music_player_t;

/* Music settings (per guild) */
typedef struct {
    char guild_id[32];
    char dj_role_id[32];
    char mod_role_id[32];
    int default_volume;
    char music_folder[256];
    time_t created_at;
    time_t updated_at;
} music_settings_t;

/* Global music state */
typedef struct {
    music_player_t *players;    /* Linked list of active players */
    pthread_mutex_t lock;
    bool initialized;
} music_state_t;

/* Global music state instance */
extern music_state_t g_music;

/* Initialization and cleanup */
int music_init(void);
void music_cleanup(void);

/* Player management */
music_player_t *music_get_player(u64snowflake guild_id);
music_player_t *music_create_player(u64snowflake guild_id);
void music_destroy_player(music_player_t *player);

/* Voice connection */
int music_voice_join(struct discord *client, u64snowflake guild_id,
                     u64snowflake channel_id, u64snowflake text_channel_id);
int music_voice_leave(struct discord *client, u64snowflake guild_id);
void music_on_voice_state_update(struct discord *client,
                                  const struct discord_voice_state *vs);
void music_on_voice_server_update(struct discord *client,
                                   const struct discord_voice_server_update *vsu);

/* Concord voice callbacks */
void music_on_voice_ready(struct discord_voice *vc);
void music_on_voice_session_descriptor(struct discord_voice *vc);

/* Start playback of next track */
int music_start_playback(music_player_t *player);

/* Playback control */
int music_play(music_player_t *player, const char *query, u64snowflake user_id);
int music_skip(music_player_t *player);
int music_stop(music_player_t *player);
int music_pause(music_player_t *player);
int music_resume(music_player_t *player);
int music_seek(music_player_t *player, int position);
int music_set_volume(music_player_t *player, int volume);

/* Queue management */
int music_queue_add(const char *guild_id, const music_track_t *track);
int music_queue_remove(const char *guild_id, int position);
int music_queue_clear(const char *guild_id);
int music_queue_shuffle(const char *guild_id);
int music_queue_move(const char *guild_id, int from, int to);
music_track_t *music_queue_get(const char *guild_id, int *count);
music_track_t *music_queue_next(const char *guild_id);

/* Track resolution */
int music_resolve_track(const char *query, music_track_t *track);
int music_search_youtube(const char *query, music_track_t *results, int max_results);
char *music_get_stream_url(const music_track_t *track);

/* Settings */
int music_get_settings(const char *guild_id, music_settings_t *settings);
int music_set_settings(const char *guild_id, const music_settings_t *settings);

/* Permission checks */
bool music_has_dj_role(struct discord *client, u64snowflake guild_id,
                       const struct discord_guild_member *member);
bool music_is_alone_in_voice(struct discord *client, u64snowflake guild_id,
                              u64snowflake channel_id);

/* History */
int music_add_to_history(const char *guild_id, const char *user_id,
                         const music_track_t *track);

/* Command handlers */
void cmd_play(struct discord *client, const struct discord_interaction *interaction);
void cmd_play_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_skip(struct discord *client, const struct discord_interaction *interaction);
void cmd_skip_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_stop(struct discord *client, const struct discord_interaction *interaction);
void cmd_stop_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_pause(struct discord *client, const struct discord_interaction *interaction);
void cmd_pause_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_resume(struct discord *client, const struct discord_interaction *interaction);
void cmd_resume_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_queue(struct discord *client, const struct discord_interaction *interaction);
void cmd_queue_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_nowplaying(struct discord *client, const struct discord_interaction *interaction);
void cmd_nowplaying_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_volume(struct discord *client, const struct discord_interaction *interaction);
void cmd_volume_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_join(struct discord *client, const struct discord_interaction *interaction);
void cmd_join_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_leave(struct discord *client, const struct discord_interaction *interaction);
void cmd_leave_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_shuffle(struct discord *client, const struct discord_interaction *interaction);
void cmd_shuffle_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_loop(struct discord *client, const struct discord_interaction *interaction);
void cmd_loop_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_remove(struct discord *client, const struct discord_interaction *interaction);
void cmd_remove_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_clear(struct discord *client, const struct discord_interaction *interaction);
void cmd_clear_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_seek(struct discord *client, const struct discord_interaction *interaction);
void cmd_seek_prefix(struct discord *client, const struct discord_message *msg, const char *args);
void cmd_musicsetup(struct discord *client, const struct discord_interaction *interaction);
void cmd_musicsetup_prefix(struct discord *client, const struct discord_message *msg, const char *args);

/* Register commands */
void register_music_commands(himiko_bot_t *bot);

#endif /* HIMIKO_COMMANDS_MUSIC_H */
