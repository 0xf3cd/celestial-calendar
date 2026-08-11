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

/*
 * #67: compile `celestial.h` as pure C and pin the ABI — field offsets and struct sizes
 * are part of the published contract, so any layout drift fails the build here.
 * The absolute offsets below assume 64-bit natural alignment (the CI matrix is all 64-bit).
 */

#include "celestial.h"

#include <stddef.h>

_Static_assert(offsetof(JulianDay, value) == 8 && sizeof(JulianDay) == 16, "JulianDay layout drifted");

_Static_assert(offsetof(UT1Time, year) == 4 && offsetof(UT1Time, month) == 8 &&
               offsetof(UT1Time, day) == 12 && offsetof(UT1Time, fraction) == 16 &&
               sizeof(UT1Time) == 24, "UT1Time layout drifted");

_Static_assert(offsetof(SunCoordinate, lon) == 8 && offsetof(SunCoordinate, lat) == 16 &&
               offsetof(SunCoordinate, r) == 24 && sizeof(SunCoordinate) == 32,
               "SunCoordinate layout drifted");

_Static_assert(offsetof(MoonCoordinate, lon) == 8 && offsetof(MoonCoordinate, lat) == 16 &&
               offsetof(MoonCoordinate, r) == 24 && sizeof(MoonCoordinate) == 32,
               "MoonCoordinate layout drifted");

_Static_assert(offsetof(MoonIllumination, illumination) == 8 && offsetof(MoonIllumination, elongation_deg) == 16 &&
               sizeof(MoonIllumination) == 24, "MoonIllumination layout drifted");

_Static_assert(offsetof(Discriminant, count) == 4 && sizeof(Discriminant) == 8, "Discriminant layout drifted");

_Static_assert(offsetof(JieqiMomentQuery, jq_idx) == 1 && offsetof(JieqiMomentQuery, y) == 4 &&
               offsetof(JieqiMomentQuery, m) == 8 && offsetof(JieqiMomentQuery, d) == 12 &&
               offsetof(JieqiMomentQuery, frac) == 16 && sizeof(JieqiMomentQuery) == 24,
               "JieqiMomentQuery layout drifted");

_Static_assert(offsetof(SupportedLunarYearRange, start) == 4 && offsetof(SupportedLunarYearRange, end) == 8 &&
               sizeof(SupportedLunarYearRange) == 12, "SupportedLunarYearRange layout drifted");

_Static_assert(offsetof(LunarYearInfo, year) == 4 && offsetof(LunarYearInfo, month) == 8 &&
               offsetof(LunarYearInfo, day) == 9 && offsetof(LunarYearInfo, leap_month) == 10 &&
               offsetof(LunarYearInfo, month_len) == 12 && sizeof(LunarYearInfo) == 16,
               "LunarYearInfo layout drifted");

_Static_assert(offsetof(DeltaT, value) == 8 && sizeof(DeltaT) == 16, "DeltaT layout drifted");

_Static_assert(offsetof(EquationOfTime, value) == 8 && sizeof(EquationOfTime) == 16,
               "EquationOfTime layout drifted");

_Static_assert(offsetof(SiderealTime, value) == 8 && sizeof(SiderealTime) == 16,
               "SiderealTime layout drifted");

_Static_assert(offsetof(ApparentSolarTime, year) == 4 && offsetof(ApparentSolarTime, month) == 8 &&
               offsetof(ApparentSolarTime, day) == 12 && offsetof(ApparentSolarTime, fraction) == 16 &&
               sizeof(ApparentSolarTime) == 24, "ApparentSolarTime layout drifted");

_Static_assert(offsetof(LunarDate, year) == 4 && offsetof(LunarDate, month) == 8 &&
               offsetof(LunarDate, is_leap) == 9 && offsetof(LunarDate, day) == 10 &&
               sizeof(LunarDate) == 12, "LunarDate layout drifted");

_Static_assert(offsetof(GregorianDate, year) == 4 && offsetof(GregorianDate, month) == 8 &&
               offsetof(GregorianDate, day) == 9 && sizeof(GregorianDate) == 12,
               "GregorianDate layout drifted");

