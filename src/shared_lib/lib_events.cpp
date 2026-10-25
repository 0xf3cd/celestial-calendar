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
#include "lib_codes.hpp"
#include "celestial.h"

#include "ephemeris_events.hpp"
#include "toolbox.hpp"

namespace {

using astro::ephemeris::EventDirection;
using astro::ephemeris::Motion;
using astro::toolbox::AngleDeg;

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
  if (end_jde_tt - start_jde_tt > astro::ephemeris::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS) {
    throw std::invalid_argument {
      std::format("Event query exceeds {} TT days", astro::ephemeris::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS)
    };
  }
}

auto direction_code_of(const EventDirection direction) -> uint32_t {
  switch (direction) {
    case EventDirection::INCREASING: return CELESTIAL_EVENT_DIRECTION_INCREASING;
    case EventDirection::DECREASING: return CELESTIAL_EVENT_DIRECTION_DECREASING;
    case EventDirection::TANGENT:    return CELESTIAL_EVENT_DIRECTION_TANGENT;
  }
  throw std::runtime_error { "Unsupported event direction" };
}

auto motion_code_of(const Motion motion) -> uint32_t {
  switch (motion) {
    case Motion::DIRECT:    return CELESTIAL_EVENT_MOTION_DIRECT;
    case Motion::RETROGRADE: return CELESTIAL_EVENT_MOTION_RETROGRADE;
  }
  throw std::runtime_error { "Unsupported station motion" };
}

} // namespace


extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): Preserve the published target/angle/TT-interval/output parameter groups.
// #303: the ephemeris-event exports. Every export validates its arguments before any
// provider work, mirrors the core's deterministic ordering, and reports the full total
// through `event_count` even when the caller's capacity truncates the fill.

#pragma region Longitude Events

auto celestial_longitude_events(
  const uint32_t target_code,
  const double target_longitude_deg,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t * const event_count,
  CelestialLongitudeEventRecord * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    lib::check_count(event_count, "event_count");
    lib::check_slots(slots, slot_count);
    const auto target = lib::codes::target(target_code);
    check_target_angle(target_longitude_deg, "target_longitude_deg");
    check_interval(start_jde_tt, end_jde_tt);

    const auto events = astro::ephemeris::longitude_events(
      target,
      AngleDeg { target_longitude_deg },
      start_jde_tt,
      end_jde_tt
    );

    *event_count = static_cast<uint32_t>(events.size());
    const auto num_written = std::min(static_cast<uint32_t>(events.size()), slot_count);
    std::transform(
      events.cbegin(), events.cbegin() + num_written, slots,
      [](const auto& event) {
        return CelestialLongitudeEventRecord {
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

auto celestial_longitude_stations(
  const uint32_t target_code,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t * const event_count,
  CelestialStationEventRecord * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    lib::check_count(event_count, "event_count");
    lib::check_slots(slots, slot_count);
    const auto target = lib::codes::target(target_code);
    check_interval(start_jde_tt, end_jde_tt);

    const auto events = astro::ephemeris::stations(
      target,
      start_jde_tt,
      end_jde_tt
    );

    *event_count = static_cast<uint32_t>(events.size());
    const auto num_written = std::min(static_cast<uint32_t>(events.size()), slot_count);
    std::transform(
      events.cbegin(), events.cbegin() + num_written, slots,
      [](const auto& event) {
        return CelestialStationEventRecord {
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

auto celestial_longitude_difference_events(
  const uint32_t first_target_code,
  const uint32_t second_target_code,
  const double target_difference_deg,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t * const event_count,
  CelestialLongitudeDifferenceEventRecord * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    lib::check_count(event_count, "event_count");
    lib::check_slots(slots, slot_count);
    const auto first = lib::codes::target(first_target_code);
    const auto second = lib::codes::target(second_target_code);
    if (first_target_code == second_target_code) {
      throw std::invalid_argument { "Longitude-difference events require distinct targets" };
    }
    check_target_angle(target_difference_deg, "target_difference_deg");
    check_interval(start_jde_tt, end_jde_tt);

    const auto events = astro::ephemeris::longitude_difference_events(
      first,
      second,
      AngleDeg { target_difference_deg },
      start_jde_tt,
      end_jde_tt
    );

    *event_count = static_cast<uint32_t>(events.size());
    const auto num_written = std::min(static_cast<uint32_t>(events.size()), slot_count);
    std::transform(
      events.cbegin(), events.cbegin() + num_written, slots,
      [](const auto& event) {
        return CelestialLongitudeDifferenceEventRecord {
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


// NOLINTEND(bugprone-easily-swappable-parameters)
} // extern "C"
