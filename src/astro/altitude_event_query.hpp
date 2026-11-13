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
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "altitude_event_certificate.hpp"
#include "altitude_event_windows.hpp"
#include "ephemeris.hpp"
#include "ephemeris_events.hpp"
#include "event_search.hpp"
#include "julian_day.hpp"
#include "observer.hpp"
#include "toolbox.hpp"

namespace astro::observer::detail::altitude_events {

namespace certificates = astro::detail::altitude_bounds;

enum class Kind : uint8_t { ALTITUDE, TRANSITS, WINDOWS };

/** @brief Decoded condition storage; convention 2 is geometric and carries no atmospheric computation. */
struct Request {
  uint32_t target {};
  double level {};
  bool above {};
  uint32_t convention {};
  earth::refraction::Params params;
};

/** @brief A complete query result, published only after every condition and proof piece succeeds. */
struct Computed {
  std::vector<Condition> conditions;
  Result composed;
  std::vector<double> transit_times;
};

inline constexpr std::size_t MAX_CONDITIONS = 2048;
inline constexpr double TIME_RESOLUTION_TT_DAYS = 0x1p-20;
inline constexpr double LEVEL_RESOLUTION_DEGREES = 0x1p-30;
inline constexpr double RADIANS_PER_DEGREE_UPPER = 0x1.1df46a2529d3ap-6;

/** @brief One fresh budget shared by all conditions and proof pieces of a complete query. */
[[nodiscard]] inline auto query_budget() -> search::Budget {
  return { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
}

/** @brief Validate one physical target against the actual position domain, not certificate availability. */
inline auto validate_target(const uint32_t target, const search::Interval query) -> void {
  if (target > 9) {
    throw std::invalid_argument { "A physical target is required" };
  }
  const auto domain = ephemeris::target_facts(static_cast<ephemeris::Target>(target)).position_domain;
  if (domain.has_value() and (query.start_jde_tt < domain->at(0) or query.end_jde_tt > domain->at(1))) {
    throw std::invalid_argument { "The query leaves its target's position domain" };
  }
}

/** @brief Admit half-open TT queries, including the inverse calendar's exclusive upper endpoint. */
inline auto validate_query(
  const Kind kind,
  const Observer& observer,
  const search::Interval query,
  const std::span<const Request> requests,
  const uint32_t target
) -> void {
  if (kind != Kind::ALTITUDE and kind != Kind::TRANSITS and kind != Kind::WINDOWS) {
    throw std::invalid_argument { "Unknown query kind" };
  }
  if ((kind == Kind::ALTITUDE and requests.size() != 1) or (kind == Kind::TRANSITS and not requests.empty())) {
    throw std::invalid_argument { "The query has the wrong condition shape" };
  }
  astro::observer::detail::validate(observer);
  const bool valid_interval = std::isfinite(query.start_jde_tt) and std::isfinite(query.end_jde_tt)
    and query.start_jde_tt < query.end_jde_tt;
  if (not valid_interval) {
    throw std::invalid_argument { "Interval endpoints must be finite with start < end" };
  }
  static_cast<void>(julian_day::jde_to_tt(query.start_jde_tt));
  const double admitted_end = query.end_jde_tt == 13689325.5
    ? std::nextafter(query.end_jde_tt, query.start_jde_tt) : query.end_jde_tt;
  static_cast<void>(julian_day::jde_to_tt(admitted_end));
  if (kind == Kind::TRANSITS) {
    validate_target(target, query);
  }
  if (requests.size() > MAX_CONDITIONS) {
    throw std::runtime_error { "The condition-list work budget is exhausted" };
  }

  for (const auto& request : requests) {
    validate_target(request.target, query);
    if (not std::isfinite(request.level) or request.convention > 2) {
      throw std::invalid_argument { "Invalid level or convention" };
    }
    if (request.convention == 2) {
      const bool inside_geometric_range = -90.0 < request.level and request.level < 90.0;
      if (not inside_geometric_range) {
        throw std::invalid_argument { "Geometric levels must be strictly between -90 and 90 degrees" };
      }
    } else {
      earth::refraction::detail::validate_params(request.params);
      if (static_cast<uint32_t>(request.params.model) != request.convention) {
        throw std::invalid_argument { "The requested refraction convention and model disagree" };
      }
    }
  }
}

/** @brief Complete canonical-quarter coverage and its origin-preserving closed proof pieces. */
struct Schedule {
  search::Interval census;
  std::vector<certificates::ClockPiece> pieces;
};

/** @brief Include the admitted preceding quarter only for an exactly aligned query start. */
[[nodiscard]] inline auto schedule(
  const Kind kind,
  const search::Interval query,
  const std::span<const Request> requests,
  const uint32_t target,
  search::Budget& budget
) -> Schedule {
  const search::Interval census {
    .start_jde_tt = 2451545.0 + (std::floor((query.start_jde_tt - 2451545.0) * 4.0) / 4.0),
    .end_jde_tt = 2451545.0 + (std::ceil((query.end_jde_tt - 2451545.0) * 4.0) / 4.0),
  };
  double admitted_start = 1867522.5;
  const auto include_domain = [&](const uint32_t code) {
    const auto domain = ephemeris::target_facts(static_cast<ephemeris::Target>(code)).position_domain;
    if (domain.has_value()) {
      admitted_start = std::max(admitted_start, domain->at(0));
    }
  };
  if (kind == Kind::TRANSITS) {
    include_domain(target);
  }
  for (const auto& request : requests) {
    include_domain(request.target);
  }
  const double visited_start = query.start_jde_tt == census.start_jde_tt
    ? std::max(admitted_start, census.start_jde_tt - 0.25) : census.start_jde_tt;
  const double cell_count = (census.end_jde_tt - visited_start) * 4.0;
  if (not std::isfinite(cell_count) or cell_count > static_cast<double>(budget.candidates_remaining)) {
    throw std::runtime_error { "The canonical-cell work budget is exhausted" };
  }

  std::vector<certificates::ClockPiece> pieces;
  for (uint32_t index = 0; index < static_cast<uint32_t>(cell_count); ++index) {
    const double left = visited_start + (static_cast<double>(index) / 4.0);
    const auto partition = certificates::clock_pieces(left, left + 0.25, budget.candidates_remaining - pieces.size());
    for (const auto& piece : partition) {
      pieces.push_back(piece);
    }
  }
  budget.candidates_remaining -= pieces.size();
  return { .census = census, .pieces = std::move(pieces) };
}

/** @brief Bound altitude and meridian objectives with the same complete source/site certificate. */
[[nodiscard]] inline auto trajectory_bounds(const certificates::ObserverInputs& input) -> search::Bounds {
  const auto composed = certificates::observer_c2::compose(input.source, input.site, 1.0);
  return {
    .first_derivative = std::max(composed.first, composed.local_vector.at(1)),
    .second_derivative = std::max(composed.second, composed.local_vector.at(2)),
  };
}

/** @brief Resolve a level only after reserving its full possible inverse charge from the query budget. */
[[nodiscard]] inline auto resolve_level(const Request& request, search::Budget& budget) -> Level {
  if (request.convention == 2) {
    return { .geometric_degrees = request.level, .error_degrees = 0.0, .evaluation_charge = 0 };
  }
  const auto certificate = certificates::REFRACTION.at(request.convention);
  const std::size_t maximum_charge = request.convention == 0 ? 1 : SAEMUNDSSON_INVERSE_MAX_CHARGE;
  if (budget.evaluations_remaining < maximum_charge) {
    throw std::runtime_error { "The shared inverse budget is exhausted" };
  }
  const auto level = inverse(
    request.level,
    request.params,
    certificate.map_error,
    certificate.residual_error,
    certificate.slope_lower,
    LEVEL_RESOLUTION_DEGREES
  );
  if (level.evaluation_charge > maximum_charge) {
    throw std::runtime_error { "The inverse exceeded its reserved work" };
  }
  budget.evaluations_remaining -= level.evaluation_charge;
  return level;
}

/** @brief Seed from the containing quarter; retain preceding-piece companions without adopting its seed. */
[[nodiscard]] inline auto condition_census(
  const Request& request,
  const Observer& observer,
  const search::Interval query,
  const Schedule& scheduled,
  search::Budget& budget
) -> Condition {
  const auto level = resolve_level(request, budget);
  const double sine_transport = search::upper(RADIANS_PER_DEGREE_UPPER * level.error_degrees);
  Condition condition {
    .complete = true, .seed_above = false, .above = request.above, .events = {}, .level = level, .residual_error = 0.0,
  };
  bool need_seed = true;
  for (const auto& piece : scheduled.pieces) {
    const auto inputs = certificates::observer_for_piece(request.target, observer.location.latitude.deg(), observer.elevation.km(), piece);
    const auto bounds = trajectory_bounds(inputs);
    const double residual_error = search::upper(inputs.value_error + search::upper(inputs.q_upper * sine_transport));
    condition.residual_error = std::max(condition.residual_error, residual_error);
    const search::Interval interval { .start_jde_tt = piece.start_jde_tt, .end_jde_tt = piece.end_jde_tt };
    const Altitude provider {
      .target = static_cast<ephemeris::Target>(request.target), .observer = observer,
      .level_sine = std::sin(toolbox::AngleDeg { level.geometric_degrees }.rad()),
      .residual_error = residual_error, .envelope = interval, .meridian = false,
    };
    if (need_seed and piece.start_jde_tt >= scheduled.census.start_jde_tt) {
      const int32_t seed = search::sign(search::sample(provider, scheduled.census.start_jde_tt, budget).evaluated);
      if (seed == 0) {
        throw std::runtime_error { "The dynamic window seed is unresolved" };
      }
      condition.seed_above = seed > 0;
      need_seed = false;
    }
    if (piece.collar) {
      const auto left = search::sample(provider, interval.start_jde_tt, budget);
      const auto right = search::sample(provider, interval.end_jde_tt, budget);
      static_cast<void>(certify_clock_collar(left, right, bounds.first_derivative, piece.jump));
    } else {
      for (const auto& event : events(provider, interval, bounds, budget, TIME_RESOLUTION_TT_DAYS)) {
        if (piece.start_jde_tt >= scheduled.census.start_jde_tt or event.jde_tt >= query.start_jde_tt) {
          condition.events.push_back({
            .jde_tt = event.jde_tt, .change = event.increasing ? 1 : -1, .origin_start_jde_tt = piece.start_jde_tt,
          });
        }
      }
    }
  }
  return condition;
}

/** @brief Certify every meridian root, then retain only upper (forward-side) passages in the half-open query. */
[[nodiscard]] inline auto transit_census(
  const uint32_t target,
  const Observer& observer,
  const search::Interval query,
  const Schedule& scheduled,
  search::Budget& budget
) -> std::vector<double> {
  std::vector<double> times;
  for (const auto& piece : scheduled.pieces) {
    const auto inputs = certificates::observer_for_piece(target, observer.location.latitude.deg(), observer.elevation.km(), piece);
    const auto bounds = trajectory_bounds(inputs);
    const search::Interval interval { .start_jde_tt = piece.start_jde_tt, .end_jde_tt = piece.end_jde_tt };
    const Altitude provider {
      .target = static_cast<ephemeris::Target>(target), .observer = observer, .level_sine = 0.0,
      .residual_error = inputs.value_error, .envelope = interval, .meridian = true,
    };
    if (piece.collar) {
      const auto left = search::sample(provider, interval.start_jde_tt, budget);
      const auto right = search::sample(provider, interval.end_jde_tt, budget);
      static_cast<void>(certify_clock_collar(left, right, bounds.first_derivative, piece.jump));
    } else {
      for (const auto& event : events(provider, interval, bounds, budget, TIME_RESOLUTION_TT_DAYS)) {
        search::charge_evaluation(event.jde_tt, budget);
        const auto observed = provider.observe(event.jde_tt);
        const double allowance = search::upper(inputs.value_error + search::upper(bounds.first_derivative * TIME_RESOLUTION_TT_DAYS));
        if (std::fabs(observed.forward) <= allowance) {
          throw std::runtime_error { "The meridian forward side is unresolved" };
        }
        if (observed.forward > allowance and query.start_jde_tt <= event.jde_tt and event.jde_tt < query.end_jde_tt) {
          times.push_back(event.jde_tt);
        }
      }
    }
  }
  return times;
}

/** @brief Use one caller-owned budget for admission, proof pieces, inverses and every actual observation. */
[[nodiscard]] inline auto compute(
  const Kind kind,
  const Observer& observer,
  const search::Interval query,
  const std::span<const Request> requests,
  const uint32_t target,
  search::Budget& budget
) -> Computed {
  validate_query(kind, observer, query, requests, target);
  ephemeris::detail::validate_event_environment();
  if (kind == Kind::WINDOWS and requests.empty()) {
    Computed result { .conditions = {}, .composed = { .states = {}, .windows = {}, .companion_events = 0, .reported_events = {} }, .transit_times = {} };
    result.composed.windows.push_back({ .start_jde_tt = query.start_jde_tt, .end_jde_tt = query.end_jde_tt });
    return result;
  }
  const auto scheduled = schedule(kind, query, requests, target, budget);
  if (kind == Kind::TRANSITS) {
    return { .conditions = {}, .composed = {}, .transit_times = transit_census(target, observer, query, scheduled, budget) };
  }

  std::vector<Condition> conditions;
  for (const auto& request : requests) {
    conditions.push_back(condition_census(request, observer, query, scheduled, budget));
  }
  auto composed = windows(conditions, query.start_jde_tt, query.end_jde_tt, scheduled.census.start_jde_tt, scheduled.census.end_jde_tt);
  return { .conditions = std::move(conditions), .composed = std::move(composed), .transit_times = {} };
}

} // namespace astro::observer::detail::altitude_events
