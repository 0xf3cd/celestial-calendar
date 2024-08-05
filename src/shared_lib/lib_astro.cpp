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

#include "astro.hpp"
#include "util.hpp"
#include "datetime.hpp"


extern "C" {

#pragma region Julian Days

struct JulianDay {
  bool   valid; // Indicates if the result is valid.
  double value; // The value. Either JD or JDE.
};

/**
 * @brief Convert UT1 datetime to Julian Day Number (JD).
 * @param y The year.
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `JulianDay` struct.
 * @details JD is based on UT1.
 */
auto ut1_to_jd(const int32_t y, const uint32_t m, const uint32_t d, const double fraction) -> JulianDay {
  try {
    const auto ymd = util::to_ymd(y, m, d);
    const auto ut1_dt = calendar::Datetime(ymd, fraction);
    const auto jd = astro::julian_day::ut1_to_jd(ut1_dt);

    return {
      .valid = true,
      .value = jd,
    };
  } catch (const std::exception& e) {
    lib::info("Error in ut1_jd: {}", e.what());
    lib::debug("ut1_to_jd: y = {}, m = {}, d = {}, fraction = {}", y, m, d, fraction);

    return {};
  }
}


/**
 * @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
 * @param y The year.
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `JulianDay` struct.
 * @details JDE is based on TT.
 */
auto ut1_to_jde(const int32_t y, const uint32_t m, const uint32_t d, const double fraction) -> JulianDay {
  try {
    const auto ymd = util::to_ymd(y, m, d);
    const auto ut1_dt = calendar::Datetime(ymd, fraction);
    const auto jde = astro::julian_day::ut1_to_jde(ut1_dt);

    return {
      .valid = true,
      .value = jde,
    };
  } catch (const std::exception& e) {
    lib::info("Error in ut1_jde: {}", e.what());
    lib::debug("ut1_to_jde: y = {}, m = {}, d = {}, fraction = {}", y, m, d, fraction);

    return {};
  }
}


struct UT1Time {
  bool     valid;    // Indicates if the result is valid.
  int32_t  year;     // The year.
  uint32_t month;    // The month.
  uint32_t day;      // The day.
  double   fraction; // The fraction of the day. Must be in the range [0.0, 1.0).
};


/**
 * @brief Convert Julian Ephemeris Day Number (JDE) to UT1 datetime.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `UT1Time` struct.
 */
auto jde_to_ut1(const double jde) -> UT1Time {
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
    lib::info("Error in jde_to_ut1: {}", e.what());
    lib::debug("jde_to_ut1: jde = {}", jde);

    return {};
  }
}


#pragma endregion


#pragma region Solar Apparent Geocentric Position

struct SunCoordinate {
  bool valid; // Indicates if the result is valid.
  double lon; // The longitude. In degrees.
  double lat; // The latitude. In degrees.
  double   r; // The radius. In AU.
};

/**
 * @brief Calculate the apparent geocentric position of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `SunCoordinate` struct.
 */
auto sun_apparent_geocentric_coord(const double jde) -> SunCoordinate {
  try {
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
  }
}

#pragma endregion


#pragma region Moon Apparent Geocentric Position

struct MoonCoordinate {
  bool valid; // Indicates if the result is valid.
  double lon; // The longitude. In degrees.
  double lat; // The latitude. In degrees.
  double   r; // The radius. In KM.
};


/**
 * @brief Calculate the apparent geocentric position of the Moon.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `MoonCoordinate` struct.
 */
auto moon_apparent_geocentric_coord(const double jde) -> MoonCoordinate {
  try {
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
  }
}

#pragma endregion


#pragma region Sun Position

struct Discriminant {
  bool     valid; // Indicates if the result is valid.
  uint32_t count; // The count of the roots, which is 0, 1, or 2.
};

/**
 * @brief Calculate the JDE discriminant, which is the count of the roots for the given `year` and `lon`.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @returns The count of the roots, which is 0, 1, or 2.
 *          0 indicates that Sun won't reach the given geocentric longitude in the given year.
 *          1 indicates that Sun will reach the given geocentric longitude once in the given year.
 *          2 indicates that Sun will reach the given geocentric longitude twice in the given year.
 */
auto solar_lon_root_discriminant(const int32_t year, const double longitude) -> Discriminant {
  try {
    return {
      .valid = true,
      .count = astro::sun::geocentric_coord::math::discriminant(year, longitude),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of root_discriminant");
    lib::debug("root_discriminant: year = {}, lon = {}, error = {}", year, longitude, e.what());
    return {};
  }
}


/**
 * @brief Find the JDE(s) at which the Sun reaches the given `longitude` in the given `year`.
 *        The result is written to the provided slots. It's caller's responsibility to allocate and free the slots.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @param slots The slots. It's caller's responsibility to allocate and free the slots.
 * @param slot_count The count of slots.
 * @return How many slots are written.
 */
auto solar_lon_roots(
  const int32_t year, 
  const double longitude, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  using namespace astro::sun::geocentric_coord::math;

  try {
    auto roots = find_roots(year, longitude);

    // Some sanity check...
    const auto root_count = discriminant(year, longitude);
    if (roots.size() != root_count) [[unlikely]] {
      lib::info("Error in copy_roots: roots.size() is {}, but expected size is {}", roots.size(), root_count);
      lib::info("No root will be written to the slots.");

      return 0;
    }

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(roots.begin(), roots.begin() + num_written, slots);

    return num_written;
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of copy_roots");
    lib::debug("copy_roots: year = {}, lon = {}, error = {}", year, longitude, e.what());

    return 0;
  }
}

#pragma endregion


#pragma region Sun Moon Conjunction

/**
 * @brief Find the JDE(s) at which the Sun and Moon are at the same logitude.
 *        The function finds the conjunction moments after `jde`.
 *        The result is written to the provided slots. It's caller's responsibility to allocate and free the slots.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param slots The slots. It's caller's responsibility to allocate and free the slots.
 * @param slot_count The count of slots.
 * @return How many slots are written.
 */
auto sun_moon_conjunctions_after_jde(
  const double jde, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  try {
    std::vector<double> roots;
    roots.reserve(slot_count);

    astro::syzygy::conjunction::sun_moon::RootGenerator gen(jde);
    std::generate_n(std::back_inserter(roots), slot_count, [&] { return gen.next(); });

    std::copy(roots.begin(), roots.end(), slots);
    return static_cast<uint32_t>(slot_count);
  } catch (const std::exception& e) {
    lib::info("Exception thrown during execution of sun_moon_conjunctions_after_jde");
    lib::debug("sun_moon_conjunctions_after_jde: jde = {}, error = {}", jde, e.what());

    return 0;
  }
}


/**
 * @brief Find the JDE(s) at which the Sun and Moon are at the same logitude.
 *        The function finds the conjunction moments in the given year.
 *        The result is written to the provided slots. It's caller's responsibility to allocate and free the slots.
 * @param year The Gregorian year.
 * @param root_count The pointer to uint32_t where the count of the roots will be written.
 * @param slots The slots. It's caller's responsibility to allocate and free the slots.
 * @param slot_count The count of slots.
 * @return How many slots are written.
 */
auto sun_moon_conjunctions_in_year(
  const int32_t year, 
  uint32_t * const root_count,
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  try {
    const auto roots = astro::syzygy::conjunction::sun_moon::moments(year);

    *root_count = static_cast<uint32_t>(roots.size());

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(roots.begin(), roots.begin() + num_written, slots);

    return num_written;
  } catch (const std::exception& e) {
    lib::info("Exception thrown during execution of sun_moon_conjunctions_in_year");
    lib::debug("sun_moon_conjunctions_in_year: year = {}, error = {}", year, e.what());

    return 0;
  }
}

#pragma endregion

}
