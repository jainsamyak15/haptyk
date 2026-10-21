// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_keymap.h
 * @brief Maps macOS virtual key codes to sound group, keyboard row and stereo pan.
 *
 * The key code is used only for this lookup and is discarded immediately after;
 * kf_key_event never carries it (privacy rule, docs/PRIVACY.md).
 */
#ifndef KF_KEYMAP_H
#define KF_KEYMAP_H

#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Classification of one physical key on an ANSI MacBook keyboard. */
typedef struct kf_keyinfo {
    uint8_t group; /**< kf_key_group. */
    uint8_t row;   /**< kf_key_row. */
    int8_t pan;    /**< -100 .. +100. */
    bool known;    /**< False for key codes not in the table. */
} kf_keyinfo;

/** O(1) table lookup. Unknown codes map to alpha, home row, centre. RT-safe. */
kf_keyinfo kf_keymap_lookup(uint16_t keycode);

/** Builds the event the core works with, dropping the key code. RT-safe. */
kf_key_event kf_keymap_make_event(uint16_t keycode, bool is_down, uint64_t t_ns, bool builtin);

#ifdef __cplusplus
}
#endif

#endif /* KF_KEYMAP_H */
