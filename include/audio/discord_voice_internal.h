/*
 * Himiko Discord Bot (C Edition) - Discord Voice Internal Structures
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This header mirrors Concord's internal discord_voice struct to allow
 * access to UDP service fields (SSRC, server IP/port, encryption key).
 *
 * Based on concord/src/discord-voice.c struct definition.
 * WARNING: This must be kept in sync with Concord's internal structure.
 */

#ifndef HIMIKO_DISCORD_VOICE_INTERNAL_H
#define HIMIKO_DISCORD_VOICE_INTERNAL_H

#include <concord/discord.h>
#include <stdbool.h>
#include <stdarg.h>
#include <curl/curl.h>

/* Forward declarations for Concord internal types */
struct websockets;
typedef struct jsmnf_pair jsmnf_pair;
typedef struct jsmntok jsmntok_t;

/*
 * Mirrored log.c types from Concord's core/log.h
 */
typedef struct {
    va_list ap;
    const char *fmt;
    const char *file;
    struct tm *time;
    void *udata;
    int line;
    int level;
} himiko_log_Event;

typedef void (*himiko_log_LogFn)(himiko_log_Event *ev);
typedef void (*himiko_log_LockFn)(bool lock, void *udata);

#define HIMIKO_LOG_MAX_CALLBACKS 32

typedef struct {
    himiko_log_LogFn fn;
    void *udata;
    int level;
} himiko_log_Callback;

typedef struct {
    void *udata;
    himiko_log_LockFn lock;
    int level;
    bool quiet;
    himiko_log_Callback callbacks[HIMIKO_LOG_MAX_CALLBACKS];
} himiko_log_Logger;

/*
 * Mirrored logconf types from Concord's core/logconf.h
 */
#define HIMIKO_LOGCONF_ID_LEN (64 + 1)

struct himiko_logconf_szbuf {
    char *start;
    size_t size;
};

struct himiko_logconf {
    char id[HIMIKO_LOGCONF_ID_LEN];
    unsigned pid;
    _Bool is_branch;
    _Bool is_disabled;
    struct himiko_logconf_szbuf file;
    int *counter;
    himiko_log_Logger *L;
    struct {
        char *fname;
        FILE *f;
    } *logger, *http;
    struct {
        size_t size;
        char **ids;
    } disable_modules;
};

/*
 * Mirror of Concord's internal discord_voice structure.
 * This allows us to access udp_service fields for voice audio transmission.
 */
struct discord_voice_internal {
    /** `DISCORD_VOICE` logging module */
    struct himiko_logconf conf;
    /** the session guild id */
    u64snowflake guild_id;
    /** the session channel id */
    u64snowflake channel_id;
    /** the session token */
    char token[128];
    /** the new session token after a voice region change */
    char new_token[128];
    /** the new url after a voice region change */
    char new_url[512];
    /** the session id */
    char session_id[128];
    /** curl multi handle */
    CURLM *mhandle;
    /** the websockets handle */
    struct websockets *ws;

    /** reconnect structure */
    struct {
        bool enable;
        unsigned char attempt;
        unsigned char threshold;
    } reconnect;

    /** will attempt to resume session if connection shutsdown */
    bool is_resumable;
    /** redirect to a different voice server */
    bool is_redirect;
    /** can start sending/receiving additional events to discord */
    bool is_ready;

    /** current iteration JSON string data */
    char *json;
    /** current iteration JSON string data length */
    size_t length;

    /** parse JSON tokens */
    struct {
        jsmnf_pair *pairs;
        unsigned npairs;
        jsmntok_t *tokens;
        unsigned ntokens;
    } parse;

    /** voice payload structure */
    struct {
        enum discord_voice_opcodes opcode;
        jsmnf_pair *data;
    } payload;

    /** heartbeat structure */
    struct {
        u64unix_ms interval_ms;
        u64unix_ms tstamp;
    } hbeat;

    /** latency in milliseconds */
    int ping_ms;

    /** if true shutdown websockets connection */
    bool shutdown;

    /** UDP service info - THIS IS WHAT WE NEED */
    struct {
        int ssrc;
        int server_port;
        char server_ip[256];
        char digest[256];
        char unique_key[128];   /* 32-byte encryption key (only first 32 bytes used) */
        int audio_udp_pid;
        uintmax_t start_time;
    } udp_service;

    /** voice callbacks */
    struct discord_voice_evcallbacks *p_voice_cbs;

    /** receive interval */
    int recv_interval;

    /** pointer to client */
    struct discord *p_client;
};

/*
 * Cast a discord_voice pointer to our internal struct.
 * Use with caution - struct layout must match Concord's internal definition.
 */
static inline struct discord_voice_internal *
voice_get_internal(struct discord_voice *vc) {
    return (struct discord_voice_internal *)vc;
}

#endif /* HIMIKO_DISCORD_VOICE_INTERNAL_H */
