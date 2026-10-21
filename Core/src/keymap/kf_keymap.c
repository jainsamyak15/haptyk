// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <kf/kf_keymap.h>

#include "internal/kf_rt.h"
#include "keymap/kf_vk.h"

#if defined(__APPLE__)
#include <Carbon/Carbon.h>
/* Fail the build if a mirrored key code drifts from the SDK. */
_Static_assert(KF_VK_A == kVK_ANSI_A, "kVK_ANSI_A");
_Static_assert(KF_VK_Z == kVK_ANSI_Z, "kVK_ANSI_Z");
_Static_assert(KF_VK_B == kVK_ANSI_B, "kVK_ANSI_B");
_Static_assert(KF_VK_Q == kVK_ANSI_Q, "kVK_ANSI_Q");
_Static_assert(KF_VK_DIGIT_1 == kVK_ANSI_1, "kVK_ANSI_1");
_Static_assert(KF_VK_DIGIT_6 == kVK_ANSI_6, "kVK_ANSI_6");
_Static_assert(KF_VK_DIGIT_5 == kVK_ANSI_5, "kVK_ANSI_5");
_Static_assert(KF_VK_DIGIT_9 == kVK_ANSI_9, "kVK_ANSI_9");
_Static_assert(KF_VK_DIGIT_0 == kVK_ANSI_0, "kVK_ANSI_0");
_Static_assert(KF_VK_O == kVK_ANSI_O, "kVK_ANSI_O");
_Static_assert(KF_VK_P == kVK_ANSI_P, "kVK_ANSI_P");
_Static_assert(KF_VK_L == kVK_ANSI_L, "kVK_ANSI_L");
_Static_assert(KF_VK_K == kVK_ANSI_K, "kVK_ANSI_K");
_Static_assert(KF_VK_M == kVK_ANSI_M, "kVK_ANSI_M");
_Static_assert(KF_VK_GRAVE == kVK_ANSI_Grave, "kVK_ANSI_Grave");
_Static_assert(KF_VK_BACKSLASH == kVK_ANSI_Backslash, "kVK_ANSI_Backslash");
_Static_assert(KF_VK_PERIOD == kVK_ANSI_Period, "kVK_ANSI_Period");
_Static_assert(KF_VK_RETURN == kVK_Return, "kVK_Return");
_Static_assert(KF_VK_TAB == kVK_Tab, "kVK_Tab");
_Static_assert(KF_VK_SPACE == kVK_Space, "kVK_Space");
_Static_assert(KF_VK_DELETE == kVK_Delete, "kVK_Delete");
_Static_assert(KF_VK_ESCAPE == kVK_Escape, "kVK_Escape");
_Static_assert(KF_VK_RIGHT_COMMAND == kVK_RightCommand, "kVK_RightCommand");
_Static_assert(KF_VK_COMMAND == kVK_Command, "kVK_Command");
_Static_assert(KF_VK_SHIFT == kVK_Shift, "kVK_Shift");
_Static_assert(KF_VK_CAPS_LOCK == kVK_CapsLock, "kVK_CapsLock");
_Static_assert(KF_VK_OPTION == kVK_Option, "kVK_Option");
_Static_assert(KF_VK_CONTROL == kVK_Control, "kVK_Control");
_Static_assert(KF_VK_RIGHT_SHIFT == kVK_RightShift, "kVK_RightShift");
_Static_assert(KF_VK_RIGHT_OPTION == kVK_RightOption, "kVK_RightOption");
_Static_assert(KF_VK_FUNCTION == kVK_Function, "kVK_Function");
_Static_assert(KF_VK_F1 == kVK_F1, "kVK_F1");
_Static_assert(KF_VK_F12 == kVK_F12, "kVK_F12");
_Static_assert(KF_VK_FORWARD_DELETE == kVK_ForwardDelete, "kVK_ForwardDelete");
_Static_assert(KF_VK_LEFT_ARROW == kVK_LeftArrow, "kVK_LeftArrow");
_Static_assert(KF_VK_UP_ARROW == kVK_UpArrow, "kVK_UpArrow");
#endif

/*
 * Horizontal key centres on a 15-unit-wide ANSI MacBook keyboard, in hundredths
 * of a key unit, mapped to pan -100..+100 with rounding. Explicit positions for
 * the edge keys reproduce the pans in the spec (Appendix A).
 */
#define KF_PAN(x_hundredths)     ((int8_t)((((x_hundredths) * 2) + 7) / 15 - 100))
#define KEY(code, group, row, x) [code] = {(uint8_t)(group), (uint8_t)(row), KF_PAN(x), true}

static const kf_keyinfo k_table[KF_VK_TABLE_SIZE] = {
    /* Function row */
    KEY(KF_VK_ESCAPE, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 60),
    KEY(KF_VK_F1, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 170),
    KEY(KF_VK_F2, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 280),
    KEY(KF_VK_F3, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 390),
    KEY(KF_VK_F4, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 500),
    KEY(KF_VK_F5, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 610),
    KEY(KF_VK_F6, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 720),
    KEY(KF_VK_F7, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 830),
    KEY(KF_VK_F8, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 940),
    KEY(KF_VK_F9, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 1050),
    KEY(KF_VK_F10, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 1160),
    KEY(KF_VK_F11, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 1270),
    KEY(KF_VK_F12, KF_GROUP_ALPHA, KF_ROW_FUNCTION, 1380),

    /* Number row */
    KEY(KF_VK_GRAVE, KF_GROUP_ALPHA, KF_ROW_NUMBER, 50),
    KEY(KF_VK_DIGIT_1, KF_GROUP_ALPHA, KF_ROW_NUMBER, 150),
    KEY(KF_VK_DIGIT_2, KF_GROUP_ALPHA, KF_ROW_NUMBER, 250),
    KEY(KF_VK_DIGIT_3, KF_GROUP_ALPHA, KF_ROW_NUMBER, 350),
    KEY(KF_VK_DIGIT_4, KF_GROUP_ALPHA, KF_ROW_NUMBER, 450),
    KEY(KF_VK_DIGIT_5, KF_GROUP_ALPHA, KF_ROW_NUMBER, 550),
    KEY(KF_VK_DIGIT_6, KF_GROUP_ALPHA, KF_ROW_NUMBER, 650),
    KEY(KF_VK_DIGIT_7, KF_GROUP_ALPHA, KF_ROW_NUMBER, 750),
    KEY(KF_VK_DIGIT_8, KF_GROUP_ALPHA, KF_ROW_NUMBER, 850),
    KEY(KF_VK_DIGIT_9, KF_GROUP_ALPHA, KF_ROW_NUMBER, 950),
    KEY(KF_VK_DIGIT_0, KF_GROUP_ALPHA, KF_ROW_NUMBER, 1050),
    KEY(KF_VK_MINUS, KF_GROUP_ALPHA, KF_ROW_NUMBER, 1150),
    KEY(KF_VK_EQUAL, KF_GROUP_ALPHA, KF_ROW_NUMBER, 1250),
    KEY(KF_VK_DELETE, KF_GROUP_BACKSPACE, KF_ROW_NUMBER, 1425),
    KEY(KF_VK_FORWARD_DELETE, KF_GROUP_BACKSPACE, KF_ROW_NUMBER, 1425),

    /* Top row */
    KEY(KF_VK_TAB, KF_GROUP_TAB, KF_ROW_TOP, 75),
    KEY(KF_VK_Q, KF_GROUP_ALPHA, KF_ROW_TOP, 200),
    KEY(KF_VK_W, KF_GROUP_ALPHA, KF_ROW_TOP, 300),
    KEY(KF_VK_E, KF_GROUP_ALPHA, KF_ROW_TOP, 400),
    KEY(KF_VK_R, KF_GROUP_ALPHA, KF_ROW_TOP, 500),
    KEY(KF_VK_T, KF_GROUP_ALPHA, KF_ROW_TOP, 600),
    KEY(KF_VK_Y, KF_GROUP_ALPHA, KF_ROW_TOP, 700),
    KEY(KF_VK_U, KF_GROUP_ALPHA, KF_ROW_TOP, 800),
    KEY(KF_VK_I, KF_GROUP_ALPHA, KF_ROW_TOP, 900),
    KEY(KF_VK_O, KF_GROUP_ALPHA, KF_ROW_TOP, 1000),
    KEY(KF_VK_P, KF_GROUP_ALPHA, KF_ROW_TOP, 1100),
    KEY(KF_VK_LEFT_BRACKET, KF_GROUP_ALPHA, KF_ROW_TOP, 1200),
    KEY(KF_VK_RIGHT_BRACKET, KF_GROUP_ALPHA, KF_ROW_TOP, 1300),
    KEY(KF_VK_BACKSLASH, KF_GROUP_ALPHA, KF_ROW_TOP, 1425),

    /* Home row */
    KEY(KF_VK_CAPS_LOCK, KF_GROUP_MODIFIER, KF_ROW_HOME, 38),
    KEY(KF_VK_A, KF_GROUP_ALPHA, KF_ROW_HOME, 225),
    KEY(KF_VK_S, KF_GROUP_ALPHA, KF_ROW_HOME, 325),
    KEY(KF_VK_D, KF_GROUP_ALPHA, KF_ROW_HOME, 425),
    KEY(KF_VK_F, KF_GROUP_ALPHA, KF_ROW_HOME, 525),
    KEY(KF_VK_G, KF_GROUP_ALPHA, KF_ROW_HOME, 625),
    KEY(KF_VK_H, KF_GROUP_ALPHA, KF_ROW_HOME, 725),
    KEY(KF_VK_J, KF_GROUP_ALPHA, KF_ROW_HOME, 825),
    KEY(KF_VK_K, KF_GROUP_ALPHA, KF_ROW_HOME, 925),
    KEY(KF_VK_L, KF_GROUP_ALPHA, KF_ROW_HOME, 1025),
    KEY(KF_VK_SEMICOLON, KF_GROUP_ALPHA, KF_ROW_HOME, 1125),
    KEY(KF_VK_QUOTE, KF_GROUP_ALPHA, KF_ROW_HOME, 1225),
    KEY(KF_VK_RETURN, KF_GROUP_ENTER, KF_ROW_HOME, 1425),

    /* Bottom row */
    KEY(KF_VK_SHIFT, KF_GROUP_MODIFIER, KF_ROW_BOTTOM, 75),
    KEY(KF_VK_Z, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 275),
    KEY(KF_VK_X, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 375),
    KEY(KF_VK_C, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 475),
    KEY(KF_VK_V, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 575),
    KEY(KF_VK_B, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 675),
    KEY(KF_VK_N, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 775),
    KEY(KF_VK_M, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 875),
    KEY(KF_VK_COMMA, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 975),
    KEY(KF_VK_PERIOD, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 1075),
    KEY(KF_VK_SLASH, KF_GROUP_ALPHA, KF_ROW_BOTTOM, 1175),
    KEY(KF_VK_RIGHT_SHIFT, KF_GROUP_MODIFIER, KF_ROW_BOTTOM, 1388),

    /* Space row */
    KEY(KF_VK_FUNCTION, KF_GROUP_MODIFIER, KF_ROW_SPACE, 50),
    KEY(KF_VK_CONTROL, KF_GROUP_MODIFIER, KF_ROW_SPACE, 150),
    KEY(KF_VK_OPTION, KF_GROUP_MODIFIER, KF_ROW_SPACE, 250),
    KEY(KF_VK_COMMAND, KF_GROUP_MODIFIER, KF_ROW_SPACE, 375),
    KEY(KF_VK_SPACE, KF_GROUP_SPACE, KF_ROW_SPACE, 750),
    KEY(KF_VK_RIGHT_COMMAND, KF_GROUP_MODIFIER, KF_ROW_SPACE, 1125),
    KEY(KF_VK_RIGHT_OPTION, KF_GROUP_MODIFIER, KF_ROW_SPACE, 1250),
    KEY(KF_VK_RIGHT_CONTROL, KF_GROUP_MODIFIER, KF_ROW_SPACE, 1300),
    KEY(KF_VK_LEFT_ARROW, KF_GROUP_ALPHA, KF_ROW_SPACE, 1350),
    KEY(KF_VK_DOWN_ARROW, KF_GROUP_ALPHA, KF_ROW_SPACE, 1400),
    KEY(KF_VK_UP_ARROW, KF_GROUP_ALPHA, KF_ROW_SPACE, 1400),
    KEY(KF_VK_RIGHT_ARROW, KF_GROUP_ALPHA, KF_ROW_SPACE, 1462),

    /* fn + arrow navigation keys share the arrow positions. */
    KEY(KF_VK_HOME, KF_GROUP_ALPHA, KF_ROW_SPACE, 1350),
    KEY(KF_VK_PAGE_UP, KF_GROUP_ALPHA, KF_ROW_SPACE, 1400),
    KEY(KF_VK_PAGE_DOWN, KF_GROUP_ALPHA, KF_ROW_SPACE, 1400),
    KEY(KF_VK_END, KF_GROUP_ALPHA, KF_ROW_SPACE, 1462),
};

#undef KEY

KF_RT_SAFE kf_keyinfo kf_keymap_lookup(uint16_t keycode) {
    if (keycode < KF_VK_TABLE_SIZE && k_table[keycode].known) {
        return k_table[keycode];
    }
    const kf_keyinfo fallback = {(uint8_t)KF_GROUP_ALPHA, (uint8_t)KF_ROW_HOME, 0, false};
    return fallback;
}

KF_RT_SAFE kf_key_event kf_keymap_make_event(uint16_t keycode, bool is_down, uint64_t t_ns,
                                             bool builtin) {
    const kf_keyinfo info = kf_keymap_lookup(keycode);
    const kf_key_event event = {
        .t_ns = t_ns,
        .group = info.group,
        .row = info.row,
        .pan = info.pan,
        .is_down = is_down,
        .builtin = builtin,
    };
    return event;
}
