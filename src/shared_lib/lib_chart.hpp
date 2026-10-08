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

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "celestial.h"
#include "chart.hpp"

namespace lib::chart {

// A larger core roster needs a separately published V2, never a wider V1 copy.
static_assert(astro::chart::BODY_COUNT == 14);
static_assert(std::tuple_size_v<decltype(astro::chart::Snapshot::bodies)> ==
              std::extent_v<decltype(ChartSnapshotV1::bodies)>);
static_assert(std::tuple_size_v<decltype(astro::house::Result::cusps)> ==
              std::extent_v<decltype(ChartHousesV1::cusps_deg)>);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): year/month/day/fraction is the published civil-date order.
inline auto validate_civil(const int32_t year, const uint32_t month, const uint32_t day, const double fraction) -> void {
  if (year < 1885 or year >= 2100) {
    throw std::invalid_argument { std::format("Chart civil year {} leaves [1885, 2100)", year) };
  }
  if (month < 1 or month > 12) {
    throw std::invalid_argument { std::format("Argument `month` must be in [1, 12], got {}", month) };
  }

  constexpr std::array<uint32_t, 12> DAYS_IN_MONTH { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  const bool leap_year = year % 4 == 0 and (year % 100 != 0 or year % 400 == 0);
  const uint32_t last_day = DAYS_IN_MONTH.at(month - 1) + (month == 2 and leap_year ? 1U : 0U);
  if (day < 1 or day > last_day) {
    throw std::invalid_argument {
      std::format("Argument `day` must be in [1, {}] for {}-{}, got {}", last_day, year, month, day)
    };
  }
  if (not std::isfinite(fraction) or fraction < 0.0 or fraction >= 1.0) {
    throw std::invalid_argument {
      std::format("Argument `fraction` must be finite and in [0, 1), got {}", fraction)
    };
  }
}

[[nodiscard]] inline auto civil_scale(const uint32_t code) -> astro::chart::Scale {
  switch (code) {
    case CHART_CIVIL_SCALE_UTC: return astro::chart::Scale::UTC;
    case CHART_CIVIL_SCALE_UT1: return astro::chart::Scale::UT1;
    default: throw std::invalid_argument { std::format("Unknown chart `civil_scale_code` {}", code) };
  }
}

[[nodiscard]] inline auto house_system(const uint32_t code) -> astro::house::System {
  switch (code) {
    case CHART_HOUSE_SYSTEM_EQUAL: return astro::house::System::EQUAL;
    case CHART_HOUSE_SYSTEM_WHOLE_SIGN: return astro::house::System::WHOLE_SIGN;
    case CHART_HOUSE_SYSTEM_PLACIDUS: return astro::house::System::PLACIDUS;
    default: throw std::invalid_argument { std::format("Unknown chart `house_system_code` {}", code) };
  }
}

[[nodiscard]] inline auto delta_t_model(const uint32_t code) -> astro::delta_t::Model {
  switch (code) {
    case CHART_DELTA_T_MODEL_DEFAULT:
    case CHART_DELTA_T_MODEL_ALGO5: return astro::delta_t::Model::ALGO5;
    case CHART_DELTA_T_MODEL_ALGO1: return astro::delta_t::Model::ALGO1;
    case CHART_DELTA_T_MODEL_ALGO2: return astro::delta_t::Model::ALGO2;
    case CHART_DELTA_T_MODEL_ALGO3: return astro::delta_t::Model::ALGO3;
    case CHART_DELTA_T_MODEL_ALGO4: return astro::delta_t::Model::ALGO4;
    default: throw std::invalid_argument { std::format("Unknown chart `delta_t_model_code` {}", code) };
  }
}

[[nodiscard]] inline auto target_code(const astro::ephemeris::Target target) -> uint32_t {
  using enum astro::ephemeris::Target;

  switch (target) {
    case SUN: return CHART_TARGET_SUN;
    case MOON: return CHART_TARGET_MOON;
    case MERCURY: return CHART_TARGET_MERCURY;
    case VENUS: return CHART_TARGET_VENUS;
    case MARS: return CHART_TARGET_MARS;
    case JUPITER: return CHART_TARGET_JUPITER;
    case SATURN: return CHART_TARGET_SATURN;
    case URANUS: return CHART_TARGET_URANUS;
    case NEPTUNE: return CHART_TARGET_NEPTUNE;
    case PLUTO: return CHART_TARGET_PLUTO;
    case MEAN_ASCENDING: return CHART_TARGET_MEAN_ASCENDING;
    case MEAN_DESCENDING: return CHART_TARGET_MEAN_DESCENDING;
    case TRUE_ASCENDING: return CHART_TARGET_TRUE_ASCENDING;
    case TRUE_DESCENDING: return CHART_TARGET_TRUE_DESCENDING;
    default: throw std::logic_error { "Chart core target has no V1 identity" };
  }
}

[[nodiscard]] inline auto snapshot_v1(const astro::chart::Snapshot& snapshot) -> ChartSnapshotV1 {
  ChartSnapshotV1 result {
    .valid = true,
    .jd_ut1 = snapshot.times.jd_ut1,
    .jde_tt = snapshot.times.jde_tt,
    .bodies = {},
    .houses = {
      .ascendant_deg = snapshot.houses.ascendant.deg(),
      .midheaven_deg = snapshot.houses.midheaven.deg(),
      .descendant_deg = snapshot.houses.descendant.deg(),
      .imum_coeli_deg = snapshot.houses.imum_coeli.deg(),
      .cusps_deg = {},
    },
  };

  for (std::size_t index = 0; index < snapshot.bodies.size(); ++index) {
    const auto& body = snapshot.bodies.at(index);
    const uint32_t code = target_code(body.target);
    if (code != index) {
      throw std::logic_error { "Chart core target order does not match V1" };
    }

    ChartBodyV1 record {
      .target_code = code,
      .present_fields = CHART_PRESENT_LATITUDE,
      .longitude_deg = body.longitude.deg(),
      .latitude_deg = body.latitude.deg(),
      .distance_au = 0.0,
      .longitude_rate_deg_per_tt_day = body.longitude_rate.deg_per_tt_day,
    };
    if (body.distance.has_value()) {
      record.present_fields |= CHART_PRESENT_DISTANCE;
      record.distance_au = body.distance->au();
    }
    std::span { result.bodies }[index] = record;
  }

  for (std::size_t index = 0; index < snapshot.houses.cusps.size(); ++index) {
    std::span { result.houses.cusps_deg }[index] = snapshot.houses.cusps.at(index).deg();
  }
  return result;
}

} // namespace lib::chart
