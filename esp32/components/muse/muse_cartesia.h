/*
 * Cartesia text-to-speech for Muse replies (community addition).
 *
 * The reply text is sent to https://api.cartesia.ai/tts/bytes on a worker
 * task and comes back as one MP3 that the chat session decodes and plays.
 * The API key and voice id live in NVS (namespace "cartesia"); set them from
 * the serial console with ">cartesia.key=sk_car_..." and ">cartesia.voice=ID".
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MUSE_CARTESIA_IDLE,
    MUSE_CARTESIA_BUSY,   /* the worker is fetching */
    MUSE_CARTESIA_DONE,   /* muse_cartesia_data() is valid */
    MUSE_CARTESIA_FAIL,
} muse_cartesia_state_t;

/* True when a key and a voice are saved. */
bool muse_cartesia_ready(void);

/* Serial console: "key", "voice", "test" (speaks a sample line), returns false for other names. */
bool muse_cartesia_command(const char *name, const char *value, const char **result);

/* Starts fetching `text` (copied). False if not configured or a fetch is still running. */
bool muse_cartesia_request(const char *text);
muse_cartesia_state_t muse_cartesia_state(void);
/* The finished MP3; valid until the next request or muse_cartesia_release(). */
const uint8_t *muse_cartesia_data(size_t *len);
void muse_cartesia_release(void);
