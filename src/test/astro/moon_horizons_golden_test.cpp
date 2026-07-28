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

#include "moon.hpp"

// Independent golden dataset for the Moon's apparent geocentric position (#94, #65), collected
// 2026-07-27 by `statistics/moon_horizons_crawler.py`:
// - Source: JPL Horizons API v1.2, ephemeris DE441, Moon (301) from the geocenter (500@399).
//   Epochs are supplied and echoed in TT, so no Horizons-side ΔT model enters the chain.
//   Quantity 31 = ObsEcLon/ObsEcLat: IAU76/80 true-ecliptic-of-date apparent position, including
//   light-time, gravitational deflection, and stellar aberration. This library computes the
//   geometric ELP position + Δψ only; the two ~20″ correction components (light-time against the
//   Moon's barycentric motion, aberration against the geocenter's) nearly cancel for the
//   co-moving Moon, leaving a net ~0.7″ planetary aberration unmodeled here (in the tolerance).
//   Quantity 20 = apparent range, converted AU → km with Horizons' own constant 149597870.700.
// - Source validation (#94: no source enters the chain unverified):
//   * Skyfield 1.54 + JPL DE421 (same JPL family, not an independent lunar theory — it guards
//     the query semantics: frame, time scale, correction level) recomputed the 26 of 33 core
//     epochs inside DE421's 1899–2053 span: worst disagreement 0.231″ in angle, 1.0 m in range.
//     The 2057–2094 core tail and the other bands reuse the identical, thus-validated pipeline.
//   * pyerfa 2.0.1.5 `eraMoon98` — SOFA's independent transcription of the same truncated
//     ELP2000-82B (Meeus ch. 47) implemented by `elp2000_82b.hpp` — reproduces Example 47.a's
//     book distance to 15 m, and its range envelope vs DE441 is: core band median 28.4 km /
//     worst 45.7 km, extended worst 34.7 km, far worst 34.9 km. The envelope is flat across
//     bands (periodic-term truncation, not secular), so one range tolerance serves all bands,
//     and a residual well beyond ~46 km would indicate a transcription bug, not truncation.
//   * Meeus Example 47.a (JD 2448724.5 TT) anchors the books' own numbers: book apparent
//     λ/β/Δ differ from DE441 by +2.00″ / +0.27″ / −29.7 km — the truncation envelope itself.
// - Tolerances declare two components (#94): the truncated series' model error (book-stated
//   ~10″ in λ, ~4″ in β; range envelope ≤ 46 km) plus source-side corrections we do not model
//   (~0.7″ aberration; nutation-series truncation ≤ ~0.5″). Source agreement itself is ≤ 0.23″.
//   Extended/far bands add the secular drift of extrapolating the mean arguments millennia
//   beyond the fitted era (tidal-acceleration mismatch grows ~quadratically with |T|).
// - Time-scale detection: 23/41 epochs have |range-rate| ≥ 0.03 km/s (non-extremum sampling,
//   #68 — the retired DiffTest2 sampled only perigees/apogees), and the Moon moves ~0.55″/s,
//   so a TT/UT1 mixup (~69 s ≈ 38″) breaches the core λ tolerance on many rows.
// Measured worst residuals are recorded above each tolerance. Mutation detection 2026-07-27,
// 4/4 red: +69 s time shift (λ axis); nutation sign flip (λ); dropped −2235·sin(L′) latitude
// perturbation term (β); +100 km range-base transcription slip (r, red in all three bands).
// Fine-grained transcription of the frozen implementation (1e-7-and-tighter deltas) stays
// guarded by the characterization tables in moon_test.cpp / elp2000_82b_test.cpp; this file
// anchors absolute accuracy at the truncation-envelope scale.

namespace astro::moon::test {

using astro::moon::geocentric_coord::apparent;

namespace {

// One Horizons golden row: JD(TT); apparent true-ecliptic-of-date longitude / latitude (deg);
// apparent range (km).
struct HorizonsRow {
  double jde;
  double lon_deg;
  double lat_deg;
  double r_km;
};

// Per-band tolerances (deg, deg, km). Never loosen silently: these are the accuracy contract
// for the truncated ELP2000-82B implementation (#94).
struct Tolerance {
  double lon_deg;
  double lat_deg;
  double r_km;
};

/** @brief Angular difference |a − b| in degrees, wrapped to [0, 180]. Both inputs live in
 *  [0, 360) — normalized λ and the golden columns — so the +540 shift keeps fmod positive. */
auto wrapped_diff_deg(const double a, const double b) -> double {
  return std::fabs(std::fmod(a - b + 540.0, 360.0) - 180.0);
}

void check_band(const std::span<const HorizonsRow> rows, const Tolerance& tol,
                const std::string_view band) {
  double worst_lon = 0.0;
  double worst_lat = 0.0;
  double worst_r = 0.0;
  for (const auto& row : rows) {
    const auto coord = apparent(row.jde);
    const double d_lon = wrapped_diff_deg(coord.λ.deg(), row.lon_deg);
    const double d_lat = std::fabs(coord.β.deg() - row.lat_deg);
    const double d_r = std::fabs(coord.r.km() - row.r_km);
    EXPECT_LE(d_lon, tol.lon_deg) << band << " λ, jde " << row.jde << ": ours "
                                  << coord.λ.deg() << " vs golden " << row.lon_deg;
    EXPECT_LE(d_lat, tol.lat_deg) << band << " β, jde " << row.jde << ": ours "
                                  << coord.β.deg() << " vs golden " << row.lat_deg;
    EXPECT_LE(d_r, tol.r_km) << band << " r, jde " << row.jde << ": ours "
                             << coord.r.km() << " vs golden " << row.r_km;
    worst_lon = std::max(worst_lon, d_lon);
    worst_lat = std::max(worst_lat, d_lat);
    worst_r = std::max(worst_r, d_r);
  }
  std::cout << band << " measured worst residuals: dlon " << worst_lon * 3600.0 << "\", dlat "
            << worst_lat * 3600.0 << "\", dr " << worst_r << " km\n";
}

// 1900–2100 + the Meeus Example 47.a anchor (row 2448724.50). 32 epochs stepped 2283.25 days —
// incommensurate with the anomalistic/synodic months, scanning all phases and radial velocities.
const std::vector<HorizonsRow> CORE_ROWS {
  {  2415385.50,   48.3163870,   1.1700667,  370415.286 },  // 1901-Jan-01 00:00 TT, rdot +0.010 km/s
  {  2417668.75,  250.7492890,   3.7936814,  368501.225 },  // 1907-Apr-03 06:00 TT, rdot +0.000 km/s
  {  2419952.00,   91.4711213,   5.0100103,  372598.859 },  // 1913-Jul-03 12:00 TT, rdot -0.049 km/s
  {  2422235.25,  295.2390834,   4.4441289,  393119.079 },  // 1919-Oct-03 18:00 TT, rdot -0.059 km/s
  {  2424518.50,  144.3774117,   2.5595135,  405719.122 },  // 1926-Jan-03 00:00 TT, rdot -0.006 km/s
  {  2426801.75,  354.5925763,  -0.1496005,  403580.582 },  // 1932-Apr-04 06:00 TT, rdot +0.022 km/s
  {  2429085.00,  203.7991463,  -2.7500198,  390428.736 },  // 1938-Jul-05 12:00 TT, rdot +0.059 km/s
  {  2431368.25,   47.4319608,  -4.6195790,  365669.383 },  // 1944-Oct-04 18:00 TT, rdot +0.059 km/s
  {  2433651.50,  243.9400737,  -4.8024364,  361603.556 },  // 1951-Jan-05 00:00 TT, rdot -0.029 km/s
  {  2435934.75,   85.6903816,  -3.0403492,  378618.267 },  // 1957-Apr-06 06:00 TT, rdot -0.043 km/s
  {  2438218.00,  291.5913219,  -0.1091934,  389092.802 },  // 1963-Jul-07 12:00 TT, rdot -0.042 km/s
  {  2440501.25,  139.1984785,   2.7740381,  400949.449 },  // 1969-Oct-06 18:00 TT, rdot -0.031 km/s
  {  2442784.50,  348.4026038,   4.4935567,  402617.103 },  // 1976-Jan-07 00:00 TT, rdot +0.024 km/s
  {  2445067.75,  195.9503051,   4.9968463,  391190.588 },  // 1982-Apr-08 06:00 TT, rdot +0.040 km/s
  {  2447351.00,   42.1402865,   4.2267773,  380419.878 },  // 1988-Jul-08 12:00 TT, rdot +0.044 km/s
  {  2448724.50,  133.1667103,  -3.2292008,  368439.405 },  // 1992-Apr-12 00:00 TT, rdot -0.012 km/s
  {  2449634.25,  245.0114676,   1.7918937,  363924.226 },  // 1994-Oct-08 18:00 TT, rdot +0.037 km/s
  {  2451917.50,   81.8526772,  -2.1337555,  362559.135 },  // 2001-Jan-08 00:00 TT, rdot -0.050 km/s
  {  2454200.75,  283.9622685,  -4.5810525,  388043.112 },  // 2007-Apr-10 06:00 TT, rdot -0.062 km/s
  {  2456484.00,  132.6254357,  -5.0466466,  402473.696 },  // 2013-Jul-10 12:00 TT, rdot -0.026 km/s
  {  2458767.25,  342.7699740,  -4.3330833,  405922.069 },  // 2019-Oct-10 18:00 TT, rdot +0.000 km/s
  {  2461050.50,  192.3679777,  -2.7282287,  396403.041 },  // 2026-Jan-10 00:00 TT, rdot +0.053 km/s
  {  2463333.75,   36.7534176,   0.2647492,  374858.394 },  // 2032-Apr-11 06:00 TT, rdot +0.054 km/s
  {  2465617.00,  238.0251613,   3.5597177,  368741.955 },  // 2038-Jul-12 12:00 TT, rdot +0.006 km/s
  {  2467900.25,   80.4653915,   5.0905531,  369916.479 },  // 2044-Oct-11 18:00 TT, rdot -0.006 km/s
  {  2470183.50,  281.3716385,   4.4592838,  374469.913 },  // 2051-Jan-12 00:00 TT, rdot -0.050 km/s
  {  2472466.75,  125.4862540,   2.4791126,  395481.376 },  // 2057-Apr-13 06:00 TT, rdot -0.054 km/s
  {  2474750.00,  335.2360181,   0.0555964,  405468.622 },  // 2063-Jul-14 12:00 TT, rdot -0.002 km/s
  {  2477033.25,  185.4222320,  -2.5928151,  402399.608 },  // 2069-Oct-13 18:00 TT, rdot +0.025 km/s
  {  2479316.50,   34.0912899,  -4.6330525,  389132.077 },  // 2076-Jan-14 00:00 TT, rdot +0.060 km/s
  {  2481599.75,  236.4207392,  -5.0615279,  363357.277 },  // 2082-Apr-15 06:00 TT, rdot +0.053 km/s
  {  2483883.00,   73.6307476,  -3.2605348,  361780.392 },  // 2088-Jul-15 12:00 TT, rdot -0.032 km/s
  {  2486166.25,  275.7230422,  -0.4400281,  380086.796 },  // 2094-Oct-15 18:00 TT, rdot -0.048 km/s
};

// ~501 / ~999 / ~1599 / ~2500 / ~3000 CE: truncation degradation away from the fitted era.
const std::vector<HorizonsRow> EXTENDED_ROWS {
  {  1904000.50,  319.4163685,  -0.0387457,  372646.267 },  // 0500-Nov-14 00:00 TT, rdot +0.043 km/s
  {  2086000.50,  103.8756791,   4.5422788,  380002.831 },  // 0999-Feb-28 00:00 TT, rdot +0.039 km/s
  {  2305000.50,  329.4914761,   0.2397865,  365401.780 },  // 1598-Oct-11 00:00 TT, rdot -0.011 km/s
  {  2634000.50,  241.6732448,   4.3528325,  364969.470 },  // 2499-Jul-19 00:00 TT, rdot -0.028 km/s
  {  2817000.50,  249.9951948,   3.0345156,  400763.976 },  // 3000-Aug-02 00:00 TT, rdot +0.037 km/s
};

// JD 2^22 = 4194304 straddle (~6771 CE; double-precision cliff regression guard, #76 context)
// and ~9420 CE. Gross-breakage guards only: at |T| ≈ 48–74 centuries the mean-argument
// extrapolation dominates every residual.
const std::vector<HorizonsRow> FAR_ROWS {
  {  4194303.50,  173.7236957,   0.8425104,  367532.707 },  // 6771-Jul-07 00:00 TT, rdot +0.022 km/s
  {  4194304.50,  187.9429346,  -0.4108899,  369790.010 },  // 6771-Jul-08 00:00 TT, rdot +0.029 km/s
  {  5161700.50,   92.9588919,  -0.7232190,  403742.696 },  // 9420-Feb-27 00:00 TT, rdot -0.014 km/s
};

}  // namespace

// The range tolerance is one number for all three bands: the truncation envelope is flat in
// time (periodic terms only — measured worst 45.7 / 34.8 / 46.3 km for core/extended/far), so
// 55 km bounds every band while staying close enough to the ~46 km envelope that a systematic
// range offset beyond ~10 km turns the test red.
constexpr double TOL_R_KM = 55.0;

TEST(MoonHorizonsGolden, CoreBand) {
  // Measured worst residuals 2026-07-27: λ 6.49″, β 2.06″, r 45.689 km. λ/β tolerances sit at
  // the book-stated model accuracy (~10″ / ~4″) with the ~0.7″ unmodeled aberration inside.
  check_band(CORE_ROWS, { .lon_deg = 0.003, .lat_deg = 0.0015, .r_km = TOL_R_KM }, "core");
}

TEST(MoonHorizonsGolden, ExtendedBand) {
  // Measured worst residuals 2026-07-27: λ 24.8″, β 1.85″, r 34.8 km — the λ budget adds the
  // mean-argument extrapolation drift at |T| up to 15 centuries.
  check_band(EXTENDED_ROWS, { .lon_deg = 0.01, .lat_deg = 0.002, .r_km = TOL_R_KM }, "extended");
}

TEST(MoonHorizonsGolden, FarBand) {
  // Measured worst residuals 2026-07-27: λ 969″, β 140″, r 46.3 km — at |T| ≈ 48–74 centuries
  // the secular (tidal-acceleration) drift of the mean arguments dominates λ/β; the range
  // column alone keeps its full detection power out here.
  check_band(FAR_ROWS, { .lon_deg = 0.4, .lat_deg = 0.06, .r_km = TOL_R_KM }, "far");
}

}  // namespace astro::moon::test
