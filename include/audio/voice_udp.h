/*
 * Himiko Discord Bot (C Edition) - Voice UDP Layer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Handles UDP voice transmission to Discord including:
 * - UDP socket management
 * - IP Discovery protocol
 * - RTP packet construction
 * - XChaCha20-Poly1305 encryption
 */

#ifndef HIMIKO_AUDIO_VOICE_UDP_H
#define HIMIKO_AUDIO_VOICE_UDP_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

/* RTP constants */
#define RTP_VERSION         2
#define RTP_PAYLOAD_TYPE    0x78    /* 120 - Opus */
#define RTP_HEADER_SIZE     12

/* Discord voice constants */
#define VOICE_FRAME_SIZE    960     /* 20ms at 48kHz */
#define VOICE_SAMPLE_RATE   48000
#define VOICE_CHANNELS      2

/* Encryption constants */
#define VOICE_SECRET_KEY_SIZE   32
#define VOICE_NONCE_SIZE        24
#define VOICE_AUTH_TAG_SIZE     16

/* IP Discovery packet sizes */
#define IP_DISCOVERY_REQUEST_SIZE   74
#define IP_DISCOVERY_RESPONSE_SIZE  74

/* Voice UDP connection state */
typedef struct voice_udp {
    int socket_fd;
    struct sockaddr_in server_addr;

    /* Local endpoint (discovered via IP discovery) */
    char local_ip[64];
    uint16_t local_port;

    /* Voice session info from Discord */
    uint32_t ssrc;
    uint8_t secret_key[VOICE_SECRET_KEY_SIZE];

    /* RTP state */
    uint16_t sequence;
    uint32_t timestamp;

    /* Connection state */
    bool connected;
    bool ready;     /* True after IP discovery and key received */
} voice_udp_t;

/* Initialize UDP connection structure */
void voice_udp_init(voice_udp_t *udp);

/* Connect to Discord voice server */
int voice_udp_connect(voice_udp_t *udp, const char *server_ip, uint16_t server_port, uint32_t ssrc);

/* Perform IP discovery to get our external IP/port */
int voice_udp_discover_ip(voice_udp_t *udp);

/* Set the encryption key (received from session descriptor) */
void voice_udp_set_secret_key(voice_udp_t *udp, const uint8_t *key);

/* Build RTP header */
void voice_udp_build_rtp_header(voice_udp_t *udp, uint8_t *header);

/* Encrypt audio data with XChaCha20-Poly1305 */
int voice_udp_encrypt(voice_udp_t *udp, const uint8_t *rtp_header,
                      const uint8_t *opus_data, size_t opus_len,
                      uint8_t *output, size_t *output_len);

/* Send encrypted audio packet */
int voice_udp_send_audio(voice_udp_t *udp, const uint8_t *opus_data, size_t opus_len);

/* Send silence frames (5 frames of silence to signal end of speaking) */
int voice_udp_send_silence(voice_udp_t *udp);

/* Close UDP connection */
void voice_udp_close(voice_udp_t *udp);

/* Get local IP (after discovery) */
const char *voice_udp_get_local_ip(voice_udp_t *udp);

/* Get local port (after discovery) */
uint16_t voice_udp_get_local_port(voice_udp_t *udp);

#endif /* HIMIKO_AUDIO_VOICE_UDP_H */
