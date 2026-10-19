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

#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "event_search.hpp"

namespace astro::detail::event_search::test {

// Uniform-bound adapters for analytic tests; production selects allowances per cell or window.

/**
 * @brief Find isolated scalar roots using whole canonical cells and half-open reported-time ownership.
 * @note Derivative bounds and provider allowances cover the grid's closed domain, including its final boundary.
 *       Unresolved regions touching the query throw; no partial list escapes. Contact classification is separate.
 *       A nonzero reporting quantum requires nearest-even rounding and preserves canonical cell boundaries.
 */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
[[nodiscard]] inline auto roots_in_interval(
  const Provider& provider,
  const Grid grid,
  const Interval query,
  const Bounds bounds,
  Budget& budget,
  const double time_resolution = 0.0,
  const double report_quantum_tt_days = 0.0
) -> std::vector<Root> {
  validate_cell(query.start_jde_tt, query.end_jde_tt, bounds);
  if (not finite_nonnegative(time_resolution)) {
    throw std::invalid_argument { "Invalid event-search interval request" };
  }
  validate_reporting_grid(grid, report_quantum_tt_days);
  std::vector<Root> roots;
  visit_query_cells(grid, query, [&](const Interval cell) {
    const auto candidates = isolate_cell(
      provider,
      cell.start_jde_tt,
      cell.end_jde_tt,
      bounds,
      budget
    );
    append_interval_roots(
      provider,
      candidates,
      query,
      bounds,
      budget,
      time_resolution,
      roots,
      report_quantum_tt_days
    );
  });
  return roots;
}

/**
 * @brief Find longitude-level crossings on whole canonical cells with half-open reported-time ownership.
 * @note The provider covers the closed grid domain; unresolved contacts or uncertified angular lifts throw.
 *       A nonzero reporting quantum requires nearest-even rounding and preserves canonical cell boundaries.
 */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, LongitudeValue>
[[nodiscard]] inline auto longitude_roots_in_interval(
  const Provider& provider,
  const toolbox::AngleDeg target,
  const Grid grid,
  const Interval query,
  const Bounds bounds,
  Budget& budget,
  const double time_resolution = 0.0,
  const double report_quantum_tt_days = 0.0
) -> std::vector<Root> {
  validate_cell(query.start_jde_tt, query.end_jde_tt, bounds);
  const auto canonical_target = toolbox::canonical_longitude_difference(target);
  if (not finite_nonnegative(time_resolution)) {
    throw std::invalid_argument { "Invalid event-search longitude resolution" };
  }
  validate_reporting_grid(grid, report_quantum_tt_days);
  const auto lifted = [&](const double jde_tt) { return longitude_residual(provider(jde_tt), canonical_target); };
  std::vector<Root> roots;
  visit_query_cells(grid, query, [&](const Interval cell) {
    const auto candidates = isolate_longitude_cell(provider, canonical_target, cell, bounds, budget);
    append_interval_roots(
      lifted,
      candidates,
      query,
      bounds,
      budget,
      time_resolution,
      roots,
      report_quantum_tt_days
    );
  });
  return roots;
}

/** @brief Resolve one exact-level numerical contact; a nonmatching level remains an ordinary crossing query. */
template <typename LongitudeProvider, typename RateProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, LongitudeValue>
     and std::invocable<const RateProvider&, double>
     and std::same_as<std::invoke_result_t<const RateProvider&, double>, Value>
[[nodiscard]] inline auto numerical_contact(
  const LongitudeProvider& longitude,
  const RateProvider& rate,
  const Candidate& candidate,
  const Interval cell,
  const toolbox::AngleDeg target,
  const TrajectoryBounds bounds,
  Budget& budget,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish refinement accuracy from reporting precision.
  const double time_resolution,
  const double report_quantum_tt_days
) -> std::optional<NumericalContact> {
  const auto outcome = certify_contact(
    longitude, rate, longitude, rate, candidate, cell, cell, target, bounds, bounds, budget,
    time_resolution, report_quantum_tt_days
  );
  if (not outcome.certified) {
    throw std::runtime_error { "Event-search numerical contact has no certified two-sided extremum region" };
  }
  return outcome.contact;
}

/**
 * @brief Find longitude crossings and exact canonical-station contacts, or fail without a partial list.
 * @note Providers bound one smooth longitude and its D_h on the closed grid domain. Reporting is query-independent.
 *       A numerical contact replaces the unresolved level near one strict extremum, not an angular proximity band.
 */
template <typename LongitudeProvider, typename RateProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, LongitudeValue>
     and std::invocable<const RateProvider&, double>
     and std::same_as<std::invoke_result_t<const RateProvider&, double>, Value>
[[nodiscard]] inline auto longitude_contacts_in_interval(
  const LongitudeProvider& longitude,
  const RateProvider& rate,
  const toolbox::AngleDeg target,
  const Grid grid,
  const Interval query,
  const TrajectoryBounds bounds,
  Budget& budget,
  const double time_resolution,
  const double report_quantum_tt_days = 0.0
) -> std::vector<LongitudeRoot> {
  validate_cell(query.start_jde_tt, query.end_jde_tt, bounds.longitude);
  validate_cell(query.start_jde_tt, query.end_jde_tt, bounds.rate);
  const bool invalid_refinement = not finite_nonnegative(time_resolution)
                                  or not finite_nonnegative(bounds.third_derivative)
                                  or not finite_nonnegative(bounds.rate_remainder);
  if (invalid_refinement) {
    throw std::invalid_argument { "Invalid event-search contact bounds or refinement" };
  }
  const auto canonical_target = toolbox::canonical_longitude_difference(target);
  validate_reporting_grid(grid, report_quantum_tt_days);
  const auto contacts = [&](const Interval cell, const Interval window, const bool visited) {
    return cell_contacts(
      longitude, rate, longitude, rate, canonical_target, cell, window, bounds, bounds, budget,
      time_resolution, report_quantum_tt_days, visited
    );
  };
  const auto crossings = [&](const Interval cell, const std::span<const Interval> exclusions) {
    return cell_crossings(
      longitude, canonical_target, cell, exclusions, query, bounds.longitude, budget,
      time_resolution, report_quantum_tt_days
    );
  };
  return contact_traversal(grid, query, contacts, crossings);
}

} // namespace astro::detail::event_search::test
