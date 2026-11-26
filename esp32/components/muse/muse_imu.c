/*
 * Motion reactions for Muse (community addition). See the header.
 */

#include "muse_imu.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "muse_board.h"
#include "muse_state.h"
#include "muse_voice.h"

static const char *TAG = "muse_imu";

#define TILT_DEAD 0.06f      /* tilt (sine of the angle) ignored below this */
#define TILT_GAIN 4.0f       /* a ~20 degree tilt turns the eyes fully */
#define SLIDE_GAIN 2.2f      /* sideways acceleration (g) that adds to it */
#define LOOK_ON 0.08f        /* gaze strength that counts as "looking" */
#define SHAKE_HIT 0.9f       /* a hard acceleration: this many g away from the baseline */
#define SHAKE_GAP 0.06f      /* seconds between two jolts */
#define SHAKE_WINDOW 0.7f    /* the jolts of one shake come within this long of each other */
#define SHAKE_COUNT 3        /* reversals that make a shake */

static bool s_debug;
static float s_last, s_dbg;
static float s_f[3];         /* accel, smoothed: the tilt */
static float s_slow[3];      /* accel, very slow: gravity, so the rest is movement */
static float s_slide;        /* smoothed sideways acceleration */
static float s_lw, s_jolt;
static float s_hit_t, s_chirp_t;
static int s_hits;
static float s_hard[3];   /* the last hard acceleration, away from the baseline */
static bool s_have_hard, s_shaking;
static bool s_have;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Shaken: the same chirp and lights as a tap on Muse (muse_ui.c), at most every 1.5 s. */
static void shake_reaction(float now)
{
    muse_state_make_happy();
    muse_mode_t mode = muse_state_mode(NULL);
    bool quiet = mode == MUSE_MODE_IDLE || mode == MUSE_MODE_ERROR;
    ESP_LOGI(TAG, "shaken: mode %d, chirp %s", (int)mode, quiet && now - s_chirp_t > 1.5f ? "yes" : "skipped");
    if (muse_board->audio_init && quiet && now - s_chirp_t > 1.5f) {
        s_chirp_t = now;
        muse_voice_request_chirp();
    }
    if (muse_board->set_led) {
        muse_board->set_led(true);   /* the rainbow runs until the hop settles */
    }
}

void muse_imu_apply(muse_pose_t *pose, float now)
{
    if (!muse_board || !muse_board->imu_read) {
        return;
    }
    float a[3], g[3];
    if (muse_board->imu_read(a, g) != ESP_OK) {
        return;
    }
    float dt = s_last > 0 ? clampf(now - s_last, 0.005f, 0.2f) : 0.04f;
    s_last = now;
    if (!s_have) {
        memcpy(s_f, a, sizeof(s_f));
        memcpy(s_slow, a, sizeof(s_slow));
        s_have = true;
    }
    float kf = 1.0f - expf(-dt * 25.0f), ks = 1.0f - expf(-dt * 0.8f);
    for (int i = 0; i < 3; i++) {
        s_f[i] += (a[i] - s_f[i]) * kf;
        s_slow[i] += (a[i] - s_slow[i]) * ks;
    }
    s_slide += ((a[0] - s_slow[0]) - s_slide) * (1.0f - expf(-dt * 20.0f));

    float mag = sqrtf(s_f[0] * s_f[0] + s_f[1] * s_f[1] + s_f[2] * s_f[2]);
    float norm = mag > 0.3f ? mag : 0.3f;
    float tx = s_f[0] / norm, ty = s_f[1] / norm;
    tx = tx > 0 ? fmaxf(tx - TILT_DEAD, 0) : fminf(tx + TILT_DEAD, 0);
    /* Tilt: the x axis points right, so tilting left (left edge down) reads positive; Muse looks
     * toward the low edge, like a ball rolling downhill. A sideways slide reads negative toward
     * the left, so it adds with its own sign. (Measured on a Core2: held left tilt gave ax +0.76.) */
    float look_x = clampf(-tx * TILT_GAIN + s_slide * SLIDE_GAIN, -1, 1);
    /* No vertical look from tilt: held upright is the normal pose and would stare up forever. */
    float look_y = 0;
    (void)ty;

    /* A shake is the movement reversing, hard, several times in a row: each time the acceleration
     * away from the baseline points the opposite way to the last hard one, that is a jolt. A
     * continuous high reading would never count, and carrying or tilting it reverses once at most. */
    float dyn[3] = { a[0] - s_slow[0], a[1] - s_slow[1], a[2] - s_slow[2] };
    float dm = sqrtf(dyn[0] * dyn[0] + dyn[1] * dyn[1] + dyn[2] * dyn[2]);
    if (now - s_hit_t > SHAKE_WINDOW) {
        s_hits = 0;
        s_have_hard = false;
    }
    if (dm > SHAKE_HIT) {
        if (s_have_hard && dyn[0] * s_hard[0] + dyn[1] * s_hard[1] + dyn[2] * s_hard[2] < 0 &&
            now - s_hit_t > SHAKE_GAP) {
            s_hits++;
            s_hit_t = now;
        } else if (!s_have_hard) {
            s_hit_t = now;
        }
        memcpy(s_hard, dyn, sizeof(s_hard));
        s_have_hard = true;
    }
    s_jolt = fmaxf(0.0f, s_jolt - dt * 1.2f);   /* the hop lasts about a second after the shake */
    if (s_hits >= SHAKE_COUNT) {
        s_jolt = 1.0f;
        if (!s_shaking) {
            s_shaking = true;
            shake_reaction(now);
        }
        s_hits = 0;
        s_have_hard = false;
    }
    if (s_shaking && s_jolt < 0.3f) {
        s_shaking = false;
        if (muse_board->set_led) {
            muse_board->set_led(false);
        }
    }

    bool active = fabsf(look_x) > LOOK_ON || fabsf(look_y) > LOOK_ON;
    s_lw = active ? fminf(s_lw + dt * 25.0f, 1.0f) : fmaxf(s_lw - dt * 6.0f, 0.0f);

    pose->look_x = look_x;
    pose->look_y = look_y;
    pose->look_w = s_lw;
    pose->lean = look_x * 0.7f * s_lw;
    pose->jolt = s_jolt;
    if (s_jolt > 0.6f) {
        muse_state_poke();   /* shaking it keeps it awake */
    }

    if (s_debug && now - s_dbg > 0.25f) {
        s_dbg = now;
        ESP_LOGI(TAG, "a % .2f % .2f % .2f g  g % 4.0f % 4.0f % 4.0f dps -> look % .2f % .2f jolt %.2f",
                 a[0], a[1], a[2], g[0], g[1], g[2], look_x, look_y, s_jolt);
    }
}

bool muse_imu_command(const char *name, const char *value, const char **result)
{
    if (strcmp(name, "imu.debug")) {
        return false;
    }
    s_debug = value[0] == '1';
    *result = "ok";
    return true;
}
