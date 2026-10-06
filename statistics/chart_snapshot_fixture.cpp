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
#include <iomanip>
#include <iostream>
#include <string_view>

#include "chart.hpp"

int main() {
  const calendar::Datetime civil_dt { util::to_ymd(2026, 1, 1), 0.5 };
  const astro::GeoLocation location {
    .latitude = astro::toolbox::AngleDeg { 51.5 },
    .longitude = astro::toolbox::AngleDeg { 0.0 },
  };
  const auto snapshot = astro::chart::calculate(
    civil_dt, astro::chart::Scale::UTC, location, astro::house::System::PLACIDUS
  );
  std::cout << std::setprecision(17)
    << "{\n  \"role\": \"Facade replay example, not an independent physical-accuracy oracle\",\n"
    << "  \"input\": {\"datetime\": \"2026-01-01T12:00:00\", \"scale\": \"UTC\", "
    << "\"latitude_north_deg\": 51.5, \"longitude_east_deg\": 0, \"system\": \"PLACIDUS\", \"delta_t_model\": \"ALGO5\"},\n"
    << "  \"times\": {\"jd_ut1\": " << snapshot.times.jd_ut1
    << ", \"jde_tt\": " << snapshot.times.jde_tt << "},\n  \"bodies\": [\n";
  bool first = true;
  constexpr std::array<std::string_view, 14> names {
    "SUN", "MOON", "MERCURY", "VENUS", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE", "PLUTO",
    "MEAN_ASCENDING", "MEAN_DESCENDING", "TRUE_ASCENDING", "TRUE_DESCENDING",
  };
  std::size_t index = 0;
  for (const auto& body : snapshot.bodies) {
    if (not first) {
      std::cout << ",\n";
    }
    first = false;
    std::cout << "    {\"target\": \"" << names.at(index++)
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
