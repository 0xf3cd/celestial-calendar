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
      .r     = coord.r,
    };
  } catch (const std::exception& e) {
    lib::info("Error in sun_apparent_geocentric_position: {}", e.what());
    lib::debug("sun_apparent_geocentric_position: jde = {}", jde);

    return {};
  }
}

#pragma endregion

}
