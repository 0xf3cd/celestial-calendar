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

_Static_assert(offsetof(MoonPositionAngle, angle_deg) == 8 && sizeof(MoonPositionAngle) == 16,
               "MoonPositionAngle layout drifted");

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

_Static_assert(offsetof(ChartBodyV1, target_code) == 0 && offsetof(ChartBodyV1, present_fields) == 4 &&
               offsetof(ChartBodyV1, longitude_deg) == 8 && offsetof(ChartBodyV1, latitude_deg) == 16 &&
               offsetof(ChartBodyV1, distance_au) == 24 &&
               offsetof(ChartBodyV1, longitude_rate_deg_per_tt_day) == 32 &&
               sizeof(ChartBodyV1) == 40 && _Alignof(ChartBodyV1) == 8, "ChartBodyV1 layout drifted");

_Static_assert(offsetof(ChartHousesV1, ascendant_deg) == 0 && offsetof(ChartHousesV1, midheaven_deg) == 8 &&
               offsetof(ChartHousesV1, descendant_deg) == 16 && offsetof(ChartHousesV1, imum_coeli_deg) == 24 &&
               offsetof(ChartHousesV1, cusps_deg) == 32 && sizeof(ChartHousesV1) == 128 &&
               _Alignof(ChartHousesV1) == 8, "ChartHousesV1 layout drifted");

_Static_assert(offsetof(ChartSnapshotV1, valid) == 0 && offsetof(ChartSnapshotV1, jd_ut1) == 8 &&
               offsetof(ChartSnapshotV1, jde_tt) == 16 && offsetof(ChartSnapshotV1, bodies) == 24 &&
               offsetof(ChartSnapshotV1, houses) == 584 && sizeof(ChartSnapshotV1) == 712 &&
               _Alignof(ChartSnapshotV1) == 8, "ChartSnapshotV1 layout drifted");

_Static_assert((sizeof(((ChartSnapshotV1*)0)->bodies) / sizeof(ChartBodyV1) == 14 &&
               offsetof(ChartSnapshotV1, bodies[1]) - offsetof(ChartSnapshotV1, bodies[0]) == 40 &&
               offsetof(ChartSnapshotV1, bodies[13]) == 544) != 0, "ChartSnapshotV1 body array drifted");

_Static_assert((sizeof(((ChartHousesV1*)0)->cusps_deg) / sizeof(double) == 12 &&
               offsetof(ChartHousesV1, cusps_deg[1]) - offsetof(ChartHousesV1, cusps_deg[0]) == 8 &&
               offsetof(ChartHousesV1, cusps_deg[11]) == 120 &&
               offsetof(ChartSnapshotV1, houses.cusps_deg) == 616) != 0, "ChartHousesV1 cusp array drifted");

_Static_assert((_Generic(((ChartBodyV1*)0)->target_code, uint32_t: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->present_fields, uint32_t: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->longitude_deg, double: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->latitude_deg, double: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->distance_au, double: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->longitude_rate_deg_per_tt_day, double: 1, default: 0)) != 0,
               "ChartBodyV1 field types drifted");

_Static_assert((_Generic(((ChartHousesV1*)0)->ascendant_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->midheaven_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->descendant_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->imum_coeli_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->cusps_deg, double*: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->valid, bool: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->jd_ut1, double: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->jde_tt, double: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->bodies, ChartBodyV1*: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->houses, ChartHousesV1: 1, default: 0)) != 0,
               "ChartSnapshotV1 or ChartHousesV1 field types drifted");

_Static_assert(_Generic(&chart_snapshot_v1,
               ChartSnapshotV1 (*)(int32_t, uint32_t, uint32_t, double, uint32_t, double, double, uint32_t, uint32_t): 1,
               default: 0) != 0, "chart_snapshot_v1 signature drifted");

_Static_assert(CHART_CIVIL_SCALE_UTC == 0 && CHART_CIVIL_SCALE_UT1 == 1 &&
               CHART_HOUSE_SYSTEM_EQUAL == 0 && CHART_HOUSE_SYSTEM_WHOLE_SIGN == 1 &&
               CHART_HOUSE_SYSTEM_PLACIDUS == 2 && CHART_DELTA_T_MODEL_DEFAULT == 0 &&
               CHART_DELTA_T_MODEL_ALGO1 == 1 && CHART_DELTA_T_MODEL_ALGO2 == 2 &&
               CHART_DELTA_T_MODEL_ALGO3 == 3 && CHART_DELTA_T_MODEL_ALGO4 == 4 &&
               CHART_DELTA_T_MODEL_ALGO5 == 5, "Chart V1 selector codes drifted");

_Static_assert(CHART_TARGET_SUN == 0 && CHART_TARGET_MOON == 1 && CHART_TARGET_MERCURY == 2 &&
               CHART_TARGET_VENUS == 3 && CHART_TARGET_MARS == 4 && CHART_TARGET_JUPITER == 5 &&
               CHART_TARGET_SATURN == 6 && CHART_TARGET_URANUS == 7 && CHART_TARGET_NEPTUNE == 8 &&
               CHART_TARGET_PLUTO == 9 && CHART_TARGET_MEAN_ASCENDING == 10 &&
               CHART_TARGET_MEAN_DESCENDING == 11 && CHART_TARGET_TRUE_ASCENDING == 12 &&
               CHART_TARGET_TRUE_DESCENDING == 13 && CHART_PRESENT_LATITUDE == 1 &&
               CHART_PRESENT_DISTANCE == 2, "Chart V1 identity or presence codes drifted");
