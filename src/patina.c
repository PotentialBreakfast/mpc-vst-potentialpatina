/* PotentialPatina (PotentialBreaks): a retro-colour effect for MPC OS standalone devices.
 * Six modules in one insert, each with an on/off switch, and one MAGNITUDE control that scales them all:
 *   NOISE   vinyl crackle + surface hiss / tape hiss / mains hum (50 Hz), mixed in before the rest
 *   WOBBLE  wow and flutter: a modulated delay line (slow wow, fast flutter, random drift)
 *   DRIVE   saturation: tape, tube or cassette
 *   DIGITAL bit depth and sample-rate reduction
 *   TONE    tilt EQ (dark <-> bright)
 *   SPACE   a small Freeverb-style room
 * then dry/wet MIX and OUTPUT. 44.1 kHz, interleaved int16 stereo, 128-frame blocks (wrapper/engine.h).
 * 1.2.0: ERA (one tap sets all six modules for a sound of its time), AGE (clean to fully worn in one move: the modules come
 * in one after another as it rises, 50 = as set, 100 = well past it), and DUCK (the noise ducks under the music). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "../shared/looks/potential_theme.h"   /* the LOOK switch (parameter "theme") */

#define FS 44100.0f
#define PI_F 3.14159265f

/* ---- parameters: same order and keys as vst/params.json ---- */
enum {
    P_MAG, P_MIX, P_OUT,
    P_NOISE_ON, P_NOISE_TYPE, P_NOISE_AMT,
    P_WOB_ON, P_WOB_AMT, P_WOB_RATE,
    P_DRV_ON, P_DRV_TYPE, P_DRV_AMT,
    P_DIG_ON, P_DIG_BITS, P_DIG_RATE,
    P_TONE_ON, P_TONE_TILT,
    P_SPC_ON, P_SPC_AMT, P_SPC_SIZE,
    P_ERA, P_AGE, P_DUCK,   /* 1.2.0 (after LOOK, DICE and WILD in params.json) */
    NP
};
static const struct { const char *key; float def; } PDEF[NP] = {
    {"magnitude", 100}, {"mix", 85}, {"output", 0},
    {"noise_on", 1}, {"noise_type", 0}, {"noise_amt", 19},
    {"wobble_on", 1}, {"wobble_amt", 18}, {"wobble_rate", 30},
    {"drive_on", 1}, {"drive_type", 0}, {"drive_amt", 23},
    {"digital_on", 0}, {"digital_bits", 12}, {"digital_rate", 26},
    {"tone_on", 1}, {"tone_tilt", -12},
    {"space_on", 0}, {"space_amt", 25}, {"space_size", 50},
    {"era", 0}, {"age", 50}, {"noise_duck", 0},
};

/* ERA: the six modules set for a sound of its time (P_MAG .. P_SPC_SIZE). Option 0 is Patina's own default. */
#define NERA 8
static const float ERA[NERA][P_ERA] = {
    /* mag mix out  noise: on type amt  wobble: on amt rate  drive: on type amt  digital: on bits rate  tone: on tilt  space: on amt size */
    {100, 85, 0,  1, 0, 19,  1, 18, 30,  1, 0, 23,  0, 12, 26,  1, -12,  0, 18, 50},   /* PATINA */
    {100, 90, 0,  1, 0, 48,  1, 22, 25,  1, 1, 34,  1, 14, 16,  1, -55,  0, 18, 50},   /* '50s 78: shellac crackle, narrow and dark */
    {100, 85, 0,  1, 0, 30,  1, 16, 22,  1, 1, 26,  0, 12, 26,  1, -28,  1, 12, 35},   /* '60s 45: vinyl, valve warmth, a little room */
    {100, 85, 0,  1, 1, 18,  1, 26, 35,  1, 0, 36,  0, 12, 26,  1, -14,  0, 18, 50},   /* '70s TAPE: reel-to-reel hiss and wow */
    {100, 85, 0,  0, 0, 19,  0, 18, 30,  1, 1, 14,  1, 12, 26,  1,  -8,  0, 18, 50},   /* '80s SAMPLER: 12-bit, 26 kHz */
    {100, 85, 0,  1, 1, 24,  1, 28, 58,  1, 2, 34,  0, 12, 26,  1, -20,  0, 18, 50},   /* '90s CASSETTE: hiss, flutter, dull top */
    {100, 90, 0,  1, 2,  9,  1,  8, 70,  1, 1, 40,  1, 14,  9,  1,  10,  0, 18, 50},   /* AM RADIO: narrow, buzzy */
    {100, 85, 0,  1, 1, 20,  1, 36, 12,  1, 2, 24,  0, 12, 26,  1, -34,  1, 10, 30},   /* VHS: slow warble, soft and dark */
};

/* ---- small DSP pieces ---- */
static inline float frand(uint32_t *s) {   /* xorshift32 -> -1..1 */
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}
static inline float urand(uint32_t *s) { return 0.5f * (frand(s) + 1.0f); }   /* 0..1 */
static inline float onepole_k(float hz) { return 1.0f - expf(-2.0f * PI_F * hz / FS); }

#define DRV_COMP 1.0f
#define WOB_LEN 4096            /* delay line for wobble (> 2 x the largest delay) */
#define WOB_BASE 300.0f         /* centre delay, samples (~6.8 ms) */

/* Freeverb tunings (44.1 kHz), stereo spread on the right */
static const int COMB_T[4] = {1116, 1188, 1277, 1356};
static const int AP_T[2] = {556, 441};
#define SPREAD 23
typedef struct { float buf[1356 + SPREAD]; int len, pos; float store; } comb_t;
typedef struct { float buf[556 + SPREAD]; int len, pos; } ap_t;

typedef struct {
    float p[NP];                       /* parameter values in their own units */
    /* smoothed controls */
    float s_mag, s_mix, s_out, s_noise, s_wob, s_wrate, s_drv, s_bits, s_rate, s_tilt, s_spc, s_size;
    float g_noise, g_wob, g_drv, g_dig, g_tone, g_spc;     /* smoothed on/off gains (click-free switching) */
    uint32_t rng[3];
    /* noise */
    float hiss_lp[2], hiss_hp[2], crack_env, crack_sign, pop_env, pop_lp, rumble, hum_ph;
    /* wobble */
    float wbuf[2][WOB_LEN], dbuf[2][WOB_LEN]; int wpos; float wow_ph, flut_ph, drift, drift_target; int drift_count;
    /* drive / cassette filter */
    float cas_lp[2];
    /* digital */
    float hold[2], hold_ph;
    /* tone */
    float tilt_lp[2];
    /* space */
    comb_t comb[2][4]; ap_t ap[2][2];
    pt_theme_t theme;                  /* which look MPC loads next time: kept out of reset() and of the saved state */
    int wild;                          /* DICE: 0 MILD, 1 MEDIUM, 2 WILD (saved) */
    uint32_t pending;                  /* parameters a roll changed, to report to MPC (bit i = parameter i) */
    uint32_t dice;                     /* the dice's own random state */
    float s_age, duck_env;             /* AGE (smoothed), DUCK's level follower */
} patina_t;

static void reset(patina_t *s) {
    pt_theme_t theme = s->theme;
    memset(s, 0, sizeof *s);
    s->theme = theme;
    for (int i = 0; i < NP; i++) s->p[i] = PDEF[i].def;
    s->rng[0] = 0x1234567u; s->rng[1] = 0x89abcdefu; s->rng[2] = 0x2468aceu;
    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < 4; i++) s->comb[c][i].len = COMB_T[i] + (c ? SPREAD : 0);
        for (int i = 0; i < 2; i++) s->ap[c][i].len = AP_T[i] + (c ? SPREAD : 0);
    }
    s->drift_target = 0; s->crack_sign = 1;
    /* start the smoothed values at their targets */
    s->s_mag = s->p[P_MAG] / 100.0f; s->s_mix = s->p[P_MIX] / 100.0f; s->s_out = powf(10.0f, s->p[P_OUT] / 20.0f);
    s->s_bits = 16.0f; s->s_rate = 44.1f; s->s_wrate = s->p[P_WOB_RATE] / 100.0f; s->s_size = s->p[P_SPC_SIZE] / 100.0f;
    s->wild = 1; s->dice = 0x9e3779b9u ^ (uint32_t)(uintptr_t)s;
    s->s_age = s->p[P_AGE] / 100.0f;
}

static void apply_era(patina_t *s, int n) {
    if (n < 0 || n >= NERA) return;
    for (int i = 0; i < P_ERA; i++) s->p[i] = ERA[n][i];
    s->p[P_ERA] = (float)n;
    s->pending |= (1u << P_ERA) - 1u;   /* every module setting, for MPC's screen */
}

/* AGE: how much of a module's setting it gets, a = AGE 0..1. The modules come in one after another (tone first, space
   last) up to 50 %, which is the setting as it is; from there on they all keep going, to 1.8 times at 100 %. */
static inline float age_f(float a, float start) {
    if (a <= 0.5f) { float f = (a - start) / (0.5f - start); return f < 0 ? 0 : f > 1 ? 1 : f; }
    return 1.0f + (a - 0.5f) * 1.6f;
}
static inline float cap1(float x) { return x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x; }

/* ---- DICE: ROLL makes a new patina; WILD (MILD / MEDIUM / WILD) says how far it goes ---- */
static float dice_u(patina_t *s) { uint32_t x = s->dice; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s->dice = x; return (x >> 8) / 16777216.0f; }
static float dice_in(patina_t *s, float lo, float hi) { return roundf(lo + (hi - lo) * dice_u(s)); }
static void roll(patina_t *s) {
    int w = s->wild;
    /* modules: noise, wobble, drive, digital, tone, space; MILD keeps to the gentle four */
    enum { NMOD = 6 };
    static const int MOD_ON[6] = {P_NOISE_ON, P_WOB_ON, P_DRV_ON, P_DIG_ON, P_TONE_ON, P_SPC_ON};
    static const int CHANCE[3][6] = {{60, 55, 50, 0, 60, 0}, {65, 60, 60, 25, 60, 30}, {85, 80, 80, 55, 70, 55}};
    static const float AMT[3][2] = {{8, 35}, {15, 60}, {35, 100}};
    int on = 0;
    for (int tries = 0; tries < 20 && on < 2 + w; tries++) {   /* at least 2 / 3 / 4 modules on */
        on = 0;
        for (int m = 0; m < NMOD; m++) { s->p[MOD_ON[m]] = dice_u(s) * 100.0f < (float)CHANCE[w][m] ? 1.0f : 0.0f; on += (int)s->p[MOD_ON[m]]; }
    }
    float lo = AMT[w][0], hi = AMT[w][1];
    s->p[P_NOISE_TYPE] = (float)(int)(dice_u(s) * (w ? 3.0f : 2.0f));   /* MILD: vinyl or tape, no hum */
    s->p[P_NOISE_AMT] = dice_in(s, lo, hi);
    s->p[P_WOB_AMT] = dice_in(s, lo, hi);
    s->p[P_WOB_RATE] = dice_in(s, 10, w == 2 ? 100 : 60);
    s->p[P_DRV_TYPE] = (float)(int)(dice_u(s) * 3.0f);
    s->p[P_DRV_AMT] = dice_in(s, lo, hi);
    s->p[P_DIG_BITS] = dice_in(s, w == 2 ? 4 : 8, w == 2 ? 10 : 14);
    s->p[P_DIG_RATE] = dice_in(s, w == 2 ? 6 : 12, w == 2 ? 22 : 32);
    s->p[P_TONE_TILT] = dice_in(s, w == 0 ? -30 : w == 1 ? -50 : -100, w == 0 ? 10 : w == 1 ? 30 : 60);
    s->p[P_SPC_AMT] = dice_in(s, lo, hi * 0.7f);
    s->p[P_SPC_SIZE] = dice_in(s, 20, 80);
    s->p[P_MAG] = dice_in(s, w == 0 ? 60 : 80, 100);
    s->p[P_MIX] = dice_in(s, w == 0 ? 60 : 75, w == 0 ? 85 : 100);
    s->pending |= ((1u << P_ERA) - 1u) & ~(1u << P_OUT);   /* every module setting, not OUTPUT */
}

int mpc_engine_changed(void *inst, char *key, int len) {
    patina_t *s = inst;
    for (int i = 0; i < NP; i++)
        if (s->pending & (1u << i)) { s->pending &= ~(1u << i); snprintf(key, len, "%s", PDEF[i].key); return 1; }
    return 0;
}

static void *create(const char *data_dir) {
    patina_t *s = calloc(1, sizeof *s);
    if (s) { reset(s); pt_theme_init(&s->theme, data_dir); }
    return s;
}
static void destroy(void *inst) { free(inst); }
static void midi(void *inst, const uint8_t *msg, int len) { (void)inst; (void)msg; (void)len; }

static int find(const char *key) {
    for (int i = 0; i < NP; i++) if (!strcmp(key, PDEF[i].key)) return i;
    return -1;
}

static void set_param(void *inst, const char *key, const char *val) {
    patina_t *s = inst;
    if (pt_theme_set(&s->theme, key, val)) return;
    if (!strcmp(key, "state")) {   /* "key=value;key=value;..." */
        char tmp[1024]; snprintf(tmp, sizeof tmp, "%s", val);
        s->p[P_AGE] = 50; s->p[P_DUCK] = 0; s->p[P_ERA] = 0;   /* a project from before 1.2.0 sounds as it did */
        for (char *tok = strtok(tmp, ";"); tok; tok = strtok(NULL, ";")) {
            char *eq = strchr(tok, '=');
            if (!eq) continue;
            *eq = 0;
            int i = find(tok);
            if (i >= 0) s->p[i] = (float)atof(eq + 1);
            else if (!strcmp(tok, "wild")) { int w = atoi(eq + 1); s->wild = w < 0 ? 0 : w > 2 ? 2 : w; }
        }
        return;
    }
    if (!strcmp(key, "dice")) { if (atof(val) > 0.5) roll(s); return; }   /* a trigger: each press rolls */
    if (!strcmp(key, "wild")) { int w = (int)lroundf((float)atof(val)); s->wild = w < 0 ? 0 : w > 2 ? 2 : w; return; }
    if (!strcmp(key, "era")) {   /* only on a change: a project reload re-sending the same era keeps its tweaks */
        int n = (int)lroundf((float)atof(val));
        if (n != (int)lroundf(s->p[P_ERA])) apply_era(s, n);
        return;
    }
    int i = find(key);
    if (i >= 0) s->p[i] = (float)atof(val);
}

static int get_param(void *inst, const char *key, char *buf, int buf_len) {
    patina_t *s = inst;
    { int n = pt_theme_get(&s->theme, key, buf, buf_len); if (n) return n; }
    if (!strcmp(key, "state")) {
        int n = 0;
        for (int i = 0; i < NP && n < buf_len; i++) n += snprintf(buf + n, buf_len - n, "%s=%g;", PDEF[i].key, (double)s->p[i]);
        if (n < buf_len) n += snprintf(buf + n, buf_len - n, "wild=%d;", s->wild);
        return n;
    }
    if (!strcmp(key, "dice")) return snprintf(buf, buf_len, "0");
    if (!strcmp(key, "wild")) return snprintf(buf, buf_len, "%d", s->wild);
    int i = find(key);
    if (i < 0) return 0;
    if (i == P_NOISE_ON || i == P_NOISE_TYPE || i == P_WOB_ON || i == P_DRV_ON || i == P_DRV_TYPE ||
        i == P_DIG_ON || i == P_DIG_BITS || i == P_TONE_ON || i == P_SPC_ON || i == P_ERA)
        return snprintf(buf, buf_len, "%d", (int)lroundf(s->p[i]));
    return snprintf(buf, buf_len, "%g", (double)s->p[i]);
}

static void render(void *inst, int16_t *out_lr, int frames) {   /* an effect: silence without input */
    (void)inst;
    memset(out_lr, 0, (size_t)frames * 2 * sizeof(int16_t));
}

static inline float smooth(float cur, float target, float k) { return cur + (target - cur) * k; }

/* saturators, all roughly unity gain for small signals */
static inline float sat_tape(float x) { return tanhf(x); }
static inline float sat_tube(float x) {        /* asymmetric: softer on the positive side, even harmonics */
    return x >= 0 ? 1.0f - expf(-x) : -(1.0f - expf(1.25f * x)) / 1.25f;
}

static void process(void *inst, const int16_t *in_lr, int16_t *out_lr, int frames) {
    patina_t *s = inst;
    const float k = 0.25f;   /* per-block smoothing (~3 ms blocks) */
    float mag_t = s->p[P_MAG] / 100.0f;
    s->s_mag = smooth(s->s_mag, mag_t, k);
    float m = s->s_mag;
    s->s_age = smooth(s->s_age, s->p[P_AGE] / 100.0f, k);
    const float ag = s->s_age;
    const float f_tone = age_f(ag, 0.0f), f_drv = age_f(ag, 0.05f), f_noise = age_f(ag, 0.10f), f_wob = age_f(ag, 0.15f),
                f_dig = age_f(ag, 0.25f), f_spc = age_f(ag, 0.25f);
    s->s_mix = smooth(s->s_mix, s->p[P_MIX] / 100.0f, k);
    s->s_out = smooth(s->s_out, powf(10.0f, s->p[P_OUT] / 20.0f), k);
    s->s_noise = smooth(s->s_noise, cap1(s->p[P_NOISE_AMT] / 100.0f * m * f_noise), k);
    s->s_wob = smooth(s->s_wob, cap1(s->p[P_WOB_AMT] / 100.0f * m * f_wob), k);
    s->s_wrate = smooth(s->s_wrate, s->p[P_WOB_RATE] / 100.0f, k);
    s->s_drv = smooth(s->s_drv, cap1(s->p[P_DRV_AMT] / 100.0f * m * f_drv), k);
    /* digital: magnitude 0 = clean (16 bit, 44.1 kHz), 1 = the knobs' settings; AGE past 50 % takes it further */
    float md = m * f_dig, mdc = md > 1 ? 1 : md, over = md > 1 ? (md - 1.0f) / 0.8f : 0;
    float bits_t = 16.0f - (16.0f - s->p[P_DIG_BITS]) * mdc - over * 0.5f * (s->p[P_DIG_BITS] - 4.0f);
    float rate_t = 44.1f - (44.1f - s->p[P_DIG_RATE]) * mdc - over * 0.5f * (s->p[P_DIG_RATE] - 4.0f);
    s->s_bits = smooth(s->s_bits, bits_t, k);
    s->s_rate = smooth(s->s_rate, rate_t, k);
    s->s_tilt = smooth(s->s_tilt, cap1(s->p[P_TONE_TILT] / 100.0f * m * f_tone), k);
    s->s_spc = smooth(s->s_spc, cap1(s->p[P_SPC_AMT] / 100.0f * m * f_spc), k);
    s->s_size = smooth(s->s_size, s->p[P_SPC_SIZE] / 100.0f, k);
    s->g_noise = smooth(s->g_noise, s->p[P_NOISE_ON] > 0.5f, k);
    s->g_wob = smooth(s->g_wob, s->p[P_WOB_ON] > 0.5f, k);
    s->g_drv = smooth(s->g_drv, s->p[P_DRV_ON] > 0.5f, k);
    s->g_dig = smooth(s->g_dig, s->p[P_DIG_ON] > 0.5f, k);
    s->g_tone = smooth(s->g_tone, s->p[P_TONE_ON] > 0.5f, k);
    s->g_spc = smooth(s->g_spc, s->p[P_SPC_ON] > 0.5f, k);

    const int ntype = (int)lroundf(s->p[P_NOISE_TYPE]);
    const int dtype = (int)lroundf(s->p[P_DRV_TYPE]);
    const float na = s->s_noise * s->g_noise;
    /* noise levels (amplitude, full scale = 1) */
    const float hiss_amp = (ntype == 1 ? 0.045f : 0.018f) * na * na + (ntype == 1 ? 0.008f : 0.003f) * na;
    const float crack_rate = (ntype == 0) ? (1.5f + 28.0f * na) / FS : 0;           /* clicks per sample */
    const float crack_amp = 0.35f * na;
    const float hum_amp = (ntype == 2) ? 0.05f * na : 0;
    const float k_hiss_lp = onepole_k(ntype == 1 ? 9000.0f : 4500.0f), k_hiss_hp = onepole_k(ntype == 1 ? 1500.0f : 250.0f);
    /* DUCK: the noise goes down while the music plays (up to about 26 dB at 100 %, from about -45 dBFS of music) */
    const float duck = s->p[P_DUCK] / 100.0f;
    const float k_da = 1.0f - expf(-1.0f / (0.005f * FS)), k_dr = 1.0f - expf(-1.0f / (0.25f * FS));

    /* wobble: depth in samples; rate morphs from slow wow to fast flutter */
    const float wa = s->s_wob;
    const float wow_hz = 0.35f + 1.4f * s->s_wrate, flut_hz = 4.5f + 8.0f * s->s_wrate;
    const float wow_depth = wa * 70.0f * (1.0f - 0.6f * s->s_wrate);   /* up to ~1.6 ms */
    const float flut_depth = wa * (2.0f + 10.0f * s->s_wrate);
    const float drift_depth = wa * 40.0f;

    /* drive */
    const float da = s->s_drv;
    const float drive = 1.0f + 14.0f * da * da + 3.0f * da;
    /* level match: unity for quiet signals, then pull back as the drive compresses (roughly constant loudness) */
    const float makeup = 1.0f / ((dtype == 1 ? sat_tube(drive * 0.35f) / 0.35f : tanhf(drive * 0.35f) / 0.35f) * (1.0f + DRV_COMP * da));
    const float k_cas = onepole_k(dtype == 2 ? 13000.0f - 7000.0f * da : 20000.0f);

    /* digital */
    const float q = powf(2.0f, s->s_bits - 1.0f);
    const float hold_step = s->s_rate * 1000.0f / FS;

    /* tone: tilt around ~800 Hz. Dark: highs down up to 12 dB, lows up 2 dB. Bright: highs up 6 dB, lows down 3 dB */
    const float tt = s->s_tilt * s->g_tone;
    const float g_hi = tt < 0 ? powf(10.0f, 12.0f * tt / 20.0f) : powf(10.0f, 6.0f * tt / 20.0f);
    const float g_lo = tt < 0 ? powf(10.0f, -2.0f * tt / 20.0f) : powf(10.0f, -3.0f * tt / 20.0f);
    const float k_tilt = onepole_k(800.0f);

    /* space */
    const float fb = 0.70f + 0.26f * s->s_size, damp = 0.25f + 0.25f * (1.0f - s->s_size);
    const float spc_wet = 0.9f * s->s_spc * s->g_spc;

    for (int n = 0; n < frames; n++) {
        float dry[2] = {in_lr[2 * n] / 32768.0f, in_lr[2 * n + 1] / 32768.0f};
        float x[2] = {dry[0], dry[1]};

        /* ---- NOISE (added first, so the rest colours it too) ---- */
        float lvl = fabsf(dry[0]) > fabsf(dry[1]) ? fabsf(dry[0]) : fabsf(dry[1]);
        s->duck_env += (lvl - s->duck_env) * (lvl > s->duck_env ? k_da : k_dr);
        if (na > 1e-5f) {
            float dg = 1.0f;
            if (duck > 0) {
                float db = 20.0f * log10f(s->duck_env + 1e-6f);
                float r = (db + 45.0f) / 30.0f; r = r < 0 ? 0 : r > 1 ? 1 : r;
                dg = 1.0f - 0.95f * duck * r;
            }
            float add_mono = 0;
            if (ntype == 0) {
                if (urand(&s->rng[2]) < crack_rate) {   /* a new click */
                    s->crack_env = crack_amp * (0.15f + 0.85f * powf(urand(&s->rng[2]), 3.0f));
                    s->crack_sign = frand(&s->rng[2]) > 0 ? 1.0f : -1.0f;
                    if (urand(&s->rng[2]) < 0.08f) s->pop_env = crack_amp * 0.6f;   /* the odd bigger, duller pop */
                }
                float click = s->crack_env * s->crack_sign * (0.6f + 0.4f * frand(&s->rng[2]));
                s->crack_env *= 0.80f;                    /* ~0.1 ms clicks */
                s->pop_lp += (s->pop_env * frand(&s->rng[2]) - s->pop_lp) * 0.15f;
                s->pop_env *= 0.995f;                     /* ~5 ms pops */
                s->rumble += (frand(&s->rng[2]) * 0.004f * na - s->rumble) * 0.002f;
                add_mono = click + s->pop_lp + s->rumble * 3.0f;
            } else if (ntype == 2) {
                s->hum_ph += 50.0f / FS; if (s->hum_ph >= 1.0f) s->hum_ph -= 1.0f;
                float ph = 2.0f * PI_F * s->hum_ph;
                add_mono = hum_amp * (0.7f * sinf(ph) + 0.25f * sinf(2 * ph) + 0.12f * fabsf(sinf(3 * ph)) - 0.076f);
            }
            for (int c = 0; c < 2; c++) {
                float w = frand(&s->rng[c]);
                s->hiss_lp[c] += (w - s->hiss_lp[c]) * k_hiss_lp;
                s->hiss_hp[c] += (s->hiss_lp[c] - s->hiss_hp[c]) * k_hiss_hp;
                float hiss = (s->hiss_lp[c] - s->hiss_hp[c]) * hiss_amp * 4.0f;
                x[c] += (hiss + add_mono * (c ? 0.92f : 1.0f)) * dg;
            }
        }

        /* ---- WOBBLE ---- */
        {
            s->wow_ph += wow_hz / FS; if (s->wow_ph >= 1.0f) s->wow_ph -= 1.0f;
            s->flut_ph += flut_hz / FS; if (s->flut_ph >= 1.0f) s->flut_ph -= 1.0f;
            if (--s->drift_count <= 0) { s->drift_count = 8000 + (int)(urand(&s->rng[2]) * 12000); s->drift_target = frand(&s->rng[2]); }
            s->drift += (s->drift_target - s->drift) * 0.00005f;
            float d = WOB_BASE + wow_depth * sinf(2 * PI_F * s->wow_ph) + flut_depth * sinf(2 * PI_F * s->flut_ph) + drift_depth * s->drift;
            int w = s->wpos;
            s->wbuf[0][w] = x[0]; s->wbuf[1][w] = x[1];
            s->dbuf[0][w] = dry[0]; s->dbuf[1][w] = dry[1];
            {   /* the dry signal, delayed by the same centre amount as the wet path (no comb filtering at MIX < 100%) */
                int rd = (w - (int)WOB_BASE + WOB_LEN) & (WOB_LEN - 1);
                dry[0] = s->dbuf[0][rd]; dry[1] = s->dbuf[1][rd];
            }
            float rp = (float)w - d; while (rp < 0) rp += WOB_LEN;
            int i0 = (int)rp; float fr = rp - i0;
            int im1 = (i0 - 1 + WOB_LEN) & (WOB_LEN - 1), i1 = (i0 + 1) & (WOB_LEN - 1), i2 = (i0 + 2) & (WOB_LEN - 1);
            i0 &= WOB_LEN - 1;
            for (int c = 0; c < 2; c++) {   /* cubic Hermite */
                float y0 = s->wbuf[c][im1], y1 = s->wbuf[c][i0], y2 = s->wbuf[c][i1], y3 = s->wbuf[c][i2];
                float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                float wob = ((c3 * fr + c2) * fr + c1) * fr + y1;
                /* the plain path is delayed by the same centre amount, so switching wobble never jumps in time */
                float plain = s->wbuf[c][(w - (int)WOB_BASE + WOB_LEN) & (WOB_LEN - 1)];
                x[c] = plain + (wob - plain) * s->g_wob;
            }
            s->wpos = (w + 1) & (WOB_LEN - 1);
        }

        /* ---- DRIVE ---- */
        if (s->g_drv > 1e-4f && da > 1e-4f) {
            for (int c = 0; c < 2; c++) {
                float z = x[c] * drive;
                float y = (dtype == 1 ? sat_tube(z) : sat_tape(z + 0.04f * da) - sat_tape(0.04f * da)) * makeup;
                if (dtype == 2) { s->cas_lp[c] += (y - s->cas_lp[c]) * k_cas; y = s->cas_lp[c]; }
                x[c] += (y - x[c]) * s->g_drv;
            }
        }

        /* ---- DIGITAL ---- */
        if (s->g_dig > 1e-4f) {
            s->hold_ph += hold_step;
            int take = s->hold_ph >= 1.0f;
            if (take) s->hold_ph -= floorf(s->hold_ph);
            for (int c = 0; c < 2; c++) {
                if (take || hold_step >= 0.999f) s->hold[c] = roundf(x[c] * q) / q;
                x[c] += (s->hold[c] - x[c]) * s->g_dig;
            }
        }

        /* ---- TONE (tilt) ---- */
        for (int c = 0; c < 2; c++) {
            s->tilt_lp[c] += (x[c] - s->tilt_lp[c]) * k_tilt;
            float lo = s->tilt_lp[c], hi = x[c] - lo;
            x[c] = lo * g_lo + hi * g_hi;
        }

        /* ---- SPACE ---- */
        if (spc_wet > 1e-4f) {
            float inp = (x[0] + x[1]) * 0.015f;
            for (int c = 0; c < 2; c++) {
                float acc = 0;
                for (int i = 0; i < 4; i++) {
                    comb_t *cb = &s->comb[c][i];
                    float o = cb->buf[cb->pos];
                    cb->store = o * (1 - damp) + cb->store * damp;
                    cb->buf[cb->pos] = inp + cb->store * fb;
                    if (++cb->pos >= cb->len) cb->pos = 0;
                    acc += o;
                }
                for (int i = 0; i < 2; i++) {
                    ap_t *a = &s->ap[c][i];
                    float b = a->buf[a->pos];
                    a->buf[a->pos] = acc + b * 0.5f;
                    if (++a->pos >= a->len) a->pos = 0;
                    acc = b - acc;
                }
                x[c] += acc * spc_wet * 2.0f;
            }
        }

        /* ---- MIX + OUTPUT ---- */
        for (int c = 0; c < 2; c++) {
            float y = (dry[c] + (x[c] - dry[c]) * s->s_mix) * s->s_out;
            if (!(y == y)) y = 0;                                  /* never pass a NaN on */
            /* gentle safety limiter above -1 dBFS, then a hard stop */
            if (y > 0.89f) y = 0.89f + 0.11f * tanhf((y - 0.89f) / 0.11f);
            else if (y < -0.89f) y = -0.89f - 0.11f * tanhf((-y - 0.89f) / 0.11f);
            out_lr[2 * n + c] = (int16_t)lrintf(y * 32767.0f);
        }
    }
}

static const mpc_engine_t ENGINE = { create, destroy, midi, set_param, get_param, render, process, NULL };
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
