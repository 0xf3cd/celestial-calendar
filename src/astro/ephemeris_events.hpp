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
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "ephemeris.hpp"
#include "event_search.hpp"

namespace astro::ephemeris::detail {

namespace event_search = astro::detail::event_search;

/** @brief Evaluate the actual target longitude; descending-node rates alone use the ascending trajectory. */
[[nodiscard]] inline auto event_longitude(const Target target, const double jde_tt) -> toolbox::AngleDeg {
  switch (target) {
    case Target::MEAN_DESCENDING:
      return astro::lunar_node::position(astro::lunar_node::Node::MEAN_DESCENDING, jde_tt);
    case Target::TRUE_DESCENDING:
      return astro::lunar_node::position(astro::lunar_node::Node::TRUE_DESCENDING, jde_tt);
    default:
      return longitude_for_rate(target, jde_tt);
  }
}

/** @brief Recognize constant real-model first-minus-second relations by target identity, not sampled positions. */
[[nodiscard]] inline auto constant_longitude_difference(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names preserve the directed pair's order.
  const Target first,
  const Target second
) -> std::optional<toolbox::AngleDeg> {
  static_cast<void>(position_window_for_rate(first));
  static_cast<void>(position_window_for_rate(second));
  if (first == second) {
    return toolbox::AngleDeg { 0.0 };
  }
  using enum Target;
  const bool mean_antipodes = (first == MEAN_ASCENDING and second == MEAN_DESCENDING)
                             or (first == MEAN_DESCENDING and second == MEAN_ASCENDING);
  const bool true_antipodes = (first == TRUE_ASCENDING and second == TRUE_DESCENDING)
                             or (first == TRUE_DESCENDING and second == TRUE_ASCENDING);
  if (mean_antipodes or true_antipodes) {
    return toolbox::AngleDeg { 180.0 };
  }
  return std::nullopt;
}

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
