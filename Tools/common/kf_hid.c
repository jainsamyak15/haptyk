// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include "kf_hid.h"

#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>

/* Copies a CFString as UTF-8, truncating on a character boundary. Empty when absent. */
static void copy_cfstring(CFTypeRef value, char *buf, size_t len) {
    buf[0] = '\0';
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID() || len == 0u) {
        return;
    }
    const CFStringRef text = (CFStringRef)value;
    CFIndex used = 0;
    (void)CFStringGetBytes(text, CFRangeMake(0, CFStringGetLength(text)), kCFStringEncodingUTF8,
                           '?', false, (UInt8 *)buf, (CFIndex)len - 1, &used);
    buf[used] = '\0';
}

static int64_t number_value(CFTypeRef value) {
    int64_t out = -1;
    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &out)) {
            out = -1;
        }
    }
    return out;
}

static int64_t device_number(IOHIDDeviceRef device, CFStringRef key) {
    return number_value(IOHIDDeviceGetProperty(device, key));
}

static int device_flag(IOHIDDeviceRef device, CFStringRef key) {
    const CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (value == NULL) {
        return -1;
    }
    if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
    }
    const int64_t number = number_value(value);
    return number < 0 ? -1 : number != 0;
}

static bool set_number(CFMutableDictionaryRef dict, CFStringRef key, int64_t value) {
    if (value < 0) {
        return true;
    }
    const CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
    if (number == NULL) {
        return false;
    }
    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
    return true;
}

static bool set_string(CFMutableDictionaryRef dict, CFStringRef key, const char *value) {
    if (value == NULL) {
        return true;
    }
    const CFStringRef text =
        CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);
    if (text == NULL) {
        return false;
    }
    CFDictionarySetValue(dict, key, text);
    CFRelease(text);
    return true;
}

bool kf_hid_match_fill(CFMutableDictionaryRef dict, const kf_tool_match *match) {
    return set_number(dict, CFSTR(kIOHIDVendorIDKey), match->vendor_id) &&
           set_number(dict, CFSTR(kIOHIDProductIDKey), match->product_id) &&
           set_number(dict, CFSTR(kIOHIDDeviceUsagePageKey), match->usage_page) &&
           set_number(dict, CFSTR(kIOHIDDeviceUsageKey), match->usage) &&
           set_number(dict, CFSTR(kIOHIDLocationIDKey), match->location_id) &&
           set_string(dict, CFSTR(kIOHIDTransportKey), match->transport) &&
           set_string(dict, CFSTR(kIOHIDProductKey), match->product);
}

CFMutableDictionaryRef kf_hid_match_dictionary(const kf_tool_match *match) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict != NULL && !kf_hid_match_fill(dict, match)) {
        CFRelease(dict);
        return NULL;
    }
    return dict;
}

static void read_usage_pairs(IOHIDDeviceRef device, kf_hid_info *out) {
    const CFTypeRef value = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDDeviceUsagePairsKey));
    if (value == NULL || CFGetTypeID(value) != CFArrayGetTypeID()) {
        return;
    }
    const CFArrayRef pairs = (CFArrayRef)value;
    const CFIndex count = CFArrayGetCount(pairs);
    for (CFIndex i = 0; i < count; ++i) {
        const CFTypeRef entry = CFArrayGetValueAtIndex(pairs, i);
        if (entry == NULL || CFGetTypeID(entry) != CFDictionaryGetTypeID()) {
            continue;
        }
        const CFDictionaryRef pair = (CFDictionaryRef)entry;
        const int64_t page =
            number_value(CFDictionaryGetValue(pair, CFSTR(kIOHIDDeviceUsagePageKey)));
        const int64_t usage = number_value(CFDictionaryGetValue(pair, CFSTR(kIOHIDDeviceUsageKey)));
        if (out->pair_count < KF_HID_MAX_PAIRS) {
            out->pairs[out->pair_count][0] = (uint16_t)(page < 0 ? 0 : page);
            out->pairs[out->pair_count][1] = (uint16_t)(usage < 0 ? 0 : usage);
        }
        out->pair_count++;
    }
}

static void read_descriptor(IOHIDDeviceRef device, kf_hid_info *out) {
    const CFTypeRef value = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey));
    if (value == NULL || CFGetTypeID(value) != CFDataGetTypeID()) {
        return;
    }
    const CFDataRef data = (CFDataRef)value;
    const CFIndex length = CFDataGetLength(data);
    out->descriptor_len = (size_t)length;
    const CFIndex kept =
        length < (CFIndex)KF_HID_MAX_DESCRIPTOR ? length : (CFIndex)KF_HID_MAX_DESCRIPTOR;
    CFDataGetBytes(data, CFRangeMake(0, kept), out->descriptor);
}

static void read_registry(IOHIDDeviceRef device, kf_hid_info *out) {
    const io_service_t service = IOHIDDeviceGetService(device);
    if (service == MACH_PORT_NULL) {
        return;
    }
    uint64_t id = 0u;
    if (IORegistryEntryGetRegistryEntryID(service, &id) == KERN_SUCCESS) {
        out->registry_id = id;
    }
    io_name_t name;
    if (IOObjectGetClass(service, name) == KERN_SUCCESS) {
        (void)snprintf(out->io_class, sizeof out->io_class, "%s", name);
    }
    const CFStringRef path = IORegistryEntryCopyPath(service, kIOServicePlane);
    if (path != NULL) {
        copy_cfstring(path, out->path, sizeof out->path);
        CFRelease(path);
    }
}

void kf_hid_info_read(IOHIDDeviceRef device, kf_hid_info *out) {
    memset(out, 0, sizeof *out);
    out->vendor_id = device_number(device, CFSTR(kIOHIDVendorIDKey));
    out->product_id = device_number(device, CFSTR(kIOHIDProductIDKey));
    out->usage_page = device_number(device, CFSTR(kIOHIDPrimaryUsagePageKey));
    out->usage = device_number(device, CFSTR(kIOHIDPrimaryUsageKey));
    out->location_id = device_number(device, CFSTR(kIOHIDLocationIDKey));
    out->max_input_report = device_number(device, CFSTR(kIOHIDMaxInputReportSizeKey));
    out->max_output_report = device_number(device, CFSTR(kIOHIDMaxOutputReportSizeKey));
    out->max_feature_report = device_number(device, CFSTR(kIOHIDMaxFeatureReportSizeKey));
    out->report_interval_us = device_number(device, CFSTR(kIOHIDReportIntervalKey));
    out->built_in = device_flag(device, CFSTR(kIOHIDBuiltInKey));
    copy_cfstring(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey)), out->product,
                  sizeof out->product);
    copy_cfstring(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDManufacturerKey)), out->manufacturer,
                  sizeof out->manufacturer);
    copy_cfstring(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDTransportKey)), out->transport,
                  sizeof out->transport);
    read_usage_pairs(device, out);
    read_descriptor(device, out);
    read_registry(device, out);
}

static uint64_t registry_id_of(const void *device) {
    uint64_t id = 0u;
    const io_service_t service = IOHIDDeviceGetService(kf_hid_ref(device));
    if (service != MACH_PORT_NULL) {
        (void)IORegistryEntryGetRegistryEntryID(service, &id);
    }
    return id;
}

static CFComparisonResult compare_registry_id(const void *a, const void *b, void *ctx) {
    (void)ctx;
    const uint64_t x = registry_id_of(a);
    const uint64_t y = registry_id_of(b);
    return x < y ? kCFCompareLessThan : (x > y ? kCFCompareGreaterThan : kCFCompareEqualTo);
}

CFArrayRef kf_hid_copy_matching_devices(IOHIDManagerRef manager, const kf_tool_match *match) {
    CFMutableDictionaryRef dict = NULL;
    if (!kf_tool_match_is_empty(match)) {
        dict = kf_hid_match_dictionary(match);
        if (dict == NULL) {
            return NULL;
        }
    }
    IOHIDManagerSetDeviceMatching(manager, dict); /* NULL matches every device. */
    if (dict != NULL) {
        CFRelease(dict);
    }

    const CFSetRef set = IOHIDManagerCopyDevices(manager);
    if (set == NULL) {
        return NULL;
    }
    const CFIndex count = CFSetGetCount(set);
    const void **values = count > 0 ? (const void **)CFAllocatorAllocate(
                                          kCFAllocatorDefault, count * (CFIndex)sizeof(void *), 0)
                                    : NULL;
    kf_hid_info *info = CFAllocatorAllocate(kCFAllocatorDefault, sizeof *info, 0);
    CFMutableArrayRef array =
        CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    if (count > 0 && values != NULL && info != NULL && array != NULL) {
        CFSetGetValues(set, values);
        for (CFIndex i = 0; i < count; ++i) {
            kf_hid_info_read(kf_hid_ref(values[i]), info);
            if (kf_tool_match_info(match, info)) {
                CFArrayAppendValue(array, values[i]);
            }
        }
        CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)), compare_registry_id, NULL);
    }
    if (values != NULL) {
        CFAllocatorDeallocate(kCFAllocatorDefault, (void *)values);
    }
    if (info != NULL) {
        CFAllocatorDeallocate(kCFAllocatorDefault, info);
    }
    CFRelease(set);
    if (array != NULL && CFArrayGetCount(array) == 0) {
        CFRelease(array);
        array = NULL;
    }
    return array;
}

const char *kf_hid_return_string(IOReturn result) {
    switch (result) {
    case kIOReturnSuccess:
        return "success";
    case kIOReturnNotPermitted:
        return "not permitted";
    case kIOReturnNotPrivileged:
        return "not privileged";
    case kIOReturnExclusiveAccess:
        return "exclusive access";
    case kIOReturnNoDevice:
        return "no device";
    case kIOReturnNotOpen:
        return "not open";
    case kIOReturnOffline:
        return "offline";
    case kIOReturnUnsupported:
        return "unsupported";
    case kIOReturnBadArgument:
        return "bad argument";
    case kIOReturnNoMemory:
        return "no memory";
    case kIOReturnTimeout:
        return "timeout";
    case kIOReturnAborted:
        return "aborted";
    case kIOReturnError:
        return "general error";
    default:
        return "unknown";
    }
}

static void read_sysctl_string(const char *name, char *buf, size_t len) {
    size_t size = len;
    if (len == 0u || sysctlbyname(name, buf, &size, NULL, 0) != 0 || size == 0u) {
        (void)snprintf(buf, len, "unknown");
        return;
    }
    buf[size < len ? size : len - 1u] = '\0';
}

void kf_hid_hw_model(char *buf, size_t len) {
    read_sysctl_string("hw.model", buf, len);
}

void kf_hid_os_version(char *buf, size_t len) {
    char version[64];
    char build[64];
    read_sysctl_string("kern.osproductversion", version, sizeof version);
    read_sysctl_string("kern.osversion", build, sizeof build);
    (void)snprintf(buf, len, "%s (%s)", version, build);
}
