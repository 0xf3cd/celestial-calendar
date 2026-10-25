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
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "lib.hpp"
#include "lib_chart.hpp"
#include "celestial.h"

#include "chart.hpp"
#include "ephemeris.hpp"
#include "house.hpp"

namespace {

using astro::ephemeris::Target;

/** @brief Reject empty, unknown and repeated targets before any provider evaluation. */
auto check_selection(const std::span<const uint32_t> codes) -> void {
  if (codes.empty()) {
    throw std::invalid_argument { "Argument `target_count` must be positive (an empty selection is invalid)." };
  }

  auto seen = std::vector<uint32_t>();
  seen.reserve(codes.size());
  for (const uint32_t code : codes) {
    static_cast<void>(lib::codes::target(code));
    if (std::ranges::find(seen, code) != seen.end()) {
      throw std::invalid_argument { std::format("Argument `target_codes` duplicates target code {}", code) };
    }
    seen.push_back(code);
  }
}

/** @brief Evaluate a complete selection before publishing a total or writing caller-owned slots. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): The helper retains the public selection/count/epoch/output order.
template <typename Record, typename MakeRecord>
auto selected_positions(
  const uint32_t* const target_codes,
  const uint32_t target_count,
  const double jde_tt,
  uint32_t* const record_count,
  Record* const slots,
  const uint32_t slot_count,
  const MakeRecord& make_record
) -> uint32_t {
  lib::check_count(record_count, "record_count");
  lib::check_slots(slots, slot_count);
  if (target_codes == nullptr) {
    throw std::invalid_argument { "Argument `target_codes` is null." };
  }
  if (target_count > lib::codes::TARGETS.size()) {
    throw std::invalid_argument { "Argument `target_count` exceeds the target roster" };
  }
  const auto selection = std::span { target_codes, target_count };
  check_selection(selection);
  if (not std::isfinite(jde_tt)) {
    throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
  }

  auto positions = std::vector<Record>();
  positions.reserve(target_count);
  for (const uint32_t code : selection) {
    positions.push_back(std::invoke(make_record, lib::codes::target(code), jde_tt));
  }

  *record_count = static_cast<uint32_t>(positions.size());
  const uint32_t written = std::min(*record_count, slot_count);
  if (written > 0) {
    std::copy_n(positions.cbegin(), written, slots);
  }
  return written;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace


extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): selection pointer/count is the published query contract.
auto celestial_positions(
  const uint32_t* const target_codes,
  const uint32_t target_count,
  const double jde_tt,
  uint32_t* const record_count,
  CelestialPositionRecord* const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    return selected_positions(
      target_codes, target_count, jde_tt, record_count, slots, slot_count,
      [](const Target target, const double epoch) {
        const auto coordinate = astro::ephemeris::position(target, epoch);
        return CelestialPositionRecord {
          .target_code = lib::codes::code_of(target),
          .present_fields = CELESTIAL_PRESENT_LATITUDE
            | (coordinate.distance.has_value() ? CELESTIAL_PRESENT_DISTANCE : UINT32_C(0)),
          .longitude_deg = coordinate.longitude.deg(),
          .latitude_deg = coordinate.latitude.deg(),
          .distance_au = coordinate.distance.has_value() ? coordinate.distance->au() : 0.0,
          .source_code = lib::codes::code_of(astro::ephemeris::source_of(target)),
        };
      }
    );
  });
}

auto celestial_equatorial_positions(
  const uint32_t* const target_codes,
  const uint32_t target_count,
  const double jde_tt,
  uint32_t* const record_count,
  CelestialEquatorialPositionRecord* const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    return selected_positions(
      target_codes, target_count, jde_tt, record_count, slots, slot_count,
      [](const Target target, const double epoch) {
        const auto coordinate = astro::ephemeris::equatorial_position(target, epoch);
        return CelestialEquatorialPositionRecord {
          .target_code = lib::codes::code_of(target),
          .present_fields = CELESTIAL_PRESENT_LATITUDE
            | (coordinate.distance.has_value() ? CELESTIAL_PRESENT_DISTANCE : UINT32_C(0)),
          .right_ascension_deg = coordinate.right_ascension.deg(),
          .declination_deg = coordinate.declination.deg(),
          .distance_au = coordinate.distance.has_value() ? coordinate.distance->au() : 0.0,
          .source_code = lib::codes::code_of(astro::ephemeris::source_of(target)),
        };
      }
    );
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTBEGIN(bugprone-easily-swappable-parameters): the civil/location/selector order is the published query contract.
auto celestial_house_query(
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double fraction,
  const uint32_t civil_scale_code,
  const uint32_t delta_t_model_code,
  const uint32_t nutation_model_code,
  const double latitude_deg,
  const double longitude_deg,
  const uint32_t house_system_code
) -> CelestialHouseQueryResult {
  return lib::wrap_export(__func__, [=] {
    const auto scale = lib::chart::civil_scale(civil_scale_code);
    const auto house_system = lib::chart::house_system(house_system_code);
    const auto delta_t_model = lib::codes::delta_t_model(delta_t_model_code);
    const auto nutation_model = lib::codes::nutation_model(nutation_model_code);

    lib::chart::validate_civil(year, month, day, fraction);
    const auto times = astro::chart::resolve_civil_times(
      calendar::Datetime { util::to_ymd(year, month, day), fraction },
      scale,
      delta_t_model
    );
    const auto houses = astro::house::calculate(
      times.jd_ut1,
      times.jde_tt,
      astro::GeoLocation { .latitude = astro::toolbox::AngleDeg { latitude_deg }, .longitude = astro::toolbox::AngleDeg { longitude_deg } },
      house_system,
      nutation_model
    );

    return CelestialHouseQueryResult {
      .valid = true,
      .house_system_code = house_system_code,
      .delta_t_identity_code = lib::codes::code_of(delta_t_model),
      .nutation_identity_code = lib::codes::code_of(nutation_model),
      .jd_ut1 = times.jd_ut1,
      .jde_tt = times.jde_tt,
      .houses = lib::chart::house_record(houses),
    };
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
