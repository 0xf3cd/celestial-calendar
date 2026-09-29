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

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <tuple>

#include <gtest/gtest.h>

#include "lunar_node.hpp"

// Retained material boundaries: V44 identifies AA+ v2.63 mean/five-term rows and V20 the
// SOFA 2023-10-11 IAU 1980 correction. They remain under their source terms and outside
// the project MIT grant.

namespace astro::lunar_node::test {

namespace {

struct GoldenRow {
  double jde_tt;
  double mean_ascending_deg;
  double true_ascending_deg;
};

// AA+ v2.63 at SwiftAA commit e70ab8d0a9a5c949eef00071bacfe3ca5731300b, followed by
// canonical SOFA 2023-10-11 `iauNut80`. Corrected PyMeeus commit
// c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92 independently corroborates the raw node columns.
// See `src/test/provenance/lunar-node/LUNAR_NODE_ORACLE_ATTRIBUTION.md`.
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
constexpr std::array GOLDEN_ROWS {
  // JDE(TT)     Mean ascending (deg)  True ascending (deg)
  GoldenRow { 2409542.5,       189.238772146437,       189.224926163870 },
  GoldenRow { 2415020.5,       259.161274008062,       260.289251782040 },
  GoldenRow { 2433282.5,        12.112292826271,        12.497864705567 },
  GoldenRow { 2448724.5,       274.405266047272,       273.742138387209 },
  GoldenRow { 2451545.0,       125.040680293008,       123.922303761409 },
  GoldenRow { 2453905.5,         0.047167256868,         0.145769436887 },
  GoldenRow { 2453906.5,       359.994197269030,         0.141973623266 },
  GoldenRow { 2453908.5,       359.888292653559,         0.048277346586 },
  GoldenRow { 2453909.5,       359.835364302452,       359.936167113757 },
  GoldenRow { 2469807.5,       237.981135450882,       239.398781865401 },
  GoldenRow { 2488068.5,       350.990678358943,       349.933574327632 },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

inline constexpr double IMPLEMENTATION_EQUIVALENCE_TOLERANCE_DEG = 1e-9;
inline constexpr std::array ALL_NODES {
  Node::MEAN_ASCENDING,
  Node::MEAN_DESCENDING,
  Node::TRUE_ASCENDING,
  Node::TRUE_DESCENDING,
};

static_assert(std::same_as<std::underlying_type_t<Node>, uint8_t>);
static_assert(std::same_as<decltype(position(Node::MEAN_ASCENDING, 2451545.0)), astro::toolbox::AngleDeg>);

} // namespace

TEST(LunarNode, AscendingGolden) {
  // This threshold tests same-definition implementation equivalence, not physical accuracy.
  for (const auto& row : GOLDEN_ROWS) {
    ASSERT_NEAR(
      position(Node::MEAN_ASCENDING, row.jde_tt).deg(),
      row.mean_ascending_deg,
      IMPLEMENTATION_EQUIVALENCE_TOLERANCE_DEG
    );
    ASSERT_NEAR(
      position(Node::TRUE_ASCENDING, row.jde_tt).deg(),
      row.true_ascending_deg,
      IMPLEMENTATION_EQUIVALENCE_TOLERANCE_DEG
    );
  }
}

TEST(LunarNode, DomainBoundaries) {
  constexpr double lower_jde_tt = 2409542.5;
  constexpr double upper_jde_tt = 2488069.5;
  constexpr double final_whole_day_jde_tt = 2488068.5;
  const double below_lower = std::nextafter(lower_jde_tt, -std::numeric_limits<double>::infinity());
  const double above_lower = std::nextafter(lower_jde_tt, upper_jde_tt);
  const double below_upper = std::nextafter(upper_jde_tt, lower_jde_tt);
  const double above_upper = std::nextafter(upper_jde_tt, std::numeric_limits<double>::infinity());

  for (const Node node : ALL_NODES) {
    EXPECT_NO_THROW(std::ignore = position(node, lower_jde_tt));
    EXPECT_THROW(std::ignore = position(node, below_lower), std::invalid_argument);
    EXPECT_NO_THROW(std::ignore = position(node, above_lower));
    EXPECT_NO_THROW(std::ignore = position(node, below_upper));
    EXPECT_THROW(std::ignore = position(node, upper_jde_tt), std::invalid_argument);
    EXPECT_THROW(std::ignore = position(node, above_upper), std::invalid_argument);
    EXPECT_NO_THROW(std::ignore = position(node, final_whole_day_jde_tt));
  }
}

TEST(LunarNode, NonFiniteInput) {
  for (const Node node : ALL_NODES) {
    EXPECT_THROW(std::ignore = position(node, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(std::ignore = position(node, std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(std::ignore = position(node, -std::numeric_limits<double>::infinity()), std::invalid_argument);
  }
}

TEST(LunarNode, InvalidEnumerator) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) - Exercises the invalid-enumerator contract.
  const auto invalid_node = static_cast<Node>(255);
  EXPECT_THROW(std::ignore = position(invalid_node, 2451545.0), std::invalid_argument);
}

TEST(LunarNode, AscendingWrapPairs) {
  EXPECT_LT(position(Node::MEAN_ASCENDING, 2453905.5).deg(), 1.0);
  EXPECT_GT(position(Node::MEAN_ASCENDING, 2453906.5).deg(), 359.0);

  EXPECT_LT(position(Node::TRUE_ASCENDING, 2453908.5).deg(), 1.0);
  EXPECT_GT(position(Node::TRUE_ASCENDING, 2453909.5).deg(), 359.0);
}

TEST(LunarNode, DescendingAntipodes) {
  for (const auto& row : GOLDEN_ROWS) {
    const auto mean_ascending = position(Node::MEAN_ASCENDING, row.jde_tt);
    const auto true_ascending = position(Node::TRUE_ASCENDING, row.jde_tt);
    EXPECT_DOUBLE_EQ(
      position(Node::MEAN_DESCENDING, row.jde_tt).deg(),
      (mean_ascending + astro::toolbox::AngleDeg { 180.0 }).normalize().deg()
    );
    EXPECT_DOUBLE_EQ(
      position(Node::TRUE_DESCENDING, row.jde_tt).deg(),
      (true_ascending + astro::toolbox::AngleDeg { 180.0 }).normalize().deg()
    );
  }
}

} // namespace astro::lunar_node::test
