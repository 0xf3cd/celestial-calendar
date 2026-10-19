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

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "ephemeris.hpp"

namespace astro::ephemeris::detail::event_search {

/** @brief A scalar observation with an absolute error allowance at the requested abscissa. */
struct Value {
  double value;
  double error;
};

/** @brief A scalar observation whose allowance applies at the requested TT time. */
struct Sample {
  double jde_tt;
  Value evaluated;
};

/** @brief Uniform absolute first- and second-derivative bounds on a closed cell. */
struct Bounds {
  double first_derivative;
  double second_derivative;
};

/** @brief Shared limits on scalar-evaluation attempts and candidate slots. */
struct Budget {
  std::size_t evaluations_remaining;
  std::size_t candidates_remaining;
};

/** @brief Whether a retained bracket has a unique-root certificate or remains unresolved. */
enum class Isolation : uint8_t { UNIQUE, UNRESOLVED };

/** @brief Exact endpoint roots belong to [left,right) for LEFT, or [left,right] for BOTH. */
enum class EndpointOwnership : uint8_t { LEFT, BOTH };

/** @brief A sufficient cell verdict: exclude, certify one root, or subdivide. */
enum class CellDecision : uint8_t { DISCARD, UNIQUE, SPLIT };

/** @brief A sampled bracket with explicit ownership of exact endpoint roots. */
struct Cell {
  Sample left;
  Sample right;
  EndpointOwnership ownership;
};

/** @brief A retained bracket, either certified to contain one root or left unresolved. */
struct Candidate {
  Sample left;
  Sample right;
  Isolation isolation;
};

/** @brief A reported TT root time and its certified local crossing direction. */
struct Root {
  double jde_tt;
  bool increasing;
};

/** @brief A nonempty half-open interval in JDE(TT). */
struct Interval {
  double start_jde_tt;
  double end_jde_tt;
};

/** @brief A query-independent cell partition; the provider must cover its closed domain. */
struct Grid {
  Interval domain;
  double cell_width_tt_days;
};

/** @brief Enclose a nearest-rounded result on its lower side. */
[[nodiscard]] inline auto lower(const double value) -> double {
  return std::nextafter(value, -std::numeric_limits<double>::infinity());
}

/** @brief Enclose a nearest-rounded result on its upper side. */
[[nodiscard]] inline auto upper(const double value) -> double {
  return std::nextafter(value, std::numeric_limits<double>::infinity());
}

/** @brief Test whether a scalar is finite and nonnegative. */
[[nodiscard]] inline auto finite_nonnegative(const double value) -> bool {
  return std::isfinite(value) and value >= 0.0;
}

/** @brief Bound an observation's absolute magnitude from below; the bound may be negative. */
[[nodiscard]] inline auto magnitude_floor(const Value evaluated) -> double {
  return lower(std::fabs(evaluated.value) - evaluated.error);
}

/** @brief Recognize a certified zero, not merely a zero-valued observation. */
[[nodiscard]] inline auto exact_zero(const Value evaluated) -> bool {
  return evaluated.value == 0.0 and evaluated.error == 0.0;
}

/** @brief Return a robust sign, or zero when the allowance includes zero. */
[[nodiscard]] inline auto sign(const Value evaluated) -> int32_t {
  if (evaluated.value > evaluated.error) {
    return 1;
  }
  return evaluated.value < -evaluated.error ? -1 : 0;
}

/** @brief Adjust a provider's error allowance when it evaluates at a different abscissa. */
[[nodiscard]] inline auto shifted(const Value evaluated, const double offset, const double first_derivative) -> Value {
  const bool invalid_allowance = not finite_nonnegative(offset) or not finite_nonnegative(first_derivative);
  if (invalid_allowance) {
    throw std::invalid_argument { "Invalid event-search abscissa allowance" };
  }
  if (offset == 0.0) {
    return evaluated;
  }
  const double shift_error = upper(first_derivative * offset);
  return {
    .value = evaluated.value,
    .error = upper(evaluated.error + shift_error),
  };
}

/** @brief Charge an evaluation even if the provider fails. */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
[[nodiscard]] inline auto sample(const Provider& provider, const double jde_tt, Budget& budget) -> Sample {
  if (budget.evaluations_remaining == 0) {
    throw std::runtime_error { "Event-search evaluation budget exhausted" };
  }
  --budget.evaluations_remaining;
  const auto evaluated = provider(jde_tt);
  const bool invalid_observation = not std::isfinite(evaluated.value) or not finite_nonnegative(evaluated.error);
  if (invalid_observation) {
    throw std::runtime_error { std::format("Invalid event-search evaluation at JDE(TT) {}", jde_tt) };
  }
  return { .jde_tt = jde_tt, .evaluated = evaluated };
}

/**
 * @brief Bound |f'| from below using the noisy secant and the curvature bound.
 * @note IEEE binary64 nearest rounding, without reassociation; bounds cover the complete bracket.
 */
[[nodiscard]] inline auto derivative_floor(const Sample left, const Sample right, const Bounds bounds) -> double {
  const double width = upper(right.jde_tt - left.jde_tt);
  const double observed_change = lower(std::fabs(right.evaluated.value - left.evaluated.value));
  const double left_adjusted_change = lower(observed_change - left.evaluated.error);
  const double change = lower(left_adjusted_change - right.evaluated.error);
  const double secant_floor = lower(change / width);

  // Integrating |f'(x)-f'(t)| bounds its distance from the secant by M2*w/2.
  const double curvature_span = upper(bounds.second_derivative * width);
  const double slope_error = upper(curvature_span / 2.0);
  return lower(secant_floor - slope_error);
}

/** @brief Require a finite positive time span and finite nonnegative derivative bounds. */
inline auto validate_cell(const double start_jde_tt, const double end_jde_tt, const Bounds bounds) -> void {
  const bool valid_time_span = std::isfinite(start_jde_tt) and std::isfinite(end_jde_tt)
                              and start_jde_tt < end_jde_tt and std::isfinite(end_jde_tt - start_jde_tt);
  const auto finite_derivatives = [bounds] {
    return finite_nonnegative(bounds.first_derivative) and finite_nonnegative(bounds.second_derivative);
  };
  if (not valid_time_span or not finite_derivatives()) {
    throw std::invalid_argument { "Invalid event-search cell or derivative bounds" };
  }
}

/** @brief Test sufficient endpoint conditions for a cell to contain no root. */
[[nodiscard]] inline auto endpoint_exclusion(const Cell& cell, const Bounds bounds) -> bool {
  const int32_t left_sign = sign(cell.left.evaluated);
  if (left_sign == 0 or left_sign != sign(cell.right.evaluated)) {
    return false;
  }

  const double width = upper(cell.right.jde_tt - cell.left.jde_tt);
  const double left_margin = magnitude_floor(cell.left.evaluated);
  const double right_margin = magnitude_floor(cell.right.evaluated);
  const double slope_change = upper(bounds.first_derivative * width);

  // Linear-interpolation remainder: |f-L| <= M2*(t-a)*(b-t)/2 <= M2*w^2/8.
  const double squared_width = upper(width * width);
  const double curvature_remainder = upper(bounds.second_derivative * squared_width);
  const double chord_error = upper(curvature_remainder / 8.0);
  return lower(left_margin + right_margin) > slope_change
         or std::min(left_margin, right_margin) > chord_error;
}

/** @brief Certify exclusion or uniqueness; otherwise request subdivision. */
[[nodiscard]] inline auto classify_cell(const Cell& cell, const Bounds bounds) -> CellDecision {
  if (endpoint_exclusion(cell, bounds)) {
    return CellDecision::DISCARD;
  }
  if (not (derivative_floor(cell.left, cell.right, bounds) > 0.0)) {
    return CellDecision::SPLIT;
  }

  const int32_t left_sign = sign(cell.left.evaluated);
  const int32_t right_sign = sign(cell.right.evaluated);
  if (left_sign != 0 and left_sign == right_sign) {
    return CellDecision::DISCARD;
  }
  if (exact_zero(cell.right.evaluated)) {
    return cell.ownership == EndpointOwnership::BOTH ? CellDecision::UNIQUE : CellDecision::DISCARD;
  }
  const bool sign_change = left_sign != 0 and right_sign == -left_sign;
  if (exact_zero(cell.left.evaluated) or sign_change) {
    return CellDecision::UNIQUE;
  }
  return CellDecision::SPLIT;
}

/** @brief Consume one candidate slot before retaining a bracket. */
inline auto append_candidate(
  const Cell& cell,
  const Isolation isolation,
  Budget& budget,
  std::vector<Candidate>& candidates
) -> void {
  if (budget.candidates_remaining == 0) {
    throw std::runtime_error { "Event-search candidate budget exhausted" };
  }
  --budget.candidates_remaining;
  candidates.push_back({ .left = cell.left, .right = cell.right, .isolation = isolation });
}

/** @brief Traverse a sampled cell, retaining unresolved brackets when subdivision stops. */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
inline auto visit_cell(
  const Provider& provider,
  const Cell& cell,
  const Bounds bounds,
  Budget& budget,
  const uint32_t depth,
  std::vector<Candidate>& candidates
) -> void {
  switch (classify_cell(cell, bounds)) {
    case CellDecision::DISCARD: return;
    case CellDecision::UNIQUE:
      append_candidate(cell, Isolation::UNIQUE, budget, candidates);
      return;
    case CellDecision::SPLIT: break;
  }

  const double middle_jde_tt = std::midpoint(cell.left.jde_tt, cell.right.jde_tt);
  const bool cannot_subdivide = depth == 0 or middle_jde_tt == cell.left.jde_tt or middle_jde_tt == cell.right.jde_tt;
  if (cannot_subdivide) {
    append_candidate(cell, Isolation::UNRESOLVED, budget, candidates);
    return;
  }
  const auto middle = sample(provider, middle_jde_tt, budget);
  const Cell left { .left = cell.left, .right = middle, .ownership = EndpointOwnership::LEFT };
  const Cell right { .left = middle, .right = cell.right, .ownership = cell.ownership };
  visit_cell(provider, left, bounds, budget, depth - 1, candidates);
  visit_cell(provider, right, bounds, budget, depth - 1, candidates);
}

/**
 * @brief Isolate a C2 scalar on a fixed canonical cell, not a query-clipped cell.
 * @note Each value allowance bounds the error at the requested time, including any abscissa shift.
 *       Derivative bounds cover the entire closed cell. Unresolved regions are not event results.
 */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
[[nodiscard]] inline auto isolate_cell(
  const Provider& provider,
  const double start_jde_tt,
  const double end_jde_tt,
  const Bounds bounds,
  Budget& budget,
  const EndpointOwnership ownership = EndpointOwnership::LEFT,
  const uint32_t maximum_depth = 32
) -> std::vector<Candidate> {
  validate_cell(start_jde_tt, end_jde_tt, bounds);
  const Cell cell {
    .left = sample(provider, start_jde_tt, budget),
    .right = sample(provider, end_jde_tt, budget),
    .ownership = ownership,
  };
  std::vector<Candidate> candidates;
  visit_cell(provider, cell, bounds, budget, maximum_depth, candidates);
  return candidates;
}

/**
 * @brief Decide whether an observation ends refinement.
 * @throw std::runtime_error If its error allowance prevents the requested time resolution.
 */
[[nodiscard]] inline auto resolves_sample(
  const double minimum_slope,
  const Sample observed,
  const double time_resolution
) -> bool {
  if (exact_zero(observed.evaluated)) {
    return true;
  }
  if (sign(observed.evaluated) != 0) {
    return false;
  }

  const double magnitude_ceiling = upper(std::fabs(observed.evaluated.value) + observed.evaluated.error);
  const double radius = upper(magnitude_ceiling / minimum_slope);
  if (radius <= time_resolution) {
    return true;
  }
  throw std::runtime_error { "Event-search evaluation error prevents time refinement" };
}

/**
 * @brief Bisect a unique-root bracket with robust signs and bounded work.
 * @note The caller has established uniqueness and handled exact endpoint roots.
 */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
[[nodiscard]] inline auto refine_bracket(
  const Provider& provider,
  const Candidate& candidate,
  const double minimum_slope,
  Budget& budget,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish time resolution from iteration count.
  const double time_resolution,
  const uint32_t maximum_iterations
) -> Root {
  const bool increasing = candidate.right.evaluated.value > candidate.left.evaluated.value;
  auto left = candidate.left;
  auto right = candidate.right;
  for (uint32_t iteration = 0; iteration < maximum_iterations; ++iteration) {
    const double middle_jde_tt = std::midpoint(left.jde_tt, right.jde_tt);
    const double radius = std::max(
      upper(middle_jde_tt - left.jde_tt),
      upper(right.jde_tt - middle_jde_tt)
    );
    const bool precision_limit = middle_jde_tt == left.jde_tt or middle_jde_tt == right.jde_tt;
    if (precision_limit or radius <= time_resolution) {
      return { .jde_tt = middle_jde_tt, .increasing = increasing };
    }

    const auto middle = sample(provider, middle_jde_tt, budget);
    if (resolves_sample(minimum_slope, middle, time_resolution)) {
      return { .jde_tt = middle_jde_tt, .increasing = increasing };
    }

    if (sign(middle.evaluated) == sign(left.evaluated)) {
      left = middle;
    } else {
      right = middle;
    }
  }
  throw std::runtime_error { "Event-search refinement budget exhausted" };
}

/** @brief Refine a certified unique root to the requested time resolution or adjacent doubles. */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
[[nodiscard]] inline auto refine(
  const Provider& provider,
  const Candidate& candidate,
  const Bounds bounds,
  Budget& budget,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish time resolution from iteration count.
  const double time_resolution = 0.0,
  const uint32_t maximum_iterations = 96
) -> Root {
  const bool invalid_request = candidate.isolation != Isolation::UNIQUE or not finite_nonnegative(time_resolution);
  if (invalid_request) {
    throw std::invalid_argument { "Invalid event-search refinement request" };
  }
  const double floor = derivative_floor(candidate.left, candidate.right, bounds);
  if (not (floor > 0.0)) {
    throw std::runtime_error { "Event-search root has no derivative certificate" };
  }
  const bool increasing = candidate.right.evaluated.value > candidate.left.evaluated.value;
  if (exact_zero(candidate.left.evaluated)) {
    return { .jde_tt = candidate.left.jde_tt, .increasing = increasing };
  }
  if (exact_zero(candidate.right.evaluated)) {
    return { .jde_tt = candidate.right.jde_tt, .increasing = increasing };
  }
  const int32_t left_sign = sign(candidate.left.evaluated);
  const bool robustly_bracketed = left_sign != 0 and sign(candidate.right.evaluated) == -left_sign;
  if (not robustly_bracketed) {
    throw std::runtime_error { "Event-search root is not bracketed by robust signs" };
  }
  return refine_bracket(provider, candidate, floor, budget, time_resolution, maximum_iterations);
}

/**
 * @brief Return a stored grid boundary, with the first and final boundaries kept exact.
 * @note The grid is validated and index is in [0, cell_count].
 */
[[nodiscard]] inline auto grid_time(
  const Grid grid,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish a boundary index from the cell count.
  const uint32_t index,
  const uint32_t cell_count
) -> double {
  if (index == 0) {
    return grid.domain.start_jde_tt;
  }
  if (index == cell_count) {
    return grid.domain.end_jde_tt;
  }
  return std::fma(static_cast<double>(index), grid.cell_width_tt_days, grid.domain.start_jde_tt);
}

/** @brief Validate distinct representable cell boundaries and a bounded index range. */
[[nodiscard]] inline auto grid_cell_count(const Grid grid) -> uint32_t {
  validate_cell(grid.domain.start_jde_tt, grid.domain.end_jde_tt, {});
  const double magnitude = std::max(
    std::fabs(grid.domain.start_jde_tt),
    std::fabs(grid.domain.end_jde_tt)
  );
  const double spacing = upper(magnitude) - magnitude;
  const bool invalid_width = not finite_nonnegative(grid.cell_width_tt_days) or grid.cell_width_tt_days < spacing;
  if (invalid_width) {
    throw std::invalid_argument { "Event-search grid width cannot represent distinct cells" };
  }
  const double span = grid.domain.end_jde_tt - grid.domain.start_jde_tt;
  const double approximate_count = std::ceil(span / grid.cell_width_tt_days);
  const double count = std::max(1.0, approximate_count);
  const bool invalid_count = not std::isfinite(count) or count >= std::numeric_limits<uint32_t>::max();
  if (invalid_count) {
    throw std::invalid_argument { "Event-search grid exceeds the cell-index range" };
  }

  auto cell_count = static_cast<uint32_t>(count);
  const auto empty_tail = [&grid, &cell_count] {
    return cell_count > 1 and grid_time(grid, cell_count - 1, cell_count) >= grid.domain.end_jde_tt;
  };
  if (empty_tail()) {
    --cell_count;
  }
  if (empty_tail()) {
    throw std::invalid_argument { "Event-search grid has indistinguishable terminal cells" };
  }
  return cell_count;
}

/** @brief Locate the canonical cell containing an admitted time, correcting quotient rounding. */
[[nodiscard]] inline auto containing_cell(
  const Grid grid,
  const uint32_t cell_count,
  const double jde_tt
) -> uint32_t {
  const double approximate = std::clamp(
    std::floor((jde_tt - grid.domain.start_jde_tt) / grid.cell_width_tt_days),
    0.0,
    static_cast<double>(cell_count - 1)
  );
  auto index = static_cast<uint32_t>(approximate);
  if (index > 0 and grid_time(grid, index, cell_count) > jde_tt) {
    --index;
  }
  if (index + 1 < cell_count and grid_time(grid, index + 1, cell_count) <= jde_tt) {
    ++index;
  }
  const bool contains_time = grid_time(grid, index, cell_count) <= jde_tt
                            and jde_tt < grid_time(grid, index + 1, cell_count);
  if (not contains_time) {
    throw std::runtime_error { "Event-search time cannot be assigned to a canonical cell" };
  }
  return index;
}

/** @brief Append only query-owned roots; intersecting unresolved regions and aliased root times fail. */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, Value>
inline auto append_interval_roots(
  const Provider& provider,
  const std::span<const Candidate> candidates,
  const Interval query,
  const Bounds bounds,
  Budget& budget,
  const double time_resolution,
  std::vector<Root>& roots
) -> void {
  for (const auto& candidate : candidates) {
    const bool outside_query = candidate.right.jde_tt < query.start_jde_tt
                               or candidate.left.jde_tt >= query.end_jde_tt;
    if (outside_query) {
      continue;
    }
    if (candidate.isolation != Isolation::UNIQUE) {
      throw std::runtime_error { "Unresolved event-search region intersects the query" };
    }
    const auto root = refine(provider, candidate, bounds, budget, time_resolution);
    const bool query_owned = query.start_jde_tt <= root.jde_tt and root.jde_tt < query.end_jde_tt;
    if (not query_owned) {
      continue;
    }
    if (not roots.empty() and root.jde_tt <= roots.back().jde_tt) {
      throw std::runtime_error { "Event-search roots cannot be separated at the reported time precision" };
    }
    roots.push_back(root);
  }
}

/**
 * @brief Find isolated scalar roots using whole canonical cells and half-open reported-time ownership.
 * @note Derivative bounds and provider allowances cover the grid's closed domain, including its final boundary.
 *       Unresolved regions touching the query throw; no partial list escapes. Contact classification is separate.
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
  const double time_resolution = 0.0
) -> std::vector<Root> {
  validate_cell(query.start_jde_tt, query.end_jde_tt, bounds);
  const bool outside_domain = query.start_jde_tt < grid.domain.start_jde_tt
                             or query.end_jde_tt > grid.domain.end_jde_tt;
  if (outside_domain or not finite_nonnegative(time_resolution)) {
    throw std::invalid_argument { "Invalid event-search interval request" };
  }
  const uint32_t cell_count = grid_cell_count(grid);
  const uint32_t containing = containing_cell(grid, cell_count, query.start_jde_tt);
  // An interior root in the preceding cell can round to this cell's start.
  const uint32_t first = containing == 0 ? 0 : containing - 1;
  std::vector<Root> roots;
  for (uint32_t index = first; index < cell_count; ++index) {
    const double start_jde_tt = grid_time(grid, index, cell_count);
    if (start_jde_tt >= query.end_jde_tt) {
      break;
    }
    const auto candidates = isolate_cell(
      provider,
      start_jde_tt,
      grid_time(grid, index + 1, cell_count),
      bounds,
      budget
    );
    append_interval_roots(provider, candidates, query, bounds, budget, time_resolution, roots);
  }
  return roots;
}

} // namespace astro::ephemeris::detail::event_search

namespace astro::ephemeris::detail {

// Centers and their 3/8-day stencils fit binary64 throughout the current domains (|JDE| < 2^23).
inline constexpr double EVENT_RATE_CENTER_QUANTUM_TT_DAYS = 0x1p-30;

/**
 * @brief Choose a query-independent legal rate center for a time in the closed rate domain.
 * @note Requires round-to-nearest-even. The final domain endpoint is virtual and selects the preceding grid center.
 */
[[nodiscard]] inline auto event_rate_center(const Target target, const double jde_tt) -> double {
  if (not std::isfinite(jde_tt)) {
    throw std::invalid_argument { "Event-search rate time must be finite" };
  }
  const auto [start_jde_tt, end_jde_tt] = longitude_rate_domain(target);
  const bool outside_domain = jde_tt < start_jde_tt or jde_tt > end_jde_tt;
  if (outside_domain) {
    throw std::invalid_argument { "Event-search rate time leaves its closed domain" };
  }
  const double nearest = std::nearbyint(jde_tt / EVENT_RATE_CENTER_QUANTUM_TT_DAYS)
                         * EVENT_RATE_CENTER_QUANTUM_TT_DAYS;
  const double center_jde_tt = std::clamp(
    nearest,
    start_jde_tt,
    end_jde_tt - EVENT_RATE_CENTER_QUANTUM_TT_DAYS
  );
  validate_rate_input(target, center_jde_tt);
  return center_jde_tt;
}

/**
 * @brief Evaluate a rate at a legal center and charge its displacement from the requested TT time.
 * @note The provider's Value bounds D_h g at the selected center; first_derivative bounds |(D_h g)'| over the domain.
 */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, event_search::Value>
[[nodiscard]] inline auto event_rate_value(
  const Provider& provider,
  const Target target,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish the TT abscissa from its rate-slope bound.
  const double jde_tt,
  const double first_derivative
) -> event_search::Value {
  if (not event_search::finite_nonnegative(first_derivative)) {
    throw std::invalid_argument { "Invalid event-search rate derivative bound" };
  }
  const double center_jde_tt = event_rate_center(target, jde_tt);
  const auto evaluated = provider(center_jde_tt);
  const bool invalid_observation = not std::isfinite(evaluated.value)
                                   or not event_search::finite_nonnegative(evaluated.error);
  if (invalid_observation) {
    throw std::runtime_error { "Invalid event-search rate observation" };
  }
  const double offset = center_jde_tt == jde_tt ? 0.0 : event_search::upper(std::fabs(jde_tt - center_jde_tt));
  return event_search::shifted(evaluated, offset, first_derivative);
}

} // namespace astro::ephemeris::detail
