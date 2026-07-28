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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "sun.hpp"

// Independent golden dataset for the Sun's apparent geocentric position (#94, #68), collected
// 2026-07-27 by `statistics/sun_jieqi_golden_crawler.py`:
// - Source: JPL Horizons API v1.2, ephemeris DE441, Sun (10) from the geocenter (500@399).
//   Epochs are supplied and echoed in TT. Quantity 31 = IAU76/80 true-ecliptic-of-date
//   apparent lon/lat — the same observable as `sun::geocentric_coord::apparent` (VSOP87D +
//   FK5 + nutation + aberration) at the sub-arcsecond level exercised here (Horizons
//   formulates light-time + stellar aberration + deflection separately; Meeus (25.11)
//   models the combined effect). The r column is the GEOMETRIC distance |r| in AU from a
//   separate VECTORS query (frame-free; the observer table's range is light-time aberrated,
//   ~250 km off geometric for the Sun; VECTORS times are TDB, |TDB−TT| < 2 ms → < 1e-9 AU).
// - Source validation (#94): Meeus Example 25.b (JD 2448908.5 TT) anchors the book chain:
//   book apparent λ 199.9060606° vs DE441 199.9059841° = 0.275″, the book's own truncation
//   scale. The jieqi golden (jieqi_golden_test.cpp) closes the loop end-to-end: crossings
//   derived from THIS query pipeline agree with the HKO almanac (HMNAO/USNO — independent
//   pipeline, same DE family) to 0.51 min worst over 168 values — validating both sources
//   before either was pinned. Pre-1582 Horizons date comments are in the Julian calendar.
// - Epoch bands mirror moon_horizons_golden_test.cpp, plus the lunar-algo2 boundary years
//   ~410 and ~5001 demanded by #94; JD 2^22 straddle guards the #76 cliff region.
// Measured worst residuals are recorded above each tolerance; mutation detection results
// are in the header of jieqi_golden_test.cpp (shared mutants exercise both files).

namespace astro::sun::test {

using astro::sun::geocentric_coord::apparent;

namespace {

// One golden row: JD(TT); apparent true-ecliptic-of-date longitude / latitude (deg);
// geometric distance (AU).
struct HorizonsSunRow {
  double jde;
  double lon_deg;
  double lat_deg;
  double r_au;
};

// Per-band tolerances (deg, deg, AU). Never loosen silently (#94).
struct Tolerance {
  double lon_deg;
  double lat_deg;
  double r_au;
};

/** @brief Angular difference |a − b| in degrees, wrapped to [0, 180]. Both inputs live in
 *  [0, 360) — normalized λ and the golden columns — so the +540 shift keeps fmod positive. */
auto wrapped_diff_deg(const double a, const double b) -> double {
  return std::fabs(std::fmod(a - b + 540.0, 360.0) - 180.0);
}

void check_band(const std::span<const HorizonsSunRow> rows, const Tolerance& tol,
                const std::string_view band) {
  double worst_lon = 0.0;
  double worst_lat = 0.0;
  double worst_r = 0.0;
  for (const auto& row : rows) {
    const auto coord = apparent(row.jde);
    const double d_lon = wrapped_diff_deg(coord.λ.deg(), row.lon_deg);
    const double d_lat = std::fabs(coord.β.deg() - row.lat_deg);
    const double d_r = std::fabs(coord.r.au() - row.r_au);
    EXPECT_LE(d_lon, tol.lon_deg) << band << " λ, jde " << row.jde << ": ours "
                                  << coord.λ.deg() << " vs golden " << row.lon_deg;
    EXPECT_LE(d_lat, tol.lat_deg) << band << " β, jde " << row.jde << ": ours "
                                  << coord.β.deg() << " vs golden " << row.lat_deg;
    EXPECT_LE(d_r, tol.r_au) << band << " r, jde " << row.jde << ": ours "
                             << coord.r.au() << " vs golden " << row.r_au;
    worst_lon = std::max(worst_lon, d_lon);
    worst_lat = std::max(worst_lat, d_lat);
    worst_r = std::max(worst_r, d_r);
  }
  // Intentional pass-or-fail print: keeps the measured residuals visible in test logs, so
  // drift against the recorded values is spottable without a local re-measurement run.
  std::cout << band << " measured worst residuals: dlon " << worst_lon * 3600.0 << "\", dlat "
            << worst_lat * 3600.0 << "\", dr " << worst_r << " AU\n";
}

// 1901–2094 (32 epochs stepped 2283.25 days) + the Meeus Example 25.b anchor (2448908.50).
const std::vector<HorizonsSunRow> SUN_CORE_ROWS {
  {  2415385.50,  279.9091090,   0.0000094,  0.9832022703 },  // 1901-Jan-01 00:00:00.000 TT, rdot -0.0201 km/s
  {  2417668.75,   12.3366840,   0.0001985,  0.9999906874 },  // 1907-Apr-03 06:00:00.000 TT, rdot +0.5089 km/s
  {  2419952.00,  100.9206247,   0.0000641,  1.0167713532 },  // 1913-Jul-03 12:00:00.000 TT, rdot +0.0054 km/s
  {  2422235.25,  189.4711622,   0.0001991,  1.0003319181 },  // 1919-Oct-03 18:00:00.000 TT, rdot -0.5085 km/s
  {  2424518.50,  281.8611965,  -0.0000351,  0.9832148669 },  // 1926-Jan-03 00:00:00.000 TT, rdot +0.0090 km/s
  {  2426801.75,   14.2568364,   0.0000664,  1.0004814820 },  // 1932-Apr-04 06:00:00.000 TT, rdot +0.5027 km/s
  {  2429085.00,  102.7944396,  -0.0000421,  1.0167253175 },  // 1938-Jul-05 12:00:00.000 TT, rdot -0.0195 km/s
  {  2431368.25,  191.3762636,  -0.0000736,  0.9998572162 },  // 1944-Oct-04 18:00:00.000 TT, rdot -0.4916 km/s
  {  2433651.50,  283.8309668,  -0.0002188,  0.9833118719 },  // 1951-Jan-05 00:00:00.000 TT, rdot +0.0214 km/s
  {  2435934.75,   16.1760696,  -0.0000845,  1.0009109094 },  // 1957-Apr-06 06:00:00.000 TT, rdot +0.4862 km/s
  {  2438218.00,  104.6570576,  -0.0000332,  1.0166997925 },  // 1963-Jul-07 12:00:00.000 TT, rdot -0.0179 km/s
  {  2440501.25,  193.3013541,   0.0001690,  0.9994460994 },  // 1969-Oct-06 18:00:00.000 TT, rdot -0.4890 km/s
  {  2442784.50,  285.8083478,   0.0002015,  0.9833333647 },  // 1976-Jan-07 00:00:00.000 TT, rdot +0.0188 km/s
  {  2445067.75,   18.0776369,   0.0000873,  1.0012812640 },  // 1982-Apr-08 06:00:00.000 TT, rdot +0.4941 km/s
  {  2447351.00,  106.5269942,   0.0000406,  1.0167020159 },  // 1988-Jul-08 12:00:00.000 TT, rdot -0.0218 km/s
  {  2448908.50,  199.9059841,   0.0002050,  0.9976085134 },  // 1992-Oct-13 00:00:00.000 TT, rdot -0.4906 km/s
  {  2449634.25,  195.2262838,   0.0000879,  0.9990338905 },  // 1994-Oct-08 18:00:00.000 TT, rdot -0.5063 km/s
  {  2451917.50,  287.7664881,  -0.0000549,  0.9833246717 },  // 2001-Jan-08 00:00:00.000 TT, rdot +0.0379 km/s
  {  2454200.75,   19.9947814,  -0.0001138,  1.0017319180 },  // 2007-Apr-10 06:00:00.000 TT, rdot +0.5051 km/s
  {  2456484.00,  108.3943539,  -0.0002019,  1.0166374199 },  // 2013-Jul-10 12:00:00.000 TT, rdot -0.0505 km/s
  {  2458767.25,  197.1346327,  -0.0002166,  0.9985811479 },  // 2019-Oct-10 18:00:00.000 TT, rdot -0.5039 km/s
  {  2461050.50,  289.7375267,  -0.0000519,  0.9834264208 },  // 2026-Jan-10 00:00:00.000 TT, rdot +0.0658 km/s
  {  2463333.75,   21.9122463,   0.0000267,  1.0022239006 },  // 2032-Apr-11 06:00:00.000 TT, rdot +0.4906 km/s
  {  2465617.00,  110.2618811,   0.0001397,  1.0165305886 },  // 2038-Jul-12 12:00:00.000 TT, rdot -0.0670 km/s
  {  2467900.25,  199.0642004,   0.0000963,  0.9981442988 },  // 2044-Oct-11 18:00:00.000 TT, rdot -0.4846 km/s
  {  2470183.50,  291.7084484,   0.0001986,  0.9834924446 },  // 2051-Jan-12 00:00:00.000 TT, rdot +0.0689 km/s
  {  2472466.75,   23.8110770,  -0.0000102,  1.0025847660 },  // 2057-Apr-13 06:00:00.000 TT, rdot +0.4791 km/s
  {  2474750.00,  112.1313134,   0.0000892,  1.0164939632 },  // 2063-Jul-14 12:00:00.000 TT, rdot -0.0629 km/s
  {  2477033.25,  200.9932765,  -0.0000912,  0.9977726037 },  // 2069-Oct-13 18:00:00.000 TT, rdot -0.4890 km/s
  {  2479316.50,  293.6719031,  -0.0001170,  0.9835378816 },  // 2076-Jan-14 00:00:00.000 TT, rdot +0.0706 km/s
  {  2481599.75,   25.7215226,  -0.0002385,  1.0029532221 },  // 2082-Apr-15 06:00:00.000 TT, rdot +0.4926 km/s
  {  2483883.00,  114.0000351,  -0.0000491,  1.0164639941 },  // 2088-Jul-15 12:00:00.000 TT, rdot -0.0756 km/s
  {  2486166.25,  202.9117822,  -0.0000328,  0.9973374702 },  // 2094-Oct-15 18:00:00.000 TT, rdot -0.5028 km/s
};

// ~410 / ~501 / ~999 / ~1599 / ~2500 / ~3000 / ~5001 CE (410 and 5001 are the lunar-algo2
// boundary years, #94).
const std::vector<HorizonsSunRow> SUN_EXTENDED_ROWS {
  {  1870800.50,  271.7952269,  -0.0000561,  0.9833447140 },  // 0409-Dec-22 00:00:00.000 TT, rdot +0.1442 km/s
  {  1904000.50,  233.9657282,  -0.0000150,  0.9840446824 },  // 0500-Nov-14 00:00:00.000 TT, rdot -0.2157 km/s
  {  2086000.50,  344.4423928,   0.0002143,  0.9963425932 },  // 0999-Feb-28 00:00:00.000 TT, rdot +0.4899 km/s
  {  2305000.50,  197.3806363,  -0.0000422,  0.9963853498 },  // 1598-Oct-11 00:00:00.000 TT, rdot -0.5012 km/s
  {  2634000.50,  117.0642436,   0.0000749,  1.0163743669 },  // 2499-Jul-19 00:00:00.000 TT, rdot -0.0572 km/s
  {  2817000.50,  130.4087188,   0.0000509,  1.0159827069 },  // 3000-Aug-02 00:00:00.000 TT, rdot -0.0969 km/s
  {  3547660.50,  303.1593554,   0.0001006,  0.9869292548 },  // 5001-Jan-24 00:00:00.000 TT, rdot -0.2291 km/s
};

// JD 2^22 = 4194304 straddle (~6771 CE, #76 cliff guard) and ~9420 CE.
const std::vector<HorizonsSunRow> SUN_FAR_ROWS {
  {  4194303.50,  108.0566192,   0.0000104,  1.0028098411 },  // 6771-Jul-07 00:00:00.000 TT, rdot +0.4094 km/s
  {  4194304.50,  109.0365470,  -0.0000267,  1.0030455660 },  // 6771-Jul-08 00:00:00.000 TT, rdot +0.4070 km/s
  {  5161700.50,  338.1310775,   0.0000780,  0.9964339347 },  // 9420-Feb-27 00:00:00.000 TT, rdot -0.3823 km/s
};

}  // namespace

TEST(SunHorizonsGolden, CoreBand) {
  // Measured worst residuals 2026-07-27: λ 0.105″, β 0.018″, r 1.39e-8 AU (~2 km). The λ
  // budget is VSOP87D truncation + the ch.22-truncated nutation (≤ ~0.5″ each); a 69 s
  // time-scale slip moves λ by ~2.8″ — far beyond the 0.36″ tolerance.
  check_band(SUN_CORE_ROWS, { .lon_deg = 1e-4, .lat_deg = 2e-5, .r_au = 5e-8 }, "core");
}

TEST(SunHorizonsGolden, ExtendedBand) {
  // Measured worst residuals 2026-07-27: λ 0.375″, β 0.160″, r 2.23e-7 AU — VSOP87D
  // degradation away from its fitted era, still sub-arcsecond at |T| ≤ 16 centuries.
  check_band(SUN_EXTENDED_ROWS, { .lon_deg = 2e-4, .lat_deg = 1e-4, .r_au = 5e-7 }, "extended");
}

TEST(SunHorizonsGolden, FarBand) {
  // Measured worst residuals 2026-07-27: λ 49.9″, β 0.35″, r 1.9e-6 AU at |T| ≈ 48–74
  // centuries — secular VSOP degradation dominates; gross-breakage guard only.
  check_band(SUN_FAR_ROWS, { .lon_deg = 0.025, .lat_deg = 3e-4, .r_au = 5e-6 }, "far");
}

}  // namespace astro::sun::test
