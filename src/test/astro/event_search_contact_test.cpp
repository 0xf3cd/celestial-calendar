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
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ephemeris_events.hpp"
#include "event_search_test_support.hpp"

namespace astro::ephemeris::test {
namespace {

namespace search = detail::event_search;
namespace range_test = astro::detail::event_search::test;

constexpr double QUANTUM = 0x1p-30;
constexpr double RESOLUTION = 0x1p-10;
constexpr search::Grid GRID {
  .domain = { .start_jde_tt = 0.0, .end_jde_tt = 1.0 }, .cell_width_tt_days = 0.25,
};
constexpr search::TrajectoryBounds BOUNDS {
  .longitude = { .first_derivative = 2.0, .second_derivative = 2.0 },
  .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
  .third_derivative = 0.0,
  .rate_remainder = 0.0,
};

/** @brief Use known quadratic/linear references; the vertex's exact rate zero needs no rounding allowance. */
auto quadratic(const double vertex, const double level = 0.0) {
  const auto longitude = [vertex, level](const double time) -> search::LongitudeValue {
    const double x = time - vertex;
    return { .longitude = toolbox::AngleDeg { (x * x) + level }, .error_deg = 1e-15 };
  };
  const auto rate = [vertex](const double time) -> search::Value {
    return { .value = 2.0 * (time - vertex), .error = time == vertex ? 0.0 : 1e-15 };
  };
  return std::pair { longitude, rate };
}

/** @brief Give each synthetic query its own bounded traversal. */
auto quadratic_contacts(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): The vertex is a time; the target is a longitude.
  const double vertex,
  const double target,
  const search::Interval query
) -> std::vector<search::LongitudeRoot> {
  const auto [longitude, rate] = quadratic(vertex);
  search::Budget budget { .evaluations_remaining = 10000, .candidates_remaining = 1000 };
  return range_test::longitude_contacts_in_interval(
    longitude, rate, toolbox::AngleDeg { target }, GRID, query, BOUNDS, budget, RESOLUTION, QUANTUM
  );
}

/** @brief Distinguish a query's intended ambiguity failure from unrelated budget or provider errors. */
auto quadratic_failure(const double vertex, const double target, const search::Interval query) -> std::string {
  try {
    static_cast<void>(quadratic_contacts(vertex, target, query));
  } catch (const std::runtime_error& error) {
    return error.what();
  }
  return "no exception";
}

} // namespace

TEST(EventSearchContacts, InteriorSeamQuadraticIsOneTangent) {
  const auto found = quadratic_contacts(0.25, 0.0, GRID.domain);
  ASSERT_EQ(found.size(), 1U);
  ASSERT_EQ(found.front().jde_tt, 0.25);
  ASSERT_EQ(found.front().direction, search::ContactDirection::TANGENT);
}

TEST(EventSearchContacts, SplitsBeforeAtAndAfterTheSeamOwnOnce) {
  for (const auto query : std::array {
         search::Interval { .start_jde_tt = 0.0, .end_jde_tt = 0.25 },
         search::Interval { .start_jde_tt = 0.0, .end_jde_tt = 0.125 },
         search::Interval { .start_jde_tt = 0.25 + QUANTUM, .end_jde_tt = 1.0 },
         search::Interval { .start_jde_tt = 0.5, .end_jde_tt = 1.0 },
       }) {
    ASSERT_TRUE(quadratic_contacts(0.25, 0.0, query).empty());
  }
  for (const auto query : std::array {
         search::Interval { .start_jde_tt = 0.25, .end_jde_tt = 1.0 },
         search::Interval { .start_jde_tt = 0.25, .end_jde_tt = 0.25 + QUANTUM },
         search::Interval { .start_jde_tt = 0.125, .end_jde_tt = 0.375 },
         search::Interval { .start_jde_tt = 0.0, .end_jde_tt = 0.5 },
       }) {
    const auto found = quadratic_contacts(0.25, 0.0, query);
    ASSERT_EQ(found.size(), 1U);
    ASSERT_EQ(found.front().jde_tt, 0.25);
    ASSERT_EQ(found.front().direction, search::ContactDirection::TANGENT);
  }
}

TEST(EventSearchContacts, DifferenceProviderUsesTheSameContactKernel) {
  const auto [first, first_rate] = quadratic(0.5, 10.0);
  const auto second = [](const double) -> search::LongitudeValue {
    return { .longitude = toolbox::AngleDeg { 10.0 }, .error_deg = 0.0 };
  };
  const auto longitude = [&](const double time) -> search::LongitudeValue {
    const auto difference = search::project_longitude_difference(first(time), second(time));
    return { .longitude = toolbox::AngleDeg { difference.value }, .error_deg = difference.error };
  };
  search::Budget budget { .evaluations_remaining = 10000, .candidates_remaining = 1000 };
  const auto found = range_test::longitude_contacts_in_interval(
    longitude, first_rate, toolbox::AngleDeg { 0.0 }, GRID, GRID.domain, BOUNDS, budget, RESOLUTION, QUANTUM
  );
  ASSERT_EQ(found.size(), 1U);
  ASSERT_EQ(found.front().jde_tt, 0.5);
  ASSERT_EQ(found.front().direction, search::ContactDirection::TANGENT);
}

TEST(EventSearchContacts, NearbyUnresolvedLevelsStillFail) {
  for (const double target : std::array { -1e-16, 1e-16 }) {
    ASSERT_EQ(quadratic_failure(0.25, target, GRID.domain), "Unresolved event-search region intersects the query");
  }
}

TEST(EventSearchContacts, ATrueDomainEndpointStillNeedsTwoSidedEvidence) {
  for (const auto query : std::array { GRID.domain, search::Interval { .start_jde_tt = 0.0, .end_jde_tt = 0.125 } }) {
    ASSERT_EQ(
      quadratic_failure(0.0, 0.0, query),
      "Event-search numerical contact has no certified two-sided extremum region"
    );
  }
}

TEST(EventSearchContacts, AnUncertifiedLookaheadEndpointDoesNotInvalidateDistantCells) {
  ASSERT_TRUE(quadratic_contacts(0.0, 0.0, { .start_jde_tt = 0.5, .end_jde_tt = 0.75 }).empty());
}

TEST(EventSearchContacts, AnInteriorRepresentativeAndRegionStayUnchanged) {
  const auto found = quadratic_contacts(0.375, 0.0, GRID.domain);
  ASSERT_EQ(found.size(), 1U);
  ASSERT_EQ(found.front().jde_tt, 0.375);
  ASSERT_EQ(found.front().direction, search::ContactDirection::TANGENT);
  const auto [longitude, rate] = quadratic(0.375);
  search::Budget budget { .evaluations_remaining = 10000, .candidates_remaining = 1000 };
  const auto stations = search::isolate_cell(rate, 0.25, 0.5, BOUNDS.rate, budget);
  ASSERT_EQ(stations.size(), 1U);
  const auto contact = range_test::numerical_contact(
    longitude, rate, stations.front(), { .start_jde_tt = 0.25, .end_jde_tt = 0.5 }, toolbox::AngleDeg { 0.0 }, BOUNDS,
    budget, RESOLUTION, QUANTUM
  );
  if (contact.has_value()) {
    ASSERT_GE(contact->region.start_jde_tt, 0.25);
    ASSERT_LE(contact->region.end_jde_tt, 0.5);
    const auto refined = search::refine(rate, stations.front(), BOUNDS.rate, budget, RESOLUTION);
    ASSERT_EQ(contact->root.jde_tt, search::reported_time(refined.jde_tt, QUANTUM));
  } else {
    FAIL() << "The in-cell contact lost its certificate";
  }
}

TEST(EventSearchContacts, OrdinaryCrossingsBesideTheSeamAreKept) {
  const auto found = quadratic_contacts(0.25, 1.0 / 64.0, GRID.domain);
  ASSERT_EQ(found.size(), 2U);
  ASSERT_NEAR(found.front().jde_tt, 0.125, RESOLUTION);
  ASSERT_NEAR(found.back().jde_tt, 0.375, RESOLUTION);
}

TEST(EventSearchContacts, ANeighborContactExclusionKeepsTheSeparateCrossing) {
  // g=(t-1/4)^2(t-3/8) on [0,1]: |g'|<=3/2, |g''|<=17/4, g'''=6.
  const auto longitude = [](const double time) -> search::LongitudeValue {
    const double x = time - 0.25;
    const bool exact_zero = time == 0.25 or time == 0.375;
    return {
      .longitude = toolbox::AngleDeg { x * x * (time - 0.375) }, .error_deg = exact_zero ? 0.0 : 1e-14,
    };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = (time - 0.25) * ((3.0 * time) - 1.0), .error = time == 0.25 ? 0.0 : 1e-14 };
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 1.5, .second_derivative = 4.25 },
    .rate = { .first_derivative = 4.25, .second_derivative = 6.0 },
    .third_derivative = 6.0,
    .rate_remainder = 0.0,
  };
  search::Budget budget { .evaluations_remaining = 10000, .candidates_remaining = 1000 };
  const auto found = range_test::longitude_contacts_in_interval(
    longitude, rate, toolbox::AngleDeg { 0.0 }, GRID, GRID.domain, bounds, budget, RESOLUTION, QUANTUM
  );
  ASSERT_EQ(found.size(), 2U);
  ASSERT_EQ(found.front().jde_tt, 0.25);
  ASSERT_EQ(found.front().direction, search::ContactDirection::TANGENT);
  ASSERT_NEAR(found.back().jde_tt, 0.375, RESOLUTION);
  ASSERT_EQ(found.back().direction, search::ContactDirection::INCREASING);
}

TEST(EventSearchContacts, FixedWindowsCrossingEitherCenturyEdgeUseWideBounds) {
  const double halo = detail::DIFFERENCE_REACH_TT_DAYS + detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS;
  const std::array edges { detail::EVENT_CENTURY_START_JDE_TT + halo, detail::EVENT_CENTURY_END_JDE_TT - halo };
  for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
    const double edge = edges.at(edge_index);
    const double start = edge_index == 0 ? std::ceil(edge / 0.25) * 0.25 : (std::floor(edge / 0.25) * 0.25) - 0.25;
    const search::Grid grid {
      .domain = { .start_jde_tt = start - 4.0, .end_jde_tt = start + 4.0 }, .cell_width_tt_days = 0.25,
    };
    const uint32_t count = search::grid_cell_count(grid);
    const uint32_t index = search::containing_cell(grid, count, start);
    const auto window = search::contact_window(grid, index, count);
    const search::Interval cell { .start_jde_tt = start, .end_jde_tt = start + 0.25 };
    const auto cell_bounds = detail::event_bounds(Target::MARS, cell);
    const auto window_bounds = detail::event_bounds(Target::MARS, window);
    const auto& wide = detail::EVENT_BOUNDS_WIDE.at(static_cast<std::size_t>(Target::MARS));
    const auto& century = detail::EVENT_BOUNDS_CENTURY.at(static_cast<std::size_t>(Target::MARS));
    ASSERT_EQ(cell_bounds.position_error_deg, century.position_error_deg);
    ASSERT_EQ(window_bounds.position_error_deg, wide.position_error_deg);
    ASSERT_EQ(window_bounds.trajectory.third_derivative, wide.trajectory.third_derivative);
  }
}

TEST(EventSearchContacts, OverlappingNeighborExclusionsCannotEraseADistinctRoot) {
  const auto contacts = [](const search::Interval cell, const search::Interval, const bool) {
    std::vector<search::NumericalContact> found;
    if (cell.start_jde_tt == 0.0 or cell.start_jde_tt == 0.25) {
      found.push_back({
        .root = { .jde_tt = cell.end_jde_tt, .direction = search::ContactDirection::TANGENT },
        .region = { .start_jde_tt = cell.start_jde_tt + 0.125, .end_jde_tt = cell.end_jde_tt + 0.375 },
      });
    }
    return found;
  };
  const auto crossings = [](const search::Interval, const std::span<const search::Interval>) {
    return std::vector<search::Root> {};
  };
  try {
    static_cast<void>(search::contact_traversal(GRID, GRID.domain, contacts, crossings));
    FAIL() << "Overlapping contact regions were accepted";
  } catch (const std::runtime_error& error) {
    ASSERT_EQ(std::string { error.what() }, "Event-search numerical contact brackets overlap");
  }
}

TEST(EventSearchContacts, DistinctContactsCannotAliasAtTheReportedTime) {
  const auto contacts = [](const search::Interval cell, const search::Interval, const bool) {
    return std::vector<search::NumericalContact> {
      {
        .root = { .jde_tt = 0.5, .direction = search::ContactDirection::TANGENT },
        .region = { .start_jde_tt = cell.start_jde_tt + 0.0625, .end_jde_tt = cell.end_jde_tt - 0.0625 },
      },
    };
  };
  const auto crossings = [](const search::Interval, const std::span<const search::Interval>) {
    return std::vector<search::Root> {};
  };
  try {
    static_cast<void>(search::contact_traversal(GRID, GRID.domain, contacts, crossings));
    FAIL() << "Aliased contact times were accepted";
  } catch (const std::runtime_error& error) {
    ASSERT_EQ(
      std::string { error.what() }, "Event-search contacts cannot be separated at the reported time precision"
    );
  }
}

TEST(EventSearchContacts, LookaheadProviderFailuresAreNeverSwallowed) {
  const auto [longitude, exact_rate] = quadratic(0.25);
  const auto rate = [&](const double time) -> search::Value {
    if (time == 0.75) {
      throw std::runtime_error { "Lookahead provider failure" };
    }
    return exact_rate(time);
  };
  search::Budget budget { .evaluations_remaining = 10000, .candidates_remaining = 1000 };
  try {
    static_cast<void>(range_test::longitude_contacts_in_interval(
      longitude, rate, toolbox::AngleDeg { 0.0 }, GRID, { .start_jde_tt = 0.0, .end_jde_tt = 0.125 },
      BOUNDS, budget, RESOLUTION, QUANTUM
    ));
    FAIL() << "The lookahead provider failure was swallowed";
  } catch (const std::runtime_error& error) {
    ASSERT_EQ(std::string { error.what() }, "Lookahead provider failure");
  }
}

} // namespace astro::ephemeris::test
