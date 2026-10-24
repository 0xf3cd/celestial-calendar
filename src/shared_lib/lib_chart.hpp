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

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "celestial.h"
#include "lib_codes.hpp"
#include "chart.hpp"
#include "datetime.hpp"
#include "ymd.hpp"

namespace lib::chart {

// A larger core roster requires a new published shape or major, not a wider ABI array.
static_assert(std::tuple_size_v<decltype(astro::chart::Snapshot::bodies)> ==
              std::extent_v<decltype(CelestialChartSnapshot::bodies)>);
static_assert(std::tuple_size_v<decltype(astro::house::Result::cusps)> ==
              std::extent_v<decltype(CelestialChartHouses::cusps_deg)>);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): year/month/day/fraction is the published civil-date order.
inline auto validate_civil(const int32_t year, const uint32_t month, const uint32_t day, const double fraction) -> void {
  const auto civil_year = util::checked_year(year);
  if (month < 1 or month > 12) {
    throw std::invalid_argument { std::format("Argument `month` must be in [1, 12], got {}", month) };
  }

  const auto month_end = civil_year / std::chrono::month { month } / std::chrono::last;
  const uint32_t last_day = static_cast<unsigned>(month_end.day());
  if (day < 1 or day > last_day) {
    throw std::invalid_argument {
      std::format("Argument `day` must be in [1, {}] for {}-{}, got {}", last_day, year, month, day)
    };
  }

  static_cast<void>(calendar::validate_fraction(fraction));
}

/** @brief Decode the civil input scale before passing it to the time layer. */
[[nodiscard]] inline auto civil_scale(const uint32_t code) -> astro::julian_day::Scale {
  switch (code) {
    case CELESTIAL_CIVIL_SCALE_UTC: return astro::julian_day::Scale::UTC;
    case CELESTIAL_CIVIL_SCALE_UT1: return astro::julian_day::Scale::UT1;
    default: throw std::invalid_argument { std::format("Unknown `civil_scale_code` {}", code) };
  }
}

/** @brief Decode the requested house geometry. */
[[nodiscard]] inline auto house_system(const uint32_t code) -> astro::house::System {
  switch (code) {
    case CELESTIAL_HOUSE_SYSTEM_EQUAL: return astro::house::System::EQUAL;
    case CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN: return astro::house::System::WHOLE_SIGN;
    case CELESTIAL_HOUSE_SYSTEM_PLACIDUS: return astro::house::System::PLACIDUS;
    default: throw std::invalid_argument { std::format("Unknown chart `house_system_code` {}", code) };
  }
}

/** @brief Copy a body and its provider identity into the fixed C record. */
[[nodiscard]] inline auto body_record(const astro::chart::BodyState& body) -> CelestialChartBody {
  return CelestialChartBody {
    .target_code = codes::code_of(body.target),
    .source_code = codes::code_of(astro::ephemeris::source_of(body.target)),
    .present_fields = CELESTIAL_PRESENT_LATITUDE | (body.distance.has_value() ? CELESTIAL_PRESENT_DISTANCE : UINT32_C(0)),
    .longitude_deg = body.longitude.deg(),
    .latitude_deg = body.latitude.deg(),
    .distance_au = body.distance.has_value() ? body.distance->au() : 0.0,
    .longitude_rate_deg_per_tt_day = body.longitude_rate.deg_per_tt_day,
  };
}

/** @brief Copy computed axes and cusps without changing their values. */
[[nodiscard]] inline auto house_record(const astro::house::Result& houses) -> CelestialChartHouses {
  CelestialChartHouses result {
    .ascendant_deg = houses.ascendant.deg(),
    .midheaven_deg = houses.midheaven.deg(),
    .descendant_deg = houses.descendant.deg(),
    .imum_coeli_deg = houses.imum_coeli.deg(),
    .cusps_deg = {},
  };
  for (std::size_t index = 0; index < houses.cusps.size(); ++index) {
    std::span { result.cusps_deg }[index] = houses.cusps.at(index).deg();
  }
  return result;
}

/** @brief Copy a snapshot together with the models resolved for this call. */
[[nodiscard]] inline auto snapshot_record(
  const astro::chart::Snapshot& snapshot,
  const astro::delta_t::Model delta_t_model,
  const astro::earth::nutation::Model nutation_model
) -> CelestialChartSnapshot {
  CelestialChartSnapshot result {
    .valid = true,
    .delta_t_identity_code = codes::code_of(delta_t_model),
    .nutation_identity_code = codes::code_of(nutation_model),
    .jd_ut1 = snapshot.times.jd_ut1,
    .jde_tt = snapshot.times.jde_tt,
    .bodies = {},
    .houses = house_record(snapshot.houses),
  };

  for (std::size_t index = 0; index < snapshot.bodies.size(); ++index) {
    const CelestialChartBody record = body_record(snapshot.bodies.at(index));
    if (record.target_code != index) {
      throw std::logic_error { "Chart core target order does not match the C snapshot" };
    }

    std::span { result.bodies }[index] = record;
  }

  return result;
}

} // namespace lib::chart
