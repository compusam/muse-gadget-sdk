/*
 * Cartesia text-to-speech for Muse replies (community addition). See the header.
 */

#include "muse_cartesia.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "muse_mem.h"
#include "nvs.h"

static const char *TAG = "muse_cartesia";

#define NS "cartesia"
#define URL "https://api.cartesia.ai/tts/bytes"
#define API_VERSION "2026-08-14"
#define MODEL "sonic-3.6"
#define MP3_CAP (192 * 1024)   /* 32 kbps: ~48 s of speech */
#define TEXT_CAP 700           /* bytes of reply text spoken */
#define KEY_MAX 96
#define VOICE_MAX 64

static char s_key[KEY_MAX];
static char s_voice[VOICE_MAX];
static bool s_loaded;

static _Atomic int s_state = MUSE_CARTESIA_IDLE;
static uint8_t *s_buf;
static _Atomic size_t s_len;   /* bytes of MP3 so far: written by the worker, read by the session */
static int64_t s_t0;           /* when the request started, for the log */
static char *s_text;

static void load(void)
{
    if (s_loaded) {
        return;
    }
    s_loaded = true;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t n = sizeof(s_key);
    nvs_get_str(h, "key", s_key, &n);
    n = sizeof(s_voice);
    nvs_get_str(h, "voice", s_voice, &n);
    nvs_close(h);
}

static bool save(const char *name, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = nvs_set_str(h, name, value) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

void muse_cartesia_init(void)
{
    load();
}

bool muse_cartesia_ready(void)
{
    /* Never reads NVS here: this runs on the chat session's task, whose stack is in PSRAM,
     * and a flash read with its stack in PSRAM aborts (cache disabled). init() loaded it at boot. */
    return s_key[0] && s_voice[0];
}

/* Plain speech: no markdown marks, emoji or links, cut at a sentence end if possible. */
static void clean(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < cap; i++) {
        unsigned char c = in[i];
        if (c >= 0xF0) {            /* 4-byte UTF-8: emoji */
            i += 3;
            continue;
        }
        if (c == '*' || c == '#' || c == '`' || c == '_' || c == '~' || c == '\r') {
            continue;
        }
        if (c == '\n') {
            c = ' ';
        }
        out[o++] = c;
    }
    out[o] = '\0';
    if (o >= cap - 1) {             /* cut: back to a sentence end, then to a character start */
        size_t cut = o;
        while (cut > cap / 2 && !strchr(".!?", out[cut - 1])) {
            cut--;
        }
        if (cut <= cap / 2) {
            cut = o;
            while (cut && (out[cut] & 0xC0) == 0x80) {
                cut--;
            }
        }
        out[cut] = '\0';
    }
}

static esp_err_t on_http(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA && esp_http_client_get_status_code(evt->client) == 200) {
        size_t len = atomic_load(&s_len);
        size_t n = evt->data_len;
        if (len + n > MP3_CAP) {
            n = MP3_CAP - len;   /* a long reply is cut, not dropped */
        }
        if (!len) {
            ESP_LOGI(TAG, "first audio after %d ms", (int)((esp_timer_get_time() - s_t0) / 1000));
        }
        memcpy(s_buf + len, evt->data, n);
        atomic_store(&s_len, len + n);   /* the session may decode what's here while the rest arrives */
    }
    return ESP_OK;
}

static bool fetch(const char *text)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "model_id", MODEL);
    cJSON_AddStringToObject(body, "transcript", text);
    cJSON *voice = cJSON_AddObjectToObject(body, "voice");
    cJSON_AddStringToObject(voice, "id", s_voice);
    cJSON_AddStringToObject(body, "language", "es");
    cJSON *fmt = cJSON_AddObjectToObject(body, "output_format");
    cJSON_AddStringToObject(fmt, "container", "mp3");
    cJSON_AddNumberToObject(fmt, "sample_rate", 24000);
    cJSON_AddNumberToObject(fmt, "bit_rate", 32000);
    char *json = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!json) {
        return false;
    }

    char auth[KEY_MAX + 8];
    snprintf(auth, sizeof(auth), "Bearer %s", s_key);
    esp_http_client_config_t cfg = {
        .url = URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 20000,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
        .event_handler = on_http,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    bool ok = false;
    if (c) {
        esp_http_client_set_header(c, "Authorization", auth);
        esp_http_client_set_header(c, "Cartesia-Version", API_VERSION);
        esp_http_client_set_header(c, "Content-Type", "application/json");
        esp_http_client_set_post_field(c, json, strlen(json));
        esp_err_t err = esp_http_client_perform(c);
        int status = esp_http_client_get_status_code(c);
        ok = err == ESP_OK && status == 200 && atomic_load(&s_len) > 0;
        if (!ok) {
            ESP_LOGW(TAG, "request failed: %s, HTTP %d", esp_err_to_name(err), status);
        }
        esp_http_client_cleanup(c);
    }
    memset(auth, 0, sizeof(auth));
    free(json);
    return ok;
}

static void worker(void *arg)
{
    bool ok = fetch(s_text);
    ESP_LOGI(TAG, "%s: %u bytes of MP3 in %d ms", ok ? "speech ready" : "no speech", (unsigned)atomic_load(&s_len),
             (int)((esp_timer_get_time() - s_t0) / 1000));
    atomic_store(&s_state, ok ? MUSE_CARTESIA_DONE : MUSE_CARTESIA_FAIL);
    vTaskDeleteWithCaps(NULL);
}

bool muse_cartesia_request(const char *text)
{
    if (!muse_cartesia_ready() || !text || !text[0]) {
        return false;
    }
    int st = atomic_load(&s_state);
    if (st == MUSE_CARTESIA_BUSY) {
        return false;
    }
    if (!s_buf) {
        s_buf = heap_caps_malloc(MP3_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_text = heap_caps_malloc(TEXT_CAP + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_buf || !s_text) {
            ESP_LOGW(TAG, "no memory for speech");
            return false;
        }
    }
    clean(text, s_text, TEXT_CAP + 1);
    if (!s_text[0]) {
        return false;
    }
    atomic_store(&s_len, 0);
    s_t0 = esp_timer_get_time();
    atomic_store(&s_state, MUSE_CARTESIA_BUSY);
    if (xTaskCreateWithCaps(worker, "cartesia", 12288, NULL, 3, NULL, MUSE_BIG_CAPS) != pdPASS) {
        atomic_store(&s_state, MUSE_CARTESIA_IDLE);
        return false;
    }
    return true;
}

muse_cartesia_state_t muse_cartesia_state(void)
{
    return (muse_cartesia_state_t)atomic_load(&s_state);
}

const uint8_t *muse_cartesia_data(size_t *len)
{
    *len = atomic_load(&s_len);
    return s_buf;
}

void muse_cartesia_release(void)
{
    if (atomic_load(&s_state) != MUSE_CARTESIA_BUSY) {
        atomic_store(&s_state, MUSE_CARTESIA_IDLE);
    }
}

bool muse_cartesia_command(const char *name, const char *value, const char **result)
{
    if (strncmp(name, "cartesia.", 9)) {
        return false;
    }
    load();
    const char *what = name + 9;
    *result = "ok";
    if (!strcmp(what, "key") || !strcmp(what, "voice")) {
        bool key = what[0] == 'k';
        char *dst = key ? s_key : s_voice;
        size_t max = key ? KEY_MAX : VOICE_MAX;
        if (strlen(value) >= max) {
            *result = "error: too long";
        } else if (!save(what, value)) {
            *result = "error: could not save";
        } else {
            strlcpy(dst, value, max);
        }
    } else if (!strcmp(what, "test")) {
        if (!muse_cartesia_request(value[0] ? value : "Hola, soy Muse. Ya puedo hablar en español.")) {
            *result = "error: not ready or busy";
        } else {
            /* Test only checks the fetch; the chat session plays replies. */
            *result = "fetching, see the log";
        }
    } else {
        *result = "error: unknown cartesia command";
    }
    return true;
}
