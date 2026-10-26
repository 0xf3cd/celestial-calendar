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
#include <cfenv>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

#include "ephemeris.hpp"
#include "ephemeris_event_facts.hpp"
#include "ephemeris_event_bounds.hpp"
#include "event_search.hpp"

namespace astro::ephemeris {

/** @brief Behavior of an angular level on the two sides of an event. */
enum class EventDirection : uint8_t { INCREASING, DECREASING, TANGENT };

/** @brief The sign of longitude motion away from a stationary instant. */
enum class Motion : uint8_t { DIRECT, RETROGRADE };

/** @brief A crossing or exact numerical contact at a supplied longitude. */
struct LongitudeEvent {
  double jde_tt;
  toolbox::AngleDeg longitude;
  LongitudeRate longitude_rate;
  EventDirection direction;
};

/** @brief A stationary instant of the published finite-difference rate, with its two-sided motion. */
struct StationEvent {
  double jde_tt;
  toolbox::AngleDeg longitude;
  LongitudeRate longitude_rate;
  Motion motion_before;
  Motion motion_after;
};

/** @brief An event in the directed, canonical longitude difference first minus second. */
struct LongitudeDifferenceEvent {
  double jde_tt;
  toolbox::AngleDeg longitude_a;
  toolbox::AngleDeg longitude_b;
  LongitudeRate longitude_rate_a;
  LongitudeRate longitude_rate_b;
  toolbox::AngleDeg signed_difference;
  EventDirection direction;
};

} // namespace astro::ephemeris

namespace astro::ephemeris::detail {

static_assert(EVENT_BOUNDS_WIDE.size() == TARGETS.size());
static_assert(EVENT_BOUNDS_CENTURY.size() == TARGETS.size());
static_assert(std::invoke([] {
  for (std::size_t index = 0; index < TARGETS.size(); ++index) {
    if (static_cast<std::size_t>(TARGETS.at(index)) != index) {
      return false;
    }
  }
  return true;
}), "Generated bounds are indexed by the target roster's underlying values");

/** @brief Evaluate the actual target longitude; descending-node rates alone use the ascending trajectory. */
[[nodiscard]] inline auto event_longitude(const Target target, const double jde_tt) -> toolbox::AngleDeg {
  const auto& facts = target_facts(target);
  if (facts.node.has_value()) {
    return astro::lunar_node::position(*facts.node, jde_tt);
  }
  return longitude_for_rate(target, jde_tt);
}

/**
 * @brief Recognize constant real-model first-minus-second relations by target identity, not sampled positions.
 * @throw std::invalid_argument If either target is unknown.
 */
[[nodiscard]] inline auto constant_longitude_difference(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names preserve the directed pair's order.
  const Target first,
  const Target second
) -> std::optional<toolbox::AngleDeg> {
  const auto& first_facts = target_facts(first);
  const auto& second_facts = target_facts(second);
  if (first == second) {
    return toolbox::AngleDeg { 0.0 };
  }
  const bool antipodes = first_facts.node.has_value() and second_facts.node.has_value()
    and first_facts.rate_node == second_facts.rate_node and first_facts.node != second_facts.node;
  if (antipodes) {
    return toolbox::AngleDeg { 180.0 };
  }
  return std::nullopt;
}

// Centers and their 3/8-day stencils fit binary64 throughout the current domains (|JDE| < 2^23).
inline constexpr double EVENT_RATE_CENTER_QUANTUM_TT_DAYS = 0x1p-30;
// Six-hour absolute cells; derivative certificates decide exclusion or subdivision, not the spacing alone.
inline constexpr double EVENT_CELL_WIDTH_TT_DAYS = 0.25;
// 84.375-second time certificate, separate from the finer deterministic reporting lattice.
inline constexpr double EVENT_TIME_RESOLUTION_TT_DAYS = 0x1p-10;
// Per-query resource policies, not sufficient-work guarantees for every admitted span.
inline constexpr std::size_t EVENT_MAXIMUM_EVALUATIONS = 65536;
inline constexpr std::size_t EVENT_MAXIMUM_CANDIDATES = 4096;
// Fixed J2000 +/- one-century analysis band; selection also reserves the complete sampling halo.
inline constexpr double EVENT_CENTURY_START_JDE_TT = astro::julian_day::J2000 - 36525.0;
inline constexpr double EVENT_CENTURY_END_JDE_TT = astro::julian_day::J2000 + 36525.0;

/**
 * @brief Choose a query-independent legal rate center for a time in the closed rate domain.
 * @note Requires round-to-nearest-even. The final domain endpoint is virtual and selects the preceding grid center.
 */
[[nodiscard]] inline auto event_rate_center(const Target target, const double jde_tt) -> double {
  if (not std::isfinite(jde_tt)) {
    throw std::invalid_argument { std::format("Event-search rate JDE(TT) must be finite, got {}", jde_tt) };
  }
  const auto [start_jde_tt, end_jde_tt] = longitude_rate_domain(target);
  const bool outside_domain = jde_tt < start_jde_tt or jde_tt > end_jde_tt;
  if (outside_domain) {
    throw std::invalid_argument {
      std::format(
        "Event-search rate JDE(TT) {} leaves closed domain [{}, {}] for target code {}",
        jde_tt, start_jde_tt, end_jde_tt, static_cast<uint32_t>(target)
      )
    };
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
     and std::same_as<std::invoke_result_t<const Provider&, double>, astro::detail::event_search::Value>
[[nodiscard]] inline auto event_rate_value(
  const Provider& provider,
  const Target target,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish the TT abscissa from its rate-slope bound.
  const double jde_tt,
  const double first_derivative
) -> astro::detail::event_search::Value {
  namespace event_search = astro::detail::event_search;
  if (not event_search::finite_nonnegative(first_derivative)) {
    throw std::invalid_argument {
      std::format("Invalid event-search rate derivative bound {} at JDE(TT) {}", first_derivative, jde_tt)
    };
  }
  const double center_jde_tt = event_rate_center(target, jde_tt);
  const auto evaluated = provider(center_jde_tt);
  const bool invalid_observation = not std::isfinite(evaluated.value)
                                    or not event_search::finite_nonnegative(evaluated.error);
  if (invalid_observation) {
    throw std::runtime_error {
      std::format(
        "Invalid event-search rate observation {} +/- {} at center JDE(TT) {} for target code {}",
        evaluated.value, evaluated.error, center_jde_tt, static_cast<uint32_t>(target)
      )
    };
  }
  const double offset = center_jde_tt == jde_tt ? 0.0 : event_search::upper(std::fabs(jde_tt - center_jde_tt));
  return event_search::shifted(evaluated, offset, first_derivative);
}

/** @brief Reject execution environments outside the source-model error contract before evaluating a provider. */
inline auto validate_event_environment() -> void {
#ifdef __FAST_MATH__
  throw std::runtime_error { "Event search does not support fast-math (__FAST_MATH__ is defined)" };
#else
  const bool binary64 = std::numeric_limits<double>::is_iec559 and std::numeric_limits<double>::digits == 53
                        and FLT_EVAL_METHOD == 0;
  if (not binary64 or std::fegetround() != FE_TONEAREST) {
    throw std::runtime_error {
      std::format(
        "Event search requires IEEE binary64 round-to-nearest-even: digits {}, FLT_EVAL_METHOD {}, rounding mode {}",
        std::numeric_limits<double>::digits, FLT_EVAL_METHOD, std::fegetround()
      )
    };
  }
  volatile const double normal = std::numeric_limits<double>::min();
  volatile const double subnormal = std::numeric_limits<double>::min() / 2.0;
  const bool gradual_underflow = normal / 2.0 == subnormal and subnormal * 2.0 == normal;
  if (not gradual_underflow) {
    throw std::runtime_error {
      std::format(
        "Event search requires gradual underflow: min normal {}, half {}, doubled half {}",
        static_cast<double>(normal), static_cast<double>(subnormal), subnormal * 2.0
      )
    };
  }
#endif
}

/**
 * @brief Validate a nonempty query against the published rate domains, then form its absolute cell grid.
 * @see docs/EPHEMERIS_EVENTS.md, "Domains, limits and failures" and "Search policies".
 */
[[nodiscard]] inline auto event_grid(
  const Target first,
  const std::optional<Target> second,
  const astro::detail::event_search::Interval query
) -> astro::detail::event_search::Grid {
  auto [start_jde_tt, end_jde_tt] = longitude_rate_domain(first);
  if (second.has_value()) {
    const auto [second_start, second_end] = longitude_rate_domain(*second);
    start_jde_tt = std::max(start_jde_tt, second_start);
    end_jde_tt = std::min(end_jde_tt, second_end);
  }
  if (not astro::detail::event_search::valid_time_span(query.start_jde_tt, query.end_jde_tt)) {
    throw std::invalid_argument {
      std::format(
        "Event query [{}, {}) JDE(TT) must be finite and nonempty, with a finite span",
        query.start_jde_tt, query.end_jde_tt
      )
    };
  }
  const bool outside_domain = query.start_jde_tt < start_jde_tt or query.end_jde_tt > end_jde_tt;
  if (outside_domain) {
    throw std::invalid_argument {
      std::format(
        "Event query leaves its longitude-rate domain: query [{}, {}) JDE(TT), domain [{}, {}], target code {}",
        query.start_jde_tt, query.end_jde_tt, start_jde_tt, end_jde_tt, static_cast<uint32_t>(first)
      )
    };
  }
  if (query.end_jde_tt - query.start_jde_tt > EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS) {
    throw std::invalid_argument {
      std::format(
        "Event query spans {} TT days, exceeding the {}-day limit",
        query.end_jde_tt - query.start_jde_tt, EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS
      )
    };
  }
  return {
    .domain = { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt },
    .cell_width_tt_days = EVENT_CELL_WIDTH_TT_DAYS,
  };
}

/** @brief Select a fixed analysis band by the entire canonical cell and sample halo, never by query endpoints. */
[[nodiscard]] inline auto event_bounds(const Target target, const astro::detail::event_search::Interval cell) -> EventBounds {
  const auto index = static_cast<std::size_t>(target);
  if (index >= EVENT_BOUNDS_WIDE.size()) {
    throw std::invalid_argument { std::format("Unsupported event-search target code {}", static_cast<uint32_t>(target)) };
  }
  const double halo = DIFFERENCE_REACH_TT_DAYS + EVENT_RATE_CENTER_QUANTUM_TT_DAYS;
  const bool within_century = cell.start_jde_tt >= EVENT_CENTURY_START_JDE_TT + halo
                              and cell.end_jde_tt <= EVENT_CENTURY_END_JDE_TT - halo;
  return within_century ? EVENT_BOUNDS_CENTURY.at(index) : EVENT_BOUNDS_WIDE.at(index);
}

/** @brief Recognize a globally certified nonzero rate without scanning its sign. */
[[nodiscard]] inline auto has_no_station(const EventBounds bounds) -> bool {
  return (bounds.minimum_rate.has_value() and *bounds.minimum_rate > 0.0)
         or (bounds.maximum_rate.has_value() and *bounds.maximum_rate < 0.0);
}

/** @brief Observe the source longitude with its selected fixed-band evaluation allowance. */
[[nodiscard]] inline auto event_position_value(
  const Target target,
  const double jde_tt,
  const EventBounds bounds
) -> astro::detail::event_search::LongitudeValue {
  return { .longitude = event_longitude(target, jde_tt), .error_deg = bounds.position_error_deg };
}

/** @brief Observe D_h at an admitted center, including displacement from the requested abscissa. */
[[nodiscard]] inline auto event_rate_observation(
  const Target target,
  const double jde_tt,
  const EventBounds bounds
) -> astro::detail::event_search::Value {
  const auto provider = [&](const double center_jde_tt) -> astro::detail::event_search::Value {
    return { .value = longitude_rate(target, center_jde_tt).deg_per_tt_day, .error = bounds.rate_error_deg_per_day };
  };
  return event_rate_value(provider, target, jde_tt, bounds.trajectory.rate.first_derivative);
}

/** @brief Subtract independent scalar observations with a conservative binary64 subtraction allowance. */
[[nodiscard]] inline auto subtract_event_rates(
  const astro::detail::event_search::Value first,
  const astro::detail::event_search::Value second
) -> astro::detail::event_search::Value {
  namespace event_search = astro::detail::event_search;
  const double magnitude = event_search::upper(std::fabs(first.value) + std::fabs(second.value));
  const double rounding = event_search::upper(std::numeric_limits<double>::epsilon() * magnitude);
  return {
    .value = first.value - second.value,
    .error = event_search::upper(event_search::upper(first.error + second.error) + rounding),
  };
}

/** @brief Sum absolute derivative bounds for a directed difference, without subtracting independent uncertainties. */
[[nodiscard]] inline auto difference_bounds(
  const EventBounds first,
  const EventBounds second
) -> astro::detail::event_search::TrajectoryBounds {
  namespace event_search = astro::detail::event_search;
  const auto sum = [](
    const event_search::Bounds a,
    const event_search::Bounds b
  ) -> event_search::Bounds {
    return {
      .first_derivative = event_search::upper(a.first_derivative + b.first_derivative),
      .second_derivative = event_search::upper(a.second_derivative + b.second_derivative),
    };
  };
  return {
    .longitude = sum(first.trajectory.longitude, second.trajectory.longitude),
    .rate = sum(first.trajectory.rate, second.trajectory.rate),
    .third_derivative = event_search::upper(first.trajectory.third_derivative + second.trajectory.third_derivative),
    .rate_remainder = event_search::upper(first.trajectory.rate_remainder + second.trajectory.rate_remainder),
  };
}

/** @brief Translate the private search classification without coupling the kernel to public result types. */
[[nodiscard]] inline auto event_direction(const astro::detail::event_search::ContactDirection direction) -> EventDirection {
  using enum astro::detail::event_search::ContactDirection;
  switch (direction) {
    case INCREASING: return EventDirection::INCREASING;
    case DECREASING: return EventDirection::DECREASING;
    case TANGENT: return EventDirection::TANGENT;
  }
  throw std::runtime_error { std::format("Unsupported event-search direction {}", static_cast<uint32_t>(direction)) };
}

/**
 * @brief Search whole absolute cells; station refinement uses the cell's allowances, contact regions the window's.
 * @see docs/EPHEMERIS_EVENTS.md, "Numerical contacts" and "Scalar cell certificates" (project derivations).
 */
[[nodiscard]] inline auto search_longitudes(
  const Target first,
  const std::optional<Target> second,
  const toolbox::AngleDeg target,
  const astro::detail::event_search::Grid grid,
  const astro::detail::event_search::Interval query
) -> std::vector<astro::detail::event_search::LongitudeRoot> {
  namespace event_search = astro::detail::event_search;

  event_search::Budget budget {
    .evaluations_remaining = EVENT_MAXIMUM_EVALUATIONS,
    .candidates_remaining = EVENT_MAXIMUM_CANDIDATES,
  };
  event_search::validate_reporting_grid(grid, EVENT_RATE_CENTER_QUANTUM_TT_DAYS);

  // One allowance selection per span; a window crossing a century edge selects WIDE.
  const auto model = [first, second](const event_search::Interval span) {
    const auto first_bounds = event_bounds(first, span);
    const auto second_bounds = second.has_value() ? event_bounds(*second, span) : first_bounds;
    const auto longitude = [first, second, first_bounds, second_bounds](const double jde_tt) -> event_search::LongitudeValue {
      const auto a = event_position_value(first, jde_tt, first_bounds);
      if (second.has_value()) {
        const auto b = event_position_value(*second, jde_tt, second_bounds);
        const auto difference = event_search::project_longitude_difference(a, b);
        return { .longitude = toolbox::AngleDeg { difference.value }, .error_deg = difference.error };
      }
      return a;
    };
    const auto rate = [first, second, first_bounds, second_bounds](const double jde_tt) {
      const auto a = event_rate_observation(first, jde_tt, first_bounds);
      return second.has_value() ? subtract_event_rates(a, event_rate_observation(*second, jde_tt, second_bounds)) : a;
    };
    const auto bounds = second.has_value() ? difference_bounds(first_bounds, second_bounds) : first_bounds.trajectory;
    // Pairs store no signed relative-rate range, so only a single target can exclude a station.
    const bool needs_contact_scan = second.has_value() or not has_no_station(first_bounds);
    return std::tuple { longitude, rate, bounds, needs_contact_scan };
  };
  const auto contacts = [&](const event_search::Interval cell, const event_search::Interval window) {
    const auto [longitude, rate, bounds, needs_contact_scan] = model(cell);
    if (not needs_contact_scan) {
      return std::vector<event_search::NumericalContact> {};
    }
    const auto [window_longitude, window_rate, window_bounds, window_needs_contact_scan] = model(window);
    return event_search::cell_contacts(
      longitude, rate, window_longitude, window_rate, target, cell, window, bounds, window_bounds, budget,
      EVENT_TIME_RESOLUTION_TT_DAYS, EVENT_RATE_CENTER_QUANTUM_TT_DAYS, query
    );
  };
  const auto crossings = [&](const event_search::Interval cell, const std::span<const event_search::Interval> exclusions) {
    const auto [longitude, rate, bounds, needs_contact_scan] = model(cell);
    return event_search::cell_crossings(
      longitude, target, cell, exclusions, query, bounds.longitude, budget, EVENT_TIME_RESOLUTION_TT_DAYS,
      EVENT_RATE_CENTER_QUANTUM_TT_DAYS
    );
  };
  return event_search::contact_traversal(grid, query, contacts, crossings);
}

} // namespace astro::ephemeris::detail

namespace astro::ephemeris {

/**
 * @brief Return every crossing or exact numerical contact at a supplied tropical longitude.
 * @param target The moving body or node.
 * @param target_longitude A finite angle; whole-turn shifts denote the same level.
 * @param start_jde_tt Inclusive start, within @ref longitude_rate_domain.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @return Chronologically ordered events, with no partial result on numerical or work-limit failure.
 * @note Completeness is conditional on the floating-point and elementary-function contract in
 *       @ref docs/EPHEMERIS_EVENTS.md.
 *       A tangent requires exact equality with the query-independent stationary output, not angular proximity.
 */
[[nodiscard]] inline auto longitude_events(
  const Target target,
  const toolbox::AngleDeg target_longitude,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): The parameter names state the half-open TT interval.
  const double start_jde_tt,
  const double end_jde_tt
) -> std::vector<LongitudeEvent> {
  const auto level = toolbox::canonical_longitude_difference(target_longitude);
  const astro::detail::event_search::Interval query { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt };
  const auto grid = detail::event_grid(target, std::nullopt, query);
  detail::validate_event_environment();
  const auto roots = detail::search_longitudes(target, std::nullopt, level, grid, query);

  const auto event = [target](const astro::detail::event_search::LongitudeRoot root) -> LongitudeEvent {
    return {
      .jde_tt = root.jde_tt,
      .longitude = detail::event_longitude(target, root.jde_tt),
      .longitude_rate = longitude_rate(target, root.jde_tt),
      .direction = detail::event_direction(root.direction),
    };
  };
  return roots | std::views::transform(event) | std::ranges::to<std::vector>();
}

/**
 * @brief Return every isolated zero of the published seven-point longitude rate in a half-open TT interval.
 * @param target The moving body or node.
 * @param start_jde_tt Inclusive start, within @ref longitude_rate_domain.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @return Ordered stationary instants and the certified motion on both sides.
 * @note The reported finite-difference rate need not be exactly zero. Unresolved isolation or refinement throws.
 *       The floating-point premises are the same as for @ref longitude_events.
 */
[[nodiscard]] inline auto stations(
  const Target target,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): The parameter names state the half-open TT interval.
  const double start_jde_tt,
  const double end_jde_tt
) -> std::vector<StationEvent> {
  namespace event_search = astro::detail::event_search;

  const event_search::Interval query { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt };
  const auto grid = detail::event_grid(target, std::nullopt, query);
  detail::validate_event_environment();
  event_search::validate_reporting_grid(grid, detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS);

  event_search::Budget budget {
    .evaluations_remaining = detail::EVENT_MAXIMUM_EVALUATIONS,
    .candidates_remaining = detail::EVENT_MAXIMUM_CANDIDATES,
  };
  std::vector<event_search::Root> roots;
  event_search::visit_query_cells(grid, query, [&](const event_search::Interval cell) {
    const auto bounds = detail::event_bounds(target, cell);
    if (detail::has_no_station(bounds)) {
      return;
    }
    const auto rate = [&](const double jde_tt) { return detail::event_rate_observation(target, jde_tt, bounds); };
    const auto candidates = event_search::isolate_cell(rate, cell.start_jde_tt, cell.end_jde_tt, bounds.trajectory.rate, budget);
    event_search::append_interval_roots(
      rate, candidates, query, bounds.trajectory.rate, budget, detail::EVENT_TIME_RESOLUTION_TT_DAYS,
      roots, detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS
    );
  });

  const auto event = [target](const event_search::Root root) -> StationEvent {
    return {
      .jde_tt = root.jde_tt,
      .longitude = detail::event_longitude(target, root.jde_tt),
      .longitude_rate = longitude_rate(target, root.jde_tt),
      .motion_before = root.increasing ? Motion::RETROGRADE : Motion::DIRECT,
      .motion_after = root.increasing ? Motion::DIRECT : Motion::RETROGRADE,
    };
  };
  return roots | std::views::transform(event) | std::ranges::to<std::vector>();
}

/**
 * @brief Return every event at a directed longitude difference, first minus second.
 * @param first The first moving body or node.
 * @param second A distinct supported target.
 * @param target_difference A finite signed angle, canonicalized to (-180,180] degrees.
 * @param start_jde_tt Inclusive start, within the intersection of the two @ref longitude_rate_domain windows.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @return Ordered crossings and exact numerical relative-station contacts, or an empty list for a constant miss.
 * @throw std::invalid_argument For self-pairs, continuously satisfied relations or malformed/out-of-window inputs.
 * @note Numerical uncertainty or work-limit exhaustion throws, never returns a prefix. The floating-point
 *       premises and numerical-contact equality rule are the same as for @ref longitude_events.
 */
[[nodiscard]] inline auto longitude_difference_events(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): These names preserve the directed target order.
  const Target first,
  const Target second,
  const toolbox::AngleDeg target_difference,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): The parameter names state the half-open TT interval.
  const double start_jde_tt,
  const double end_jde_tt
) -> std::vector<LongitudeDifferenceEvent> {
  const auto level = toolbox::canonical_longitude_difference(target_difference);
  const astro::detail::event_search::Interval query { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt };
  const auto grid = detail::event_grid(first, second, query);
  if (first == second) {
    throw std::invalid_argument {
      std::format("Longitude-difference events require distinct target codes, got {} twice", static_cast<uint32_t>(first))
    };
  }
  const auto constant = detail::constant_longitude_difference(first, second);
  if (constant.has_value() and constant->deg() == level.deg()) {
    throw std::invalid_argument {
      std::format(
        "Longitude relation at {} deg holds throughout [{}, {}) JDE(TT) for target codes {} and {}",
        level.deg(), start_jde_tt, end_jde_tt, static_cast<uint32_t>(first), static_cast<uint32_t>(second)
      )
    };
  }
  detail::validate_event_environment();
  if (constant.has_value()) {
    return {};
  }
  const auto roots = detail::search_longitudes(first, second, level, grid, query);

  const auto event = [first, second](const astro::detail::event_search::LongitudeRoot root) -> LongitudeDifferenceEvent {
    const auto longitude_a = detail::event_longitude(first, root.jde_tt);
    const auto longitude_b = detail::event_longitude(second, root.jde_tt);
    const auto difference = astro::detail::event_search::project_longitude_difference(
      { .longitude = longitude_a, .error_deg = 0.0 }, { .longitude = longitude_b, .error_deg = 0.0 }
    );
    return {
      .jde_tt = root.jde_tt,
      .longitude_a = longitude_a,
      .longitude_b = longitude_b,
      .longitude_rate_a = longitude_rate(first, root.jde_tt),
      .longitude_rate_b = longitude_rate(second, root.jde_tt),
      .signed_difference = toolbox::AngleDeg { difference.value },
      .direction = detail::event_direction(root.direction),
    };
  };
  return roots | std::views::transform(event) | std::ranges::to<std::vector>();
}

} // namespace astro::ephemeris
