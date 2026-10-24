// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Sensor lifecycle and state (main thread) and the sensor thread's setup. The hot path lives in
 * kf_sensor_rt.c. The device is opened read-only and never seized. While it is open, the main
 * thread starts the accelerometer stream and restores the driver's original settings at stop
 * (ADR-0011). */

#include <kf/kf_sensor.h>

#include <string.h>

#include <kf/kf_config.h>
#include <kf/kf_time.h>

#include "internal/kf_rt.h"
#include "sensor/kf_sensor_internal.h"

static kf_sensor_ctx g_ctx;

kf_sensor_ctx *kf_sensor_context(void) {
    return &g_ctx;
}

static kf_sensor_state load_state(void) {
    return (kf_sensor_state)atomic_load_explicit(&g_ctx.state, memory_order_acquire);
}

static bool is_running(kf_sensor_state state) {
    return state == KF_SENSOR_LIVE || state == KF_SENSOR_STALE;
}

void kf_sensor_set_state(kf_sensor_state state) {
    atomic_store_explicit(&g_ctx.state, (int)state, memory_order_release);
}

void kf_sensor_reset(void) {
    if (!kf_calibration_is_valid(&g_ctx.calibration)) {
        g_ctx.calibration = kf_calibration_default();
    }
    kf_accel_ring_reset(&g_ctx.ring);
    kf_pending_queue_reset(&g_ctx.pending);
    kf_peak_queue_reset(&g_ctx.peaks);
    kf_calibration_queue_reset(&g_ctx.calibrations);
    kf_autoadapt_init(&g_ctx.autoadapt, &g_ctx.calibration);
    atomic_store_explicit(&g_ctx.first_sample_ns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.last_sample_ns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.last_velocity, -1.0f, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.rolling_velocity, -1.0f, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.samples, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.keys_resolved, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.keys_defaulted, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.dropped_reports, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.peaks_dropped, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.noise_until_ns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.noise_floor_g, -1.0f, memory_order_relaxed);
    g_ctx.recent_count = 0u;
    g_ctx.noise_sample_count = 0u;
    g_ctx.next_noise_probe_ns = 0u;
    kf_sensor_set_state(KF_SENSOR_STOPPED);
}

KF_RT_SAFE kf_sensor_state kf_sensor_state_now(uint64_t now_ns) {
    const kf_sensor_state state = load_state();
    if (state != KF_SENSOR_LIVE) {
        return state;
    }
    const uint64_t last = atomic_load_explicit(&g_ctx.last_sample_ns, memory_order_acquire);
    if (now_ns > last && now_ns - last > KF_SENSOR_STALE_NS) {
        return KF_SENSOR_STALE;
    }
    return KF_SENSOR_LIVE;
}

kf_status kf_sensor_set_calibration(const kf_calibration *cal) {
    if (!kf_calibration_is_valid(cal)) {
        return KF_ERR_INVALID_ARGUMENT;
    }
    if (is_running(load_state())) {
        return kf_calibration_queue_push(&g_ctx.calibrations, cal) ? KF_OK : KF_ERR_LIMIT;
    }
    g_ctx.calibration = *cal;
    kf_autoadapt_init(&g_ctx.autoadapt, cal);
    return KF_OK;
}

float kf_sensor_last_velocity(void) {
    return atomic_load_explicit(&g_ctx.last_velocity, memory_order_relaxed);
}

KF_RT_SAFE float kf_sensor_rolling_velocity(void) {
    return atomic_load_explicit(&g_ctx.rolling_velocity, memory_order_relaxed);
}

bool kf_sensor_pop_peak(kf_sensor_peak *out) {
    return out != NULL && kf_peak_queue_pop(&g_ctx.peaks, out);
}

void kf_sensor_begin_noise_measurement(uint64_t duration_ns) {
    atomic_store_explicit(&g_ctx.noise_floor_g, -1.0f, memory_order_relaxed);
    g_ctx.noise_sample_count = 0u; /* Read by the sensor thread only after the release below. */
    atomic_store_explicit(&g_ctx.noise_until_ns, kf_time_now_ns() + duration_ns,
                          memory_order_release);
}

float kf_sensor_noise_floor_g(void) {
    return atomic_load_explicit(&g_ctx.noise_floor_g, memory_order_acquire);
}

void kf_sensor_get_counters(kf_sensor_counters *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->samples = atomic_load_explicit(&g_ctx.samples, memory_order_relaxed);
    out->keys_resolved = atomic_load_explicit(&g_ctx.keys_resolved, memory_order_relaxed);
    out->keys_defaulted = atomic_load_explicit(&g_ctx.keys_defaulted, memory_order_relaxed);
    out->dropped_reports = atomic_load_explicit(&g_ctx.dropped_reports, memory_order_relaxed);
    out->peaks_dropped = atomic_load_explicit(&g_ctx.peaks_dropped, memory_order_relaxed);
    const uint64_t first = atomic_load_explicit(&g_ctx.first_sample_ns, memory_order_relaxed);
    const uint64_t last = atomic_load_explicit(&g_ctx.last_sample_ns, memory_order_relaxed);
    if (out->samples > 1u && last > first) {
        out->sample_rate_hz = (double)(out->samples - 1u) * 1e9 / (double)(last - first);
    }
}

#if defined(__APPLE__) && KF_SENSOR_LAYOUT_MEASURED

#include <pthread.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <dispatch/dispatch.h>

void kf_sensor_on_report(void *ref, IOReturn result, void *sender, IOHIDReportType type,
                         uint32_t report_id, uint8_t *report, CFIndex length, uint64_t timestamp);
void kf_sensor_on_timer(CFRunLoopTimerRef timer, void *info);

static pthread_t g_thread;
static atomic_bool g_thread_running;
static atomic_bool g_stop;
static dispatch_semaphore_t g_ready;
static kf_status g_start_status;
static CFRunLoopRef g_loop;
static uint8_t g_report[KF_SENSOR_REPORT_BYTES * 4u];

static void put_number(CFMutableDictionaryRef dict, CFStringRef key, int value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    if (number != NULL) {
        CFDictionarySetValue(dict, key, number);
        CFRelease(number);
    }
}

/* The keys IOHIDDevice's matchPropertyTable honours; Product is ignored there (M0). */
static void fill_matching(CFMutableDictionaryRef dict) {
    put_number(dict, CFSTR(kIOHIDVendorIDKey), (int)KF_SENSOR_VENDOR_ID);
    put_number(dict, CFSTR(kIOHIDDeviceUsagePageKey), (int)KF_SENSOR_USAGE_PAGE);
    put_number(dict, CFSTR(kIOHIDDeviceUsageKey), (int)KF_SENSOR_USAGE);
    CFDictionarySetValue(dict, CFSTR(kIOHIDTransportKey), CFSTR(KF_SENSOR_TRANSPORT));
}

bool kf_sensor_probe(void) {
    CFMutableDictionaryRef dict = IOServiceMatching(kIOHIDDeviceKey);
    if (dict == NULL) {
        return false;
    }
    fill_matching(dict);
    const io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, dict);
    if (service == MACH_PORT_NULL) {
        return false;
    }
    IOObjectRelease(service);
    return true;
}

static void on_removed(void *ref, IOReturn result, void *sender) {
    (void)ref;
    (void)result;
    (void)sender;
    kf_sensor_set_state(KF_SENSOR_ABSENT); /* Keytap stops routing keys here at once. */
    CFRunLoopStop(CFRunLoopGetCurrent());
}

static void stop_timer_fired(CFRunLoopTimerRef timer, void *info) {
    (void)timer;
    (void)info;
    CFRunLoopStop(CFRunLoopGetCurrent());
}

/* CF collections expose opaque values as const void pointers; IOHID's API uses the non-const
 * opaque reference type. The uintptr_t bridge preserves that ABI exactly. */
static IOHIDDeviceRef hid_ref(const void *value) {
    return (IOHIDDeviceRef)(uintptr_t)value; // NOLINT(performance-no-int-to-ptr): opaque CF bridge.
}

static IOHIDDeviceRef open_device(void) {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOHIDDeviceRef device = NULL;
    if (manager != NULL && dict != NULL) {
        fill_matching(dict);
        IOHIDManagerSetDeviceMatching(manager, dict);
        CFSetRef set = IOHIDManagerCopyDevices(manager);
        if (set != NULL && CFSetGetCount(set) == 1) {
            const void *value = NULL;
            CFSetGetValues(set, &value);
            device = hid_ref(value);
            CFRetain(device);
        }
        if (set != NULL) {
            CFRelease(set);
        }
    }
    if (dict != NULL) {
        CFRelease(dict);
    }
    if (manager != NULL) {
        CFRelease(manager);
    }
    return device;
}

static void *sensor_thread(void *arg) {
    (void)arg;
    (void)pthread_setname_np("com.keyfeel.sensor");
    IOHIDDeviceRef device = open_device();
    if (device == NULL) {
        g_start_status = KF_ERR_NOT_FOUND;
        dispatch_semaphore_signal(g_ready);
        return NULL;
    }
    const IOReturn opened = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone); /* Never seized. */
    if (opened != kIOReturnSuccess) {
        g_start_status = opened == kIOReturnNotPermitted || opened == kIOReturnNotPrivileged
                             ? KF_ERR_NO_PERMISSION
                             : KF_ERR_DEVICE;
        CFRelease(device);
        dispatch_semaphore_signal(g_ready);
        return NULL;
    }
    g_loop = CFRunLoopGetCurrent();
    IOHIDDeviceRegisterInputReportWithTimeStampCallback(device, g_report, sizeof g_report,
                                                        kf_sensor_on_report, NULL);
    IOHIDDeviceRegisterRemovalCallback(device, on_removed, NULL);
    IOHIDDeviceScheduleWithRunLoop(device, g_loop, kCFRunLoopDefaultMode);
    const CFTimeInterval period = (double)KF_PENDING_MAX_AGE_NS / 2e9;
    CFRunLoopTimerRef watchdog =
        CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + period, period, 0u,
                             0, kf_sensor_on_timer, NULL);
    CFRunLoopAddTimer(g_loop, watchdog, kCFRunLoopDefaultMode);
    g_start_status = KF_OK;
    dispatch_semaphore_signal(g_ready);

    while (!atomic_load_explicit(&g_stop, memory_order_acquire) &&
           kf_sensor_state_now(kf_time_now_ns()) != KF_SENSOR_ABSENT) {
        (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, false);
    }

    CFRunLoopTimerInvalidate(watchdog);
    CFRelease(watchdog);
    IOHIDDeviceRegisterInputReportWithTimeStampCallback(device, g_report, sizeof g_report, NULL,
                                                        NULL);
    IOHIDDeviceUnscheduleFromRunLoop(device, g_loop, kCFRunLoopDefaultMode);
    (void)IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    CFRelease(device);
    /* This thread is the pending queue's consumer: resolve what is left before leaving. */
    kf_sensor_flush_pending();
    return NULL;
}

kf_status kf_sensor_start_with_wake(kf_sensor_wake level) {
    if (atomic_load_explicit(&g_thread_running, memory_order_acquire)) {
        return KF_OK;
    }
    if (!kf_sensor_probe()) {
        kf_sensor_set_state(KF_SENSOR_ABSENT);
        return KF_ERR_NOT_FOUND;
    }
    kf_sensor_reset(); /* STOPPED: keys play at once until the first sample. */
    const kf_status wake = kf_sensor_driver_wake(level, KF_SENSOR_REPORT_INTERVAL_US);
    if (wake != KF_OK) {
        kf_sensor_set_state(wake == KF_ERR_NO_PERMISSION ? KF_SENSOR_DENIED : KF_SENSOR_ABSENT);
        return wake;
    }
    atomic_store_explicit(&g_stop, false, memory_order_relaxed);
    g_ready = dispatch_semaphore_create(0);
    if (g_ready == NULL) {
        (void)kf_sensor_driver_restore();
        return KF_ERR_NO_MEMORY;
    }
    pthread_attr_t attr;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, 0);
    const int created = pthread_create(&g_thread, &attr, sensor_thread, NULL);
    (void)pthread_attr_destroy(&attr);
    if (created != 0) {
        dispatch_release(g_ready);
        g_ready = NULL;
        (void)kf_sensor_driver_restore();
        return KF_ERR_NO_MEMORY;
    }
    (void)dispatch_semaphore_wait(g_ready, DISPATCH_TIME_FOREVER);
    dispatch_release(g_ready);
    g_ready = NULL;
    if (g_start_status != KF_OK) {
        (void)pthread_join(g_thread, NULL);
        (void)kf_sensor_driver_restore();
        kf_sensor_set_state(g_start_status == KF_ERR_NO_PERMISSION ? KF_SENSOR_DENIED
                                                                   : KF_SENSOR_ABSENT);
        return g_start_status;
    }
    atomic_store_explicit(&g_thread_running, true, memory_order_release);
    return KF_OK;
}

kf_status kf_sensor_start(void) {
    return kf_sensor_start_with_wake(KF_SENSOR_WAKE_NONE);
}

void kf_sensor_stop(void) {
    if (!atomic_load_explicit(&g_thread_running, memory_order_acquire)) {
        (void)kf_sensor_driver_restore();
        kf_sensor_flush_pending();
        return;
    }
    const bool removed = load_state() == KF_SENSOR_ABSENT;
    kf_sensor_set_state(KF_SENSOR_STOPPED); /* The keytap stops routing keys here first. */
    atomic_store_explicit(&g_stop, true, memory_order_release);
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(),
                                                   0.0, 0u, 0, stop_timer_fired, NULL);
    CFRunLoopAddTimer(g_loop, timer, kCFRunLoopDefaultMode);
    CFRunLoopWakeUp(g_loop);
    (void)pthread_join(g_thread, NULL);
    CFRelease(timer);
    atomic_store_explicit(&g_thread_running, false, memory_order_release);
    (void)kf_sensor_driver_restore();
    kf_sensor_flush_pending(); /* Anything pushed during the hand-off. */
    if (removed) {
        kf_sensor_set_state(KF_SENSOR_ABSENT);
    }
}

#else /* No measured layout (or a host build): no sensor, fallback mode (ADR-0011). */

bool kf_sensor_probe(void) {
    return false;
}

kf_status kf_sensor_start_with_wake(kf_sensor_wake level) {
    (void)level;
    kf_sensor_set_state(KF_SENSOR_ABSENT);
    return KF_ERR_NOT_FOUND;
}

kf_status kf_sensor_start(void) {
    return kf_sensor_start_with_wake(KF_SENSOR_WAKE_NONE);
}

void kf_sensor_stop(void) {
    kf_sensor_flush_pending();
    if (is_running(load_state())) {
        kf_sensor_set_state(KF_SENSOR_STOPPED);
    }
}

#endif
