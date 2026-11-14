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
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "altitude_event_windows.hpp"
#include "delta_t.hpp"
#include "ephemeris.hpp"
#include "ephemeris_events.hpp"
#include "event_search.hpp"
#include "julian_day.hpp"
#include "observer.hpp"
#include "toolbox.hpp"

namespace astro::observer::detail::altitude_events {

/** @brief Select the result to compose from the shared interval search. */
enum class Kind : uint8_t { ALTITUDE, TRANSITS, WINDOWS };

/** @brief Decoded condition storage; convention 2 is geometric and carries no atmospheric computation. */
struct Request {
  uint32_t target {};
  double level {};
  bool above {};
  uint32_t convention {};
  earth::refraction::Params params;
};

/** @brief Query result, published only after every condition has finished successfully. */
struct Computed {
  std::vector<Condition> conditions;
  Result composed;
  std::vector<double> transit_times;
};

inline constexpr std::size_t MAX_CONDITIONS = 2048;

/** @brief One fresh budget shared by all conditions, inverse levels and root searches. */
[[nodiscard]] inline auto query_budget() -> search::Budget {
  return { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
}

/** @brief Validate one physical target against its position domain. */
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
      refraction::detail::validate_params(request.params);
      if (static_cast<uint32_t>(request.params.model) != request.convention) {
        throw std::invalid_argument { "The requested refraction convention and model disagree" };
      }
    }
  }
}

/** @brief The target/calendar sample domain, with its excluded upper bound moved inward by one ULP. */
[[nodiscard]] inline auto sample_domain(const uint32_t target) -> Domain {
  const auto provider = ephemeris::target_facts(static_cast<ephemeris::Target>(target)).position_domain;
  return {
    .first = provider.has_value() ? std::max(1867522.5, provider->at(0)) : 1867522.5,
    .last = search::lower(provider.has_value() ? std::min(13689325.5, provider->at(1)) : 13689325.5),
  };
}

/** @brief Fixed-grid pieces separated at actual ALGO5 model discontinuities. */
struct Schedule {
  std::vector<Grid> pieces;
  std::size_t sample_count;
};

/** @brief Plan every sample before observing a target; no stencil crosses a model jump. */
[[nodiscard]] inline auto schedule(
  const uint32_t target,
  const search::Interval query,
  const std::size_t maximum_samples
) -> Schedule {
  const auto domain = sample_domain(target);
  Schedule result { .pieces = {}, .sample_count = 0 };

  const auto append = [&](const Domain branch) {
    if (branch.first >= query.end_jde_tt or branch.last < query.start_jde_tt) {
      return;
    }

    const search::Interval part {
      .start_jde_tt = std::max(query.start_jde_tt, branch.first),
      .end_jde_tt = std::min(query.end_jde_tt, search::upper(branch.last)),
    };
    auto planned = grid(part, branch, maximum_samples - result.sample_count);
    result.sample_count += planned.times.size();
    result.pieces.push_back(std::move(planned));
  };

  double first = domain.first;
  for (const int32_t year : delta_t::algo5::DISCONTINUITY_YEARS) {
    if (year < 401) {
      continue;
    }
    const double join = julian_day::tt_to_jde(
      calendar::Datetime { std::chrono::year { year } / std::chrono::January / 1, 0.0 }
    );
    if (join <= domain.first or join > domain.last) {
      continue;
    }
    append({ .first = first, .last = search::lower(join) });
    first = join;
  }
  append({ .first = first, .last = domain.last });
  return result;
}

/** @brief Resolve only atmospheric thresholds; geometric conditions require no inverse work. */
[[nodiscard]] inline auto resolve_level(const Request& request, search::Budget& budget) -> double {
  return request.convention == 2 ? request.level : inverse(request.level, request.params, budget);
}

/** @brief Search each smooth branch and reject a condition-side change across a clock jump. */
[[nodiscard]] inline auto condition_events(
  const Request& request,
  const Observer& observer,
  const Schedule& scheduled,
  search::Budget& budget
) -> Condition {
  const double level = resolve_level(request, budget);
  Condition condition { .seed_above = false, .above = request.above, .events = {}, .geometric_degrees = level };
  bool first = true;
  bool previous_side = false;
  for (const auto& piece : scheduled.pieces) {
    const Altitude provider {
      .target = static_cast<ephemeris::Target>(request.target),
      .observer = observer,
      .level_sine = std::sin(toolbox::AngleDeg { level }.rad()),
      .domain = piece.domain,
      .meridian = false,
    };
    const auto found = scan(provider, piece, true, budget);

    if (first) {
      condition.seed_above = found.initially_above;
      first = false;
    } else if (previous_side != found.initially_above) {
      throw std::runtime_error { "Altitude crossing coincides with a Delta T model join" };
    }
    previous_side = found.finally_above;
    condition.events.insert(condition.events.end(), found.events.begin(), found.events.end());
  }
  return condition;
}

/** @brief Retain forward-side meridian crossings in the requested half-open interval. */
[[nodiscard]] inline auto transit_events(
  const uint32_t target,
  const Observer& observer,
  const search::Interval query,
  const Schedule& scheduled,
  search::Budget& budget
) -> std::vector<double> {
  std::vector<double> times;
  bool first = true;
  bool previous_side = false;
  for (const auto& piece : scheduled.pieces) {
    const Altitude provider {
      .target = static_cast<ephemeris::Target>(target),
      .observer = observer,
      .level_sine = 0.0,
      .domain = piece.domain,
      .meridian = true,
    };
    const auto found = scan(provider, piece, false, budget);

    if (not first and previous_side != found.initially_above) {
      throw std::runtime_error { "Meridian crossing coincides with a Delta T model join" };
    }
    first = false;
    previous_side = found.finally_above;

    for (const auto& event : found.events) {
      if (event.jde_tt < query.start_jde_tt or event.jde_tt >= query.end_jde_tt) {
        continue;
      }
      search::charge_evaluation(event.jde_tt, budget);
      const double forward = provider.observe(event.jde_tt).forward;
      if (std::abs(forward) <= CONTACT_RESIDUAL) {
        throw std::runtime_error { "The meridian forward side is unresolved" };
      }
      if (forward > 0.0) {
        times.push_back(event.jde_tt);
      }
    }
  }
  return times;
}

/** @brief Validate, preflight sample work, search every condition, then compose the requested result. */
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

  if (kind == Kind::TRANSITS) {
    const auto planned = schedule(target, query, budget.evaluations_remaining);
    return { .conditions = {}, .composed = {}, .transit_times = transit_events(target, observer, query, planned, budget) };
  }

  std::vector<Schedule> plans;
  std::size_t sample_count = 0;
  for (const auto& request : requests) {
    auto planned = schedule(request.target, query, budget.evaluations_remaining - sample_count);
    sample_count += planned.sample_count;
    plans.push_back(std::move(planned));
  }

  std::vector<Condition> conditions;
  double scan_start = query.start_jde_tt;
  double scan_end = query.end_jde_tt;
  for (std::size_t index = 0; index < requests.size(); ++index) {
    const auto& planned = plans.at(index);
    scan_start = std::min(scan_start, planned.pieces.front().start);
    scan_end = std::max(scan_end, planned.pieces.back().end);
    conditions.push_back(condition_events(requests[index], observer, planned, budget));
  }
  auto composed = windows(conditions, query.start_jde_tt, query.end_jde_tt, scan_start, scan_end);
  return { .conditions = std::move(conditions), .composed = std::move(composed), .transit_times = {} };
}

} // namespace astro::observer::detail::altitude_events
