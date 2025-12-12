/*
 * Himiko Discord Bot (C Edition) - Audio Streaming
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Handles audio streaming pipeline:
 * - FFmpeg subprocess for audio decoding
 * - Opus encoding
 * - Precise 20ms frame timing
 * - Integration with voice UDP layer
 */

#ifndef HIMIKO_AUDIO_STREAM_H
#define HIMIKO_AUDIO_STREAM_H

#include "audio/voice_udp.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/* Audio constants */
#define AUDIO_SAMPLE_RATE       48000
#define AUDIO_CHANNELS          2
#define AUDIO_FRAME_SAMPLES     960     /* 20ms at 48kHz */
#define AUDIO_FRAME_SIZE        (AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS * sizeof(int16_t))  /* 3840 bytes */
#define AUDIO_FRAME_MS          20
#define AUDIO_OPUS_BITRATE      128000

/* Opus buffer size */
#define AUDIO_OPUS_MAX_SIZE     4000

/* Audio stream state */
typedef enum {
    AUDIO_STREAM_IDLE,
    AUDIO_STREAM_STARTING,
    AUDIO_STREAM_PLAYING,
    AUDIO_STREAM_PAUSED,
    AUDIO_STREAM_STOPPING
} audio_stream_state_t;

/* Forward declaration */
struct discord_voice;

/* Audio stream context */
typedef struct audio_stream {
    /* Thread management */
    pthread_t thread;
    pthread_mutex_t lock;
    bool thread_running;

    /* State */
    audio_stream_state_t state;
    bool should_stop;
    bool paused;

    /* Voice UDP connection */
    voice_udp_t *udp;

    /* Opus encoder */
    void *opus_encoder;

    /* FFmpeg subprocess */
    pid_t ffmpeg_pid;
    int ffmpeg_pipe;    /* Read end of pipe */

    /* Timing */
    struct timespec next_frame_time;
    uint64_t frames_sent;

    /* Current track info */
    char current_url[2048];

    /* Volume (0-200, 100 = normal) */
    int volume;

    /* Discord voice pointer for speaking indicator */
    struct discord_voice *voice_connection;

    /* Callback for track completion */
    void (*on_track_end)(void *user_data);
    void *user_data;
} audio_stream_t;

/* Initialize audio stream */
int audio_stream_init(audio_stream_t *stream);

/* Cleanup audio stream */
void audio_stream_cleanup(audio_stream_t *stream);

/* Set the voice UDP connection to use */
void audio_stream_set_udp(audio_stream_t *stream, voice_udp_t *udp);

/* Set the Discord voice connection (for speaking indicator) */
void audio_stream_set_voice(audio_stream_t *stream, struct discord_voice *vc);

/* Set track end callback */
void audio_stream_set_callback(audio_stream_t *stream,
                                void (*callback)(void *), void *user_data);

/* Start playing a URL (spawns FFmpeg and audio thread) */
int audio_stream_play(audio_stream_t *stream, const char *url);

/* Stop playback */
void audio_stream_stop(audio_stream_t *stream);

/* Pause playback */
void audio_stream_pause(audio_stream_t *stream);

/* Resume playback */
void audio_stream_resume(audio_stream_t *stream);

/* Set volume (0-200) */
void audio_stream_set_volume(audio_stream_t *stream, int volume);

/* Get current state */
audio_stream_state_t audio_stream_get_state(audio_stream_t *stream);

/* Check if currently playing */
bool audio_stream_is_playing(audio_stream_t *stream);

/* Get frames sent count */
uint64_t audio_stream_get_frames_sent(audio_stream_t *stream);

#endif /* HIMIKO_AUDIO_STREAM_H */
