// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_test_list.h
 * @brief Every core test, in run order. Add a test here and define test_<name>() in a test file.
 */
#ifndef KF_TEST_LIST_H
#define KF_TEST_LIST_H

#define KF_TEST_LIST(X)                       \
    X(status_strings_cover_every_code)        \
    X(time_conversion_is_exact)               \
    X(rng_stays_in_range)                     \
    X(rng_never_repeats_last)                 \
    X(select_matches_sort)                    \
    X(spsc_push_pop_order)                    \
    X(spsc_full_and_empty)                    \
    X(spsc_wraparound)                        \
    X(spsc_peek_does_not_consume)             \
    X(spsc_threaded_stress)                   \
    X(keymap_known_keys)                      \
    X(keymap_spec_pans)                       \
    X(keymap_unknown_key_defaults)            \
    X(keymap_make_event_drops_keycode)        \
    X(velocity_needs_enough_data)             \
    X(velocity_below_noise_floor_is_zero)     \
    X(velocity_log_mapping)                   \
    X(velocity_baseline_removes_gravity)      \
    X(velocity_row_gain)                      \
    X(velocity_rejects_invalid_calibration)   \
    X(tier_weights_at_key_points)             \
    X(tier_mix_is_equal_power)                \
    X(tier_mix_skips_quiet_voice)             \
    X(autoadapt_moves_toward_percentiles)     \
    X(autoadapt_respects_clamp)               \
    X(autoadapt_disabled_keeps_calibration)   \
    X(calibration_compute_fits_rows)          \
    X(calibration_compute_rejects_bad_input)  \
    X(emit_key_down_blends_tiers)             \
    X(emit_key_up_plays_release)              \
    X(emit_drops_when_queue_full)             \
    X(render_drains_queues_and_is_silent)     \
    X(render_drops_stale_requests)            \
    X(sensor_resolves_after_window)           \
    X(sensor_flush_uses_default_velocity)     \
    X(keytap_dispatch_routes_by_sensor_state) \
    X(tool_parse_numbers)                     \
    X(tool_format_hex_and_decimal)            \
    X(tool_match_arguments)                   \
    X(tool_match_info)

#endif /* KF_TEST_LIST_H */
