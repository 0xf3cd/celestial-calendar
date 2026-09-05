/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "datetime.hpp"
#include "jieqi.hpp"
#include "julian_day.hpp"
#include "util.hpp"
#include "ymd.hpp"

// Retained material boundaries (V04/V10): the HKO and JPL Horizons DE441 output blocks below remain
// under their respective source terms and outside the project MIT grant.
// Independent golden datasets for jieqi instants (#94, #68), collected 2026-07-27 by
// `statistics/sun_jieqi_golden_crawler.py`. Two sources, cross-validated against each
// other before either was pinned (#94: worst |HKO − DE441| = 0.51 min over all 168 shared
// values, inside HKO's ±0.5 min rounding):
// - DE441 crossings: instants where the DE441 apparent solar longitude (Horizons quantity
//   31, IAU76/80 true-ecliptic-of-date) crosses k·15°, solved by the crawler's batched
//   fixed-slope Newton iteration to < 0.05 s. Pure TT on both sides of the comparison —
//   no ΔT model anywhere in this axis. Sampling: all 24 for 2026; the equinox/solstice
//   quartet for 1900/1950/2000/2050/2100 (core), 410/1000/1600/3000/5001 (extended — 410 is
//   the lunar-algo2 lower bound; 5001 is far-future coverage, not the algo2 ceiling, #139)
//   and 6771/6772/9420 (JD 2^22 straddle, #76 guard).
//   Row semantics: the crossing CONTAINED in the calendar year, like `jieqi_jde` (the
//   crawler pins containment on proleptic-Gregorian TT civil days while the C++ window is
//   UT1-based — a ΔT-scale difference with days of margin for every jieqi). The crawler's
//   first run mapped the lon ≥ 285° block to the wrong cycle and the HKO cross-validation
//   flagged all 35 rows at exactly one year — the containment gate now makes that
//   structural. Pre-1582 date comments are Julian.
// - HKO almanac: hko.gov.hk `24SolarTerms_{Y}.xml` (years 2022–2028, all that exist),
//   minute precision, HKT = UTC+8, computed by HMNAO/USNO — an independently published
//   product and pipeline, though modern almanac ephemerides derive from the same JPL DE
//   family (a chain cross-check, not an independent dynamical theory). This axis runs
//   through `jieqi_ut1_moment`, i.e. it exercises the FULL production chain including the
//   ΔT model (algo5), exactly what the bazi consumer sees. UT1 ≈ UTC adds ≤ 0.9 s.
// This file supersedes jieqi_test.cpp's retired UT1Moment test (wolfram/bmcx/weather.gov
// values of mixed, undocumented time scales; split ymd+fraction assertion — #68), using a
// single continuous JD quantity per #68's acceptance criteria.
// Measured worst residuals are recorded above each tolerance. Mutation detection
// 2026-07-27, 4/4 red (shared with sun_horizons_golden_test.cpp): aberration sign flip
// (sun λ + both jieqi axes); nutation dropped (same); ΔT bypass in jieqi_ut1_moment
// (HKO axis ONLY — demonstrating the two axes' separation); VSOP radius ×1.00001
// transcription slip (sun r columns, all bands).

namespace calendar::jieqi::test {

namespace {

// One DE441 crossing row: calendar year, target apparent solar longitude (deg), JDE (TT).
struct CrossingRow {
  int32_t year;
  double lon_deg;
  double jde_tt;
};

// One HKO almanac row: calendar year, entry index in calendar order (0 = 小寒, 285°),
// month/day/hour/minute in HKT (UTC+8).
struct HkoRow {
  int32_t year;
  int32_t entry;
  uint32_t month;
  uint32_t day;
  uint32_t hour;
  uint32_t minute;
};

/** @brief Inverse of `JIEQI_SOLAR_LONGITUDE` (24 entries, exact doubles). */
auto jieqi_from_lon(const double lon_deg) -> Jieqi {
  for (const auto jq : JIEQI_LIST) {
    if (longitude_of(jq) == lon_deg) {
      return jq;
    }
  }
  throw std::invalid_argument { "no jieqi has solar longitude " + std::to_string(lon_deg) };
}

/** @brief HKO calendar-order entry index (0 = 小寒) → enum. */
auto jieqi_from_entry(const int32_t entry) -> Jieqi {
  if (entry < 0 or std::cmp_greater_equal(entry, JIEQI_COUNT)) {
    throw std::out_of_range { "HKO entry index must be in [0, 24)" };
  }
  return from_index(static_cast<uint8_t>((to_index(Jieqi::小寒) + entry) % JIEQI_COUNT));
}

// NOLINTBEGIN(modernize-use-designated-initializers)
const std::vector<CrossingRow> DE441_ROWS {
  {  2026, 285.0,  2461045.850228372 },  // 2026-Jan-05 08:24:19.731 TT
  {  2026, 300.0,  2461060.573684652 },  // 2026-Jan-20 01:46:06.354 TT
  {  2026, 315.0,  2461075.335627717 },  // 2026-Feb-03 20:03:18.235 TT
  {  2026, 330.0,  2461090.161872587 },  // 2026-Feb-18 15:53:05.792 TT
  {  2026, 345.0,  2461105.083442423 },  // 2026-Mar-05 14:00:09.425 TT
  {  2026,   0.0,  2461120.116062020 },  // 2026-Mar-20 14:47:07.759 TT
  {  2026,  15.0,  2461135.278582136 },  // 2026-Apr-04 18:41:09.497 TT
  {  2026,  30.0,  2461150.569639403 },  // 2026-Apr-20 01:40:16.844 TT
  {  2026,  45.0,  2461165.992986076 },  // 2026-May-05 11:49:53.997 TT
  {  2026,  60.0,  2461181.526327162 },  // 2026-May-21 00:37:54.667 TT
  {  2026,  75.0,  2461197.159400877 },  // 2026-Jun-05 15:49:32.236 TT
  {  2026,  90.0,  2461212.851165462 },  // 2026-Jun-21 08:25:40.696 TT
  {  2026, 105.0,  2461228.582033893 },  // 2026-Jul-07 01:58:07.728 TT
  {  2026, 120.0,  2461244.301569163 },  // 2026-Jul-22 19:14:15.576 TT
  {  2026, 135.0,  2461259.988829602 },  // 2026-Aug-07 11:43:54.878 TT
  {  2026, 150.0,  2461275.597208073 },  // 2026-Aug-23 02:19:58.777 TT
  {  2026, 165.0,  2461291.112818985 },  // 2026-Sep-07 14:42:27.560 TT
  {  2026, 180.0,  2461306.504438590 },  // 2026-Sep-23 00:06:23.494 TT
  {  2026, 195.0,  2461321.771155305 },  // 2026-Oct-08 06:30:27.818 TT
  {  2026, 210.0,  2461336.902159685 },  // 2026-Oct-23 09:39:06.597 TT
  {  2026, 225.0,  2461351.911975487 },  // 2026-Nov-07 09:53:14.682 TT
  {  2026, 240.0,  2461366.808691201 },  // 2026-Nov-22 07:24:30.920 TT
  {  2026, 255.0,  2461381.620623141 },  // 2026-Dec-07 02:53:41.839 TT
  {  2026, 270.0,  2461396.369034288 },  // 2026-Dec-21 20:51:24.562 TT
  {  1900,   0.0,  2415099.568796398 },  // 1900-Mar-21 01:39:04.009 TT
  {  1900,  90.0,  2415192.402665390 },  // 1900-Jun-21 21:39:50.290 TT
  {  1900, 180.0,  2415286.014056498 },  // 1900-Sep-23 12:20:14.481 TT
  {  1900, 270.0,  2415375.778909320 },  // 1900-Dec-22 06:41:37.765 TT
  {  1950,   0.0,  2433361.691410038 },  // 1950-Mar-21 04:35:37.827 TT
  {  1950,  90.0,  2433454.483694582 },  // 1950-Jun-21 23:36:31.212 TT
  {  1950, 180.0,  2433548.113921230 },  // 1950-Sep-23 14:44:02.794 TT
  {  1950, 270.0,  2433637.926264365 },  // 1950-Dec-22 10:13:49.241 TT
  {  2000,   0.0,  2451623.816894684 },  // 2000-Mar-20 07:36:19.701 TT
  {  2000,  90.0,  2451716.575549247 },  // 2000-Jun-21 01:48:47.455 TT
  {  2000, 180.0,  2451810.228249046 },  // 2000-Sep-22 17:28:40.718 TT
  {  2000, 270.0,  2451900.068411969 },  // 2000-Dec-21 13:38:30.794 TT
  {  2050,   0.0,  2469885.931175829 },  // 2050-Mar-20 10:20:53.592 TT
  {  2050,  90.0,  2469978.648847175 },  // 2050-Jun-21 03:34:20.396 TT
  {  2050, 180.0,  2470072.312388782 },  // 2050-Sep-22 19:29:50.391 TT
  {  2050, 270.0,  2470162.194469132 },  // 2050-Dec-21 16:40:02.133 TT
  {  2100,   0.0,  2488148.046331224 },  // 2100-Mar-20 13:06:43.018 TT
  {  2100,  90.0,  2488240.732801653 },  // 2100-Jun-21 05:35:14.063 TT
  {  2100, 180.0,  2488334.419111444 },  // 2100-Sep-22 22:03:31.229 TT
  {  2100, 270.0,  2488424.329126024 },  // 2100-Dec-21 19:53:56.488 TT
  {   410,   0.0,  1870888.575615850 },  // 0410-Mar-20 01:48:53.209 TT
  {   410,  90.0,  1870982.344604605 },  // 0410-Jun-21 20:16:13.838 TT
  {   410, 180.0,  1871075.103461574 },  // 0410-Sep-22 14:28:59.080 TT
  {   410, 270.0,  1871163.974875666 },  // 0410-Dec-20 11:23:49.258 TT
  {  1000,   0.0,  2086381.485092369 },  // 1000-Mar-14 23:38:31.981 TT
  {  1000,  90.0,  2086474.933914073 },  // 1000-Jun-16 10:24:50.176 TT
  {  1000, 180.0,  2086568.082234075 },  // 1000-Sep-17 13:58:25.024 TT
  {  1000, 270.0,  2086657.264106066 },  // 1000-Dec-15 18:20:18.764 TT
  {  1600,   0.0,  2305526.863930452 },  // 1600-Mar-20 08:44:03.591 TT
  {  1600,  90.0,  2305619.910841567 },  // 1600-Jun-21 09:51:36.711 TT
  {  1600, 180.0,  2305713.384891056 },  // 1600-Sep-22 21:14:14.587 TT
  {  1600, 270.0,  2305802.950775349 },  // 1600-Dec-21 10:49:06.990 TT
  {  3000,   0.0,  2816866.228003458 },  // 3000-Mar-20 17:28:19.499 TT
  {  3000,  90.0,  2816958.204441832 },  // 3000-Jun-20 16:54:23.774 TT
  {  3000, 180.0,  2817052.117687357 },  // 3000-Sep-22 14:49:28.188 TT
  {  3000, 270.0,  2817142.720458746 },  // 3000-Dec-22 05:17:27.636 TT
  {  5001,   0.0,  3547716.494668497 },  // 5001-Mar-20 23:52:19.358 TT
  {  5001,  90.0,  3547806.929828515 },  // 5001-Jun-19 10:18:57.184 TT
  {  5001, 180.0,  3547900.620952301 },  // 5001-Sep-21 02:54:10.279 TT
  {  5001, 270.0,  3547992.775320746 },  // 5001-Dec-22 06:36:27.712 TT
  {  6771,   0.0,  4194195.692503474 },  // 6771-Mar-21 04:37:12.300 TT
  {  6771,  90.0,  4194285.161407845 },  // 6771-Jun-18 15:52:25.638 TT
  {  6771, 180.0,  4194377.958443175 },  // 6771-Sep-19 11:00:09.490 TT
  {  6771, 270.0,  4194471.115560341 },  // 6771-Dec-21 14:46:24.413 TT
  {  6772,   0.0,  4194560.929936860 },  // 6772-Mar-20 10:19:06.545 TT
  {  6772,  90.0,  4194650.399727046 },  // 6772-Jun-17 21:35:36.417 TT
  {  6772, 180.0,  4194743.199691923 },  // 6772-Sep-18 16:47:33.382 TT
  {  6772, 270.0,  4194836.354141500 },  // 6772-Dec-20 20:29:57.826 TT
  {  9420,   0.0,  5161722.433612062 },  // 9420-Mar-19 22:24:24.082 TT
  {  9420,  90.0,  5161811.648034234 },  // 9420-Jun-17 03:33:10.158 TT
  {  9420, 180.0,  5161902.647076671 },  // 9420-Sep-16 03:31:47.424 TT
  {  9420, 270.0,  5161996.085451838 },  // 9420-Dec-18 14:03:03.039 TT
};

// 2022–2028 × 24 = 168 rows.
const std::vector<HkoRow> HKO_ROWS {
  { 2022,  0,  1,  5, 17, 14 },
  { 2022,  1,  1, 20, 10, 39 },
  { 2022,  2,  2,  4,  4, 51 },
  { 2022,  3,  2, 19,  0, 43 },
  { 2022,  4,  3,  5, 22, 44 },
  { 2022,  5,  3, 20, 23, 33 },
  { 2022,  6,  4,  5,  3, 20 },
  { 2022,  7,  4, 20, 10, 24 },
  { 2022,  8,  5,  5, 20, 26 },
  { 2022,  9,  5, 21,  9, 23 },
  { 2022, 10,  6,  6,  0, 26 },
  { 2022, 11,  6, 21, 17, 14 },
  { 2022, 12,  7,  7, 10, 38 },
  { 2022, 13,  7, 23,  4,  7 },
  { 2022, 14,  8,  7, 20, 29 },
  { 2022, 15,  8, 23, 11, 16 },
  { 2022, 16,  9,  7, 23, 32 },
  { 2022, 17,  9, 23,  9,  4 },
  { 2022, 18, 10,  8, 15, 22 },
  { 2022, 19, 10, 23, 18, 36 },
  { 2022, 20, 11,  7, 18, 45 },
  { 2022, 21, 11, 22, 16, 20 },
  { 2022, 22, 12,  7, 11, 46 },
  { 2022, 23, 12, 22,  5, 48 },
  { 2023,  0,  1,  5, 23,  5 },
  { 2023,  1,  1, 20, 16, 30 },
  { 2023,  2,  2,  4, 10, 43 },
  { 2023,  3,  2, 19,  6, 34 },
  { 2023,  4,  3,  6,  4, 36 },
  { 2023,  5,  3, 21,  5, 24 },
  { 2023,  6,  4,  5,  9, 13 },
  { 2023,  7,  4, 20, 16, 14 },
  { 2023,  8,  5,  6,  2, 19 },
  { 2023,  9,  5, 21, 15,  9 },
  { 2023, 10,  6,  6,  6, 18 },
  { 2023, 11,  6, 21, 22, 58 },
  { 2023, 12,  7,  7, 16, 31 },
  { 2023, 13,  7, 23,  9, 50 },
  { 2023, 14,  8,  8,  2, 23 },
  { 2023, 15,  8, 23, 17,  1 },
  { 2023, 16,  9,  8,  5, 27 },
  { 2023, 17,  9, 23, 14, 50 },
  { 2023, 18, 10,  8, 21, 16 },
  { 2023, 19, 10, 24,  0, 21 },
  { 2023, 20, 11,  8,  0, 36 },
  { 2023, 21, 11, 22, 22,  3 },
  { 2023, 22, 12,  7, 17, 33 },
  { 2023, 23, 12, 22, 11, 27 },
  { 2024,  0,  1,  6,  4, 49 },
  { 2024,  1,  1, 20, 22,  7 },
  { 2024,  2,  2,  4, 16, 27 },
  { 2024,  3,  2, 19, 12, 13 },
  { 2024,  4,  3,  5, 10, 23 },
  { 2024,  5,  3, 20, 11,  6 },
  { 2024,  6,  4,  4, 15,  2 },
  { 2024,  7,  4, 19, 22,  0 },
  { 2024,  8,  5,  5,  8, 10 },
  { 2024,  9,  5, 20, 21,  0 },
  { 2024, 10,  6,  5, 12, 10 },
  { 2024, 11,  6, 21,  4, 51 },
  { 2024, 12,  7,  6, 22, 20 },
  { 2024, 13,  7, 22, 15, 44 },
  { 2024, 14,  8,  7,  8,  9 },
  { 2024, 15,  8, 22, 22, 55 },
  { 2024, 16,  9,  7, 11, 11 },
  { 2024, 17,  9, 22, 20, 44 },
  { 2024, 18, 10,  8,  3,  0 },
  { 2024, 19, 10, 23,  6, 15 },
  { 2024, 20, 11,  7,  6, 20 },
  { 2024, 21, 11, 22,  3, 56 },
  { 2024, 22, 12,  6, 23, 17 },
  { 2024, 23, 12, 21, 17, 21 },
  { 2025,  0,  1,  5, 10, 33 },
  { 2025,  1,  1, 20,  4,  0 },
  { 2025,  2,  2,  3, 22, 10 },
  { 2025,  3,  2, 18, 18,  7 },
  { 2025,  4,  3,  5, 16,  7 },
  { 2025,  5,  3, 20, 17,  1 },
  { 2025,  6,  4,  4, 20, 49 },
  { 2025,  7,  4, 20,  3, 56 },
  { 2025,  8,  5,  5, 13, 57 },
  { 2025,  9,  5, 21,  2, 55 },
  { 2025, 10,  6,  5, 17, 57 },
  { 2025, 11,  6, 21, 10, 42 },
  { 2025, 12,  7,  7,  4,  5 },
  { 2025, 13,  7, 22, 21, 29 },
  { 2025, 14,  8,  7, 13, 52 },
  { 2025, 15,  8, 23,  4, 34 },
  { 2025, 16,  9,  7, 16, 52 },
  { 2025, 17,  9, 23,  2, 19 },
  { 2025, 18, 10,  8,  8, 41 },
  { 2025, 19, 10, 23, 11, 51 },
  { 2025, 20, 11,  7, 12,  4 },
  { 2025, 21, 11, 22,  9, 36 },
  { 2025, 22, 12,  7,  5,  5 },
  { 2025, 23, 12, 21, 23,  3 },
  { 2026,  0,  1,  5, 16, 23 },
  { 2026,  1,  1, 20,  9, 45 },
  { 2026,  2,  2,  4,  4,  2 },
  { 2026,  3,  2, 18, 23, 52 },
  { 2026,  4,  3,  5, 21, 59 },
  { 2026,  5,  3, 20, 22, 46 },
  { 2026,  6,  4,  5,  2, 40 },
  { 2026,  7,  4, 20,  9, 39 },
  { 2026,  8,  5,  5, 19, 49 },
  { 2026,  9,  5, 21,  8, 37 },
  { 2026, 10,  6,  5, 23, 48 },
  { 2026, 11,  6, 21, 16, 25 },
  { 2026, 12,  7,  7,  9, 57 },
  { 2026, 13,  7, 23,  3, 13 },
  { 2026, 14,  8,  7, 19, 43 },
  { 2026, 15,  8, 23, 10, 19 },
  { 2026, 16,  9,  7, 22, 41 },
  { 2026, 17,  9, 23,  8,  5 },
  { 2026, 18, 10,  8, 14, 29 },
  { 2026, 19, 10, 23, 17, 38 },
  { 2026, 20, 11,  7, 17, 52 },
  { 2026, 21, 11, 22, 15, 23 },
  { 2026, 22, 12,  7, 10, 53 },
  { 2026, 23, 12, 22,  4, 50 },
  { 2027,  0,  1,  5, 22, 10 },
  { 2027,  1,  1, 20, 15, 30 },
  { 2027,  2,  2,  4,  9, 46 },
  { 2027,  3,  2, 19,  5, 33 },
  { 2027,  4,  3,  6,  3, 40 },
  { 2027,  5,  3, 21,  4, 25 },
  { 2027,  6,  4,  5,  8, 17 },
  { 2027,  7,  4, 20, 15, 18 },
  { 2027,  8,  5,  6,  1, 25 },
  { 2027,  9,  5, 21, 14, 18 },
  { 2027, 10,  6,  6,  5, 26 },
  { 2027, 11,  6, 21, 22, 11 },
  { 2027, 12,  7,  7, 15, 37 },
  { 2027, 13,  7, 23,  9,  5 },
  { 2027, 14,  8,  8,  1, 27 },
  { 2027, 15,  8, 23, 16, 14 },
  { 2027, 16,  9,  8,  4, 28 },
  { 2027, 17,  9, 23, 14,  2 },
  { 2027, 18, 10,  8, 20, 17 },
  { 2027, 19, 10, 23, 23, 33 },
  { 2027, 20, 11,  7, 23, 39 },
  { 2027, 21, 11, 22, 21, 16 },
  { 2027, 22, 12,  7, 16, 38 },
  { 2027, 23, 12, 22, 10, 42 },
  { 2028,  0,  1,  6,  3, 55 },
  { 2028,  1,  1, 20, 21, 22 },
  { 2028,  2,  2,  4, 15, 31 },
  { 2028,  3,  2, 19, 11, 26 },
  { 2028,  4,  3,  5,  9, 25 },
  { 2028,  5,  3, 20, 10, 17 },
  { 2028,  6,  4,  4, 14,  3 },
  { 2028,  7,  4, 19, 21,  9 },
  { 2028,  8,  5,  5,  7, 12 },
  { 2028,  9,  5, 20, 20, 10 },
  { 2028, 10,  6,  5, 11, 16 },
  { 2028, 11,  6, 21,  4,  2 },
  { 2028, 12,  7,  6, 21, 30 },
  { 2028, 13,  7, 22, 14, 54 },
  { 2028, 14,  8,  7,  7, 21 },
  { 2028, 15,  8, 22, 22,  1 },
  { 2028, 16,  9,  7, 10, 22 },
  { 2028, 17,  9, 22, 19, 45 },
  { 2028, 18, 10,  8,  2,  9 },
  { 2028, 19, 10, 23,  5, 13 },
  { 2028, 20, 11,  7,  5, 27 },
  { 2028, 21, 11, 22,  2, 54 },
  { 2028, 22, 12,  6, 22, 25 },
  { 2028, 23, 12, 21, 16, 20 },
};
// NOLINTEND(modernize-use-designated-initializers)

}  // namespace

TEST(JieqiGolden, De441Crossings) {
  // Measured worst residuals 2026-07-27: core 2.76 s, extended 10.9 s, far 1198.6 s —
  // coherent with the sun longitude residuals (0.105″/0.375″/49.9″) divided by the mean
  // motion 0.9856°/day. Core/extended pin ~3× margins; far pins ~1.5× as a gross-breakage
  // guard where the secular VSOP drift dominates.
  const auto tol_s = [](const int32_t year) {
    if (1900 <= year and year <= 2100) { return 10.0; }
    if (year >= 6000) { return 1800.0; }
    return 30.0;
  };
  double worst_core = 0.0;
  double worst_ext = 0.0;
  double worst_far = 0.0;
  for (const auto& row : DE441_ROWS) {
    const auto jq = jieqi_from_lon(row.lon_deg);
    const double ours = jieqi_jde(row.year, jq);
    const double diff_s = std::fabs(ours - row.jde_tt) * 86400.0;
    EXPECT_LE(diff_s, tol_s(row.year))
        << "year " << row.year << " lon " << row.lon_deg << ": ours " << ours
        << " vs golden " << row.jde_tt;
    auto& worst = [&]() -> double& {
      if (1900 <= row.year and row.year <= 2100) { return worst_core; }
      if (row.year >= 6000) { return worst_far; }
      return worst_ext;
    }();
    worst = std::max(worst, diff_s);
  }
  // Intentional pass-or-fail print (drift visibility; see moon_horizons_golden_test.cpp).
  std::cout << "jieqi DE441 measured worst: core " << worst_core << " s, extended "
            << worst_ext << " s, far " << worst_far << " s\n";
}

TEST(JieqiGolden, HkoAlmanac) {
  using namespace std::chrono;
  double worst_min = 0.0;
  for (const auto& row : HKO_ROWS) {
    const auto jq = jieqi_from_entry(row.entry);

    // Ours: JDE(TT) → UT1 civil moment (full production chain incl. ΔT) → continuous JD.
    const auto ours_ut1 = jieqi_ut1_moment(row.year, jq);
    const double ours_jd = astro::julian_day::ut1_to_jd(ours_ut1);

    // HKO: HKT wall clock → the same continuous JD scale (HKT = UTC+8; UT1 ≈ UTC ± 0.9 s).
    const calendar::Datetime hko_hkt {
      util::to_ymd(row.year, static_cast<int32_t>(row.month), static_cast<int32_t>(row.day)),
      hh_mm_ss<nanoseconds> { hours { row.hour } + minutes { row.minute } },
    };
    const double hko_jd = astro::julian_day::ut1_to_jd(hko_hkt) - (8.0 / 24.0);

    const double diff_min = std::fabs(ours_jd - hko_jd) * 1440.0;
    // Measured worst residual 2026-07-27: 0.525 min over all 168 values — essentially
    // HKO's own ±0.5 min rounding. Budget: rounding (±0.5 min) + UT1/UTC conflation
    // (±0.9 s) + full-chain error (seconds); a ΔT-class 69 s slip adds ~1.15 min → red.
    EXPECT_LE(diff_min, 1.0) << "year " << row.year << " entry " << row.entry << ": ours JD "
                             << ours_jd << " vs HKO JD " << hko_jd;
    worst_min = std::max(worst_min, diff_min);
  }
  std::cout << "jieqi HKO measured worst: " << worst_min << " min over " << HKO_ROWS.size()
            << " values\n";
}

}  // namespace calendar::jieqi::test
