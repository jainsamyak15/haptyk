// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_hid.h
 * @brief IOKit HID helpers shared by hid-dump and sensor-log (macOS only, never shipped).
 */
#ifndef KF_HID_H
#define KF_HID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDManager.h>

#include "kf_tool.h"

/**
 * An IOHIDDeviceRef from a value a CF collection handed out as `const void *`. CF gives these as
 * const, but the HID API takes a non-const ref; the uintptr_t round-trip drops the qualifier
 * without tripping `-Wcast-qual`. One documented place, so call sites stay plain.
 */
static inline IOHIDDeviceRef kf_hid_ref(const void *value) {
    return (IOHIDDeviceRef)(uintptr_t)value; // NOLINT(performance-no-int-to-ptr): see above.
}

/**
 * Adds `match` to `dict` as top-level HID matching keys, which both IOHIDManager and
 * IOHIDDevice::matchPropertyTable understand. Usage page and usage match any
 * DeviceUsagePairs entry. False when allocation fails.
 */
bool kf_hid_match_fill(CFMutableDictionaryRef dict, const kf_tool_match *match);

/** A new dictionary holding only `match`. NULL when allocation fails. The caller releases it. */
CFMutableDictionaryRef kf_hid_match_dictionary(const kf_tool_match *match);

/** Reads every reported property of `device`. */
void kf_hid_info_read(IOHIDDeviceRef device, kf_hid_info *out);

/**
 * The devices that meet every criterion in `match` (all devices when it is empty), sorted
 * by registry entry ID so output is stable. Uses kernel matching to narrow the set, then
 * kf_tool_match_info() to verify it. Returns NULL when none match. The caller releases it.
 */
CFArrayRef kf_hid_copy_matching_devices(IOHIDManagerRef manager, const kf_tool_match *match);

/** Short name of an IOReturn, for example "not permitted". Never NULL. */
const char *kf_hid_return_string(IOReturn result);

/** Writes the hw.model sysctl, for example "Mac17,3", or "unknown". */
void kf_hid_hw_model(char *buf, size_t len);

/** Writes the macOS version and build, for example "27.0 (26A428)", or "unknown". */
void kf_hid_os_version(char *buf, size_t len);

#endif /* KF_HID_H */
