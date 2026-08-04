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

// TRANSITIONAL -- this file is deleted later in the same pull request that adds it (#81).
//
// The golden datasets compare with tolerances, so a one-ulp divergence would not turn CI red: they
// have no discriminating power for "the numbers did not move". This file supplies that power for
// the one commit range where it is meaningful. It holds a verbatim copy of the pre-#81 `evaluate`
// and asserts bit-equality against the current one, so both sides run on the same libm on whichever
// leg CI happens to be -- no golden file, nothing that can drift out from under it.
//
// Once the migration is merged the old formula is gone, and a test comparing against a private copy
// of it would only pin that copy in place forever. So it goes; the evidence lives in the pull
// request and the green run for each leg.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <format>
#include <ranges>
#include <vector>
#include <cstddef>
#include <numeric>
#include <cstdint>

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


/** @brief Julian centuries spanning roughly year 0 to year 4000, either side of the library's range. */
[[nodiscard]] auto sample_jcs() -> std::vector<double> {
  constexpr std::size_t COUNT = 4096;
  std::vector<double> jcs;
  jcs.reserve(COUNT);
  for (std::size_t i = 0; i < COUNT; ++i) {
    jcs.push_back(-20.0 + (40.0 * static_cast<double>(i)) / static_cast<double>(COUNT));
  }
  return jcs;
}


TEST(Elp2000Migration, EvaluateIsBitIdenticalToPre81) {
  std::size_t mismatches = 0;
  for (const double jc : sample_jcs()) {
    const auto before = evaluate_pre_81(jc);
    const auto after  = evaluate(jc);

    // Exact comparison on purpose -- see the file header. `EXPECT_DOUBLE_EQ` allows 4 ULPs.
    if (before.Σl != after.Σl or before.Σb != after.Σb or before.Σr != after.Σr) {
      if (mismatches < 5) { // Report the first few, then just count.
        ADD_FAILURE() << std::format(
          "jc = {}\n  Σl {:a} vs {:a}\n  Σb {:a} vs {:a}\n  Σr {:a} vs {:a}",
          jc, before.Σl, after.Σl, before.Σb, after.Σb, before.Σr, after.Σr);
      }
      ++mismatches;
    }
  }
  EXPECT_EQ(0U, mismatches) << "out of " << sample_jcs().size() << " sampled julian centuries";
}


TEST(Elp2000Migration, TheComparisonCanActuallyFail) {
  // Without this, a comparison that silently degraded to "always equal" would still pass above.
  const auto before = evaluate_pre_81(0.5);
  auto nudged = before;
  nudged.Σl = std::nextafter(nudged.Σl, 1e300);
  EXPECT_NE(before.Σl, nudged.Σl);
}

} // namespace astro::elp2000_82b::migration_test
