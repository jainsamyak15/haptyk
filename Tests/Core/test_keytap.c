// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

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
    ctx->callback = NULL;
}
