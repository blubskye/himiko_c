/*
 * Himiko Discord Bot (C Edition) - Voice UDP Layer
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "audio/voice_udp.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

#ifdef HAVE_SODIUM
#include <sodium.h>
#endif

/* Opus silence frame (3 bytes) */
static const uint8_t OPUS_SILENCE[] = { 0xF8, 0xFF, 0xFE };

/* Initialize UDP connection structure */
void voice_udp_init(voice_udp_t *udp) {
    if (!udp) return;

    memset(udp, 0, sizeof(voice_udp_t));
    udp->socket_fd = -1;
    udp->connected = false;
    udp->ready = false;
}

/* Connect to Discord voice server */
int voice_udp_connect(voice_udp_t *udp, const char *server_ip, uint16_t server_port, uint32_t ssrc) {
    if (!udp || !server_ip) return -1;

    /* Close existing socket if any */
    if (udp->socket_fd >= 0) {
        close(udp->socket_fd);
    }

    /* Create UDP socket */
    udp->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp->socket_fd < 0) {
        DEBUG_LOG("Failed to create UDP socket: %s", strerror(errno));
        return -1;
    }

    /* Set up server address */
    memset(&udp->server_addr, 0, sizeof(udp->server_addr));
    udp->server_addr.sin_family = AF_INET;
    udp->server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &udp->server_addr.sin_addr) <= 0) {
        DEBUG_LOG("Invalid server IP address: %s", server_ip);
        close(udp->socket_fd);
        udp->socket_fd = -1;
        return -1;
    }

    /* Connect the socket (for send/recv instead of sendto/recvfrom) */
    if (connect(udp->socket_fd, (struct sockaddr *)&udp->server_addr,
                sizeof(udp->server_addr)) < 0) {
        DEBUG_LOG("Failed to connect UDP socket: %s", strerror(errno));
        close(udp->socket_fd);
        udp->socket_fd = -1;
        return -1;
    }

    udp->ssrc = ssrc;
    udp->sequence = 0;
    udp->timestamp = 0;
    udp->connected = true;

    DEBUG_LOG("Voice UDP connected to %s:%u (SSRC: %u)", server_ip, server_port, ssrc);
    return 0;
}

/* Perform IP discovery to get our external IP/port */
int voice_udp_discover_ip(voice_udp_t *udp) {
    if (!udp || udp->socket_fd < 0 || !udp->connected) {
        DEBUG_LOG("Cannot perform IP discovery: not connected");
        return -1;
    }

    /*
     * IP Discovery Request Format (74 bytes):
     * Bytes 0-1:  Type (0x0001 = request)
     * Bytes 2-3:  Length (70)
     * Bytes 4-7:  SSRC (big-endian)
     * Bytes 8-73: Padding (zeros)
     */
    uint8_t request[IP_DISCOVERY_REQUEST_SIZE];
    memset(request, 0, sizeof(request));

    /* Type: 0x0001 (request) */
    request[0] = 0x00;
    request[1] = 0x01;

    /* Length: 70 (big-endian) */
    request[2] = 0x00;
    request[3] = 0x46;  /* 70 */

    /* SSRC (big-endian) */
    request[4] = (udp->ssrc >> 24) & 0xFF;
    request[5] = (udp->ssrc >> 16) & 0xFF;
    request[6] = (udp->ssrc >> 8) & 0xFF;
    request[7] = udp->ssrc & 0xFF;

    /* Send discovery request */
    ssize_t sent = send(udp->socket_fd, request, sizeof(request), 0);
    if (sent != sizeof(request)) {
        DEBUG_LOG("Failed to send IP discovery request: %s", strerror(errno));
        return -1;
    }

    /* Wait for response with timeout */
    struct pollfd pfd = {
        .fd = udp->socket_fd,
        .events = POLLIN
    };

    int ret = poll(&pfd, 1, 5000);  /* 5 second timeout */
    if (ret <= 0) {
        DEBUG_LOG("IP discovery timeout or error");
        return -1;
    }

    /*
     * IP Discovery Response Format (74 bytes):
     * Bytes 0-1:  Type (0x0002 = response)
     * Bytes 2-3:  Length (70)
     * Bytes 4-7:  SSRC (big-endian)
     * Bytes 8-71: IP address (null-terminated string)
     * Bytes 72-73: Port (big-endian)
     */
    uint8_t response[IP_DISCOVERY_RESPONSE_SIZE];
    ssize_t received = recv(udp->socket_fd, response, sizeof(response), 0);
    if (received != sizeof(response)) {
        DEBUG_LOG("Invalid IP discovery response size: %zd", received);
        return -1;
    }

    /* Verify response type */
    if (response[0] != 0x00 || response[1] != 0x02) {
        DEBUG_LOG("Invalid IP discovery response type: %02x%02x", response[0], response[1]);
        return -1;
    }

    /* Extract IP address (null-terminated string starting at offset 8) */
    strncpy(udp->local_ip, (char *)&response[8], sizeof(udp->local_ip) - 1);
    udp->local_ip[sizeof(udp->local_ip) - 1] = '\0';

    /* Extract port (big-endian at offset 72) */
    udp->local_port = ((uint16_t)response[72] << 8) | response[73];

    DEBUG_LOG("IP Discovery: local endpoint is %s:%u", udp->local_ip, udp->local_port);
    return 0;
}

/* Set the encryption key (received from session descriptor) */
void voice_udp_set_secret_key(voice_udp_t *udp, const uint8_t *key) {
    if (!udp || !key) return;

    memcpy(udp->secret_key, key, VOICE_SECRET_KEY_SIZE);
    udp->ready = true;

    DEBUG_LOG("Voice UDP encryption key set, ready for audio");
}

/* Build RTP header */
void voice_udp_build_rtp_header(voice_udp_t *udp, uint8_t *header) {
    if (!udp || !header) return;

    /*
     * RTP Header (12 bytes):
     * Byte 0:    V=2, P=0, X=0, CC=0  → 0x80
     * Byte 1:    M=0, PT=120          → 0x78
     * Bytes 2-3: Sequence number (big-endian)
     * Bytes 4-7: Timestamp (big-endian)
     * Bytes 8-11: SSRC (big-endian)
     */
    header[0] = 0x80;  /* Version 2, no padding, no extension, no CSRC */
    header[1] = 0x78;  /* Payload type 120 (Opus) */

    /* Sequence number (big-endian) */
    header[2] = (udp->sequence >> 8) & 0xFF;
    header[3] = udp->sequence & 0xFF;

    /* Timestamp (big-endian) */
    header[4] = (udp->timestamp >> 24) & 0xFF;
    header[5] = (udp->timestamp >> 16) & 0xFF;
    header[6] = (udp->timestamp >> 8) & 0xFF;
    header[7] = udp->timestamp & 0xFF;

    /* SSRC (big-endian) */
    header[8] = (udp->ssrc >> 24) & 0xFF;
    header[9] = (udp->ssrc >> 16) & 0xFF;
    header[10] = (udp->ssrc >> 8) & 0xFF;
    header[11] = udp->ssrc & 0xFF;
}

/* Encrypt audio data with XChaCha20-Poly1305 */
int voice_udp_encrypt(voice_udp_t *udp, const uint8_t *rtp_header,
                      const uint8_t *opus_data, size_t opus_len,
                      uint8_t *output, size_t *output_len) {
#ifdef HAVE_SODIUM
    if (!udp || !rtp_header || !opus_data || !output || !output_len) return -1;

    /*
     * Discord voice encryption: aead_xchacha20_poly1305_rtpsize
     *
     * Nonce (24 bytes): RTP header (12 bytes) + 12 zero bytes
     * Additional data: RTP header (12 bytes) - authenticated but not encrypted
     * Plaintext: Opus audio data
     * Output: Encrypted data + 16-byte auth tag
     */
    uint8_t nonce[VOICE_NONCE_SIZE];
    memset(nonce, 0, sizeof(nonce));
    memcpy(nonce, rtp_header, RTP_HEADER_SIZE);

    unsigned long long ciphertext_len;
    int ret = crypto_aead_xchacha20poly1305_ietf_encrypt(
        output,                     /* ciphertext output */
        &ciphertext_len,            /* ciphertext length */
        opus_data,                  /* plaintext */
        opus_len,                   /* plaintext length */
        rtp_header,                 /* additional data */
        RTP_HEADER_SIZE,            /* additional data length */
        NULL,                       /* nsec (unused) */
        nonce,                      /* 24-byte nonce */
        udp->secret_key             /* 32-byte key */
    );

    if (ret != 0) {
        DEBUG_LOG("Encryption failed");
        return -1;
    }

    *output_len = (size_t)ciphertext_len;
    return 0;
#else
    (void)udp;
    (void)rtp_header;
    (void)opus_data;
    (void)opus_len;
    (void)output;
    (void)output_len;
    DEBUG_LOG("Encryption not available: libsodium not compiled in");
    return -1;
#endif
}

/* Send encrypted audio packet */
int voice_udp_send_audio(voice_udp_t *udp, const uint8_t *opus_data, size_t opus_len) {
    if (!udp || !udp->ready || udp->socket_fd < 0) {
        return -1;
    }

    /* Build RTP header */
    uint8_t rtp_header[RTP_HEADER_SIZE];
    voice_udp_build_rtp_header(udp, rtp_header);

    /* Encrypt audio data */
    uint8_t encrypted[4096];
    size_t encrypted_len;

    if (voice_udp_encrypt(udp, rtp_header, opus_data, opus_len,
                          encrypted, &encrypted_len) != 0) {
        return -1;
    }

    /* Build final packet: RTP header + encrypted data */
    uint8_t packet[4096 + RTP_HEADER_SIZE];
    memcpy(packet, rtp_header, RTP_HEADER_SIZE);
    memcpy(packet + RTP_HEADER_SIZE, encrypted, encrypted_len);

    size_t packet_len = RTP_HEADER_SIZE + encrypted_len;

    /* Send packet */
    ssize_t sent = send(udp->socket_fd, packet, packet_len, 0);
    if (sent != (ssize_t)packet_len) {
        DEBUG_LOG("Failed to send audio packet: %s", strerror(errno));
        return -1;
    }

    /* Increment sequence and timestamp for next packet */
    udp->sequence++;
    udp->timestamp += VOICE_FRAME_SIZE;

    return 0;
}

/* Send silence frames (5 frames of silence to signal end of speaking) */
int voice_udp_send_silence(voice_udp_t *udp) {
    if (!udp || !udp->ready) return -1;

    /* Send 5 silence frames as per Discord protocol */
    for (int i = 0; i < 5; i++) {
        if (voice_udp_send_audio(udp, OPUS_SILENCE, sizeof(OPUS_SILENCE)) != 0) {
            return -1;
        }
    }

    DEBUG_LOG("Sent 5 silence frames");
    return 0;
}

/* Close UDP connection */
void voice_udp_close(voice_udp_t *udp) {
    if (!udp) return;

    if (udp->socket_fd >= 0) {
        close(udp->socket_fd);
        udp->socket_fd = -1;
    }

    udp->connected = false;
    udp->ready = false;

    DEBUG_LOG("Voice UDP connection closed");
}

/* Get local IP (after discovery) */
const char *voice_udp_get_local_ip(voice_udp_t *udp) {
    if (!udp) return NULL;
    return udp->local_ip;
}

/* Get local port (after discovery) */
uint16_t voice_udp_get_local_port(voice_udp_t *udp) {
    if (!udp) return 0;
    return udp->local_port;
}
