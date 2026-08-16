/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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

const binding = (cName, result, readsLastError = false) => Object.freeze({
  cName,
  wasmName: `_${cName}`,
  result,
  readsLastError,
});

// Internal metadata for the complete celestial.h surface. Package entry points consume
// this table; it is not itself part of the JavaScript package's public exports.
export const BINDINGS = Object.freeze([
  binding("set_log_verbosity", "bool"),
  binding("last_error", "borrowed-string"),
  binding("ut1_to_jd", "sret:JulianDay", true),
  binding("ut1_to_jde", "sret:JulianDay", true),
  binding("jde_to_ut1", "sret:UT1Time", true),
  binding("sun_apparent_geocentric_coord", "sret:SunCoordinate"),
  binding("moon_apparent_geocentric_coord", "sret:MoonCoordinate"),
  binding("moon_illumination", "sret:MoonIllumination", true),
  binding("moon_position_angle", "sret:MoonPositionAngle", true),
  binding("moon_phase_moments", "count-fill", true),
  binding("solar_lon_root_discriminant", "sret:Discriminant"),
  binding("solar_lon_roots", "companion-fill"),
  binding("new_moons_after_jde", "requested-fill"),
  binding("new_moons_in_year", "count-fill"),
  binding("equation_of_time", "sret:EquationOfTime"),
  binding("apparent_solar_time", "sret:ApparentSolarTime"),
  binding("local_apparent_sidereal_time", "sret:SiderealTime", true),
  binding("query_jieqi_moment", "sret:JieqiMomentQuery"),
  binding("get_jieqi_name", "caller-string"),
  binding("get_supported_lunar_year_range", "sret:SupportedLunarYearRange"),
  binding("get_lunar_year_info", "sret:LunarYearInfo"),
  binding("gregorian_to_lunar", "sret:LunarDate"),
  binding("lunar_to_gregorian", "sret:GregorianDate"),
  binding("delta_t_algo1", "sret:DeltaT"),
  binding("delta_t_algo2", "sret:DeltaT"),
  binding("delta_t_algo3", "sret:DeltaT"),
  binding("delta_t_algo4", "sret:DeltaT"),
  binding("delta_t_algo5", "sret:DeltaT"),
  binding("delta_t", "sret:DeltaT"),
]);
