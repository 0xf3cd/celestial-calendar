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

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>

#include "ymd.hpp"
#include "toolbox.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"
#include "solar_time.hpp"
#include "sunrise_sunset.hpp"

namespace astro::solar_time::test {

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;

namespace {

struct EotRow {
  double jde;   // TT
  double e_deg; // Reference equation of time, degrees of hour angle (×4 for minutes of time).
};

// pymeeus `Sun.equation_of_time` internals (full-precision degrees; the public API truncates
// to whole seconds), same Meeus (28.1) formula family. 60 uniform points over 1900–2100 plus
// 18 points within ±2 days of March equinoxes (where α and L0 straddle the 0°/360° seam and
// an unwrapped E would come out ±357°). Seed 42, generated 2026-07-29. Measured worst
// residual 3.0e-6° (0.0007 s of time) — tolerance 1e-5° ≈ 3× that.
constexpr std::array<EotRow, 78> PYMEEUS_ROWS {{
  { 2415495.227896,  +0.261331808 },
  { 2416847.510658,  -0.943318947 },
  { 2416958.926049,  +0.407016838 },
  { 2417197.157083,  +0.974282776 },
  { 2418367.925402,  -3.105212924 },
  { 2420849.224122,  +1.024880664 },
  { 2421371.294785,  +0.883670896 },
  { 2421795.491113,  -1.538837203 },
  { 2422085.534611,  +0.866191972 },
  { 2422398.553417,  -2.300894491 },
  { 2422404.627799,  -1.859196660 },
  { 2422404.959999,  -1.834359004 },
  { 2422406.358093,  -1.729267813 },
  { 2426378.121982,  -3.523935312 },
  { 2426956.888767,  +0.407140200 },
  { 2427522.007112,  -1.553778104 },
  { 2429545.391545,  +3.084230130 },
  { 2430324.779091,  +3.192724571 },
  { 2430991.785421,  +1.912132845 },
  { 2431123.466999,  -3.445763378 },
  { 2431325.821211,  -0.649347648 },
  { 2431668.241138,  -1.560706332 },
  { 2432025.641458,  -1.582988297 },
  { 2433362.138457,  -1.836583233 },
  { 2433362.504000,  -1.809281686 },
  { 2433362.771408,  -1.789268416 },
  { 2434522.962923,  +0.805566902 },
  { 2435111.116678,  -1.090549009 },
  { 2435326.193734,  -1.461131430 },
  { 2436160.001353,  +3.752619126 },
  { 2439608.394926,  +0.594228559 },
  { 2439875.459981,  -2.674901743 },
  { 2441671.125842,  +0.674310593 },
  { 2442061.849467,  -2.239731077 },
  { 2442672.057721,  +1.244332266 },
  { 2442739.340565,  +3.473532923 },
  { 2445841.467006,  +0.872954032 },
  { 2451622.528401,  -1.952890449 },
  { 2451623.316192,  -1.894573223 },
  { 2451623.661812,  -1.868875967 },
  { 2451936.198441,  -3.155704532 },
  { 2454191.425853,  -1.037079658 },
  { 2454827.930217,  -0.303257584 },
  { 2455346.516074,  +0.638473206 },
  { 2457195.496859,  -0.456707138 },
  { 2458065.768941,  +4.064481441 },
  { 2459122.082865,  +2.470146694 },
  { 2459516.910833,  +4.077949289 },
  { 2460202.749390,  +1.153327957 },
  { 2461119.243931,  -1.913794068 },
  { 2461119.470964,  -1.897170067 },
  { 2461122.171639,  -1.697345773 },
  { 2461456.612969,  -3.437488203 },
  { 2461729.988201,  +3.616503840 },
  { 2462358.836857,  -1.335636248 },
  { 2462493.908295,  +0.214514268 },
  { 2464452.720857,  +0.810301398 },
  { 2466018.884664,  -0.983382694 },
  { 2466488.768064,  +2.812950983 },
  { 2468282.483463,  +4.076989891 },
  { 2468326.677286,  +1.581710623 },
  { 2468818.985723,  +0.161277468 },
  { 2469885.658712,  -1.865055942 },
  { 2469887.021755,  -1.764774515 },
  { 2469887.905471,  -1.699367208 },
  { 2473884.790527,  -3.046893353 },
  { 2473980.413234,  +0.361832237 },
  { 2474148.585430,  +3.604633929 },
  { 2475607.681319,  +3.713111678 },
  { 2476929.115967,  -1.034701155 },
  { 2477967.327361,  +0.820015679 },
  { 2480193.325241,  +0.165443418 },
  { 2480843.982528,  -1.790237458 },
  { 2480844.235407,  -1.771642042 },
  { 2480846.058190,  -1.636893957 },
  { 2483442.180978,  +0.723345994 },
  { 2484943.957712,  +0.033835542 },
  { 2486105.633443,  -1.095643546 },
}};

// Definitional oracle, independent of the Meeus formula chain: Skyfield 1.49 + DE440s,
// E = GHA of the apparent Sun + 180° − UT1 fraction of day × 360°. 40 uniform points over
// 1900–2100 (seed 43) + the Example 28.a epoch, generated 2026-07-29. The residual is the
// systematic gap between the UT1-based mean sun and Meeus's (28.2) mean sun (it grows
// ∝ ΔT, ~0.0027·ΔT) plus IAU model differences: measured mean 0.17 s, worst 0.21 s of
// time — tolerance 2e-3° (0.48 s) ≈ 2.3× the worst case.
constexpr std::array<EotRow, 41> SKYFIELD_ROWS {{
  { 2416419.907584,  +4.071353845 },
  { 2417836.673312,  +1.354031150 },
  { 2418594.038208,  +3.462217922 },
  { 2419491.878848,  -1.163905081 },
  { 2421894.316548,  +4.006518277 },
  { 2422014.438695,  -3.352984347 },
  { 2422547.520825,  -1.285380925 },
  { 2425534.677890,  +3.522550190 },
  { 2427890.864830,  -1.275844762 },
  { 2436654.676030,  -1.427682980 },
  { 2442222.098059,  -0.523823310 },
  { 2442476.571910,  -2.935757461 },
  { 2442554.507897,  +0.865721660 },
  { 2446606.072297,  -0.580472418 },
  { 2446728.743475,  +3.964141380 },
  { 2448125.519950,  -0.764008065 },
  { 2448808.018683,  -1.105206693 },
  { 2448908.500000,  +3.426596034 },
  { 2451251.141316,  -2.386113158 },
  { 2451418.788115,  -0.348128080 },
  { 2452032.676060,  +0.782435075 },
  { 2452453.024365,  -0.748798375 },
  { 2453170.539544,  -0.053093343 },
  { 2454977.127225,  +0.762141651 },
  { 2457035.284328,  -2.071776118 },
  { 2457708.660055,  +3.804419112 },
  { 2458646.598709,  +0.064012266 },
  { 2464083.624472,  +0.712334763 },
  { 2464639.155880,  +4.085114335 },
  { 2465878.990544,  -1.048154268 },
  { 2470195.639718,  -2.976157801 },
  { 2471436.315115,  -0.286411532 },
  { 2472944.797443,  -1.546736743 },
  { 2474936.749835,  -2.459913334 },
  { 2476315.784517,  +4.041255146 },
  { 2477176.086268,  -2.822049707 },
  { 2477279.332366,  -0.252892375 },
  { 2477527.653026,  -3.401291844 },
  { 2479942.964921,  +2.634293544 },
  { 2483159.676504,  -1.668588454 },
  { 2483825.617097,  +0.846087960 },
}};

// NOAA solcalc spreadsheet formulas (truncated-series family: Meeus (28.3)/Smart with the
// low-precision sun of ch. 25), reimplemented, 20 uniform points over 1950–2050, seed 44,
// generated 2026-07-29. Different formula family — catches structural errors (wrap, L0,
// missing terms) rather than precision. Measured worst gap 2.66 s of time — tolerance
// 2.5e-2° (6 s) ≈ 2.3× that.
constexpr std::array<EotRow, 20> NOAA_ROWS {{
  { 2433604.125486,  +3.714276729 },
  { 2434345.277176,  +2.962950714 },
  { 2435969.039052,  +0.915032459 },
  { 2437373.658936,  -2.276580697 },
  { 2437543.160734,  -0.069946517 },
  { 2439005.614489,  +0.044894303 },
  { 2439735.212654,  -0.025777349 },
  { 2441506.451489,  -1.230486876 },
  { 2445194.028347,  -1.257667749 },
  { 2447104.530510,  +4.112536168 },
  { 2448076.868533,  -1.075060781 },
  { 2448204.272990,  +4.067278399 },
  { 2452045.874941,  +0.912561328 },
  { 2453078.027633,  -2.340086848 },
  { 2455826.458890,  +1.737429396 },
  { 2458532.584178,  -3.494663897 },
  { 2458612.962299,  +0.883461873 },
  { 2464019.123809,  -3.246362000 },
  { 2464768.387417,  -2.149671549 },
  { 2469423.728552,  +1.418854472 },
}};

struct ApparentRow {
  int32_t  year;
  uint32_t month;
  uint32_t day;
  double   utc_frac;      // Input civil time as a fraction of the day.
  double   lon_deg;       // Observer longitude, positive east.
  double   apparent_frac; // Expected local apparent solar time, fraction of the day.
  int      day_offset;    // Expected date shift of the result vs the input date.
};

// Skyfield 1.49 + DE440s local apparent time: GHA of the apparent Sun + longitude + 180°,
// generated 2026-07-29. Covers both hemispheres, both dateline day-carries, and one
// pre-1972 row fed as UT1 (matching `apparent`'s documented UT1-proxy fallback). Rows
// carry the |UT1−UTC| steering gap plus the mean-sun definitional gap: measured worst
// 0.20 s — tolerance 0.5 s ≈ 2.5× that. New rows must stick to low-|DUT1| epochs
// (≲ 0.3 s): the documented 0.9 s steering gap would otherwise eat the tolerance.
constexpr std::array<ApparentRow, 6> APPARENT_ROWS {{
  { 1992, 10, 13, 0.500000000,   +0.0000, +0.509604750, +0 }, // Greenwich noon
  { 2024,  2, 11, 0.166666667, +120.0000, +0.490145643, +0 }, // 120°E, near the EoT minimum
  { 2024, 11,  3, 0.708333333,  -74.0060, +0.514183699, +0 }, // New York, near the EoT maximum
  { 2030,  6,  1, 0.958333333, +179.9000, +0.459478199, +1 }, // +12 h offset carries to next day
  { 2030,  6,  1, 0.041666667, -179.9000, +0.543466642, -1 }, // −12 h offset carries to previous day
  { 1960,  7,  1, 0.250000000,  +90.0000, +0.497426816, +0 }, // Pre-1972, input read as UT1
}};

struct TransitCase {
  int32_t  year;
  uint32_t month;
  uint32_t day;
  astro::sunrise_sunset::GeoLocation location;
};

const std::array<TransitCase, 4> TRANSIT_CASES {{
  { 2024,  2, 11, { .latitude = Angle<DEG> { 39.9042 }, .longitude = Angle<DEG> { 116.4074 } } },
  { 2024, 11,  3, { .latitude = Angle<DEG> { 40.7128 }, .longitude = Angle<DEG> { -74.0060 } } },
  { 1992, 10, 13, { .latitude = Angle<DEG> { 51.4769 }, .longitude = Angle<DEG> {   0.0    } } },
  { 2024,  6, 21, { .latitude = Angle<DEG> { 69.65   }, .longitude = Angle<DEG> {  18.96   } } },
}};

} // namespace


TEST(SolarTime, MeeusExample28a) {
  // Meeus Example 28.a: 1992 October 13.0 TD. Book values: L0 = 201°.807200 (via 28.2),
  // apparent α = 198°.378178 (full VSOP87), E = +3°.427351 = 13m42.6s. Our VSOP87D α sits
  // 0.21″ (5.7e-5°) from the book's α and E inherits that gap — tolerance 1.5e-4° ≈ 2.6×.
  const double jde = 2448908.5;

  const double L0 = astro::toolbox::normalize_deg(detail::sun_mean_longitude(jde).deg());
  ASSERT_NEAR(L0, 201.807200, 2.0e-5);
  ASSERT_NEAR(astro::sun::equatorial_coord::apparent(jde).α.deg(), 198.378178, 1.5e-4);
  ASSERT_NEAR(equation_of_time(jde).deg(), 3.427351, 1.5e-4);

  // The book's headline figure in time, to its printed precision (13m42.6s).
  ASSERT_NEAR(equation_of_time(jde).deg() * SECONDS_OF_TIME_PER_DEGREE, 822.6, 0.05);
}

TEST(SolarTime, MeanLongitudeAnchorsAtTauPmOne) {
  // At τ = ±1 (JDE = J2000 ± 365250) every (28.2) coefficient appears at face value, so a
  // high-order transcription error is visible: a τ⁴-term sign flip moves L0 by 1.3e-4° here
  // but only 7e-7° inside 1900–2100 — below every golden tolerance, which is why this anchor
  // exists. Exact-rational reference values (python Fraction, 2026-07-29); tolerance 1e-6°
  // ≫ the ~1e-10° double-rounding floor.
  ASSERT_NEAR(detail::sun_mean_longitude(2816795.0).deg(),  360288.195009048, 1.0e-6);
  ASSERT_NEAR(detail::sun_mean_longitude(2086295.0).deg(), -359727.201585807, 1.0e-6);
}

TEST(SolarTime, PymeeusCross) {
  for (const auto& row : PYMEEUS_ROWS) {
    ASSERT_NEAR(equation_of_time(row.jde).deg(), row.e_deg, 1.0e-5) << "jde " << row.jde;
  }
}

TEST(SolarTime, SkyfieldDefinitionalOracle) {
  for (const auto& row : SKYFIELD_ROWS) {
    ASSERT_NEAR(equation_of_time(row.jde).deg(), row.e_deg, 2.0e-3) << "jde " << row.jde;
  }
}

TEST(SolarTime, NoaaSpreadsheetFamilyCross) {
  for (const auto& row : NOAA_ROWS) {
    ASSERT_NEAR(equation_of_time(row.jde).deg(), row.e_deg, 2.5e-2) << "jde " << row.jde;
  }
}

TEST(SolarTime, BoundedAndSmooth1900To2100) {
  // 5-day grid over 1900–2100. Measured: E ∈ [−3.61°, +4.13°] = [−14.4, +16.5] min, worst
  // 5-day step 0.63°. |E| ≤ 4.25° (17 min) and step < 0.8° hold with margin; the range
  // must also actually reach both seasonal extremes — a damped or unwrapped E fails here.
  double prev = equation_of_time(2415020.5).deg();
  double max_e = prev;
  double min_e = prev;
  for (double jde = 2415025.5; jde <= 2488069.5; jde += 5.0) {
    const double e = equation_of_time(jde).deg();
    ASSERT_LT(std::fabs(e), 4.25) << "jde " << jde;
    ASSERT_LT(std::fabs(e - prev), 0.8) << "jde " << jde;
    max_e = std::max(max_e, e);
    min_e = std::min(min_e, e);
    prev = e;
  }
  ASSERT_GT(max_e, 4.0);
  ASSERT_LT(min_e, -3.5);
}

TEST(SolarTime, TransitReadsApparentNoon) {
  // Cross-check against the transit solver: at `transit_jde` the local apparent solar time
  // must read 12:00:00. The transit route goes through sidereal time (GMST polynomial),
  // ours through the (28.2) mean longitude — an error in either shows up here, on top of
  // the 28.a anchor both are pinned to. Measured worst gap 0.17 s — tolerance 0.5 s.
  for (const auto& c : TRANSIT_CASES) {
    const double transit = astro::sunrise_sunset::transit_jde(util::to_ymd(c.year, c.month, c.day), c.location);
    const auto utc = astro::julian_day::jde_to_utc(transit);
    const auto apparent_dt = apparent(utc, c.location.longitude);
    ASSERT_NEAR(apparent_dt.fraction(), 0.5, 0.5 / 86400.0) << c.year << '-' << c.month << '-' << c.day;
  }
}

TEST(SolarTime, ApparentSolarTimeGolden) {
  for (const auto& row : APPARENT_ROWS) {
    const auto ymd = util::to_ymd(row.year, row.month, row.day);
    const auto apparent_dt = apparent(
      calendar::Datetime { ymd, row.utc_frac },
      Angle<DEG> { row.lon_deg }
    );

    const auto day_diff = (std::chrono::sys_days { apparent_dt.ymd } - std::chrono::sys_days { ymd }).count();
    ASSERT_EQ(day_diff, row.day_offset) << "lon " << row.lon_deg;
    ASSERT_NEAR(apparent_dt.fraction() * 86400.0, row.apparent_frac * 86400.0, 0.5) << "lon " << row.lon_deg;
  }
}

TEST(SolarTime, ApparentRejectsBadLongitude) {
  const auto utc = calendar::Datetime { util::to_ymd(2024, 6, 1), 0.5 };
  ASSERT_THROW({ apparent(utc, Angle<DEG> { 180.5 }); },  std::invalid_argument);
  ASSERT_THROW({ apparent(utc, Angle<DEG> { -181.0 }); }, std::invalid_argument);
  ASSERT_THROW({ apparent(utc, Angle<DEG> { std::numeric_limits<double>::quiet_NaN() }); }, std::invalid_argument);
  ASSERT_NO_THROW({ apparent(utc, Angle<DEG> { 180.0 }); });
  ASSERT_NO_THROW({ apparent(utc, Angle<DEG> { -180.0 }); });
}

} // namespace astro::solar_time::test
