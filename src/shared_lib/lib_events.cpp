/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <cmath>
#include <format>

#include "lib.hpp"
#include "celestial.h"

#include "ephemeris_events.hpp"
#include "toolbox.hpp"

namespace {

using astro::ephemeris::EventDirection;
using astro::ephemeris::Motion;
using astro::ephemeris::Target;
using astro::toolbox::AngleDeg;

constexpr uint32_t TARGET_COUNT = 14;
// The event-search core's published limit (ephemeris_events.hpp); the boundary states it
// in the error message so an FFI caller learns the bound without reading C++.
constexpr double MAXIMUM_QUERY_SPAN_TT_DAYS = 3660.0;

auto check_event_count_ptr(uint32_t * const event_count) -> void {
  if (event_count == nullptr) {
    throw std::invalid_argument { "Argument `event_count` is null." };
  }
  // Deterministic out-parameter state: every failure path below leaves *event_count == 0.
  *event_count = 0;
}

auto check_event_slots(const auto * const slots, const uint32_t slot_count) -> void {
  if (slots == nullptr and slot_count > 0) {
    throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
  }
}

auto check_target_code(const uint32_t target_code, const char * const name) -> void {
  if (target_code >= TARGET_COUNT) {
    throw std::invalid_argument {
      std::format("Argument `{}` must be an EVENT_TARGET_* code in [0, {}], got {}", name, TARGET_COUNT - 1, target_code)
    };
  }
}

auto check_target_angle(const double angle_deg, const char * const name) -> void {
  if (not std::isfinite(angle_deg)) {
    throw std::invalid_argument {
      std::format("Argument `{}` is not finite, got {}", name, angle_deg)
    };
  }
}

auto check_interval(const double start_jde_tt, const double end_jde_tt) -> void {
  if (not std::isfinite(start_jde_tt) or not std::isfinite(end_jde_tt)) {
    throw std::invalid_argument { "Event query interval must be finite" };
  }
  if (not (start_jde_tt < end_jde_tt)) {
    throw std::invalid_argument { "Event query interval must be nonempty" };
  }
  if (end_jde_tt - start_jde_tt > MAXIMUM_QUERY_SPAN_TT_DAYS) {
    throw std::invalid_argument {
      std::format("Event query exceeds {} TT days", MAXIMUM_QUERY_SPAN_TT_DAYS)
    };
  }
}

auto direction_code_of(const EventDirection direction) -> uint32_t {
  switch (direction) {
    case EventDirection::INCREASING: return EVENT_DIRECTION_INCREASING;
    case EventDirection::DECREASING: return EVENT_DIRECTION_DECREASING;
    case EventDirection::TANGENT:    return EVENT_DIRECTION_TANGENT;
  }
  throw std::runtime_error { "Unsupported event direction" };
}

auto motion_code_of(const Motion motion) -> uint32_t {
  switch (motion) {
    case Motion::DIRECT:    return EVENT_MOTION_DIRECT;
    case Motion::RETROGRADE: return EVENT_MOTION_RETROGRADE;
  }
  throw std::runtime_error { "Unsupported station motion" };
}

} // namespace


extern "C" {

// #303: the ephemeris-event exports. Every export validates its arguments before any
// provider work, mirrors the core's deterministic ordering, and reports the full total
// through `event_count` even when the caller's capacity truncates the fill.

#pragma region Longitude Events

auto longitude_events(
  const uint32_t target_code,
  const double target_longitude_deg,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- start/end is the published half-open TT interval.
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t * const event_count,
  LongitudeEventRecord * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("longitude_events", [=] {
    check_event_count_ptr(event_count);
    check_event_slots(slots, slot_count);
    check_target_code(target_code, "target_code");
    check_target_angle(target_longitude_deg, "target_longitude_deg");
    check_interval(start_jde_tt, end_jde_tt);

    const auto events = astro::ephemeris::longitude_events(
      static_cast<Target>(target_code),
      AngleDeg { target_longitude_deg },
      start_jde_tt,
      end_jde_tt
    );

    *event_count = static_cast<uint32_t>(events.size());
    const auto num_written = std::min(static_cast<uint32_t>(events.size()), slot_count);
    std::transform(
      events.cbegin(), events.cbegin() + num_written, slots,
      [](const auto& event) {
        return LongitudeEventRecord {
          .jde_tt = event.jde_tt,
          .longitude_deg = event.longitude.deg(),
          .longitude_rate_deg_per_tt_day = event.longitude_rate.deg_per_tt_day,
          .direction_code = direction_code_of(event.direction),
        };
      }
    );
    return num_written;
  });
}

#pragma endregion


#pragma region Longitude Stations

auto longitude_stations(
  const uint32_t target_code,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- start/end is the published half-open TT interval.
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t * const event_count,
  StationEventRecord * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("longitude_stations", [=] {
    check_event_count_ptr(event_count);
    check_event_slots(slots, slot_count);
    check_target_code(target_code, "target_code");
    check_interval(start_jde_tt, end_jde_tt);

    const auto events = astro::ephemeris::stations(
      static_cast<Target>(target_code),
      start_jde_tt,
      end_jde_tt
    );

    *event_count = static_cast<uint32_t>(events.size());
    const auto num_written = std::min(static_cast<uint32_t>(events.size()), slot_count);
    std::transform(
      events.cbegin(), events.cbegin() + num_written, slots,
      [](const auto& event) {
        return StationEventRecord {
          .jde_tt = event.jde_tt,
          .longitude_deg = event.longitude.deg(),
          .longitude_rate_deg_per_tt_day = event.longitude_rate.deg_per_tt_day,
          .motion_before_code = motion_code_of(event.motion_before),
          .motion_after_code = motion_code_of(event.motion_after),
        };
      }
    );
    return num_written;
  });
}

#pragma endregion


#pragma region Longitude Difference Events

auto longitude_difference_events(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- first/second is the published directed pair order.
  const uint32_t first_target_code,
  const uint32_t second_target_code,
  const double target_difference_deg,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- start/end is the published half-open TT interval.
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t * const event_count,
  LongitudeDifferenceEventRecord * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("longitude_difference_events", [=] {
    check_event_count_ptr(event_count);
    check_event_slots(slots, slot_count);
    check_target_code(first_target_code, "first_target_code");
    check_target_code(second_target_code, "second_target_code");
    if (first_target_code == second_target_code) {
      throw std::invalid_argument { "Longitude-difference events require distinct targets" };
    }
    check_target_angle(target_difference_deg, "target_difference_deg");
    check_interval(start_jde_tt, end_jde_tt);

    const auto events = astro::ephemeris::longitude_difference_events(
      static_cast<Target>(first_target_code),
      static_cast<Target>(second_target_code),
      AngleDeg { target_difference_deg },
      start_jde_tt,
      end_jde_tt
    );

    *event_count = static_cast<uint32_t>(events.size());
    const auto num_written = std::min(static_cast<uint32_t>(events.size()), slot_count);
    std::transform(
      events.cbegin(), events.cbegin() + num_written, slots,
      [](const auto& event) {
        return LongitudeDifferenceEventRecord {
          .jde_tt = event.jde_tt,
          .longitude_a_deg = event.longitude_a.deg(),
          .longitude_b_deg = event.longitude_b.deg(),
          .longitude_rate_a_deg_per_tt_day = event.longitude_rate_a.deg_per_tt_day,
          .longitude_rate_b_deg_per_tt_day = event.longitude_rate_b.deg_per_tt_day,
          .signed_difference_deg = event.signed_difference.deg(),
          .direction_code = direction_code_of(event.direction),
        };
      }
    );
    return num_written;
  });
}

#pragma endregion


}
