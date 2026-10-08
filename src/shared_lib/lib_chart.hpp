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
#include "chart.hpp"
#include "datetime.hpp"

namespace lib::chart {

// A larger core roster needs a separately published V2, never a wider V1 copy.
static_assert(astro::chart::BODY_COUNT == 14);
static_assert(std::tuple_size_v<decltype(astro::chart::Snapshot::bodies)> ==
              std::extent_v<decltype(ChartSnapshotV1::bodies)>);
static_assert(std::tuple_size_v<decltype(astro::house::Result::cusps)> ==
              std::extent_v<decltype(ChartHousesV1::cusps_deg)>);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): year/month/day/fraction is the published civil-date order.
inline auto validate_civil(const int32_t year, const uint32_t month, const uint32_t day, const double fraction) -> void {
  if (year < static_cast<int>(std::chrono::year::min()) or year > static_cast<int>(std::chrono::year::max())) {
    throw std::invalid_argument { std::format("Argument `year` cannot be represented by std::chrono::year: {}", year) };
  }
  if (month < 1 or month > 12) {
    throw std::invalid_argument { std::format("Argument `month` must be in [1, 12], got {}", month) };
  }

  const auto month_end = std::chrono::year { year } / std::chrono::month { month } / std::chrono::last;
  const uint32_t last_day = static_cast<unsigned>(month_end.day());
  if (day < 1 or day > last_day) {
    throw std::invalid_argument {
      std::format("Argument `day` must be in [1, {}] for {}-{}, got {}", last_day, year, month, day)
    };
  }

  static_cast<void>(calendar::validate_fraction(fraction));
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

[[nodiscard]] inline auto body_v1(const astro::chart::BodyState& body) -> ChartBodyV1 {
  return ChartBodyV1 {
    .target_code = target_code(body.target),
    .present_fields = CHART_PRESENT_LATITUDE | (body.distance.has_value() ? CHART_PRESENT_DISTANCE : UINT32_C(0)),
    .longitude_deg = body.longitude.deg(),
    .latitude_deg = body.latitude.deg(),
    .distance_au = body.distance.has_value() ? body.distance->au() : 0.0,
    .longitude_rate_deg_per_tt_day = body.longitude_rate.deg_per_tt_day,
  };
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
    const ChartBodyV1 record = body_v1(snapshot.bodies.at(index));
    if (record.target_code != index) {
      throw std::logic_error { "Chart core target order does not match V1" };
    }

    std::span { result.bodies }[index] = record;
  }

  for (std::size_t index = 0; index < snapshot.houses.cusps.size(); ++index) {
    std::span { result.houses.cusps_deg }[index] = snapshot.houses.cusps.at(index).deg();
  }
  return result;
}

} // namespace lib::chart
