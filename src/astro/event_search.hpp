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

/** @file
 * @brief Fixed-grid root isolation and conditional numerical certification.
 * @see docs/EPHEMERIS_EVENTS.md for cell, certificate, allowance and boundary terminology.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "toolbox.hpp"

namespace astro::detail::event_search {

// Finite dyadic region search, not an existence guarantee; uncertified query-owned contacts still fail.
inline constexpr uint32_t MAXIMUM_CONTACT_REGION_ATTEMPTS = 24;

/** @brief A scalar observation with an absolute error allowance at the requested abscissa. */
struct Value {
  double value;
  double error;
};

/** @brief A longitude observation with an absolute circular error allowance in degrees. */
struct LongitudeValue {
  toolbox::AngleDeg longitude;
  double error_deg;
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

/** @brief The level's direction across a canonical stationary contact, not the sign of its rate at that time. */
enum class ContactDirection : uint8_t { INCREASING, DECREASING, TANGENT };

/**
 * @brief Shared limits on scalar-evaluation attempts and candidate slots.
 * @note One instance covers the whole query, including recursive cells and contact-window probes.
 */
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

/** @brief A sampled bracket with an explicit rule for including exact endpoint roots. */
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

/** @brief A longitude-level event, including a contact at a canonical numerical station. */
struct LongitudeRoot {
  double jde_tt;
  ContactDirection direction;
};

/** @brief Uniform smooth-longitude and D_h derivative bounds, with their absolute rate discrepancy. */
struct TrajectoryBounds {
  Bounds longitude;
  Bounds rate;
  double third_derivative;
  double rate_remainder;
};

/** @brief A nonempty half-open interval in JDE(TT). */
struct Interval {
  double start_jde_tt;
  double end_jde_tt;
};

/** @brief A canonical contact and the entire certified extremum region it replaces. */
struct NumericalContact {
  LongitudeRoot root;
  Interval region;
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
    throw std::invalid_argument {
      std::format("Invalid event-search abscissa allowance: offset {}, first-derivative bound {}", offset, first_derivative)
    };
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
    throw std::runtime_error {
      std::format("Event-search evaluation budget exhausted at JDE(TT) {}; {} candidate slots remain", jde_tt, budget.candidates_remaining)
    };
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
 * @see docs/EPHEMERIS_EVENTS.md, "Scalar cell certificates" (project derivation).
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

/** @brief Test finite, strictly ordered TT endpoints whose difference is finite. */
[[nodiscard]] inline auto valid_time_span(const double start_jde_tt, const double end_jde_tt) -> bool {
  return std::isfinite(start_jde_tt) and std::isfinite(end_jde_tt)
         and start_jde_tt < end_jde_tt and std::isfinite(end_jde_tt - start_jde_tt);
}

/** @brief Require a finite positive time span and finite nonnegative derivative bounds. */
inline auto validate_cell(const double start_jde_tt, const double end_jde_tt, const Bounds bounds) -> void {
  const auto finite_derivatives = [bounds] {
    return finite_nonnegative(bounds.first_derivative) and finite_nonnegative(bounds.second_derivative);
  };
  if (not valid_time_span(start_jde_tt, end_jde_tt) or not finite_derivatives()) {
    throw std::invalid_argument {
      std::format(
        "Invalid event-search cell [{}, {}] or derivative bounds ({}, {})",
        start_jde_tt, end_jde_tt, bounds.first_derivative, bounds.second_derivative
      )
    };
  }
}

/**
 * @brief Test sufficient endpoint conditions for a cell to contain no root.
 * @see docs/EPHEMERIS_EVENTS.md, "Scalar cell certificates" (project derivation).
 */
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
    throw std::runtime_error {
      std::format(
        "Event-search candidate budget exhausted in [{}, {}] JDE(TT); {} evaluations remain",
        cell.left.jde_tt, cell.right.jde_tt, budget.evaluations_remaining
      )
    };
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
  const bool uncertain_middle = sign(middle.evaluated) == 0 and not exact_zero(middle.evaluated);
  if (uncertain_middle) {
    const double inner_left_jde_tt = std::midpoint(cell.left.jde_tt, middle_jde_tt);
    const double inner_right_jde_tt = std::midpoint(middle_jde_tt, cell.right.jde_tt);
    const bool distinct_inner_times = cell.left.jde_tt < inner_left_jde_tt
                                      and inner_left_jde_tt < inner_right_jde_tt
                                      and inner_right_jde_tt < cell.right.jde_tt;
    if (distinct_inner_times) {
      const auto inner_left = sample(provider, inner_left_jde_tt, budget);
      const auto inner_right = sample(provider, inner_right_jde_tt, budget);
      const Cell bridge { .left = inner_left, .right = inner_right, .ownership = EndpointOwnership::LEFT };
      const int32_t inner_sign = sign(inner_left.evaluated);
      const bool brackets_uncertain_middle = inner_sign != 0 and sign(inner_right.evaluated) == -inner_sign;
      if (brackets_uncertain_middle and classify_cell(bridge, bounds) == CellDecision::UNIQUE) {
        // Keep a noisy midpoint inside one certified bracket instead of making it an undecidable endpoint twice.
        const Cell left { .left = cell.left, .right = inner_left, .ownership = EndpointOwnership::LEFT };
        const Cell right { .left = inner_right, .right = cell.right, .ownership = cell.ownership };
        visit_cell(provider, left, bounds, budget, depth - 1, candidates);
        append_candidate(bridge, Isolation::UNIQUE, budget, candidates);
        visit_cell(provider, right, bounds, budget, depth - 1, candidates);
        return;
      }
    }
  }
  const Cell left { .left = cell.left, .right = middle, .ownership = EndpointOwnership::LEFT };
  const Cell right { .left = middle, .right = cell.right, .ownership = cell.ownership };
  visit_cell(provider, left, bounds, budget, depth - 1, candidates);
  visit_cell(provider, right, bounds, budget, depth - 1, candidates);
}

/**
 * @brief Isolate a C2 scalar on a fixed canonical cell, not a query-clipped cell.
 * @note Each value allowance bounds the error at the requested time, including any abscissa shift.
 *       Derivative bounds cover the entire closed cell. Unresolved regions are not event results.
 * @param maximum_depth Dyadic subdivision cap; 32 levels divide a quarter-day cell below 6e-11 day.
 *        Observation error can stop certification sooner; the cap retains unresolved candidates.
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
 * @brief Observe first minus second modulo 360 degrees, including subtraction roundoff.
 * @note The allowance is circular; scalar isolation requires a certified continuous lift over its cell.
 * @see Knuth, The Art of Computer Programming, Vol. 2, Section 4.2.2 (error-free addition/subtraction).
 */
[[nodiscard]] inline auto project_longitude_difference(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names preserve the oriented first-minus-second order.
  const LongitudeValue first,
  const LongitudeValue second
) -> Value {
  const auto valid_observation = [](const LongitudeValue observed) {
    return std::isfinite(observed.longitude.deg()) and finite_nonnegative(observed.error_deg);
  };
  if (not valid_observation(first) or not valid_observation(second)) {
    throw std::runtime_error {
      std::format(
        "Invalid event-search longitude observations: first {} +/- {} deg, second {} +/- {} deg",
        first.longitude.deg(), first.error_deg, second.longitude.deg(), second.error_deg
      )
    };
  }
  const double first_deg = toolbox::canonical_longitude_difference(first.longitude).deg();
  const double second_deg = toolbox::canonical_longitude_difference(second.longitude).deg();
  const double difference = first_deg - second_deg;
  const double recovered_second = first_deg - difference;
  const double recovered_first = difference + recovered_second;
  const double roundoff = (first_deg - recovered_first) + (recovered_second - second_deg);
  const bool exact_inputs = first.error_deg == 0.0 and second.error_deg == 0.0;
  const double input_error = exact_inputs ? 0.0 : upper(first.error_deg + second.error_deg);
  return {
    .value = toolbox::canonical_longitude_difference(toolbox::AngleDeg { difference }).deg(),
    .error = roundoff == 0.0 ? input_error : upper(input_error + std::fabs(roundoff)),
  };
}

/** @brief Return a scalar level residual only when its observation cannot straddle the antipodal cut. */
[[nodiscard]] inline auto longitude_residual(const LongitudeValue observed, const toolbox::AngleDeg target) -> Value {
  const auto evaluated = project_longitude_difference(observed, { .longitude = target, .error_deg = 0.0 });
  const double extent = upper(std::fabs(evaluated.value) + evaluated.error);
  if (extent >= toolbox::LONGITUDE_PERIOD_DEG / 2.0) {
    throw std::runtime_error {
      std::format(
        "Event-search longitude observation crosses the antipodal cut: {} +/- {} deg at target {} deg",
        evaluated.value, evaluated.error, target.deg()
      )
    };
  }
  return evaluated;
}

/**
 * @brief Certify one sign of f'' throughout three observations' span; zero means unresolved.
 * @note The samples bound one C3 scalar, and third_derivative bounds |f'''| throughout their closed span.
 * @see https://en.wikipedia.org/wiki/Mean_value_theorem_for_divided_differences (n=2).
 */
[[nodiscard]] inline auto curvature_sign(
  const Sample left,
  const Sample middle,
  const Sample right,
  const double third_derivative
) -> int32_t {
  validate_cell(left.jde_tt, right.jde_tt, {});
  validate_cell(left.jde_tt, middle.jde_tt, {});
  validate_cell(middle.jde_tt, right.jde_tt, {});
  if (not finite_nonnegative(third_derivative)) {
    throw std::invalid_argument {
      std::format("Invalid event-search third-derivative bound {} in [{}, {}] JDE(TT)", third_derivative, left.jde_tt, right.jde_tt)
    };
  }
  for (const auto observed : std::array { left.evaluated, middle.evaluated, right.evaluated }) {
    if (not std::isfinite(observed.value) or not finite_nonnegative(observed.error)) {
      throw std::runtime_error {
        std::format(
          "Invalid event-search curvature observation {} +/- {} in [{}, {}] JDE(TT)",
          observed.value, observed.error, left.jde_tt, right.jde_tt
        )
      };
    }
  }
  const double left_width_lower = lower(middle.jde_tt - left.jde_tt);
  const double right_width_lower = lower(right.jde_tt - middle.jde_tt);
  if (left_width_lower <= 0.0 or right_width_lower <= 0.0) {
    return 0;
  }
  const double left_width_upper = upper(middle.jde_tt - left.jde_tt);
  const double right_width_upper = upper(right.jde_tt - middle.jde_tt);
  const double width = upper(right.jde_tt - left.jde_tt);
  const auto convex = [&](const double orientation) {
    const double left_error = upper(left.evaluated.error + middle.evaluated.error);
    const double right_error = upper(middle.evaluated.error + right.evaluated.error);
    const double left_change = upper(upper((orientation * middle.evaluated.value) - (orientation * left.evaluated.value))
                                     + left_error);
    const double right_change = lower(lower((orientation * right.evaluated.value) - (orientation * middle.evaluated.value))
                                      - right_error);
    const double left_slope = upper(left_change / (left_change >= 0.0 ? left_width_lower : left_width_upper));
    const double right_slope = lower(right_change / (right_change >= 0.0 ? right_width_upper : right_width_lower));
    const double slope_increase = lower(right_slope - left_slope);
    if (not (slope_increase > 0.0)) {
      return false;
    }
    // Twice the divided difference equals f'' somewhere in the span; |f'''| carries that sign across it.
    const double point_curvature = lower(lower(2.0 * slope_increase) / width);
    return point_curvature > upper(third_derivative * width);
  };
  if (convex(1.0)) {
    return 1;
  }
  return convex(-1.0) ? -1 : 0;
}

/**
 * @brief Classify an exact numerical contact from certified observations on both sides.
 * @note The caller supplies one query-independent stationary reference and excludes other turning points in its region.
 *       Equal-side contacts also require a matching curvature certificate; angular proximity is never equality.
 */
[[nodiscard]] inline auto contact_direction(
  const LongitudeValue at_station,
  const toolbox::AngleDeg target,
  const Value before,
  const Value after,
  const int32_t curvature
) -> ContactDirection {
  const auto residual = project_longitude_difference(at_station, { .longitude = target, .error_deg = 0.0 });
  if (residual.value != 0.0) {
    throw std::runtime_error {
      std::format("Event-search level {} deg is not the canonical stationary longitude {} deg", target.deg(), at_station.longitude.deg())
    };
  }
  for (const auto observed : std::array { before, after }) {
    if (not std::isfinite(observed.value) or not finite_nonnegative(observed.error)) {
      throw std::runtime_error {
        std::format("Invalid event-search contact-side observation {} +/- {} at target {} deg", observed.value, observed.error, target.deg())
      };
    }
  }
  const int32_t before_sign = sign(before);
  const int32_t after_sign = sign(after);
  if (before_sign == 0 or after_sign == 0) {
    throw std::runtime_error {
      std::format(
        "Event-search contact at {} deg has unresolved sides: {} +/- {}, {} +/- {}",
        target.deg(), before.value, before.error, after.value, after.error
      )
    };
  }
  if (before_sign != after_sign) {
    return before_sign < after_sign ? ContactDirection::INCREASING : ContactDirection::DECREASING;
  }
  if (curvature != before_sign) {
    throw std::runtime_error {
      std::format("Event-search contact at {} deg has curvature certificate {}, expected {}", target.deg(), curvature, before_sign)
    };
  }
  return ContactDirection::TANGENT;
}

/**
 * @brief Isolate longitude-level roots after excluding distant cells or certifying a continuous scalar lift.
 * @note Bounds cover the smooth angular lift on the whole closed cell. Unresolved contacts remain candidates.
 */
template <typename Provider>
requires std::invocable<const Provider&, double>
     and std::same_as<std::invoke_result_t<const Provider&, double>, LongitudeValue>
[[nodiscard]] inline auto isolate_longitude_cell(
  const Provider& provider,
  const toolbox::AngleDeg target,
  const Interval cell,
  const Bounds bounds,
  Budget& budget,
  const EndpointOwnership ownership = EndpointOwnership::LEFT
) -> std::vector<Candidate> {
  validate_cell(cell.start_jde_tt, cell.end_jde_tt, bounds);
  const auto canonical_target = toolbox::canonical_longitude_difference(target);
  const auto projected = [&](const double jde_tt) {
    return project_longitude_difference(provider(jde_tt), { .longitude = canonical_target, .error_deg = 0.0 });
  };
  const double middle_jde_tt = std::midpoint(cell.start_jde_tt, cell.end_jde_tt);
  const auto middle = sample(projected, middle_jde_tt, budget);
  const double left_radius = upper(middle_jde_tt - cell.start_jde_tt);
  const double right_radius = upper(cell.end_jde_tt - middle_jde_tt);
  const double radius = std::max(left_radius, right_radius);
  const double excursion = upper(bounds.first_derivative * radius);
  if (magnitude_floor(middle.evaluated) > excursion) {
    return {};
  }
  const double middle_extent = upper(std::fabs(middle.evaluated.value) + middle.evaluated.error);
  const double cell_extent = upper(middle_extent + excursion);
  if (cell_extent >= toolbox::LONGITUDE_PERIOD_DEG / 2.0) {
    throw std::runtime_error {
      std::format(
        "Event-search longitude cell [{}, {}] JDE(TT) has no continuous-lift certificate: extent {} deg at target {} deg",
        cell.start_jde_tt, cell.end_jde_tt, cell_extent, canonical_target.deg()
      )
    };
  }
  const auto lifted = [&](const double jde_tt) { return longitude_residual(provider(jde_tt), canonical_target); };
  return isolate_cell(lifted, cell.start_jde_tt, cell.end_jde_tt, bounds, budget, ownership);
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
  throw std::runtime_error {
    std::format(
      "Event-search evaluation error at JDE(TT) {} requires radius {}, exceeding time resolution {} days (slope floor {})",
      observed.jde_tt, radius, time_resolution, minimum_slope
    )
  };
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
    if (precision_limit) {
      return { .jde_tt = middle_jde_tt, .increasing = increasing };
    }
    if (radius <= time_resolution) {
      const double left_magnitude = std::fabs(left.evaluated.value);
      const double magnitude_sum = left_magnitude + std::fabs(right.evaluated.value);
      if (std::isfinite(magnitude_sum) and magnitude_sum > 0.0) {
        const double trial_jde_tt = std::lerp(left.jde_tt, right.jde_tt, left_magnitude / magnitude_sum);
        if (left.jde_tt < trial_jde_tt and trial_jde_tt < right.jde_tt) {
          const auto trial = sample(provider, trial_jde_tt, budget);
          const double magnitude = upper(std::fabs(trial.evaluated.value) + trial.evaluated.error);
          const double trial_radius = upper(magnitude / minimum_slope);
          // A secant trial improves the representative only when its value allowance certifies the same time bound.
          if (trial_radius <= time_resolution) {
            return { .jde_tt = trial_jde_tt, .increasing = increasing };
          }
        }
      }
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
  throw std::runtime_error {
    std::format(
      "Event-search refinement exhausted {} iterations in [{}, {}] JDE(TT) at time resolution {} days; {} evaluations remain",
      maximum_iterations, left.jde_tt, right.jde_tt, time_resolution, budget.evaluations_remaining
    )
  };
}

/**
 * @brief Refine a certified unique root to the requested time resolution or adjacent doubles.
 * @param maximum_iterations Work cap; 96 bisections exceed the binary64 significand width, but cannot
 *        overcome unresolved observation error. Exhaustion throws without a post-budget acceptance check.
 */
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
    throw std::invalid_argument {
      std::format(
        "Invalid event-search refinement request in [{}, {}] JDE(TT): isolation {}, time resolution {} days",
        candidate.left.jde_tt, candidate.right.jde_tt, static_cast<uint32_t>(candidate.isolation), time_resolution
      )
    };
  }
  const double floor = derivative_floor(candidate.left, candidate.right, bounds);
  if (not (floor > 0.0)) {
    throw std::runtime_error {
      std::format("Event-search root in [{}, {}] JDE(TT) has no positive derivative certificate: {}", candidate.left.jde_tt, candidate.right.jde_tt, floor)
    };
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
    throw std::runtime_error {
      std::format(
        "Event-search root in [{}, {}] JDE(TT) is not bracketed by robust signs: {} +/- {}, {} +/- {}",
        candidate.left.jde_tt, candidate.right.jde_tt, candidate.left.evaluated.value, candidate.left.evaluated.error,
        candidate.right.evaluated.value, candidate.right.evaluated.error
      )
    };
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
    throw std::invalid_argument {
      std::format("Event-search grid cannot represent distinct cells: width {} days, spacing {} days", grid.cell_width_tt_days, spacing)
    };
  }
  const double span = grid.domain.end_jde_tt - grid.domain.start_jde_tt;
  const double approximate_count = std::ceil(span / grid.cell_width_tt_days);
  const double count = std::max(1.0, approximate_count);
  const bool invalid_count = not std::isfinite(count) or count >= std::numeric_limits<uint32_t>::max();
  if (invalid_count) {
    throw std::invalid_argument {
      std::format("Event-search grid needs {} cells for span {} days at width {} days, exceeding uint32_t indexing", count, span, grid.cell_width_tt_days)
    };
  }

  auto cell_count = static_cast<uint32_t>(count);
  const auto empty_tail = [&grid, &cell_count] {
    return cell_count > 1 and grid_time(grid, cell_count - 1, cell_count) >= grid.domain.end_jde_tt;
  };
  if (empty_tail()) {
    --cell_count;
  }
  if (empty_tail()) {
    throw std::invalid_argument {
      std::format("Event-search grid has indistinguishable terminal cells at JDE(TT) {} with width {} days", grid.domain.end_jde_tt, grid.cell_width_tt_days)
    };
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
    throw std::runtime_error {
      std::format("Event-search JDE(TT) {} cannot be assigned to cell {} of {} in [{}, {}]", jde_tt, index, cell_count, grid.domain.start_jde_tt, grid.domain.end_jde_tt)
    };
  }
  return index;
}

/** @brief Visit whole canonical cells touching a query, including its preceding cell for rounded roots. */
template <typename Visitor>
requires std::invocable<const Visitor&, Interval>
inline auto visit_query_cells(const Grid grid, const Interval query, const Visitor& visitor) -> void {
  validate_cell(query.start_jde_tt, query.end_jde_tt, {});
  const bool outside_domain = query.start_jde_tt < grid.domain.start_jde_tt
                             or query.end_jde_tt > grid.domain.end_jde_tt;
  if (outside_domain) {
    throw std::invalid_argument {
      std::format(
        "Event-search query [{}, {}) JDE(TT) leaves grid domain [{}, {}]",
        query.start_jde_tt, query.end_jde_tt, grid.domain.start_jde_tt, grid.domain.end_jde_tt
      )
    };
  }
  const uint32_t cell_count = grid_cell_count(grid);
  const uint32_t containing = containing_cell(grid, cell_count, query.start_jde_tt);
  // An interior root in the preceding cell can round to this cell's start.
  const uint32_t first = containing == 0 ? 0 : containing - 1;
  for (uint32_t index = first; index < cell_count; ++index) {
    const double start_jde_tt = grid_time(grid, index, cell_count);
    if (start_jde_tt >= query.end_jde_tt) {
      break;
    }
    visitor(Interval {
      .start_jde_tt = start_jde_tt,
      .end_jde_tt = grid_time(grid, index + 1, cell_count),
    });
  }
}

/**
 * @brief Round a reported TT time to a binary lattice; zero quantum preserves the refined double.
 * @note Requires a validated reporting quantum and nearest-even rounding.
 */
[[nodiscard]] inline auto reported_time(const double jde_tt, const double quantum_tt_days) -> double {
  return quantum_tt_days == 0.0 ? jde_tt : std::nearbyint(jde_tt / quantum_tt_days) * quantum_tt_days;
}

/** @brief Require a power-of-two reporting lattice that fixes every canonical cell boundary. */
inline auto validate_reporting_grid(const Grid grid, const double quantum_tt_days) -> void {
  if (quantum_tt_days == 0.0) {
    return;
  }
  if (not finite_nonnegative(quantum_tt_days)) {
    throw std::invalid_argument { std::format("Invalid event-search reporting quantum {} TT days", quantum_tt_days) };
  }
  int32_t exponent = 0;
  const double fraction = std::frexp(quantum_tt_days, &exponent);
  const double magnitude = std::max(std::fabs(grid.domain.start_jde_tt), std::fabs(grid.domain.end_jde_tt));
  const bool exact_indices = fraction == 0.5 and magnitude / quantum_tt_days <= 0x1p53;
  if (not exact_indices) {
    throw std::invalid_argument {
      std::format("Event-search reporting lattice cannot represent exact indices: quantum {} TT days, maximum |JDE| {}", quantum_tt_days, magnitude)
    };
  }
  const bool fixed_boundaries = std::remainder(grid.domain.start_jde_tt, quantum_tt_days) == 0.0
                                and std::remainder(grid.domain.end_jde_tt, quantum_tt_days) == 0.0
                                and std::remainder(grid.cell_width_tt_days, quantum_tt_days) == 0.0;
  if (not fixed_boundaries) {
    throw std::invalid_argument {
      std::format(
        "Event-search reporting quantum {} TT days does not preserve grid [{}, {}] at width {} days",
        quantum_tt_days, grid.domain.start_jde_tt, grid.domain.end_jde_tt, grid.cell_width_tt_days
      )
    };
  }
}

/**
 * @brief Append only query-owned roots; intersecting unresolved regions and aliased root times fail.
 * @note roots retains earlier cells so the strict-order guard also covers cell seams.
 */
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
  std::vector<Root>& roots,
  const double report_quantum_tt_days = 0.0
) -> void {
  for (const auto& candidate : candidates) {
    const bool outside_query = reported_time(candidate.right.jde_tt, report_quantum_tt_days) < query.start_jde_tt
                               or reported_time(candidate.left.jde_tt, report_quantum_tt_days) >= query.end_jde_tt;
    if (outside_query) {
      continue;
    }
    if (candidate.isolation != Isolation::UNIQUE) {
      throw std::runtime_error {
        std::format(
          "Unresolved event-search region intersects the query: region [{}, {}], query [{}, {}) JDE(TT)",
          candidate.left.jde_tt, candidate.right.jde_tt, query.start_jde_tt, query.end_jde_tt
        )
      };
    }
    const auto refined = refine(provider, candidate, bounds, budget, time_resolution);
    const Root root {
      .jde_tt = reported_time(refined.jde_tt, report_quantum_tt_days),
      .increasing = refined.increasing,
    };
    const bool query_owned = query.start_jde_tt <= root.jde_tt and root.jde_tt < query.end_jde_tt;
    if (not query_owned) {
      continue;
    }
    if (not roots.empty() and root.jde_tt <= roots.back().jde_tt) {
      throw std::runtime_error {
        std::format("Event-search roots at JDE(TT) {} and {} cannot be separated with reporting quantum {} days", roots.back().jde_tt, root.jde_tt, report_quantum_tt_days)
      };
    }
    roots.push_back(root);
  }
}

/** @brief A station's reported TT instant and contact outcome; an uncertified one has no two-sided region. */
struct StationContact {
  double station_jde_tt {};
  bool certified {};
  std::optional<NumericalContact> contact;
};

/** @brief A contact region's certified direction, if any; crosses_level marks a level sign change across it. */
struct RegionDirection {
  std::optional<ContactDirection> direction;
  bool crosses_level {};
};

/**
 * @brief Return the fixed neighbor window of a canonical cell, clipped to the grid domain.
 * @note Depends only on the cell index, never on the query, budget or target.
 */
[[nodiscard]] inline auto contact_window(
  const Grid grid,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish a cell index from the cell count.
  const uint32_t index,
  const uint32_t cell_count
) -> Interval {
  return {
    .start_jde_tt = grid_time(grid, index == 0 ? 0 : index - 1, cell_count),
    .end_jde_tt = grid_time(grid, std::min(index + 2, cell_count), cell_count),
  };
}

/**
 * @brief Certify one exact-level numerical contact, trying its cell before its fixed neighbor window.
 * @note The stationary instant uses the cell's rate provider and bounds, as stations() does. Window samples use
 *       providers and bounds covering the whole window, which never extends past a grid-domain endpoint.
 */
template <typename LongitudeProvider, typename RateProvider, typename WindowLongitudeProvider, typename WindowRateProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, LongitudeValue>
     and std::invocable<const RateProvider&, double>
     and std::same_as<std::invoke_result_t<const RateProvider&, double>, Value>
     and std::invocable<const WindowLongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const WindowLongitudeProvider&, double>, LongitudeValue>
     and std::invocable<const WindowRateProvider&, double>
     and std::same_as<std::invoke_result_t<const WindowRateProvider&, double>, Value>
[[nodiscard]] inline auto certify_contact(
  const LongitudeProvider& longitude,
  const RateProvider& rate,
  const WindowLongitudeProvider& window_longitude,
  const WindowRateProvider& window_rate,
  const Candidate& candidate,
  const Interval cell,
  const Interval window,
  const toolbox::AngleDeg target,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish owning-cell from window allowances.
  const TrajectoryBounds bounds,
  const TrajectoryBounds window_bounds,
  Budget& budget,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish refinement accuracy from reporting precision.
  const double time_resolution,
  const double report_quantum_tt_days
) -> StationContact {
  const auto stationary = refine(rate, candidate, bounds.rate, budget, time_resolution);
  const double jde_tt = reported_time(stationary.jde_tt, report_quantum_tt_days);
  const auto projected = [&](const double time) {
    return project_longitude_difference(longitude(time), { .longitude = target, .error_deg = 0.0 });
  };
  const auto at_station = sample(projected, jde_tt, budget);
  if (at_station.evaluated.value != 0.0) {
    return { .station_jde_tt = jde_tt, .certified = true, .contact = std::nullopt };
  }
  const int32_t expected_curvature = stationary.increasing ? 1 : -1;
  const auto region_direction = [&](const Interval region,
    const auto& region_rate,
    const auto& region_longitude,
    const TrajectoryBounds region_bounds
  ) -> RegionDirection {
    const double left_jde_tt = region.start_jde_tt;
    const double right_jde_tt = region.end_jde_tt;
    const bool interior_reference = left_jde_tt < jde_tt and jde_tt < right_jde_tt;
    if (not interior_reference) {
      return { .direction = std::nullopt, .crosses_level = false };
    }
    const auto left_rate = left_jde_tt == candidate.left.jde_tt ? candidate.left : sample(region_rate, left_jde_tt, budget);
    const auto right_rate = right_jde_tt == candidate.right.jde_tt ? candidate.right : sample(region_rate, right_jde_tt, budget);
    const bool contains_extremum = magnitude_floor(left_rate.evaluated) > region_bounds.rate_remainder
                                   and magnitude_floor(right_rate.evaluated) > region_bounds.rate_remainder
                                   and sign(left_rate.evaluated) == -expected_curvature
                                   and sign(right_rate.evaluated) == expected_curvature;
    if (not contains_extremum) {
      return { .direction = std::nullopt, .crosses_level = false };
    }
    const double region_radius = std::max(upper(jde_tt - left_jde_tt), upper(right_jde_tt - jde_tt));
    const double extent = upper(at_station.evaluated.error + upper(region_bounds.longitude.first_derivative * region_radius));
    if (extent >= toolbox::LONGITUDE_PERIOD_DEG / 2.0) {
      return { .direction = std::nullopt, .crosses_level = false };
    }
    const auto lifted = [&](const double time) { return longitude_residual(region_longitude(time), target); };
    const auto before = sample(lifted, left_jde_tt, budget);
    const auto after = sample(lifted, right_jde_tt, budget);
    const int32_t before_sign = sign(before.evaluated);
    const int32_t after_sign = sign(after.evaluated);
    const bool crosses_level = before_sign != 0 and after_sign == -before_sign;
    if (crosses_level) {
      return { .direction = std::nullopt, .crosses_level = true };
    }
    const int32_t curvature = curvature_sign(before, at_station, after, region_bounds.third_derivative);
    const bool unresolved_sides = before_sign == 0 or after_sign == 0 or curvature != expected_curvature;
    if (unresolved_sides) {
      return { .direction = std::nullopt, .crosses_level = false };
    }
    return {
      .direction = contact_direction(
        { .longitude = toolbox::AngleDeg { at_station.evaluated.value }, .error_deg = at_station.evaluated.error },
        toolbox::AngleDeg { 0.0 },
        before.evaluated,
        after.evaluated,
        curvature
      ),
      .crosses_level = false,
    };
  };
  const auto centered = [jde_tt](const Interval span, const double radius) -> Interval {
    return { .start_jde_tt = std::max(span.start_jde_tt, jde_tt - radius),
             .end_jde_tt = std::min(span.end_jde_tt, jde_tt + radius) };
  };
  const auto certified = [jde_tt](const ContactDirection direction, const Interval region) -> StationContact {
    return {
      .station_jde_tt = jde_tt,
      .certified = true,
      .contact = NumericalContact { .root = { .jde_tt = jde_tt, .direction = direction }, .region = region },
    };
  };
  const Interval isolated { .start_jde_tt = candidate.left.jde_tt, .end_jde_tt = candidate.right.jde_tt };
  const double available_radius = std::min(jde_tt - cell.start_jde_tt, cell.end_jde_tt - jde_tt);
  for (uint32_t attempt = 0; attempt < MAXIMUM_CONTACT_REGION_ATTEMPTS; ++attempt) {
    const Interval region = [&] {
      if (attempt == 0) {
        return isolated;
      }
      return attempt == 1 ? cell : centered(cell, std::ldexp(available_radius, -static_cast<int32_t>(attempt)));
    }();
    const auto outcome = region_direction(region, rate, longitude, bounds);
    if (outcome.direction.has_value()) {
      return certified(*outcome.direction, region);
    }
    if (outcome.crosses_level and attempt == 0) {
      return { .station_jde_tt = jde_tt, .certified = true, .contact = std::nullopt };
    }
  }
  const bool has_neighbor = window.start_jde_tt < cell.start_jde_tt or cell.end_jde_tt < window.end_jde_tt;
  if (not has_neighbor) {
    return { .station_jde_tt = jde_tt, .certified = false, .contact = std::nullopt };
  }
  // Try neighboring cells only after all in-cell regions fail, so in-cell successes keep their regions.
  // A station on a cell boundary needs these two-sided observations.
  const double window_radius = std::min(jde_tt - window.start_jde_tt, window.end_jde_tt - jde_tt);
  for (uint32_t attempt = 0; attempt < MAXIMUM_CONTACT_REGION_ATTEMPTS; ++attempt) {
    const Interval region = attempt == 0
                              ? window
                              : centered(window, std::ldexp(window_radius, 1 - static_cast<int32_t>(attempt)));
    const auto outcome = region_direction(region, window_rate, window_longitude, window_bounds);
    if (outcome.direction.has_value()) {
      return certified(*outcome.direction, region);
    }
  }
  return { .station_jde_tt = jde_tt, .certified = false, .contact = std::nullopt };
}

/**
 * @brief Verify isolated stations in a fixed grid cell using observations on both sides.
 * @note A reported instant inside the query requires two-sided evidence, including at cell boundaries.
 * @note Providers and bounds cover the cell; the window providers and bounds cover its @ref contact_window.
 */
template <typename LongitudeProvider, typename RateProvider, typename WindowLongitudeProvider, typename WindowRateProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, LongitudeValue>
     and std::invocable<const RateProvider&, double>
     and std::same_as<std::invoke_result_t<const RateProvider&, double>, Value>
     and std::invocable<const WindowLongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const WindowLongitudeProvider&, double>, LongitudeValue>
     and std::invocable<const WindowRateProvider&, double>
     and std::same_as<std::invoke_result_t<const WindowRateProvider&, double>, Value>
[[nodiscard]] inline auto cell_contacts(
  const LongitudeProvider& longitude,
  const RateProvider& rate,
  const WindowLongitudeProvider& window_longitude,
  const WindowRateProvider& window_rate,
  const toolbox::AngleDeg target,
  const Interval cell,
  const Interval window,
  const TrajectoryBounds bounds,
  const TrajectoryBounds window_bounds,
  Budget& budget,
  const double time_resolution,
  const double report_quantum_tt_days,
  const Interval query
) -> std::vector<NumericalContact> {
  const auto stations = isolate_cell(rate, cell.start_jde_tt, cell.end_jde_tt, bounds.rate, budget);
  std::vector<NumericalContact> contacts;
  for (const auto& candidate : stations) {
    if (candidate.isolation != Isolation::UNIQUE) {
      continue;
    }
    const auto outcome = certify_contact(
      longitude,
      rate,
      window_longitude,
      window_rate,
      candidate,
      cell,
      window,
      target,
      bounds,
      window_bounds,
      budget,
      time_resolution,
      report_quantum_tt_days
    );
    const bool query_owned = query.start_jde_tt <= outcome.station_jde_tt and outcome.station_jde_tt < query.end_jde_tt;
    if (query_owned and not outcome.certified) {
      throw std::runtime_error {
        std::format(
          "Event-search numerical contact has no certified two-sided extremum region at JDE(TT) {}; cell [{}, {}], window [{}, {}]",
          outcome.station_jde_tt, cell.start_jde_tt, cell.end_jde_tt, window.start_jde_tt, window.end_jde_tt
        )
      };
    }
    if (outcome.contact.has_value()) {
      contacts.push_back(*outcome.contact);
    }
  }
  return contacts;
}

/** @brief Scan one visited cell for crossings outside its sorted, disjoint contact exclusions. */
template <typename LongitudeProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, LongitudeValue>
[[nodiscard]] inline auto cell_crossings(
  const LongitudeProvider& longitude,
  const toolbox::AngleDeg target,
  const Interval cell,
  const std::span<const Interval> exclusions,
  const Interval query,
  const Bounds bounds,
  Budget& budget,
  const double time_resolution,
  const double report_quantum_tt_days
) -> std::vector<Root> {
  const auto lifted = [&](const double jde_tt) { return longitude_residual(longitude(jde_tt), target); };
  std::vector<Root> crossings;
  const auto search_segment = [&](const Interval segment) {
    if (segment.start_jde_tt == segment.end_jde_tt) {
      return;
    }
    const auto candidates = isolate_longitude_cell(longitude, target, segment, bounds, budget);
    append_interval_roots(lifted, candidates, query, bounds, budget, time_resolution, crossings, report_quantum_tt_days);
  };
  double start_jde_tt = cell.start_jde_tt;
  for (const auto region : exclusions) {
    search_segment({ .start_jde_tt = start_jde_tt, .end_jde_tt = region.start_jde_tt });
    start_jde_tt = region.end_jde_tt;
  }
  search_segment({ .start_jde_tt = start_jde_tt, .end_jde_tt = cell.end_jde_tt });
  return crossings;
}

/**
 * @brief Find crossings and exact contacts, reporting each contact from the cell containing its reported time.
 * @note Visited cells are those of @ref visit_query_cells; one lookahead cell per side supplies contact regions
 *       that reach them. Every output is filtered by its reported time, and overlapping exclusions throw.
 */
template <typename Contacts, typename Crossings>
requires std::invocable<const Contacts&, Interval, Interval>
     and std::same_as<std::invoke_result_t<const Contacts&, Interval, Interval>, std::vector<NumericalContact>>
     and std::invocable<const Crossings&, Interval, std::span<const Interval>>
     and std::same_as<std::invoke_result_t<const Crossings&, Interval, std::span<const Interval>>, std::vector<Root>>
[[nodiscard]] inline auto contact_traversal(
  const Grid grid,
  const Interval query,
  const Contacts& contacts,
  const Crossings& crossings
) -> std::vector<LongitudeRoot> {
  std::vector<uint32_t> visited;
  const uint32_t cell_count = grid_cell_count(grid);
  visit_query_cells(grid, query, [&](const Interval cell) {
    visited.push_back(containing_cell(grid, cell_count, cell.start_jde_tt));
  });

  const uint32_t first_cell = visited.at(0);
  const uint32_t last_cell = visited.at(visited.size() - 1);
  const uint32_t lookahead_first = first_cell == 0 ? 0 : first_cell - 1;
  const uint32_t lookahead_last = std::min(last_cell + 1, cell_count - 1);

  const auto cell_at = [&](const uint32_t index) -> Interval {
    return { .start_jde_tt = grid_time(grid, index, cell_count), .end_jde_tt = grid_time(grid, index + 1, cell_count) };
  };

  std::vector<std::vector<NumericalContact>> owned;
  for (uint32_t index = lookahead_first; index <= lookahead_last; ++index) {
    owned.push_back(contacts(cell_at(index), contact_window(grid, index, cell_count)));
  }

  const auto query_owned = [query](const NumericalContact& contact) {
    return query.start_jde_tt <= contact.root.jde_tt and contact.root.jde_tt < query.end_jde_tt;
  };
  auto results = owned | std::views::join | std::views::filter(query_owned)
                      | std::views::transform(&NumericalContact::root) | std::ranges::to<std::vector>();

  for (const uint32_t index : visited) {
    const auto cell = cell_at(index);
    std::vector<Interval> exclusions;
    const uint32_t owner_first = std::max(index == 0 ? 0 : index - 1, lookahead_first);
    const uint32_t owner_last = std::min(index + 1, lookahead_last);
    for (uint32_t owner = owner_first; owner <= owner_last; ++owner) {
      for (const auto contact : owned.at(owner - lookahead_first)) {
        const Interval clipped {
          .start_jde_tt = std::max(contact.region.start_jde_tt, cell.start_jde_tt),
          .end_jde_tt = std::min(contact.region.end_jde_tt, cell.end_jde_tt),
        };
        if (clipped.start_jde_tt < clipped.end_jde_tt) {
          exclusions.push_back(clipped);
        }
      }
    }

    std::ranges::sort(exclusions, {}, &Interval::start_jde_tt);
    const auto overlaps = [](const Interval left, const Interval right) {
      return right.start_jde_tt < left.end_jde_tt;
    };
    // NOLINTNEXTLINE(readability-qualified-auto): Iterator representation varies by standard library.
    const auto overlap = std::ranges::adjacent_find(exclusions, overlaps);
    if (overlap != exclusions.end()) {
      throw std::runtime_error {
        std::format("Event-search numerical contact brackets overlap after [{}, {}] JDE(TT)", overlap->start_jde_tt, overlap->end_jde_tt)
      };
    }

    for (const auto crossing : crossings(cell, std::span<const Interval> { exclusions })) {
      results.push_back({
        .jde_tt = crossing.jde_tt,
        .direction = crossing.increasing ? ContactDirection::INCREASING : ContactDirection::DECREASING,
      });
    }
  }

  std::ranges::sort(results, {}, &LongitudeRoot::jde_tt);
  const auto unordered = [](const LongitudeRoot left, const LongitudeRoot right) {
    return right.jde_tt <= left.jde_tt;
  };
  // NOLINTNEXTLINE(readability-qualified-auto): Iterator representation varies by standard library.
  const auto collision = std::ranges::adjacent_find(results, unordered);
  if (collision != results.end()) {
    throw std::runtime_error {
      std::format("Event-search contacts cannot be separated at the reported time precision after JDE(TT) {}", collision->jde_tt)
    };
  }
  return results;
}

} // namespace astro::detail::event_search
