#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Fails if any real-time translation unit (*_rt.o) imports a symbol that can
# allocate, lock, block, log, do I/O or send Objective-C messages.
# Portable to macOS's Bash 3.2 and BSD grep. Usage:
#   scripts/check-rt-symbols.sh <object-dir> [nm-command]
set -euo pipefail

obj_dir="${1:?usage: check-rt-symbols.sh <object-dir> [nm-command]}"
nm_cmd="${2:-nm}"

banned_symbols=(
    malloc calloc realloc reallocf free valloc posix_memalign aligned_alloc
    pthread_mutex_lock pthread_mutex_trylock pthread_cond_wait pthread_cond_timedwait
    'pthread_rwlock_[a-z]+' pthread_join os_unfair_lock_lock 'dispatch_[a-z_]+'
    'objc_msgSend[A-Za-z_]*' objc_retain objc_release 'objc_autorelease[A-Za-z_]*'
    _os_log_impl 'os_log_[a-z_]+' NSLog CFRetain CFRelease syslog
    printf fprintf vprintf vfprintf puts fputs putchar fwrite fflush
    open close read write fopen fclose fread
    sleep usleep nanosleep mach_wait_until sem_wait semaphore_wait
    _Znwm _Znam _ZdlPv _ZdaPv
)
banned="^_?($(
    IFS='|'
    echo "${banned_symbols[*]}"
))\$"

# Self-test: a pattern this grep rejects or misreads must fail loudly, never pass silently.
matches() { grep -Eq -- "$banned" <<<"$1"; }
if ! matches "_malloc" || ! matches "objc_msgSend" || matches "_memcpy" || matches "_sqrtf"; then
    echo "check-rt: pattern self-test failed with this grep; refusing to report clean" >&2
    exit 2
fi

objects=() # Bash 3.2 compatible: no mapfile.
while IFS= read -r object; do
    objects+=("$object")
done < <(find "$obj_dir" -name '*_rt.o' | sort)
if ((${#objects[@]} == 0)); then
    echo "check-rt: no *_rt.o objects under $obj_dir" >&2
    exit 2
fi

status=0
for object in "${objects[@]}"; do
    # shellcheck disable=SC2086 # nm_cmd may be "xcrun nm".
    symbols="$($nm_cmd -u "$object" | awk '{print $NF}')"
    set +e
    hits="$(grep -E -- "$banned" <<<"$symbols")"
    rc=$?
    set -e
    if ((rc > 1)); then
        echo "check-rt: grep failed on ${object#"$obj_dir"/}" >&2
        exit 2
    fi
    if [[ -n "$hits" ]]; then
        echo "check-rt: ${object#"$obj_dir"/} imports banned symbols:" >&2
        sed 's/^/    /' <<<"$hits" >&2
        status=1
    fi
done

if ((status == 0)); then
    echo "check-rt: ${#objects[@]} real-time units clean"
fi
exit "$status"
