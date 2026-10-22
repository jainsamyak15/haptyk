// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <string.h>

#include <kf/kf_config.h>
#include <kf/kf_engine.h>

#include "engine/kf_engine_internal.h"
#include "keymap/kf_vk.h"
#include "keytap/kf_keytap_internal.h"
#include "kf_test.h"
#include "sensor/kf_sensor_internal.h"

#define MS 1000000ULL

void test_keytap_modifier_down_and_up(void) {
    bool down = false;
    KF_EXPECT(kf_keytap_modifier_change(KF_VK_SHIFT, 0u, KF_FLAG_LEFT_SHIFT, &down) && down);
    KF_EXPECT(kf_keytap_modifier_change(KF_VK_SHIFT, KF_FLAG_LEFT_SHIFT, 0u, &down) && !down);
    /* Right Shift pressed while left is held: only the right key changes. */
    const uint64_t both = KF_FLAG_LEFT_SHIFT | KF_FLAG_RIGHT_SHIFT;
    KF_EXPECT(kf_keytap_modifier_change(KF_VK_RIGHT_SHIFT, KF_FLAG_LEFT_SHIFT, both, &down) &&
              down);
    KF_EXPECT(!kf_keytap_modifier_change(KF_VK_SHIFT, KF_FLAG_LEFT_SHIFT, both, &down));
    /* Every modifier key maps to its own bit. */
    const struct {
        uint16_t key;
        uint64_t flag;
    } keys[] = {
        {KF_VK_CONTROL, KF_FLAG_LEFT_CONTROL}, {KF_VK_RIGHT_CONTROL, KF_FLAG_RIGHT_CONTROL},
        {KF_VK_OPTION, KF_FLAG_LEFT_OPTION},   {KF_VK_RIGHT_OPTION, KF_FLAG_RIGHT_OPTION},
        {KF_VK_COMMAND, KF_FLAG_LEFT_COMMAND}, {KF_VK_RIGHT_COMMAND, KF_FLAG_RIGHT_COMMAND},
        {KF_VK_FUNCTION, KF_FLAG_FUNCTION},    {KF_VK_RIGHT_SHIFT, KF_FLAG_RIGHT_SHIFT},
    };
    for (size_t i = 0u; i < sizeof keys / sizeof keys[0]; ++i) {
        down = false;
        KF_EXPECT(kf_keytap_modifier_change(keys[i].key, 0u, keys[i].flag, &down) && down);
        KF_EXPECT(!kf_keytap_modifier_change(keys[i].key, 0u, KF_FLAG_CAPS_LOCK, &down));
    }
    /* Caps Lock: each toggle is a press, never a release. */
    down = false;
    KF_EXPECT(kf_keytap_modifier_change(KF_VK_CAPS_LOCK, 0u, KF_FLAG_CAPS_LOCK, &down) && down);
    down = false;
    KF_EXPECT(kf_keytap_modifier_change(KF_VK_CAPS_LOCK, KF_FLAG_CAPS_LOCK, 0u, &down) && down);
    KF_EXPECT(!kf_keytap_modifier_change(KF_VK_CAPS_LOCK, 0u, 0u, &down));
    /* Not a modifier at all. */
    KF_EXPECT(!kf_keytap_modifier_change(KF_VK_A, 0u, KF_FLAG_LEFT_SHIFT, &down));
}

void test_keytap_builtin_rule(void) {
    const uint64_t t = 1000u * MS;
    /* No external keyboard: always built-in, report times irrelevant. */
    KF_EXPECT(kf_keytap_is_builtin(t, 0u, 0u, 0u));
    /* External connected: needs a built-in report within 5 ms. */
    KF_EXPECT(kf_keytap_is_builtin(t, t - 2u * MS, 0u, 1u));
    KF_EXPECT(kf_keytap_is_builtin(t, t + 2u * MS, 0u, 1u)); /* The report may arrive later. */
    KF_EXPECT(!kf_keytap_is_builtin(t, t - 6u * MS, 0u, 1u));
    KF_EXPECT(!kf_keytap_is_builtin(t, 0u, 0u, 1u)); /* Unmatched: the safe side. */
    KF_EXPECT(!kf_keytap_is_builtin(t, 0u, t - 1u * MS, 1u));
    /* Both within the window: the nearer wins; a tie goes to built-in. */
    KF_EXPECT(kf_keytap_is_builtin(t, t - 1u * MS, t - 3u * MS, 1u));
    KF_EXPECT(!kf_keytap_is_builtin(t, t - 3u * MS, t - 1u * MS, 1u));
    KF_EXPECT(kf_keytap_is_builtin(t, t - 2u * MS, t + 2u * MS, 1u));
    /* External report far away: no conflict. */
    KF_EXPECT(kf_keytap_is_builtin(t, t - 4u * MS, t - 50u * MS, 2u));
}

static int g_seen;
static kf_key_event g_last;

static void on_key(const kf_key_event *event, void *ctx) {
    (void)ctx;
    g_seen++;
    g_last = *event;
}

void test_keytap_handle_key_routes_and_reports(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    kf_sensor_reset(); /* STOPPED: keys play at once on queue A. */
    kf_keytap_ctx *ctx = kf_keytap_context();
    ctx->callback = on_key;
    ctx->callback_ctx = NULL;
    atomic_store(&ctx->external_keyboards, 0u);
    g_seen = 0;
    kf_keytap_counters before;
    kf_keytap_get_counters(&before);

    kf_keytap_handle_key(ctx, KF_VK_SPACE, true, 42u * MS);
    KF_EXPECT(g_seen == 1);
    KF_EXPECT(g_last.group == KF_GROUP_SPACE && g_last.row == KF_ROW_SPACE && g_last.is_down);
    KF_EXPECT(g_last.builtin && g_last.t_ns == 42u * MS);
    KF_EXPECT(kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_KEYTAP)) == 2u);

    kf_keytap_handle_key(ctx, KF_VK_RETURN, false, 43u * MS);
    KF_EXPECT(g_seen == 2 && g_last.group == KF_GROUP_ENTER && !g_last.is_down);
    KF_EXPECT(kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_KEYTAP)) == 3u);

    atomic_store(&ctx->external_keyboards, 1u); /* External keyboard, no matching report. */
    ctx->last_builtin_ns = 0u;
    kf_keytap_handle_key(ctx, KF_VK_A, true, 44u * MS);
    KF_EXPECT(!g_last.builtin);
    atomic_store(&ctx->external_keyboards, 0u);

    kf_keytap_counters after;
    kf_keytap_get_counters(&after);
    KF_EXPECT(after.events == before.events + 3u);

    /* Settings: no release sounds; external keyboards muted. The callback still sees both. */
    const uint32_t queued_before =
        kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_KEYTAP));
    kf_keytap_set_release_sounds(false);
    kf_keytap_handle_key(ctx, KF_VK_A, false, 45u * MS);
    KF_EXPECT(g_seen == 4);
    kf_keytap_set_mute_external(true);
    atomic_store(&ctx->external_keyboards, 1u);
    kf_keytap_handle_key(ctx, KF_VK_A, true, 46u * MS);
    KF_EXPECT(g_seen == 5 && !g_last.builtin);
    KF_EXPECT(kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_KEYTAP)) == queued_before);
    atomic_store(&ctx->external_keyboards, 0u);
    kf_keytap_handle_key(ctx, KF_VK_A, true, 47u * MS); /* Built-in: still plays. */
    KF_EXPECT(kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_KEYTAP)) ==
              queued_before + 2u);
    kf_keytap_set_release_sounds(true);
    kf_keytap_set_mute_external(false);
    ctx->callback = NULL;
}

void test_keytap_shout_rule(void) {
    const uint64_t none = 0u;
    const uint16_t letters[] = {KF_VK_A, KF_VK_M, KF_VK_Q, KF_VK_Z, KF_VK_B, KF_VK_P};
    for (size_t i = 0u; i < sizeof letters / sizeof letters[0]; ++i) {
        KF_EXPECT(kf_keytap_shout_flags(letters[i], none, 0.8f) == KF_FLAG_SHIFT);
        KF_EXPECT(kf_keytap_shout_flags(letters[i], none, 0.79f) == none);
    }
    /* Every one of the 26 letter keys, and nothing else below 64. */
    int shouted = 0;
    for (uint16_t vk = 0u; vk < 128u; ++vk) {
        shouted += kf_keytap_shout_flags(vk, none, 1.0f) != none ? 1 : 0;
    }
    KF_EXPECT(shouted == 26);
    const uint16_t others[] = {KF_VK_DIGIT_1, KF_VK_SPACE, KF_VK_RETURN, KF_VK_TAB,
                               KF_VK_DELETE,  KF_VK_COMMA, KF_VK_SHIFT,  KF_VK_F5};
    for (size_t i = 0u; i < sizeof others / sizeof others[0]; ++i) {
        KF_EXPECT(kf_keytap_shout_flags(others[i], none, 1.0f) == none);
    }
    /* Never under Command, Control or Option; other flags are kept. */
    const uint64_t shortcuts[] = {KF_FLAG_COMMAND | KF_FLAG_LEFT_COMMAND, KF_FLAG_CONTROL,
                                  KF_FLAG_OPTION};
    for (size_t i = 0u; i < sizeof shortcuts / sizeof shortcuts[0]; ++i) {
        KF_EXPECT(kf_keytap_shout_flags(KF_VK_C, shortcuts[i], 1.0f) == shortcuts[i]);
    }
    KF_EXPECT(kf_keytap_shout_flags(KF_VK_C, KF_FLAG_FUNCTION, 1.0f) ==
              (KF_FLAG_FUNCTION | KF_FLAG_SHIFT));
    KF_EXPECT(kf_keytap_shout_flags(KF_VK_C, KF_FLAG_SHIFT, 1.0f) == KF_FLAG_SHIFT);
    /* No force yet (-1) or NaN: nothing changes. */
    KF_EXPECT(kf_keytap_shout_flags(KF_VK_A, none, -1.0f) == none);
    const float nan = __builtin_nanf("");
    KF_EXPECT(kf_keytap_shout_flags(KF_VK_A, none, nan) == none);
}

void test_keytap_shout_needs_live_builtin_force(void) {
    kf_keytap_ctx *ctx = kf_keytap_context();
    kf_sensor_ctx *sensor = kf_sensor_context();
    kf_sensor_reset();
    atomic_store(&ctx->external_keyboards, 0u);
    const uint64_t t = 500u * MS;
    kf_keytap_counters before;
    kf_keytap_get_counters(&before);

    kf_keytap_set_shout(false);
    atomic_store(&sensor->rolling_velocity, 0.9f);
    kf_sensor_set_state(KF_SENSOR_LIVE);
    atomic_store(&sensor->last_sample_ns, t);
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_A, 0u, t) == 0u); /* Off. */
    kf_keytap_set_shout(true);
    KF_EXPECT(kf_keytap_shout_enabled());
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_A, 0u, t) == KF_FLAG_SHIFT);
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_DIGIT_1, 0u, t) == 0u);
    /* Stale sensor: the rolling force is old news. */
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_A, 0u, t + KF_SENSOR_STALE_NS + 1u) == 0u);
    /* An external keyboard's key: the rolling force describes built-in keys only. */
    atomic_store(&ctx->external_keyboards, 1u);
    ctx->last_builtin_ns = 0u;
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_A, 0u, t) == 0u);
    atomic_store(&ctx->external_keyboards, 0u);
    /* Softer typing, or a stopped sensor. */
    atomic_store(&sensor->rolling_velocity, 0.5f);
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_A, 0u, t) == 0u);
    atomic_store(&sensor->rolling_velocity, 0.9f);
    kf_sensor_set_state(KF_SENSOR_STOPPED);
    KF_EXPECT(kf_keytap_shout(ctx, KF_VK_A, 0u, t) == 0u);

    kf_keytap_counters after;
    kf_keytap_get_counters(&after);
    KF_EXPECT(after.shout_keys == before.shout_keys + 1u);
    kf_keytap_set_shout(false);
    kf_sensor_reset();
}

void test_keytap_shout_trips_after_timeouts(void) {
    kf_keytap_ctx *ctx = kf_keytap_context();
    const uint64_t s = 1000u * MS;
    memset(ctx->timeouts_ns, 0, sizeof ctx->timeouts_ns);
    ctx->timeouts_next = 0u;
    (void)kf_keytap_take_shout_tripped();

    kf_keytap_set_shout(false);
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 1u)); /* Off: timeouts don't count. */
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 2u));
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 3u));

    /* Spaced out: a fourth timeout 49 s after the second is the third within a minute. */
    kf_keytap_set_shout(true);
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 1u * s));
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 41u * s));
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 81u * s)); /* 80 s after the first. */
    KF_EXPECT(!kf_keytap_take_shout_tripped());
    KF_EXPECT(kf_keytap_note_timeout(ctx, 90u * s));
    KF_EXPECT(!kf_keytap_shout_enabled());     /* Turned itself off... */
    KF_EXPECT(kf_keytap_take_shout_tripped()); /* ...and says so once. */
    KF_EXPECT(!kf_keytap_take_shout_tripped());
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 91u * s));

    /* Three in quick succession. */
    memset(ctx->timeouts_ns, 0, sizeof ctx->timeouts_ns);
    ctx->timeouts_next = 0u;
    kf_keytap_set_shout(true);
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 200u * s));
    KF_EXPECT(!kf_keytap_note_timeout(ctx, 200u * s + 5u * MS));
    KF_EXPECT(kf_keytap_note_timeout(ctx, 260u * s)); /* Exactly one minute after the first. */
    KF_EXPECT(kf_keytap_take_shout_tripped());
    kf_keytap_set_shout(false);
}
