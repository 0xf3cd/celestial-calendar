/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>

#include "lib.hpp"
#include "celestial.h"

#include "astro.hpp"
#include "util.hpp"
#include "datetime.hpp"


extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

#pragma region Julian Days

auto ut1_to_jd(const int32_t y, const uint32_t m, const uint32_t d, const double fraction) -> JulianDay {
  lib::clear_last_error();

  try {
    const auto ymd = util::to_ymd(y, m, d);
    const auto ut1_dt = calendar::Datetime(ymd, fraction);
    const auto jd = astro::julian_day::ut1_to_jd(ut1_dt);

    return {
      .valid = true,
      .value = jd,
    };
  } catch (const std::exception& e) {
    lib::set_last_error(e.what());
    lib::info("Error in ut1_jd: {}", e.what());
    lib::debug("ut1_to_jd: y = {}, m = {}, d = {}, fraction = {}", y, m, d, fraction);

    return {};
  } catch (...) {
    lib::set_last_error("Unknown error in ut1_to_jd.");
    return {};
  }
}


auto ut1_to_jde(const int32_t y, const uint32_t m, const uint32_t d, const double fraction) -> JulianDay {
  lib::clear_last_error();

  try {
    const auto ymd = util::to_ymd(y, m, d);
    const auto ut1_dt = calendar::Datetime(ymd, fraction);
    const auto jde = astro::julian_day::ut1_to_jde(ut1_dt);

    return {
      .valid = true,
      .value = jde,
    };
  } catch (const std::exception& e) {
    lib::set_last_error(e.what());
    lib::info("Error in ut1_jde: {}", e.what());
    lib::debug("ut1_to_jde: y = {}, m = {}, d = {}, fraction = {}", y, m, d, fraction);

    return {};
  } catch (...) {
    lib::set_last_error("Unknown error in ut1_to_jde.");
    return {};
  }
}


auto jde_to_ut1(const double jde) -> UT1Time {
  lib::clear_last_error();

  try {
    const auto ut1_dt = astro::julian_day::jde_to_ut1(jde);

    const auto [y, m, d] = util::from_ymd(ut1_dt.ymd);
    const double fraction = ut1_dt.fraction();

    return {
      .valid      = true,
      .year       = y,
      .month      = m,
      .day        = d,
      .fraction   = fraction,
    };
  } catch (const std::exception& e) {
    lib::set_last_error(e.what());
    lib::info("Error in jde_to_ut1: {}", e.what());
    lib::debug("jde_to_ut1: jde = {}", jde);

    return {};
  } catch (...) {
    lib::set_last_error("Unknown error in jde_to_ut1.");
    return {};
  }
}


#pragma endregion


#pragma region Solar Apparent Geocentric Position

auto sun_apparent_geocentric_coord(const double jde) -> SunCoordinate {
  try {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, whose value is {}", jde)
      };
    }

    const auto coord = astro::sun::geocentric_coord::apparent(jde);

    return {
      .valid = true,
      .lon   = coord.λ.deg(),
      .lat   = coord.β.deg(),
      .r     = coord.r.au(),
    };
  } catch (const std::exception& e) {
    lib::info("Error in sun_apparent_geocentric_position: {}", e.what());
    lib::debug("sun_apparent_geocentric_position: jde = {}", jde);

    return {};
  } catch (...) {
    return {};
  }
}

#pragma endregion


#pragma region Moon Apparent Geocentric Position

auto moon_apparent_geocentric_coord(const double jde) -> MoonCoordinate {
  try {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, whose value is {}", jde)
      };
    }

    const auto coord = astro::moon::geocentric_coord::apparent(jde);

    return {
      .valid = true,
      .lon   = coord.λ.deg(),
      .lat   = coord.β.deg(),
      .r     = coord.r.km(),
    };
  } catch (const std::exception& e) {
    lib::info("Error in moon_apparent_geocentric_position: {}", e.what());
    lib::debug("moon_apparent_geocentric_position: jde = {}", jde);

    return {};
  } catch (...) {
    return {};
  }
}

#pragma endregion


#pragma region Solar Longitude Roots

auto solar_lon_root_discriminant(const int32_t year, const double longitude) -> Discriminant {
  try {
    if (not std::isfinite(longitude)) {
      throw std::invalid_argument {
        std::format("Argument `longitude` is not finite, whose value is {}", longitude)
      };
    }

    return {
      .valid = true,
      .count = astro::sun::geocentric_coord::math::discriminant(year, longitude),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of root_discriminant");
    lib::debug("root_discriminant: year = {}, lon = {}, error = {}", year, longitude, e.what());
    return {};
  } catch (...) {
    return {};
  }
}


auto solar_lon_roots(
  const int32_t year, 
  const double longitude, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  using namespace astro::sun::geocentric_coord::math;

  if (slots == nullptr and slot_count > 0) {
    lib::info("Error in solar_lon_roots: `slots` is null, but `slot_count` is {}.", slot_count);
    return 0;
  }

  try {
    if (not std::isfinite(longitude)) {
      throw std::invalid_argument {
        std::format("Argument `longitude` is not finite, whose value is {}", longitude)
      };
    }

    auto roots = find_roots(year, longitude);

    // Some sanity check...
    const auto root_count = discriminant(year, longitude);
    if (roots.size() != root_count) [[unlikely]] {
      lib::info("Error in copy_roots: roots.size() is {}, but expected size is {}", roots.size(), root_count);
      lib::info("No root will be written to the slots.");

      return 0;
    }

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(cbegin(roots), cbegin(roots) + num_written, slots);

    return num_written;
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of copy_roots");
    lib::debug("copy_roots: year = {}, lon = {}, error = {}", year, longitude, e.what());

    return 0;
  } catch (...) {
    return 0;
  }
}

#pragma endregion


#pragma region Sun Moon Conjunction

auto new_moons_after_jde(
  const double jde, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  if (slots == nullptr and slot_count > 0) {
    lib::info("Error in new_moons_after_jde: `slots` is null, but `slot_count` is {}.", slot_count);
    return 0;
  }

  try {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, whose value is {}", jde)
      };
    }

    std::vector<double> roots;
    roots.reserve(slot_count);

    astro::moon_phase::new_moon::RootGenerator gen(jde);
    std::generate_n(std::back_inserter(roots), slot_count, [&] { return gen.next(); });

    std::copy(cbegin(roots), cend(roots), slots);
    return static_cast<uint32_t>(slot_count);
  } catch (const std::exception& e) {
    lib::info("Exception thrown during execution of sun_moon_conjunctions_after_jde");
    lib::debug("sun_moon_conjunctions_after_jde: jde = {}, error = {}", jde, e.what());

    return 0;
  } catch (...) {
    return 0;
  }
}


auto new_moons_in_year(
  const int32_t year, 
  uint32_t * const root_count,
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  if (root_count == nullptr) {
    lib::info("Error in new_moons_in_year: `root_count` is null.");
    return 0;
  }
  if (slots == nullptr and slot_count > 0) {
    lib::info("Error in new_moons_in_year: `slots` is null, but `slot_count` is {}.", slot_count);
    return 0;
  }

  try {
    const auto roots = astro::moon_phase::new_moon::moments(year);

    *root_count = static_cast<uint32_t>(roots.size());

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(cbegin(roots), cbegin(roots) + num_written, slots);

    return num_written;
  } catch (const std::exception& e) {
    lib::info("Exception thrown during execution of sun_moon_conjunctions_in_year");
    lib::debug("sun_moon_conjunctions_in_year: year = {}, error = {}", year, e.what());

    return 0;
  } catch (...) {
    return 0;
  }
}

#pragma endregion


#pragma region Solar Time

auto equation_of_time(const double jde) -> EquationOfTime {
  try {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, whose value is {}", jde)
      };
    }

    return {
      .valid = true,
      .value = astro::solar_time::equation_of_time(jde).deg(),
    };
  } catch (const std::exception& e) {
    lib::info("Error in equation_of_time: {}", e.what());
    lib::debug("equation_of_time: jde = {}", jde);

    return {};
  } catch (...) {
    return {};
  }
}


auto apparent_solar_time(
  const int32_t y,
  const uint32_t m,
  const uint32_t d,
  const double fraction,
  const double longitude
) -> ApparentSolarTime {
  try {
    const auto ymd = util::to_ymd(y, m, d);
    const auto utc_dt = calendar::Datetime(ymd, fraction);
    const auto lon = astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> { longitude };
    const auto apparent_dt = astro::solar_time::apparent(utc_dt, lon);

    const auto [ay, am, ad] = util::from_ymd(apparent_dt.ymd);

    return {
      .valid    = true,
      .year     = ay,
      .month    = am,
      .day      = ad,
      .fraction = apparent_dt.fraction(),
    };
  } catch (const std::exception& e) {
    lib::info("Error in apparent_solar_time: {}", e.what());
    lib::debug(
      "apparent_solar_time: y = {}, m = {}, d = {}, fraction = {}, longitude = {}",
      y, m, d, fraction, longitude
    );

    return {};
  } catch (...) {
    return {};
  }
}

#pragma endregion

}
