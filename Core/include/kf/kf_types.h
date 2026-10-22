// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_types.h
 * @brief Plain data types shared by every core module.
 */
#ifndef KF_TYPES_H
#define KF_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#define KF_STATIC_ASSERT static_assert
extern "C" {
#else
#define KF_STATIC_ASSERT _Static_assert
#endif

/** Result of a core operation. KF_OK is zero; every other value is an error. */
typedef enum kf_status {
    KF_OK = 0,
    KF_ERR_INVALID_ARGUMENT, /**< A parameter was NULL or out of range. */
    KF_ERR_NO_MEMORY,        /**< An allocation failed (never on real-time threads). */
    KF_ERR_NO_PERMISSION,    /**< A macOS permission, such as Input Monitoring, is missing. */
    KF_ERR_NOT_FOUND,        /**< A device or file does not exist. */
    KF_ERR_DEVICE,           /**< A hardware device failed or disappeared. */
    KF_ERR_AUDIO,            /**< CoreAudio returned an error. */
    KF_ERR_FORMAT,           /**< Data was malformed or in an unsupported format. */
    KF_ERR_LIMIT,            /**< A size or count limit was exceeded. */
    KF_ERR_STATE,            /**< The call is not valid in the current state. */
    KF_ERR_NOT_IMPLEMENTED,  /**< Placeholder until the owning milestone lands. */
} kf_status;

/** Static, human-readable name of a status. Never returns NULL. */
const char *kf_status_string(kf_status status);

/** Which sample set a key plays. Packs fall back to KF_GROUP_ALPHA. */
typedef enum kf_key_group {
    KF_GROUP_ALPHA = 0,
    KF_GROUP_SPACE,
    KF_GROUP_ENTER,
    KF_GROUP_BACKSPACE,
    KF_GROUP_MODIFIER,
    KF_GROUP_TAB,
    KF_GROUP_COUNT
} kf_key_group;

/** Sounds a pack holds besides its key groups. Big Enter's boom (Labs) plays like a key group
 * with one tier and no release sound; keys never fall back to it, and it falls back to
 * nothing. */
enum {
    KF_SOUND_BOOM = KF_GROUP_COUNT, /**< Big Enter: added to the pack while the feature is on. */
    KF_SOUND_SLOTS                  /**< Rows of a pack's index: key groups, then the boom. */
};

/** Physical keyboard row; drives per-row sensor calibration. */
typedef enum kf_key_row {
    KF_ROW_NUMBER = 0,
    KF_ROW_TOP,
    KF_ROW_HOME,
    KF_ROW_BOTTOM,
    KF_ROW_SPACE,
    KF_ROW_FUNCTION, /**< Function row and anything else. */
    KF_ROW_COUNT
} kf_key_row;

/** Recorded force tiers, softest first. */
typedef enum kf_tier {
    KF_TIER_SOFT = 0,
    KF_TIER_MEDIUM,
    KF_TIER_HARD,
    KF_TIER_SLAM,
    KF_TIER_COUNT
} kf_tier;

/** One keystroke as the core sees it. Deliberately holds no key code. */
typedef struct kf_key_event {
    uint64_t t_ns; /**< Event time, ns on the mach_absolute_time clock. */
    uint8_t group; /**< kf_key_group. */
    uint8_t row;   /**< kf_key_row. */
    int8_t pan;    /**< -100 (left edge) .. +100 (right edge). */
    bool is_down;  /**< Key-down; false for key-up. */
    bool builtin;  /**< Came from the MacBook's own keyboard. */
} kf_key_event;

/** A request for the audio thread to start one voice. */
typedef struct kf_voice_req {
    uint64_t t_ns; /**< Keystroke time; stale requests are dropped. */
    uint8_t group; /**< kf_key_group, or KF_SOUND_BOOM. */
    uint8_t tier;  /**< kf_tier; ignored for key-up sounds. */
    bool is_up;    /**< Release sound. */
    float gain;    /**< 0..1. */
    float pan;     /**< -1 (left) .. +1 (right). */
} kf_voice_req;

/** Producer of a voice request. Each queue has exactly one producer thread. */
typedef enum kf_queue {
    KF_QUEUE_KEYTAP = 0, /**< Queue A: keys without sensor data and all key-ups. */
    KF_QUEUE_SENSOR,     /**< Queue B: keys resolved by the sensor thread. */
    KF_QUEUE_MAIN,       /**< The main thread's: previews and test sounds from the UI. */
    KF_QUEUE_COUNT
} kf_queue;

#ifdef __cplusplus
}
#endif

#endif /* KF_TYPES_H */
