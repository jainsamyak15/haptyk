// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <stddef.h>

#include <kf/kf_keymap.h>

#include "keymap/kf_vk.h"
#include "kf_test.h"

void test_keymap_known_keys(void) {
    int known = 0;
    for (uint16_t code = 0u; code < 128u; ++code) {
        const kf_keyinfo info = kf_keymap_lookup(code);
        KF_EXPECT(info.group < KF_GROUP_COUNT);
        KF_EXPECT(info.row < KF_ROW_COUNT);
        KF_EXPECT(info.pan >= -100 && info.pan <= 100);
        known += info.known ? 1 : 0;
    }
    KF_EXPECT(known >= 80);

    const kf_keyinfo a = kf_keymap_lookup(KF_VK_A);
    KF_EXPECT(a.known && a.group == KF_GROUP_ALPHA && a.row == KF_ROW_HOME);
    KF_EXPECT(kf_keymap_lookup(KF_VK_SPACE).group == KF_GROUP_SPACE);
    KF_EXPECT(kf_keymap_lookup(KF_VK_RETURN).group == KF_GROUP_ENTER);
    KF_EXPECT(kf_keymap_lookup(KF_VK_DELETE).group == KF_GROUP_BACKSPACE);
    KF_EXPECT(kf_keymap_lookup(KF_VK_FORWARD_DELETE).group == KF_GROUP_BACKSPACE);
    KF_EXPECT(kf_keymap_lookup(KF_VK_TAB).group == KF_GROUP_TAB);
    KF_EXPECT(kf_keymap_lookup(KF_VK_SHIFT).group == KF_GROUP_MODIFIER);
    KF_EXPECT(kf_keymap_lookup(KF_VK_CAPS_LOCK).group == KF_GROUP_MODIFIER);
    KF_EXPECT(kf_keymap_lookup(KF_VK_F1).row == KF_ROW_FUNCTION);
    KF_EXPECT(kf_keymap_lookup(KF_VK_DIGIT_1).row == KF_ROW_NUMBER);
    KF_EXPECT(kf_keymap_lookup(KF_VK_Q).row == KF_ROW_TOP);
    KF_EXPECT(kf_keymap_lookup(KF_VK_Z).row == KF_ROW_BOTTOM);
    KF_EXPECT(kf_keymap_lookup(KF_VK_A).pan < kf_keymap_lookup(KF_VK_L).pan);
}

void test_keymap_spec_pans(void) {
    /* Spec, Appendix A. */
    KF_EXPECT(kf_keymap_lookup(KF_VK_DELETE).pan == 90);
    KF_EXPECT(kf_keymap_lookup(KF_VK_TAB).pan == -90);
    KF_EXPECT(kf_keymap_lookup(KF_VK_CAPS_LOCK).pan == -95);
    KF_EXPECT(kf_keymap_lookup(KF_VK_RETURN).pan == 90);
    KF_EXPECT(kf_keymap_lookup(KF_VK_SHIFT).pan == -90);
    KF_EXPECT(kf_keymap_lookup(KF_VK_RIGHT_SHIFT).pan == 85);
    KF_EXPECT(kf_keymap_lookup(KF_VK_SPACE).pan == 0);
    KF_EXPECT(kf_keymap_lookup(KF_VK_LEFT_ARROW).pan == 80);
    KF_EXPECT(kf_keymap_lookup(KF_VK_RIGHT_ARROW).pan == 95);
}

void test_keymap_unknown_key_defaults(void) {
    const uint16_t unknown[] = {0x0Au, 0x7Fu, 0x80u, 0xFFFFu};
    for (size_t i = 0; i < sizeof unknown / sizeof unknown[0]; ++i) {
        const kf_keyinfo info = kf_keymap_lookup(unknown[i]);
        KF_EXPECT(!info.known);
        KF_EXPECT(info.group == KF_GROUP_ALPHA);
        KF_EXPECT(info.row == KF_ROW_HOME);
        KF_EXPECT(info.pan == 0);
    }
}

void test_keymap_make_event_drops_keycode(void) {
    const kf_key_event event = kf_keymap_make_event(KF_VK_RETURN, true, 123u, true);
    KF_EXPECT(event.t_ns == 123u);
    KF_EXPECT(event.group == KF_GROUP_ENTER);
    KF_EXPECT(event.row == KF_ROW_HOME);
    KF_EXPECT(event.pan == 90);
    KF_EXPECT(event.is_down);
    KF_EXPECT(event.builtin);
    /* kf_key_event has no key-code field: the privacy rule is enforced by the type. */
    KF_EXPECT(sizeof(kf_key_event) <= 16u);
}
