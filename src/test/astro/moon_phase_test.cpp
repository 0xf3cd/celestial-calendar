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
  // print digits; measured gaps recomputing from the rounded inputs: i 3.8e-5 deg, k 3.3e-5.
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

TEST(Illumination, PymeeusCrossDataset) {
  // Provenance: pymeeus `Moon.illuminated_fraction_disk` at 60 seeded epochs (uniform year in
  // [1900, 2100], uniform day-of-year; seed 42), generated 2026-08-10 — regenerable with
  // pymeeus at the same seed. pymeeus implements the approximate (48.4) path, so the gap vs
  // the exact (48.2)+(48.3) path is formula-level, not transcription-level (the book's own
  // Example 48.a shows (48.4) landing 1.6e-3 off the exact path). Measured max gap over the
  // dataset: 2.26e-3; tolerance 3e-3 ≈ 1.3× the measured max.
  const std::vector<std::pair<double, double>> dataset {
    { 2415661.705882, 0.5744942573 },
    { 2419303.122370, 0.0115935997 },
    { 2420940.297755, 0.9602622885 },
    { 2423500.084223, 0.1403494467 },
    { 2423695.604705, 0.5148993205 },
    { 2424762.818613, 0.0747153135 },
    { 2425302.134240, 0.2263787042 },
    { 2427988.967571, 0.1933014889 },
    { 2429883.622740, 0.7374126951 },
    { 2429917.816889, 0.9907848617 },
    { 2430130.245872, 0.7362033238 },
    { 2434622.443763, 0.3445383105 },
    { 2435456.925558, 0.0114056616 },
    { 2435722.699587, 0.0275041743 },
    { 2435890.291806, 0.5753612055 },
    { 2436503.636144, 0.9976968016 },
    { 2436884.900055, 0.9191876171 },
    { 2439542.325436, 0.8056783514 },
    { 2439763.413027, 0.2047232637 },
    { 2442407.120634, 0.8472281484 },
    { 2442715.403298, 0.2826306086 },
    { 2444316.896062, 0.0401455011 },
    { 2444385.523982, 0.8606832238 },
    { 2446468.704541, 0.0422868571 },
    { 2446897.645168, 0.9562060720 },
    { 2447381.256275, 0.2045835863 },
    { 2448832.656181, 0.0065961828 },
    { 2450112.183272, 0.7112773044 },
    { 2450484.704761, 0.0860882650 },
    { 2450550.682392, 0.2589244546 },
    { 2450953.409420, 0.4211645692 },
    { 2452407.266706, 0.0010118690 },
    { 2454181.740386, 0.1414635124 },
    { 2454478.068895, 0.1711910211 },
    { 2454683.592887, 0.1595332936 },
    { 2455159.307530, 0.3984987257 },
    { 2458257.621781, 0.1761725939 },
    { 2458677.097622, 0.8186877049 },
    { 2461550.510012, 0.8160821436 },
    { 2462356.634817, 0.0461960237 },
    { 2465104.936723, 0.0004837875 },
    { 2465821.145735, 0.5425807506 },
    { 2467322.876905, 0.1237975152 },
    { 2468300.497816, 0.4405895754 },
    { 2469908.761418, 0.7643007247 },
    { 2473051.804436, 0.1229509146 },
    { 2474252.794468, 0.3943262704 },
    { 2474494.116195, 0.0055678367 },
    { 2474596.024509, 0.8928529375 },
    { 2474805.990953, 0.9963444502 },
    { 2475087.522074, 0.0136169798 },
    { 2475312.490513, 0.7566098284 },
    { 2475906.782290, 0.3436284625 },
    { 2476830.454972, 0.0543428978 },
    { 2480740.442270, 0.9927113619 },
    { 2480789.818074, 0.1611430406 },
    { 2483045.611177, 0.9936233548 },
    { 2484152.610672, 0.0003388020 },
    { 2484621.838001, 0.0984980504 },
    { 2486890.963968, 0.4174081992 },
  };

  for (const auto& [jde, expected] : dataset) {
    ASSERT_NEAR(illumination::fraction(jde), expected, 3e-3);
  }
}

TEST(Illumination, HorizonsGoldenDataset) {
  // Provenance: JPL Horizons, Moon (301), observer quantity 10 (Illu%), geocenter 500@399,
  // TT scale, DE441 (gated at collection: the response must name {source: DE441}); 31 epochs
  // = 30 seeded (uniform in [1900, 2053], seed 42) + the Example 48.a anchor, collected
  // 2026-08-10. An independent ephemeris axis vs the pymeeus layer: DE441 positions instead
  // of a formula re-implementation. Measured max gap 2.96e-5 (anchor 2.09e-5) — the VSOP87D
  // + truncated-ELP position gap to DE441 in fraction units; tolerance 1e-4 ≈ 3.4× the
  // measured max. (Collection note: Horizons sorts TLIST internally — pair rows with epochs
  // by the date column or send the list sorted, never by input order.)
  const std::vector<std::pair<double, double>> dataset {
    { 2415385.489840, 0.8103975 },
    { 2416425.179046, 0.9731384 },
    { 2416510.839665, 0.9706886 },
    { 2416694.001235, 0.7827412 },
    { 2419903.245657, 0.7499212 },
    { 2420229.384802, 0.8136035 },
    { 2420452.381872, 0.0499964 },
    { 2423752.695148, 0.7254120 },
    { 2426187.818976, 0.2671733 },
    { 2427299.864579, 0.2969376 },
    { 2427401.106656, 0.5326651 },
    { 2427556.684687, 0.9966361 },
    { 2430466.971608, 0.0349559 },
    { 2433924.659437, 0.1972981 },
    { 2434129.989759, 0.1092300 },
    { 2438716.895159, 0.9288772 },
    { 2443402.769046, 0.1019231 },
    { 2445626.048375, 0.8909732 },
    { 2448115.428604, 0.6945596 },
    { 2448724.500000, 0.6785466 },
    { 2448927.565100, 0.3761539 },
    { 2450932.627282, 0.1011004 },
    { 2451519.959679, 0.0034017 },
    { 2453025.973312, 0.0039855 },
    { 2454230.102841, 0.5643179 },
    { 2456382.932801, 0.7987653 },
    { 2460277.726641, 0.9606601 },
    { 2460480.542738, 0.9018989 },
    { 2462618.326097, 0.2913258 },
    { 2465127.981061, 0.3368562 },
    { 2468780.457774, 0.9812148 },
  };

  for (const auto& [jde, expected] : dataset) {
    ASSERT_NEAR(illumination::fraction(jde), expected, 1e-4);
  }
}

} // namespace astro::moon_phase::test
