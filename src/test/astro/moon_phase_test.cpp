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

#include <gtest/gtest.h>
#include <print>
#include <tuple>
#include <vector>
#include <chrono>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include "julian_day.hpp"
#include "util.hpp"
#include "astro.hpp"
#include "ymd.hpp"

namespace astro::moon_phase::test {

using namespace astro::moon_phase::new_moon;

TEST(NewMoon, RootGenerator) {
  using namespace std::ranges;
  const auto jde = astro::julian_day::J2000 + util::random(-200000.0, 200000.0);
  
  const auto roots = std::invoke([&] {
    RootGenerator gen(jde);
    std::vector<double> roots;
    for (int i = 0; i < 64; ++i) {
      roots.push_back(gen.next()); // NOLINT(performance-inefficient-vector-operation)
    }
    return roots;
  });

  for (const auto root : roots) {
    ASSERT_GT(root, jde);
    const double diff = longitude_diff(root);

    constexpr double epsilon = 0.00001;
    ASSERT_TRUE(diff < epsilon or diff > 360.0 - epsilon);
  }

  // TODO: Use `std::views::pairwise` once every CI leg has it (./linter.py --features).
  for (auto it = cbegin(roots); it != cend(roots); ++it) {
    const auto next = std::next(it);
    if (next == cend(roots)) {
      break;
    }

    // Ensure next > current
    ASSERT_GT(*next, *it);
    // Ensure the gap is around 29.5 days, which is the avg length of a lunar month.
    const double day_diff = *next - *it;
    ASSERT_NEAR(day_diff, 29.5, 0.75);
  }
}


TEST(NewMoon, InvalidArgument) {
  // `newton_method` demands both endpoints within BRACKET_TOLERANCE_DEG (15 deg) of conjunction —
  // the left one trailing (elongation just under 360 deg) and the right one leading (just above
  // 0 deg). The Moon's elongation rate runs 10.5-14.5 deg/day, so these cases sit far outside
  // the window in either configuration and must be rejected.
  const double root = moments(2024).front();

  // Three days past conjunction, the left endpoint already leads by tens of degrees:
  // not a bracket at all.
  ASSERT_THROW(std::ignore = newton_method(root + 3.0, root + 4.0), std::invalid_argument);

  // Half a day before conjunction the left endpoint is fine, but two days after, the right
  // endpoint has left the tolerance window.
  ASSERT_THROW(std::ignore = newton_method(root - 0.5, root + 2.0), std::invalid_argument);

  // `next_root` insists its seed is a root: ten days past conjunction the elongation is
  // ~120 deg away.
  ASSERT_THROW(std::ignore = next_root(root + 10.0), std::invalid_argument);
}


TEST(NewMoon, Moments) {
  // Find roots in consecutive 10 years, and ensure the roots are consecutive as expected.
  const int32_t year = util::random(1700, 2050);

  std::vector<double> roots;
  for (int i = 0; i < 10; ++i) {
    const auto roots_in_year = moments(year + i);

    // Ensure all roots in this year are really in this year. The year is bounded in UTC (#84).
    for (const auto root : roots_in_year) {
      const auto dt = astro::julian_day::jde_to_utc(root);
      const auto [y, _, __] = util::from_ymd(dt.ymd);
      ASSERT_EQ(y, year + i);
    }

    roots.insert(end(roots), cbegin(roots_in_year), cend(roots_in_year));
  }

  // Ensure `roots` are in order and consecutive.
  for (auto it = cbegin(roots); it != cend(roots); ++it) {
    const auto next = std::next(it);
    if (next == cend(roots)) {
      break;
    }
    // Ensure next root is greater than current
    ASSERT_GT(*next, *it);
    // Ensure the gap is around 29.5 days, which is the avg length of a lunar month.
    const double day_diff = *next - *it;
    ASSERT_NEAR(day_diff, 29.5, 0.75);
  }
}


// DiffTest1 (nong-wiki conjunctions) retired under #68/#84 — the source states no time scale;
// superseded by DiffTest2 (HKO) and the boundary tests below.


TEST(NewMoon, DiffTest2) {
  using namespace std::ranges;
  using namespace std::chrono_literals;
  using hms = std::chrono::hh_mm_ss<std::chrono::nanoseconds>;

  // Datetimes in UTC+8 (Hong Kong time).
  // Data source: https://www.hko.gov.hk/tc/gts/astronomy/Moon_Phase.htm
  const std::vector<calendar::Datetime> datetimes {
    calendar::Datetime { util::to_ymd(2024,  1, 11), hms { 19h + 57min } },
    calendar::Datetime { util::to_ymd(2024,  2, 10), hms {  6h + 59min } },
    calendar::Datetime { util::to_ymd(2024,  3, 10), hms { 17h +  0min } },
    calendar::Datetime { util::to_ymd(2024,  4,  9), hms {  2h + 21min } },
    calendar::Datetime { util::to_ymd(2024,  5,  8), hms { 11h + 22min } },
    calendar::Datetime { util::to_ymd(2024,  6,  6), hms { 20h + 38min } },
    calendar::Datetime { util::to_ymd(2024,  7,  6), hms {  6h + 57min } },
    calendar::Datetime { util::to_ymd(2024,  8,  4), hms { 19h + 13min } },
    calendar::Datetime { util::to_ymd(2024,  9,  3), hms {  9h + 56min } },
    calendar::Datetime { util::to_ymd(2024, 10,  3), hms {  2h + 49min } },
    calendar::Datetime { util::to_ymd(2024, 11,  1), hms { 20h + 47min } },
    calendar::Datetime { util::to_ymd(2024, 12,  1), hms { 14h + 21min } },
    calendar::Datetime { util::to_ymd(2024, 12, 31), hms {  6h + 27min } },
  };

  const auto jdes = datetimes
                  | views::transform([](const calendar::Datetime& utc8) { // UTC+8 civil time → UTC → JDE (#84)
                      return astro::julian_day::utc_to_jde(calendar::add_seconds(utc8, -8.0 * 3600.0));
                    })
                  | to<std::vector>();

  const auto actual_roots = moments(2024);

  ASSERT_EQ(size(actual_roots), size(jdes));

  for (const auto [root, expected_root] : views::zip(actual_roots, jdes)) {
    ASSERT_NEAR(root, expected_root, 0.0005);
  }
}


TEST(NewMoon, MomentsYearBoundaryIsUtc) {
  // #84: the year bounds are UTC; under the old UT1 bounds a conjunction between the two
  // midnights was attributed to the neighbouring year. The gap is ΔT − (ΔAT + 32.184) s:
  // sub-second today, hours by the fifth millennium.
  // Provenance: flip cases from a 1972-5000 scan of this library's own (Horizons-validated)
  // chain, 2026-07-28; the assertion under test is the year attribution, whose boundary
  // formula is pinned against pyerfa in `LeapSecond.UtcBoundaryFormula`.
  struct Case { int32_t year; std::size_t n; std::size_t n_prev; double first; };
  // NOLINTBEGIN(modernize-use-designated-initializers)
  const std::vector<Case> cases {
    { 3312, 13, 12, 2930742.50177006 }, // conjunction 83.7 s after UTC new-year midnight
    { 3999, 13, 12, 3181664.62194254 },
    { 4933, 13, 12, 3522801.58333821 },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  for (const auto& c : cases) {
    const auto roots = moments(c.year);
    ASSERT_EQ(roots.size(), c.n);
    ASSERT_NEAR(roots.front(), c.first, 1e-5);

    // The boundary-adjacent conjunction must not leak into the previous year.
    const auto prev_roots = moments(c.year - 1);
    ASSERT_EQ(prev_roots.size(), c.n_prev);
    ASSERT_LT(prev_roots.back(), c.first - 25.0); // a synodic month before, not the same root
  }
}


TEST(NewMoon, MomentsAcrossTheUlpStep) {
  // Regression for issues #76 / #63. The conjunction solver shares its parameters with the solar
  // one, so it shared the failure: past JD 2^22 (6771-07-07) the difference step fell below half
  // an ulp, f' collapsed, and the candidate was clamped to the edge of its bracket. A conjunction
  // spoiled that way then failed `next_root`'s "is this a root" guard, `moments` threw, and every
  // remaining conjunction of that year was silently lost -- 6700-6850 used to yield 1807 roots
  // where it now yields 1868.
  const std::vector<int32_t> years { 6770, 6771, 6772, 6773, 8000, 9040, 9050 };

  for (const auto year : years) {
    const auto roots = moments(year); // must not throw
    ASSERT_GE(roots.size(), 12U); // a Gregorian year holds 12 or 13 conjunctions
    ASSERT_LE(roots.size(), 13U);

    for (const auto root : roots) {
      // One ulp of JDE moves the Moon 5.7e-9 deg away from the Sun, and past the step twice that,
      // so the residual cannot be driven below roughly 1.1e-8 deg. The existing conjunction tests
      // assert 1e-5, which this stays well inside.
      const auto diff = longitude_diff(root);
      ASSERT_TRUE((diff < 1e-7) or (diff > 360.0 - 1e-7));
    }

    // Consecutive conjunctions stay one synodic month apart -- a clamped root would break this.
    for (auto it = cbegin(roots); std::next(it) != cend(roots); ++it) {
      ASSERT_NEAR(*std::next(it) - *it, 29.5, 0.75);
    }
  }
}


TEST(NewMoon, BracketEndpointsClearNewtonTolerance) {
  // Issue #63. The bracket used to be laid out around the mean-rate estimate, so its endpoints sat
  // however far that estimate had missed, plus whatever the Moon's rate varied over the bracket
  // itself. Measured across 401-9999 that left one endpoint within 0.67 deg of the tolerance
  // `newton_method` demands -- a margin nobody chose, and one that would have let `f` go
  // discontinuous inside its own bracket had it ever been crossed.
  //
  // `first_root_range_after` now steps the estimate onto the root before bracketing it, so the
  // endpoints' distance from conjunction follows from BRACKET_HALF_WIDTH_DAYS. The tightest
  // endpoint measured over the same span is 6.11 deg inside the limit; asserting half of that
  // pins the design without pinning the measurement.
  constexpr double REQUIRED_MARGIN_DEG = 3.0;

  // Fixed years spanning the supported range, plus a random one to reach where they do not.
  std::vector<int32_t> years { 401, 1900, 2026, 6771, 6772, 9420, 9980 };
  years.push_back(util::random(401, 9980));

  for (const auto year : years) {
    double jde = astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year, 1, 1), 0.0 });

    for (int i = 0; i < 15; ++i) {
      const auto [left, right] = first_root_range_after(jde);

      // Laying the bracket out around the refined estimate is what decouples it from the miss:
      // its width is now BRACKET_HALF_WIDTH_DAYS either side, where it used to be twice however
      // far the mean-rate extrapolation had landed from conjunction.
      ASSERT_NEAR(right - left, 2.0 * BRACKET_HALF_WIDTH_DAYS, 1e-9);

      ASSERT_GT(longitude_diff(left),  360.0 - BRACKET_TOLERANCE_DEG + REQUIRED_MARGIN_DEG);
      ASSERT_LT(longitude_diff(right), BRACKET_TOLERANCE_DEG - REQUIRED_MARGIN_DEG);

      // The bracket must still contain the root it was built for.
      const double root = newton_method(left, right);
      ASSERT_GT(root, left);
      ASSERT_LT(root, right);

      jde = root + 1.0;
    }
  }
}

TEST(Illumination, Example48aFormulaLayer) {
  // Provenance: Meeus Example 48.a (1992 April 12, 0h TT), mirrored value-by-value. The inputs
  // are the book's own printed (rounded) positions from Example 47.a, so this layer pins the
  // formula transcription exactly — no position-model error is absorbed. Tolerances are the
  // print digits; measured gaps recomputing from the rounded inputs: i 3.8e-5 deg, k 3.2e-5.
  const astro::toolbox::SphericalCoordinate sun_pos {
    .λ = astro::toolbox::AngleDeg { 20.6579 },    // α₀ — equatorial plugs into (λ, β) as-is
    .β = astro::toolbox::AngleDeg { 8.6964 },     // δ₀
    .r = astro::toolbox::DistanceAu { 1.0024977 },
  };
  const astro::toolbox::SphericalCoordinate moon_pos {
    .λ = astro::toolbox::AngleDeg { 134.6885 },   // α
    .β = astro::toolbox::AngleDeg { 13.7684 },    // δ
    .r = astro::toolbox::DistanceAu { astro::toolbox::DistanceKm { 368410.0 } },
  };

  const auto i = illumination::phase_angle(sun_pos, moon_pos);
  ASSERT_NEAR(i.deg(), 69.0756, 5e-5);
  ASSERT_NEAR(illumination::fraction(i), 0.6786, 5e-5);
}

TEST(Illumination, Example48aEndToEnd) {
  // The same instant through the library's own positions (VSOP87D + truncated ELP2000-82B).
  // Measured gap vs the book's 0.6786: 3.3e-5 — below the print digit.
  ASSERT_NEAR(illumination::fraction(2448724.5), 0.6786, 5e-5);
}

TEST(Illumination, HorizonsGoldenDataset) {
  // Provenance: JPL Horizons, Moon (301), observer quantity 10 (Illu%), geocenter 500@399,
  // TT scale, DE441 (gated at collection: the response must name {source: DE441}) — 121
  // epochs = 120 seeded (uniform in [1900, 2100], seed 42) + the Example 48.a anchor,
  // collected 2026-08-10. Regenerable: statistics/moon_illumination_horizons_crawler.py
  // prints this exact block. Measured max gap 4.33e-5 — the VSOP87D + truncated-ELP position
  // gap to DE441 in fraction units; tolerance 1e-4 ≈ 2.3x the measured max.
  const std::vector<std::pair<double, double>> dataset {
    { 2415495.221397, 0.0499048 },
    { 2415859.165691, 0.9064972 },
    { 2416847.485648, 0.1605078 },
    { 2416958.899513, 0.8035806 },
    { 2417197.127286, 0.7013731 },
    { 2417365.358617, 0.8653171 },
    { 2418367.879577, 0.8078125 },
    { 2418462.256992, 0.9945935 },
    { 2419661.071879, 0.2347863 },
    { 2420849.144330, 0.8684526 },
    { 2421371.207846, 0.0309711 },
    { 2421661.250740, 0.2244547 },
    { 2421795.398367, 0.8981585 },
    { 2422085.437895, 0.4966899 },
    { 2422398.452416, 0.2973007 },
    { 2423030.149674, 0.4164011 },
    { 2423169.163177, 0.0611201 },
    { 2426185.102889, 0.0478350 },
    { 2426377.966502, 0.9117812 },
    { 2426956.725364, 0.3527883 },
    { 2427521.835974, 0.7085953 },
    { 2428070.662569, 0.0592515 },
    { 2429545.192707, 0.2326025 },
    { 2430324.569584, 0.5347329 },
    { 2430432.374758, 0.1100487 },
    { 2430991.566783, 0.2482774 },
    { 2431041.442815, 0.9893363 },
    { 2431123.246559, 0.6514258 },
    { 2431325.598000, 0.1565728 },
    { 2431668.013240, 0.5500878 },
    { 2431752.003561, 0.9225587 },
    { 2432025.408667, 0.2702747 },
    { 2433036.146182, 0.0169205 },
    { 2434213.249019, 0.0463504 },
    { 2434260.543379, 0.9872230 },
    { 2434369.466402, 0.4040310 },
    { 2434511.546476, 0.0106313 },
    { 2434522.695945, 0.8747469 },
    { 2434578.435508, 0.5726832 },
    { 2435110.841648, 0.6672698 },
    { 2435325.915760, 0.9280245 },
    { 2436159.711965, 0.2484178 },
    { 2436821.295126, 0.4866637 },
    { 2438007.089837, 0.8930581 },
    { 2438063.714255, 0.7030747 },
    { 2439608.058331, 0.8712686 },
    { 2439875.119731, 0.8295192 },
    { 2441670.761009, 0.9795634 },
    { 2442061.479286, 0.6382433 },
    { 2442671.679186, 0.8380223 },
    { 2442738.961110, 0.8731821 },
    { 2442897.025641, 0.0074965 },
    { 2443920.619109, 0.8606838 },
    { 2444195.908099, 0.0207052 },
    { 2445841.045084, 0.7074255 },
    { 2445858.441255, 0.5294866 },
    { 2446779.232031, 0.9772301 },
    { 2448164.109300, 0.7384120 },
    { 2448538.710128, 0.0217882 },
    { 2448724.500000, 0.6785466 },
    { 2451587.314386, 0.4833009 },
    { 2451935.693085, 0.0247754 },
    { 2452240.376700, 0.8396190 },
    { 2453671.244681, 0.2571356 },
    { 2454190.889625, 0.9521361 },
    { 2454245.108696, 0.5725217 },
    { 2454827.385275, 0.0044279 },
    { 2455345.964033, 0.9755430 },
    { 2455704.564883, 0.6674708 },
    { 2456027.319464, 0.8698555 },
    { 2457194.919507, 0.2275006 },
    { 2457723.337414, 0.0177715 },
    { 2458065.179676, 0.8348281 },
    { 2459121.479139, 0.9218714 },
    { 2459504.556005, 0.8742156 },
    { 2459516.301702, 0.5048929 },
    { 2460202.130871, 0.0025634 },
    { 2460854.178454, 0.0663995 },
    { 2461455.977285, 0.9677618 },
    { 2461694.482701, 0.9963768 },
    { 2461729.348774, 0.6350671 },
    { 2461771.202458, 0.1313261 },
    { 2461841.477463, 0.9988023 },
    { 2462358.188821, 0.0027702 },
    { 2462493.258411, 0.9675424 },
    { 2462898.983622, 0.7018253 },
    { 2464452.044157, 0.1425957 },
    { 2464818.079037, 0.6506119 },
    { 2465030.201806, 0.9884338 },
    { 2466018.186525, 0.0497111 },
    { 2466488.063492, 0.2494912 },
    { 2467667.787461, 0.3669594 },
    { 2468281.754337, 0.9381064 },
    { 2468325.947554, 0.0883016 },
    { 2468818.249252, 0.4388863 },
    { 2471705.741539, 0.9873530 },
    { 2472880.313408, 0.6231106 },
    { 2473883.984708, 0.6576397 },
    { 2473979.606106, 0.9919025 },
    { 2474147.775999, 0.3964703 },
    { 2475606.851914, 0.8424850 },
    { 2476589.147066, 0.0643513 },
    { 2476928.268473, 0.8631709 },
    { 2477898.735689, 0.4197385 },
    { 2477966.465654, 0.9715314 },
    { 2478610.140489, 0.7286617 },
    { 2478999.805589, 0.9961750 },
    { 2479037.402379, 0.3069011 },
    { 2480192.433062, 0.0659402 },
    { 2480604.666001, 0.1448120 },
    { 2481686.138408, 0.9964132 },
    { 2481826.372335, 0.5698777 },
    { 2483441.244323, 0.0611491 },
    { 2483898.168813, 0.9215425 },
    { 2484694.845874, 0.9051406 },
    { 2484943.000499, 0.3613973 },
    { 2485955.833328, 0.0979221 },
    { 2486104.660327, 0.1380310 },
    { 2487303.201717, 0.5540065 },
    { 2487785.174584, 0.0047887 },
    { 2487888.627079, 0.9742504 },
  };

  for (const auto& [jde, expected] : dataset) {
    ASSERT_NEAR(illumination::fraction(jde), expected, 1e-4);
  }
}

TEST(Illumination, Example48aPositionAngleFormulaLayer) {
  // Provenance: Meeus Example 48.a (1992 April 12, 0h TT), printed equatorial positions.
  // The book rounds χ to 285°.0; the inputs are rounded to 4 decimals, so the formula-layer
  // tolerance is half a print digit. Measured recomputation from the rounded inputs: 285.0000.
  const astro::coords::EquatorialCoord sun_eq {
    .α = astro::toolbox::AngleDeg { 20.6579 },
    .δ = astro::toolbox::AngleDeg { 8.6964 },
  };
  const astro::coords::EquatorialCoord moon_eq {
    .α = astro::toolbox::AngleDeg { 134.6885 },
    .δ = astro::toolbox::AngleDeg { 13.7684 },
  };

  const auto χ = illumination::position_angle(sun_eq, moon_eq);
  ASSERT_NEAR(χ.deg(), 285.0, 0.05);
}

TEST(Illumination, Example48aPositionAngleEndToEnd) {
  // The same instant through the library's own positions (VSOP87D + truncated ELP2000-82B).
  // The book rounds the result to 285°.0; the tolerance keeps the print-digit anchor
  // and the model-to-model spread to pure-Python Meeus well inside.
  ASSERT_NEAR(illumination::position_angle(2448724.5).deg(), 285.0442, 0.001);
}

TEST(Illumination, ConjunctionReadsZero) {
  // Conjunction invariant: identical longitudes/latitudes give ψ = 0; with Δ ≪ R the
  // phase angle reads 180° and k = 0 — finite, never NaN. β is tuned so cos_ψ lands an
  // ulp above 1: the clamp's corner is exercised (without the clamp this test reads NaN).
  const astro::toolbox::SphericalCoordinate sun_pos {
    .λ = astro::toolbox::AngleDeg { 100.0 },
    .β = astro::toolbox::AngleDeg { 0.100008 },
    .r = astro::toolbox::DistanceAu { 1.0 },
  };
  const astro::toolbox::SphericalCoordinate moon_pos {
    .λ = astro::toolbox::AngleDeg { 100.0 },
    .β = astro::toolbox::AngleDeg { 0.100008 },
    .r = astro::toolbox::DistanceAu { astro::toolbox::DistanceKm { 384400.0 } },
  };
  const auto i = illumination::phase_angle(sun_pos, moon_pos);
  ASSERT_TRUE(std::isfinite(i.rad()));
  ASSERT_NEAR(illumination::fraction(i), 0.0, 1e-15);
}


TEST(PhaseMoments, UsnoGolden2024) {
  using astro::moon_phase::phase_moments::moments;
  using astro::moon_phase::phase_moments::PhaseKind;

  // Provenance: USNO Moon Phases API, https://aa.usno.navy.mil/api/moon/phases/year?year=2024.
  // Times are UTC to the nearest minute; converted to JDE with the standard Gregorian-to-JD
  // formula. Collected 2026-08-11.
  // NOLINTBEGIN(modernize-use-designated-initializers)
  const std::vector<double> usno_new_moon_2024 {
    2460320.997917, 2460350.457639, 2460379.875000, 2460409.264583,
    2460438.640278, 2460468.026389, 2460497.456250, 2460526.967361,
    2460556.579861, 2460586.284028, 2460616.032639, 2460645.764583,
    2460675.435417,
  };
  const std::vector<double> usno_first_quarter_2024 {
    2460327.661111, 2460357.125694, 2460386.674306, 2460416.300694,
    2460445.991667, 2460475.720833, 2460505.450694, 2460535.138194,
    2460564.753472, 2460594.288194, 2460623.746528, 2460653.143056,
  };
  const std::vector<double> usno_full_moon_2024 {
    2460335.245833, 2460365.020833, 2460394.791667, 2460424.492361,
    2460454.078472, 2460483.547222, 2460512.928472, 2460542.268056,
    2460571.606944, 2460600.976389, 2460630.394444, 2460659.876389,
  };
  const std::vector<double> usno_last_quarter_2024 {
    2460313.645833, 2460343.470833, 2460373.140972, 2460402.635417,
    2460431.977083, 2460461.217361, 2460490.411806, 2460519.618750,
    2460548.893056, 2460578.284722, 2460607.835417, 2460637.561111,
    2460667.429167,
  };
  // NOLINTEND(modernize-use-designated-initializers)

  const auto check = [](const std::vector<double>& expected, const PhaseKind kind) {
    const auto actual = moments(2024, kind);
    ASSERT_EQ(expected.size(), actual.size()) << "phase kind = " << static_cast<int>(kind);
    for (std::size_t i = 0; i < expected.size(); ++i) {
      // USNO rounds to the minute; the tolerance covers that plus the ephemeris/model
      // difference between VSOP87D + truncated ELP2000-82B and USNO's underlying series.
      ASSERT_NEAR(actual[i], expected[i], 0.003) << "phase kind = " << static_cast<int>(kind) << ", index = " << i;
    }
  };

  check(usno_new_moon_2024, PhaseKind::NEW_MOON);
  check(usno_first_quarter_2024, PhaseKind::FIRST_QUARTER);
  check(usno_full_moon_2024, PhaseKind::FULL_MOON);
  check(usno_last_quarter_2024, PhaseKind::LAST_QUARTER);
}


TEST(PhaseMoments, RootGeneratorOrderAndSpacing) {
  using astro::moon_phase::phase_moments::PhaseKind;
  using astro::moon_phase::phase_moments::RootGenerator;

  const auto jde = astro::julian_day::J2000 + util::random(-200000.0, 200000.0);

  for (const auto kind : {
    PhaseKind::NEW_MOON,
    PhaseKind::FIRST_QUARTER,
    PhaseKind::FULL_MOON,
    PhaseKind::LAST_QUARTER,
  }) {
    RootGenerator gen(jde, kind);
    double prev = gen.next();
    for (int i = 0; i < 32; ++i) {
      const double cur = gen.next();
      ASSERT_GT(cur, prev) << "kind = " << static_cast<int>(kind);
      ASSERT_NEAR(cur - prev, 29.5, 0.75) << "kind = " << static_cast<int>(kind);
      prev = cur;
    }
  }
}


TEST(PhaseMoments, InvalidArgument) {
  using astro::moon_phase::phase_moments::moments;
  using astro::moon_phase::phase_moments::newton_method;
  using astro::moon_phase::phase_moments::next_root;
  using astro::moon_phase::phase_moments::PhaseKind;

  const auto roots = moments(2024, PhaseKind::NEW_MOON);
  const double root = roots.front();

  // Both endpoints lead the target: not a bracket at all.
  ASSERT_THROW(std::ignore = newton_method(root + 3.0, root + 4.0, 0.0), std::invalid_argument);

  // Left endpoint trails, but the right endpoint has left the tolerance window.
  ASSERT_THROW(std::ignore = newton_method(root - 0.5, root + 2.0, 0.0), std::invalid_argument);

  // next_root insists its seed is a root.
  ASSERT_THROW(std::ignore = next_root(root + 10.0, 0.0), std::invalid_argument);
}

} // namespace astro::moon_phase::test
