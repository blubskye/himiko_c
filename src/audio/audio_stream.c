/*
 * Himiko Discord Bot (C Edition) - Audio Streaming
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "audio/audio_stream.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#ifdef HAVE_OPUS
#include <opus/opus.h>
#endif

#ifdef CCORD_VOICE
#include <concord/discord.h>
#endif

/* Helper to sleep until next frame time with precise timing */
static void sleep_until_next_frame(struct timespec *next) {
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, next, NULL);

    /* Advance to next frame (20ms = 20,000,000 ns) */
    next->tv_nsec += 20000000;
    if (next->tv_nsec >= 1000000000) {
        next->tv_sec++;
        next->tv_nsec -= 1000000000;
    }
}

/* Apply volume to PCM samples */
static void apply_volume(int16_t *samples, size_t count, int volume) {
    if (volume == 100) return;  /* No change needed */

    float factor = (float)volume / 100.0f;
    for (size_t i = 0; i < count; i++) {
        float sample = (float)samples[i] * factor;
        /* Clamp to int16 range */
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        samples[i] = (int16_t)sample;
    }
}

/* Read exactly n bytes from file descriptor */
static ssize_t read_full(int fd, void *buf, size_t count) {
    size_t total = 0;
    uint8_t *ptr = buf;

    while (total < count) {
        ssize_t n = read(fd, ptr + total, count - total);
        if (n <= 0) {
            if (n == 0) return (ssize_t)total;  /* EOF */
            if (errno == EINTR) continue;
            return -1;  /* Error */
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

/* Start FFmpeg subprocess */
static int start_ffmpeg(audio_stream_t *stream, const char *url) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        DEBUG_LOG("Failed to create pipe: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        DEBUG_LOG("Failed to fork: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        close(pipefd[0]);  /* Close read end */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        /* Redirect stderr to /dev/null */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        /* Execute FFmpeg */
        execlp("ffmpeg", "ffmpeg",
               "-reconnect", "1",
               "-reconnect_streamed", "1",
               "-reconnect_delay_max", "5",
               "-i", url,
               "-f", "s16le",           /* Raw PCM output */
               "-ar", "48000",          /* 48kHz sample rate */
               "-ac", "2",              /* Stereo */
               "-acodec", "pcm_s16le",
               "pipe:1",                /* Output to stdout */
               NULL);

        /* If exec fails */
        _exit(1);
    }

    /* Parent process */
    close(pipefd[1]);  /* Close write end */

    stream->ffmpeg_pid = pid;
    stream->ffmpeg_pipe = pipefd[0];

    DEBUG_LOG("Started FFmpeg (PID %d) for URL: %s", pid, url);
    return 0;
}

/* Stop FFmpeg subprocess */
static void stop_ffmpeg(audio_stream_t *stream) {
    if (stream->ffmpeg_pid > 0) {
        kill(stream->ffmpeg_pid, SIGTERM);

        /* Wait briefly for graceful shutdown */
        int status;
        pid_t result = waitpid(stream->ffmpeg_pid, &status, WNOHANG);
        if (result == 0) {
            /* Still running, give it a moment */
            usleep(100000);  /* 100ms */
            result = waitpid(stream->ffmpeg_pid, &status, WNOHANG);
            if (result == 0) {
                /* Force kill */
                kill(stream->ffmpeg_pid, SIGKILL);
                waitpid(stream->ffmpeg_pid, &status, 0);
            }
        }

        DEBUG_LOG("Stopped FFmpeg (PID %d)", stream->ffmpeg_pid);
        stream->ffmpeg_pid = -1;
    }

    if (stream->ffmpeg_pipe >= 0) {
        close(stream->ffmpeg_pipe);
        stream->ffmpeg_pipe = -1;
    }
}

/* Audio thread main function */
static void *audio_thread_func(void *arg) {
    audio_stream_t *stream = arg;

#ifdef HAVE_OPUS
    /* Buffers */
    int16_t pcm_buffer[AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS];
    uint8_t opus_buffer[AUDIO_OPUS_MAX_SIZE];

    /* Initialize timing */
    clock_gettime(CLOCK_MONOTONIC, &stream->next_frame_time);

    DEBUG_LOG("Audio thread started");

    while (!stream->should_stop) {
        /* Check if paused */
        if (stream->paused) {
            usleep(50000);  /* 50ms */
            /* Reset timing when resuming */
            clock_gettime(CLOCK_MONOTONIC, &stream->next_frame_time);
            continue;
        }

        /* Read PCM data from FFmpeg */
        ssize_t bytes_read = read_full(stream->ffmpeg_pipe, pcm_buffer, AUDIO_FRAME_SIZE);

        if (bytes_read <= 0) {
            /* EOF or error - track ended */
            DEBUG_LOG("FFmpeg stream ended (read returned %zd)", bytes_read);
            break;
        }

        if (bytes_read < (ssize_t)AUDIO_FRAME_SIZE) {
            /* Partial frame at end - pad with silence */
            memset((uint8_t *)pcm_buffer + bytes_read, 0, AUDIO_FRAME_SIZE - bytes_read);
        }

        /* Apply volume */
        apply_volume(pcm_buffer, AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS, stream->volume);

        /* Encode to Opus */
        OpusEncoder *encoder = (OpusEncoder *)stream->opus_encoder;
        int opus_len = opus_encode(encoder, pcm_buffer, AUDIO_FRAME_SAMPLES,
                                   opus_buffer, sizeof(opus_buffer));

        if (opus_len < 0) {
            DEBUG_LOG("Opus encode error: %s", opus_strerror(opus_len));
            continue;
        }

        /* Send via UDP */
        if (stream->udp && stream->udp->ready) {
            if (voice_udp_send_audio(stream->udp, opus_buffer, (size_t)opus_len) != 0) {
                /* UDP send failed - might be disconnected */
                DEBUG_LOG("UDP send failed");
            }
        }

        stream->frames_sent++;

        /* Sleep until next frame */
        sleep_until_next_frame(&stream->next_frame_time);
    }

    /* Send silence frames to signal end of speaking */
    if (stream->udp && stream->udp->ready) {
        voice_udp_send_silence(stream->udp);
    }

    DEBUG_LOG("Audio thread stopping (sent %lu frames)", stream->frames_sent);

#else
    (void)stream;
    DEBUG_LOG("Audio thread: Opus not available");
#endif

    /* Update state */
    pthread_mutex_lock(&stream->lock);
    stream->state = AUDIO_STREAM_IDLE;
    stream->thread_running = false;
    pthread_mutex_unlock(&stream->lock);

    /* Call completion callback */
    if (stream->on_track_end && !stream->should_stop) {
        stream->on_track_end(stream->user_data);
    }

    return NULL;
}

/* Initialize audio stream */
int audio_stream_init(audio_stream_t *stream) {
    if (!stream) return -1;

    memset(stream, 0, sizeof(audio_stream_t));

    pthread_mutex_init(&stream->lock, NULL);
    stream->ffmpeg_pid = -1;
    stream->ffmpeg_pipe = -1;
    stream->volume = 100;
    stream->state = AUDIO_STREAM_IDLE;

#ifdef HAVE_OPUS
    /* Create Opus encoder */
    int error;
    stream->opus_encoder = opus_encoder_create(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS,
                                                OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK) {
        DEBUG_LOG("Failed to create Opus encoder: %s", opus_strerror(error));
        pthread_mutex_destroy(&stream->lock);
        return -1;
    }

    /* Configure encoder */
    opus_encoder_ctl((OpusEncoder *)stream->opus_encoder,
                     OPUS_SET_BITRATE(AUDIO_OPUS_BITRATE));
    opus_encoder_ctl((OpusEncoder *)stream->opus_encoder,
                     OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    DEBUG_LOG("Audio stream initialized with Opus encoder");
#else
    DEBUG_LOG("Audio stream initialized (Opus not available)");
#endif

    return 0;
}

/* Cleanup audio stream */
void audio_stream_cleanup(audio_stream_t *stream) {
    if (!stream) return;

    /* Stop any active playback */
    audio_stream_stop(stream);

#ifdef HAVE_OPUS
    if (stream->opus_encoder) {
        opus_encoder_destroy((OpusEncoder *)stream->opus_encoder);
        stream->opus_encoder = NULL;
    }
#endif

    pthread_mutex_destroy(&stream->lock);
    DEBUG_LOG("Audio stream cleaned up");
}

/* Set the voice UDP connection to use */
void audio_stream_set_udp(audio_stream_t *stream, voice_udp_t *udp) {
    if (!stream) return;

    pthread_mutex_lock(&stream->lock);
    stream->udp = udp;
    pthread_mutex_unlock(&stream->lock);
}

/* Set the Discord voice connection (for speaking indicator) */
void audio_stream_set_voice(audio_stream_t *stream, struct discord_voice *vc) {
    if (!stream) return;

    pthread_mutex_lock(&stream->lock);
    stream->voice_connection = vc;
    pthread_mutex_unlock(&stream->lock);
}

/* Set track end callback */
void audio_stream_set_callback(audio_stream_t *stream,
                                void (*callback)(void *), void *user_data) {
    if (!stream) return;

    pthread_mutex_lock(&stream->lock);
    stream->on_track_end = callback;
    stream->user_data = user_data;
    pthread_mutex_unlock(&stream->lock);
}

/* Start playing a URL */
int audio_stream_play(audio_stream_t *stream, const char *url) {
    if (!stream || !url) return -1;

#ifndef HAVE_OPUS
    DEBUG_LOG("Cannot play: Opus not available");
    return -1;
#endif

    pthread_mutex_lock(&stream->lock);

    /* Stop any existing playback */
    if (stream->thread_running) {
        pthread_mutex_unlock(&stream->lock);
        audio_stream_stop(stream);
        pthread_mutex_lock(&stream->lock);
    }

    /* Store URL */
    strncpy(stream->current_url, url, sizeof(stream->current_url) - 1);

    /* Reset state */
    stream->should_stop = false;
    stream->paused = false;
    stream->frames_sent = 0;
    stream->state = AUDIO_STREAM_STARTING;

    pthread_mutex_unlock(&stream->lock);

    /* Start FFmpeg */
    if (start_ffmpeg(stream, url) != 0) {
        stream->state = AUDIO_STREAM_IDLE;
        return -1;
    }

    /* Send speaking indicator */
#ifdef CCORD_VOICE
    if (stream->voice_connection) {
        discord_send_speaking(stream->voice_connection, 1, 0);
    }
#endif

    /* Start audio thread */
    stream->thread_running = true;
    stream->state = AUDIO_STREAM_PLAYING;

    if (pthread_create(&stream->thread, NULL, audio_thread_func, stream) != 0) {
        DEBUG_LOG("Failed to create audio thread");
        stop_ffmpeg(stream);
        stream->thread_running = false;
        stream->state = AUDIO_STREAM_IDLE;
        return -1;
    }

    pthread_detach(stream->thread);

    DEBUG_LOG("Started playback: %s", url);
    return 0;
}

/* Stop playback */
void audio_stream_stop(audio_stream_t *stream) {
    if (!stream) return;

    pthread_mutex_lock(&stream->lock);

    if (!stream->thread_running) {
        pthread_mutex_unlock(&stream->lock);
        return;
    }

    stream->should_stop = true;
    stream->state = AUDIO_STREAM_STOPPING;

    pthread_mutex_unlock(&stream->lock);

    /* Stop FFmpeg to make the thread exit */
    stop_ffmpeg(stream);

    /* Wait for thread to finish (with timeout) */
    for (int i = 0; i < 50; i++) {  /* 5 second timeout */
        pthread_mutex_lock(&stream->lock);
        bool running = stream->thread_running;
        pthread_mutex_unlock(&stream->lock);

        if (!running) break;
        usleep(100000);  /* 100ms */
    }

    /* Send stop speaking indicator */
#ifdef CCORD_VOICE
    if (stream->voice_connection) {
        discord_send_speaking(stream->voice_connection, 0, 0);
    }
#endif

    pthread_mutex_lock(&stream->lock);
    stream->state = AUDIO_STREAM_IDLE;
    pthread_mutex_unlock(&stream->lock);

    DEBUG_LOG("Stopped playback");
}

/* Pause playback */
void audio_stream_pause(audio_stream_t *stream) {
    if (!stream) return;

    pthread_mutex_lock(&stream->lock);

    if (stream->state == AUDIO_STREAM_PLAYING) {
        stream->paused = true;
        stream->state = AUDIO_STREAM_PAUSED;

#ifdef CCORD_VOICE
        if (stream->voice_connection) {
            discord_send_speaking(stream->voice_connection, 0, 0);
        }
#endif
    }

    pthread_mutex_unlock(&stream->lock);
}

/* Resume playback */
void audio_stream_resume(audio_stream_t *stream) {
    if (!stream) return;

    pthread_mutex_lock(&stream->lock);

    if (stream->state == AUDIO_STREAM_PAUSED) {
        stream->paused = false;
        stream->state = AUDIO_STREAM_PLAYING;

#ifdef CCORD_VOICE
        if (stream->voice_connection) {
            discord_send_speaking(stream->voice_connection, 1, 0);
        }
#endif
    }

    pthread_mutex_unlock(&stream->lock);
}

/* Set volume (0-200) */
void audio_stream_set_volume(audio_stream_t *stream, int volume) {
    if (!stream) return;

    if (volume < 0) volume = 0;
    if (volume > 200) volume = 200;

    pthread_mutex_lock(&stream->lock);
    stream->volume = volume;
    pthread_mutex_unlock(&stream->lock);
}

/* Get current state */
audio_stream_state_t audio_stream_get_state(audio_stream_t *stream) {
    if (!stream) return AUDIO_STREAM_IDLE;

    pthread_mutex_lock(&stream->lock);
    audio_stream_state_t state = stream->state;
    pthread_mutex_unlock(&stream->lock);

    return state;
}

/* Check if currently playing */
bool audio_stream_is_playing(audio_stream_t *stream) {
    audio_stream_state_t state = audio_stream_get_state(stream);
    return state == AUDIO_STREAM_PLAYING || state == AUDIO_STREAM_PAUSED;
}

/* Get frames sent count */
uint64_t audio_stream_get_frames_sent(audio_stream_t *stream) {
    if (!stream) return 0;

    pthread_mutex_lock(&stream->lock);
    uint64_t frames = stream->frames_sent;
    pthread_mutex_unlock(&stream->lock);

    return frames;
}
