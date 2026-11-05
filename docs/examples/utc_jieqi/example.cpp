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

#include <format>
#include <iostream>
#include <string_view>

#include "datetime.hpp"
#include "jieqi.hpp"
#include "julian_day.hpp"
#include "ymd.hpp"

int main() {
  const calendar::Datetime cutoff_utc {
    util::to_ymd(2026, 10, 28), (19 * 3600 + 35 * 60 + 37) / 86400.0
  };
  const double cutoff_jde_tt = astro::julian_day::utc_to_jde(cutoff_utc);
  calendar::jieqi::JieqiGenerator events { cutoff_jde_tt };
  const auto [jieqi, jde_tt] = events.next();
  const auto utc = astro::julian_day::jde_to_utc(jde_tt);
  const auto east_eight = calendar::add_seconds(utc, 8 * 3600.0);

  const auto print_civil = [](const std::string_view label, const calendar::Datetime& civil) {
    std::cout << std::format(
      "{}: {:04}-{:02}-{:02} {:02}:{:02}\n",
      label,
      civil.year(),
      civil.month(),
      civil.day(),
      civil.time_of_day.hours().count(),
      civil.time_of_day.minutes().count()
    );
  };
  std::cout << calendar::jieqi::name_of(jieqi) << '\n';
  print_civil("UTC", utc);
  print_civil("UTC+8", east_eight);
}
