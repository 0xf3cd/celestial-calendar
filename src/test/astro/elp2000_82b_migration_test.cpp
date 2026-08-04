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

// TRANSITIONAL -- this file is deleted later in the same pull request that adds it (#81).
//
// The golden datasets compare with tolerances, so they cannot say whether #81's rewrite of
// `evaluate` moved a number. This file can. It keeps a verbatim copy of the pre-#81 `evaluate`,
// runs both through the same downstream arithmetic, and compares exactly -- both sides on the same
// libm, on whichever leg CI happens to be, so there is no golden file to drift.
//
// It compares `apparent`, not `evaluate`, and that is the second criterion this migration has had.
//
//   The first was "Σl, Σb and Σr are bit-identical". Apple clang / libc++ said no: 12 of 4096
//   sampled Σl values moved by up to 2 ULP, while the other three legs saw nothing at all. In
//   longitude that is 3.4e-12 arcseconds, against a truncated model whose own error is around 10
//   arcseconds. So the criterion was measuring the wrong thing: Σl is an intermediate, and no
//   caller can see it.
//
//   `apparent` is what callers do see. New moon search, lunar months, the C ABI -- all of them
//   consume λ, β and r and nothing else.
//
// This file measures; it is not the gate. The gate is the rest of the suite, and specifically the
// almanac golden tests, which compare lunar dates as integers with no tolerance anywhere and pass
// on every leg. A perturbation would have to move a new moon across midnight to reach them, and
// one ULP of λ moves it by about 0.2 nanoseconds.

#include <gtest/gtest.h>

#include <cmath>
#include <format>
#include <algorithm>
#include <ranges>
#include <vector>
#include <cstddef>
#include <numeric>

#include "moon.hpp"
#include "earth.hpp"
#include "toolbox.hpp"
#include "julian_day.hpp"
#include "elp2000_82b.hpp"

namespace astro::elp2000_82b::migration_test {

/** @brief The pre-#81 `evaluate`, copied verbatim. Three lazy views, three sums, `pow` per row. */
[[nodiscard]] auto evaluate_pre_81(const double jc) -> Evaluation {
  using namespace std::ranges;

  const auto ctx = create_context(jc);

  const auto lon_terms = coeff::LR | views::transform([&](const coeff::LRCoefficients& coeff) {
    const toolbox::AngleDeg θ {
      coeff.D  * ctx.D.deg()  +
      coeff.M  * ctx.M.deg()  +
      coeff.Mp * ctx.Mp.deg() +
      coeff.F  * ctx.F.deg()
    };

    const auto M_correction = std::pow(ctx.E, std::abs(coeff.M));
    return coeff.argL * std::sin(θ.rad()) * M_correction;
  });

  const auto rad_terms = coeff::LR | views::transform([&](const coeff::LRCoefficients& coeff) {
    const toolbox::AngleDeg θ {
      coeff.D  * ctx.D.deg()  +
      coeff.M  * ctx.M.deg()  +
      coeff.Mp * ctx.Mp.deg() +
      coeff.F  * ctx.F.deg()
    };

    const auto M_correction = std::pow(ctx.E, std::abs(coeff.M));
    return coeff.argR * std::cos(θ.rad()) * M_correction;
  });

  const auto lat_terms = coeff::B | views::transform([&](const coeff::BCoefficients& coeff) {
    const toolbox::AngleDeg θ {
      coeff.D  * ctx.D.deg()  +
      coeff.M  * ctx.M.deg()  +
      coeff.Mp * ctx.Mp.deg() +
      coeff.F  * ctx.F.deg()
    };

    const auto M_correction = std::pow(ctx.E, std::abs(coeff.M));
    return coeff.argB * std::sin(θ.rad()) * M_correction;
  });

  return {
    .Σl  = std::reduce(cbegin(lon_terms), cend(lon_terms)),
    .Σb  = std::reduce(cbegin(lat_terms), cend(lat_terms)),
    .Σr  = std::reduce(cbegin(rad_terms), cend(rad_terms)),
    .ctx = ctx
  };
}


/** @brief `moon::geocentric_coord::apparent`, copied verbatim except for which `evaluate` it calls. */
[[nodiscard]] auto apparent_pre_81(const double jde) -> toolbox::SphericalCoordinate {
  const double jc = astro::julian_day::jde_to_jc(jde);

  const auto evaluated = evaluate_pre_81(jc);

  const auto Σl = evaluated.Σl + moon::perturbation::longitude(evaluated.ctx);
  const auto lon_nutation = astro::earth::nutation::longitude(jde);
  const toolbox::AngleDeg lon =
    evaluated.ctx.Lp + toolbox::AngleDeg { Σl / LON_LAT_SCALING_FACTOR } + lon_nutation;

  const auto Σb = evaluated.Σb + moon::perturbation::latitude(evaluated.ctx);
  const toolbox::AngleDeg lat { Σb / LON_LAT_SCALING_FACTOR };

  const toolbox::DistanceKm r { 385000.56 + evaluated.Σr / RADIUS_SCALING_FACTOR };

  return {
    .λ = lon.normalize(),
    .β = lat,
    .r = toolbox::DistanceAu { r }
  };
}


/** @brief Julian ephemeris days across the supported window, roughly year 400 to year 5100. */
[[nodiscard]] auto sample_jdes() -> std::vector<double> {
  constexpr std::size_t COUNT = 20000;
  constexpr double FIRST_JDE  = 1867157.0; // Around year 400.
  constexpr double LAST_JDE   = 3582645.0; // Around year 5100.

  std::vector<double> jdes;
  jdes.reserve(COUNT);
  for (std::size_t i = 0; i < COUNT; ++i) {
    const double fraction = static_cast<double>(i) / static_cast<double>(COUNT);
    jdes.push_back(FIRST_JDE + (LAST_JDE - FIRST_JDE) * fraction);
  }
  return jdes;
}


/** @brief How far apart two doubles are, counted in representable steps. */
[[nodiscard]] auto ulps_apart(const double lhs, const double rhs) -> std::size_t {
  std::size_t steps = 0;
  double walk = lhs;
  while (walk != rhs and steps < 64) { // 64 is "far", and stops a runaway on NaN.
    walk = std::nextafter(walk, rhs);
    ++steps;
  }
  return steps;
}

} // namespace astro::elp2000_82b::migration_test


namespace {

using astro::elp2000_82b::migration_test::apparent_pre_81;
using astro::elp2000_82b::migration_test::sample_jdes;
using astro::elp2000_82b::migration_test::ulps_apart;

TEST(Elp2000Migration, ApparentPositionMovesByAtMostOneUlp) {
  const auto jdes = sample_jdes();

  std::size_t moved = 0;
  std::size_t worst = 0;
  std::size_t reported = 0;

  for (const double jde : jdes) {
    const auto before = apparent_pre_81(jde);
    const auto after  = astro::moon::geocentric_coord::apparent(jde);

    const auto steps = std::max({ ulps_apart(before.λ.deg(), after.λ.deg()),
                                  ulps_apart(before.β.deg(), after.β.deg()),
                                  ulps_apart(before.r.au(),  after.r.au()) });
    if (steps == 0) {
      continue;
    }

    ++moved;
    worst = std::max(worst, steps);

    // One ULP is the measured effect of #81 on Apple clang / libc++, and is 0.2 ns of lunar
    // motion. Anything larger is not that, and wants looking at.
    if (steps > 1 and reported < 5) {
      ADD_FAILURE() << std::format(
        "jde = {} moved by {} ULP\n  λ {:a} vs {:a}\n  β {:a} vs {:a}\n  r {:a} vs {:a}",
        jde, steps, before.λ.deg(), after.λ.deg(), before.β.deg(), after.β.deg(),
        before.r.au(), after.r.au());
      ++reported;
    }
  }

  // Printed either way: the count is the measurement this file exists to take.
  GTEST_LOG_(INFO) << std::format("{} of {} sampled moments moved; worst was {} ULP",
                                  moved, jdes.size(), worst);
  EXPECT_LE(worst, 1U);
}


TEST(Elp2000Migration, TheComparisonCanActuallyFail) {
  // Without this, a comparison that had silently degraded to "always equal" would still pass above.
  const double jde = sample_jdes().at(7777);
  const double λ = astro::moon::geocentric_coord::apparent(jde).λ.deg();

  EXPECT_EQ(0U, ulps_apart(λ, λ));
  EXPECT_EQ(1U, ulps_apart(λ, std::nextafter(λ, 1e300)));
  EXPECT_EQ(2U, ulps_apart(λ, std::nextafter(std::nextafter(λ, 1e300), 1e300)));
}

} // namespace
