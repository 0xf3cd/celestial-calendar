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

#include <array>
#include <format>
#include <iomanip>
#include <iostream>
#include <string_view>

#include "chart.hpp"

auto main() -> int {
  const calendar::Datetime civil_dt { util::to_ymd(2026, 1, 1), 0.5 };
  const astro::GeoLocation location {
    .latitude = astro::toolbox::AngleDeg { 51.5 },
    .longitude = astro::toolbox::AngleDeg { 0.0 },
  };
  constexpr auto scale = astro::chart::Scale::UTC;
  constexpr auto system = astro::house::System::PLACIDUS;
  constexpr auto model = astro::delta_t::Model::ALGO5;
  constexpr std::array<std::string_view, 3> SYSTEM_NAMES { "EQUAL", "WHOLE_SIGN", "PLACIDUS" };
  constexpr std::array<std::string_view, 5> MODEL_NAMES { "ALGO1", "ALGO2", "ALGO3", "ALGO4", "ALGO5" };
  const auto snapshot = astro::chart::calculate(civil_dt, scale, location, system, model);
  const auto datetime = std::format("{}T{}", civil_dt.ymd, civil_dt.time_of_day);
  std::cout << std::setprecision(17)
    << "{\n  \"role\": \"Chart snapshot replay example, not an independent physical-accuracy oracle\",\n"
    << "  \"input\": {\"datetime\": \"" << datetime
    << "\", \"scale\": \"" << (scale == astro::chart::Scale::UTC ? "UTC" : "UT1") << "\", "
    << "\"latitude_north_deg\": " << location.latitude.deg()
    << ", \"longitude_east_deg\": " << location.longitude.deg()
    << ", \"system\": \"" << SYSTEM_NAMES.at(static_cast<std::size_t>(system))
    << "\", \"delta_t_model\": \"" << MODEL_NAMES.at(static_cast<std::size_t>(model)) << "\"},\n"
    << "  \"times\": {\"jd_ut1\": " << snapshot.times.jd_ut1
    << ", \"jde_tt\": " << snapshot.times.jde_tt << "},\n  \"bodies\": [\n";
  bool first = true;
  constexpr std::array<std::string_view, 14> TARGET_NAMES {
    "SUN", "MOON", "MERCURY", "VENUS", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE", "PLUTO",
    "MEAN_ASCENDING", "MEAN_DESCENDING", "TRUE_ASCENDING", "TRUE_DESCENDING",
  };
  for (const auto& body : snapshot.bodies) {
    if (not first) {
      std::cout << ",\n";
    }
    first = false;
    std::cout << "    {\"target\": \"" << TARGET_NAMES.at(static_cast<std::size_t>(body.target))
      << "\", \"longitude_deg\": " << body.longitude.deg()
      << ", \"latitude_deg\": " << body.latitude.deg() << ", \"distance_au\": ";
    if (body.distance) {
      std::cout << body.distance->au();
    } else {
      std::cout << "null";
    }
    std::cout << ", \"longitude_rate_deg_per_tt_day\": " << body.longitude_rate.deg_per_tt_day << '}';
  }
  std::cout << "\n  ],\n  \"houses\": {\"ascendant_deg\": " << snapshot.houses.ascendant.deg()
    << ", \"midheaven_deg\": " << snapshot.houses.midheaven.deg()
    << ", \"descendant_deg\": " << snapshot.houses.descendant.deg()
    << ", \"imum_coeli_deg\": " << snapshot.houses.imum_coeli.deg() << ", \"cusps_deg\": [";
  first = true;
  for (const auto& cusp : snapshot.houses.cusps) {
    if (not first) {
      std::cout << ", ";
    }
    first = false;
    std::cout << cusp.deg();
  }
  std::cout << "]}\n}\n";
}
