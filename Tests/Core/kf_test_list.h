// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_test_list.h
 * @brief Every core test, in run order. Add a test here and define test_<name>() in a test file.
 */
#ifndef KF_TEST_LIST_H
#define KF_TEST_LIST_H

#define KF_TEST_LIST(X)                        \
    X(status_strings_cover_every_code)         \
    X(time_conversion_is_exact)                \
    X(rng_stays_in_range)                      \
    X(rng_never_repeats_last)                  \
    X(select_matches_sort)                     \
    X(spsc_push_pop_order)                     \
    X(spsc_full_and_empty)                     \
    X(spsc_wraparound)                         \
    X(spsc_peek_does_not_consume)              \
    X(spsc_threaded_stress)                    \
    X(keymap_known_keys)                       \
    X(keymap_spec_pans)                        \
    X(keymap_unknown_key_defaults)             \
    X(keymap_make_event_drops_keycode)         \
    X(velocity_needs_enough_data)              \
    X(velocity_below_noise_floor_is_zero)      \
    X(velocity_log_mapping)                    \
    X(velocity_baseline_removes_gravity)       \
    X(velocity_row_gain)                       \
    X(velocity_rejects_invalid_calibration)    \
    X(tier_weights_at_key_points)              \
    X(tier_mix_is_equal_power)                 \
    X(tier_mix_skips_quiet_voice)              \
    X(autoadapt_moves_toward_percentiles)      \
    X(autoadapt_respects_clamp)                \
    X(autoadapt_disabled_keeps_calibration)    \
    X(calibration_compute_fits_rows)           \
    X(calibration_compute_rejects_bad_input)   \
    X(emit_key_down_blends_tiers)              \
    X(emit_key_up_plays_release)               \
    X(emit_drops_when_queue_full)              \
    X(render_drains_queues_and_is_silent)      \
    X(render_drops_stale_requests)             \
    X(sensor_resolves_after_window)            \
    X(sensor_flush_uses_default_velocity)      \
    X(keytap_dispatch_routes_by_sensor_state)  \
    X(tool_parse_numbers)                      \
    X(tool_format_hex_and_decimal)             \
    X(tool_match_arguments)                    \
    X(tool_match_info)                         \
    X(mixer_plays_with_constant_power_pan)     \
    X(mixer_voice_ends_and_goes_silent)        \
    X(mixer_steals_the_oldest_voice)           \
    X(mixer_clipper_keeps_output_within_unity) \
    X(mixer_mute_and_volume)                   \
    X(mixer_pack_swap_fades_then_releases)     \
    X(engine_callback_counters)                \
    X(pack_fallback_rules)                     \
    X(pack_derived_flags)                      \
    X(pack_rejects_bad_input)                  \
    X(pack_decodes_files)                      \
    X(keytap_modifier_down_and_up)             \
    X(keytap_builtin_rule)                     \
    X(keytap_handle_key_routes_and_reports)    \
    X(sensor_decodes_each_layout)              \
    X(sensor_falls_back_without_a_layout)      \
    X(sensor_expired_keys_play_at_default)     \
    X(sensor_reports_peaks_and_rolling_force)  \
    X(sensor_takes_calibration_while_running)  \
    X(sensor_measures_noise_floor)

#endif /* KF_TEST_LIST_H */
