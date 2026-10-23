// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* The accelerometer driver's reporting settings (ADR-0011, amended 2026-10-24). Main thread.
 *
 * The accelerometer sends no reports until its driver's reporting is switched on, not even to
 * root (docs/SENSOR_NOTES.md). The setting names come from open-source readers of this sensor
 * and are verified on each Mac with `sensor-log --wake`. Only the driver whose primary usage is
 * the accelerometer's, on the SPU transport, is ever written. Its settings are recorded first,
 * and kf_sensor_driver_restore() writes them back. When the driver is already reporting for
 * another app, nothing is written at all. */

#include <kf/kf_sensor.h>

#include <stddef.h>

#include <kf/kf_config.h>

#include "sensor/kf_sensor_internal.h"

static const char k_reporting[] = "SensorPropertyReportingState";
static const char k_power[] = "SensorPropertyPowerState";
static const char k_interval[] = "ReportInterval";

static bool plan_is_valid(kf_sensor_wake level, uint32_t interval_us) {
    return level >= KF_SENSOR_WAKE_INTERVAL && level <= KF_SENSOR_WAKE_POWER &&
           interval_us >= KF_SENSOR_MIN_INTERVAL_US && interval_us <= KF_SENSOR_MAX_INTERVAL_US;
}

uint32_t kf_sensor_wake_plan(kf_sensor_wake level, uint32_t interval_us,
                             kf_driver_write out[KF_DRIVER_MAX_WRITES]) {
    if (out == NULL || !plan_is_valid(level, interval_us)) {
        return 0u;
    }
    uint32_t n = 0u;
    if (level >= KF_SENSOR_WAKE_REPORTING) {
        out[n++] = (kf_driver_write){k_reporting, 1};
    }
    if (level >= KF_SENSOR_WAKE_POWER) {
        out[n++] = (kf_driver_write){k_power, 1};
    }
    out[n++] = (kf_driver_write){k_interval, (int32_t)interval_us};
    return n;
}

static int32_t saved_or_zero(int64_t value) {
    return value >= 0 && value <= INT32_MAX ? (int32_t)value : 0;
}

uint32_t kf_sensor_restore_plan(kf_sensor_wake level, uint32_t count,
                                const kf_sensor_driver_state *saved,
                                kf_driver_write out[KF_DRIVER_MAX_WRITES]) {
    if (out == NULL || saved == NULL) {
        return 0u;
    }
    kf_driver_write plan[KF_DRIVER_MAX_WRITES];
    const uint32_t planned = kf_sensor_wake_plan(level, KF_SENSOR_MIN_INTERVAL_US, plan);
    const uint32_t n = count < planned ? count : planned;
    for (uint32_t i = 0u; i < n; ++i) {
        const char *key = plan[i].key;
        int64_t value = saved->report_interval_us;
        if (key == k_reporting) {
            value = saved->reporting_state;
        } else if (key == k_power) {
            value = saved->power_state;
        }
        out[i] = (kf_driver_write){key, saved_or_zero(value)};
    }
    return n;
}

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>

static kf_sensor_wake g_woken = KF_SENSOR_WAKE_NONE;
static kf_sensor_driver_state g_saved;

static int64_t number_of(CFTypeRef value) {
    int64_t n = -1;
    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID() &&
        CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &n)) {
        return n;
    }
    return -1;
}

static int64_t read_number(io_registry_entry_t entry, CFStringRef key) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0u);
    const int64_t n = number_of(value);
    if (value != NULL) {
        CFRelease(value);
    }
    return n;
}

static bool string_is(io_registry_entry_t entry, CFStringRef key, CFStringRef expected) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0u);
    const bool same = value != NULL && CFGetTypeID(value) == CFStringGetTypeID() &&
                      CFStringCompare((CFStringRef)value, expected, 0) == kCFCompareEqualTo;
    if (value != NULL) {
        CFRelease(value);
    }
    return same;
}

static int64_t read_events(io_registry_entry_t entry) {
    CFTypeRef debug =
        IORegistryEntryCreateCFProperty(entry, CFSTR("DebugState"), kCFAllocatorDefault, 0u);
    int64_t n = -1;
    if (debug != NULL && CFGetTypeID(debug) == CFDictionaryGetTypeID()) {
        n = number_of(CFDictionaryGetValue((CFDictionaryRef)debug, CFSTR("_num_events")));
    }
    if (debug != NULL) {
        CFRelease(debug);
    }
    return n;
}

static void read_state(io_registry_entry_t driver, kf_sensor_driver_state *out) {
    out->report_interval_us = read_number(driver, CFSTR("ReportInterval"));
    out->reporting_state = read_number(driver, CFSTR("SensorPropertyReportingState"));
    out->power_state = read_number(driver, CFSTR("SensorPropertyPowerState"));
    out->events = read_events(driver);
}

static bool is_accelerometer(io_registry_entry_t entry) {
    return read_number(entry, CFSTR(kIOHIDPrimaryUsagePageKey)) == (int64_t)KF_SENSOR_USAGE_PAGE &&
           read_number(entry, CFSTR(kIOHIDPrimaryUsageKey)) == (int64_t)KF_SENSOR_USAGE &&
           read_number(entry, CFSTR(kIOHIDVendorIDKey)) == (int64_t)KF_SENSOR_VENDOR_ID &&
           string_is(entry, CFSTR(kIOHIDTransportKey), CFSTR(KF_SENSOR_TRANSPORT));
}

static void put_number(CFMutableDictionaryRef dict, CFStringRef key, int value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    if (number != NULL) {
        CFDictionarySetValue(dict, key, number);
        CFRelease(number);
    }
}

/* The one AppleSPUHIDDriver for the accelerometer: narrowed by the kernel, every key re-checked
 * here. MACH_PORT_NULL unless exactly one matches. */
static io_service_t find_driver(void) {
    CFMutableDictionaryRef match = IOServiceMatching("AppleSPUHIDDriver");
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (match == NULL || props == NULL) {
        if (match != NULL) {
            CFRelease(match);
        }
        if (props != NULL) {
            CFRelease(props);
        }
        return MACH_PORT_NULL;
    }
    put_number(props, CFSTR(kIOHIDPrimaryUsagePageKey), (int)KF_SENSOR_USAGE_PAGE);
    put_number(props, CFSTR(kIOHIDPrimaryUsageKey), (int)KF_SENSOR_USAGE);
    CFDictionarySetValue(match, CFSTR(kIOPropertyMatchKey), props);
    CFRelease(props);
    io_iterator_t it = MACH_PORT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &it) != KERN_SUCCESS) {
        return MACH_PORT_NULL; /* The call consumed `match`. */
    }
    io_service_t found = MACH_PORT_NULL;
    uint32_t count = 0u;
    for (io_service_t s = IOIteratorNext(it); s != MACH_PORT_NULL; s = IOIteratorNext(it)) {
        if (is_accelerometer(s) && count++ == 0u) {
            found = s;
        } else {
            IOObjectRelease(s);
        }
    }
    IOObjectRelease(it);
    if (count != 1u && found != MACH_PORT_NULL) {
        IOObjectRelease(found);
        found = MACH_PORT_NULL;
    }
    return found;
}

static kf_status status_of(kern_return_t result) {
    if (result == KERN_SUCCESS) {
        return KF_OK;
    }
    if (result == kIOReturnNotPrivileged || result == kIOReturnNotPermitted) {
        return KF_ERR_NO_PERMISSION;
    }
    return result == kIOReturnNoMemory ? KF_ERR_NO_MEMORY : KF_ERR_DEVICE;
}

static kern_return_t write_one(io_service_t driver, const kf_driver_write *w) {
    const int32_t value = w->value;
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFStringRef key =
        CFStringCreateWithCString(kCFAllocatorDefault, w->key, kCFStringEncodingASCII);
    const kern_return_t result = number != NULL && key != NULL
                                     ? IORegistryEntrySetCFProperty(driver, key, number)
                                     : kIOReturnNoMemory;
    if (number != NULL) {
        CFRelease(number);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

/* Writes in order. `all`: keep going after a failure (restoring); otherwise stop (waking).
 * Returns the first failure; `done` counts the writes made before it. */
static kern_return_t write_all(io_service_t driver, const kf_driver_write *writes, uint32_t n,
                               bool all, uint32_t *done) {
    kern_return_t first = KERN_SUCCESS;
    *done = 0u;
    for (uint32_t i = 0u; i < n; ++i) {
        const kern_return_t result = write_one(driver, &writes[i]);
        if (result != KERN_SUCCESS) {
            first = first == KERN_SUCCESS ? result : first;
            if (!all) {
                return first;
            }
        } else if (first == KERN_SUCCESS) {
            *done = i + 1u;
        }
    }
    return first;
}

kf_status kf_sensor_driver_read(kf_sensor_driver_state *out) {
    if (out == NULL) {
        return KF_ERR_INVALID_ARGUMENT;
    }
    const io_service_t driver = find_driver();
    if (driver == MACH_PORT_NULL) {
        return KF_ERR_NOT_FOUND;
    }
    read_state(driver, out);
    IOObjectRelease(driver);
    return KF_OK;
}

kf_status kf_sensor_driver_wake(kf_sensor_wake level, uint32_t interval_us) {
    if (level == KF_SENSOR_WAKE_NONE) {
        return KF_OK;
    }
    kf_driver_write plan[KF_DRIVER_MAX_WRITES];
    const uint32_t n = kf_sensor_wake_plan(level, interval_us, plan);
    if (n == 0u) {
        return KF_ERR_INVALID_ARGUMENT;
    }
    if (g_woken != KF_SENSOR_WAKE_NONE) {
        return KF_ERR_STATE;
    }
    const io_service_t driver = find_driver();
    if (driver == MACH_PORT_NULL) {
        return KF_ERR_NOT_FOUND;
    }
    kf_sensor_driver_state saved;
    read_state(driver, &saved);
    if (saved.report_interval_us > 0) { /* Reporting for another app: leave it as it is. */
        IOObjectRelease(driver);
        return KF_OK;
    }
    uint32_t done = 0u;
    const kern_return_t result = write_all(driver, plan, n, false, &done);
    if (result != KERN_SUCCESS) {
        kf_driver_write undo[KF_DRIVER_MAX_WRITES];
        const uint32_t m = kf_sensor_restore_plan(level, done, &saved, undo);
        uint32_t undone = 0u;
        (void)write_all(driver, undo, m, true, &undone);
        IOObjectRelease(driver);
        return status_of(result);
    }
    IOObjectRelease(driver);
    g_saved = saved;
    g_woken = level;
    return KF_OK;
}

kf_status kf_sensor_driver_restore(void) {
    if (g_woken == KF_SENSOR_WAKE_NONE) {
        return KF_OK;
    }
    const io_service_t driver = find_driver();
    if (driver == MACH_PORT_NULL) {
        g_woken = KF_SENSOR_WAKE_NONE; /* The driver, and its settings, are gone. */
        return KF_ERR_NOT_FOUND;
    }
    kf_driver_write undo[KF_DRIVER_MAX_WRITES];
    const uint32_t m = kf_sensor_restore_plan(g_woken, KF_DRIVER_MAX_WRITES, &g_saved, undo);
    uint32_t done = 0u;
    const kern_return_t result = write_all(driver, undo, m, true, &done);
    IOObjectRelease(driver);
    if (result != KERN_SUCCESS) {
        return status_of(result); /* Still woken: a later call tries again. */
    }
    g_woken = KF_SENSOR_WAKE_NONE;
    return KF_OK;
}

kf_sensor_wake kf_sensor_driver_woken(void) {
    return g_woken;
}

#else /* No IOKit: no driver. */

kf_status kf_sensor_driver_read(kf_sensor_driver_state *out) {
    return out == NULL ? KF_ERR_INVALID_ARGUMENT : KF_ERR_NOT_FOUND;
}

kf_status kf_sensor_driver_wake(kf_sensor_wake level, uint32_t interval_us) {
    kf_driver_write plan[KF_DRIVER_MAX_WRITES];
    if (level == KF_SENSOR_WAKE_NONE) {
        return KF_OK;
    }
    return kf_sensor_wake_plan(level, interval_us, plan) == 0u ? KF_ERR_INVALID_ARGUMENT
                                                               : KF_ERR_NOT_FOUND;
}

kf_status kf_sensor_driver_restore(void) {
    return KF_OK;
}

kf_sensor_wake kf_sensor_driver_woken(void) {
    return KF_SENSOR_WAKE_NONE;
}

#endif
