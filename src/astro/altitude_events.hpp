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
 * @brief Complete interval altitude events, upper transits and observing windows.
 * @note Inputs and reported times use JDE(TT); observer rotation uses fixed ALGO5 and IAU1980.
 *       Levels refer to body centres. Existing rise_set::sun geometric constants supply the
 *       fixed -50 arcminute sunrise/sunset convention and the three twilight levels.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

#include "altitude_event_query.hpp"
#include "delta_t.hpp"
#include "earth/refraction.hpp"
#include "ephemeris.hpp"
#include "ephemeris_events.hpp"
#include "observer.hpp"
#include "toolbox.hpp"

namespace astro::observer {

/** @brief Geometric centre-altitude threshold, strictly inside (-90,90) degrees. */
struct GeometricLevel {
  toolbox::AngleDeg altitude;
};

/** @brief Attainable apparent centre-altitude threshold with its refraction model and weather. */
struct RefractedLevel {
  toolbox::AngleDeg altitude;
  earth::refraction::Params params;
};

using AltitudeLevel = std::variant<GeometricLevel, RefractedLevel>;

/** @brief Side of a centre-altitude threshold that satisfies a condition. */
enum class AltitudeSense : uint8_t { ABOVE, BELOW };

/** @brief Physical target, centre-altitude convention and satisfying side. */
struct AltitudeCondition {
  ephemeris::Target target;
  AltitudeLevel level;
  AltitudeSense sense;
};

/** @brief A requested condition, resolved geometric threshold and truth immediately after start replay. */
struct AltitudeConditionState {
  AltitudeCondition condition;
  ephemeris::PositionSource source;
  delta_t::Model delta_t_model;
  toolbox::AngleDeg geometric_level;
  bool initially_satisfied;
};

/** @brief One condition's altitude crossing/contact in JDE(TT), associated by zero-based condition index. */
struct AltitudeEvent {
  double jde_tt;
  uint32_t condition_index;
  ephemeris::EventDirection direction;
};

/** @brief Complete single-condition result over the requested half-open TT interval. */
struct AltitudeEvents {
  AltitudeConditionState condition;
  std::vector<AltitudeEvent> events;
};

/** @brief Positive-duration operational interval in JDE(TT). */
struct ObservingWindow {
  double start_jde_tt;
  double end_jde_tt;
};

/** @brief Complete condition states, companion events and maximal intersected observing windows. */
struct ObservingWindows {
  std::vector<AltitudeConditionState> conditions;
  std::vector<AltitudeEvent> events;
  std::vector<ObservingWindow> windows;
};

/** @brief Upper meridian passage in JDE(TT), relative to the observer's supplied longitude at a pole. */
struct UpperTransit {
  double jde_tt;
  ephemeris::Target target;
  ephemeris::PositionSource source;
  delta_t::Model delta_t_model;
};

namespace detail::altitude_events {

/** @brief Decode only the fields belonging to the requested geometric or atmospheric convention. */
[[nodiscard]] inline auto request(const AltitudeCondition& condition) -> Request {
  if (condition.sense != AltitudeSense::ABOVE and condition.sense != AltitudeSense::BELOW) {
    throw std::invalid_argument { "Unknown altitude condition sense" };
  }
  return std::visit([&](const auto& level) -> Request {
    if constexpr (std::is_same_v<std::decay_t<decltype(level)>, GeometricLevel>) {
      return {
        .target = static_cast<uint32_t>(condition.target), .level = level.altitude.deg(),
        .above = condition.sense == AltitudeSense::ABOVE, .convention = 2,
        .params = { .temperature_c = 0.0, .pressure_hpa = 0.0, .model = earth::refraction::Model::BENNETT },
      };
    } else {
      if (level.params.model != earth::refraction::Model::BENNETT and level.params.model != earth::refraction::Model::SAEMUNDSSON) {
        throw std::invalid_argument { "Unknown refraction model" };
      }
      return {
        .target = static_cast<uint32_t>(condition.target), .level = level.altitude.deg(),
        .above = condition.sense == AltitudeSense::ABOVE, .convention = static_cast<uint32_t>(level.params.model), .params = level.params,
      };
    }
  }, condition.level);
}

/** @brief Attach physical-source and fixed-clock metadata to a completely resolved condition. */
[[nodiscard]] inline auto condition_state(
  const AltitudeCondition& requested,
  const Condition& resolved,
  const SideState state
) -> AltitudeConditionState {
  return {
    .condition = requested, .source = ephemeris::source_of(requested.target), .delta_t_model = delta_t::Model::ALGO5,
    .geometric_level = toolbox::AngleDeg { resolved.level.geometric_degrees },
    .initially_satisfied = state.initially_satisfied != 0,
  };
}

/** @brief Translate a complete-census side change to the existing angular-event direction. */
[[nodiscard]] inline auto event_direction(const int32_t change) -> ephemeris::EventDirection {
  switch (change) {
    case -1: return ephemeris::EventDirection::DECREASING;
    case 0: return ephemeris::EventDirection::TANGENT;
    case 1: return ephemeris::EventDirection::INCREASING;
    default: throw std::runtime_error { "A completed altitude event has an invalid side change" };
  }
}

/** @brief Project the chronological, condition-associated complete census. */
[[nodiscard]] inline auto reported_events(const Result& composed) -> std::vector<AltitudeEvent> {
  std::vector<AltitudeEvent> result;
  result.reserve(composed.reported_events.size());
  for (const auto& event : composed.reported_events) {
    result.push_back({
      .jde_tt = event.jde_tt, .condition_index = event.condition,
      .direction = event_direction(event.change),
    });
  }
  return result;
}

} // namespace detail::altitude_events

/**
 * @brief Find the complete altitude-event census for one physical target over [start,end) in TT.
 * @note Events at start are included and already reflected in initially_satisfied; events at end are excluded.
 *       Rounded residual zero alone does not establish a tangent. Unresolved event counts fail the whole query.
 * @throw std::invalid_argument For malformed inputs or queries outside the calendar/physical-provider domain.
 * @throw std::runtime_error For unavailable numerical certificates, unresolved counts or exhausted work.
 */
[[nodiscard]] inline auto altitude_events(
  const Observer& observer,
  const AltitudeCondition& condition,
  const double start_jde_tt,
  const double end_jde_tt
) -> AltitudeEvents {
  namespace query = detail::altitude_events;
  const std::array requests { query::request(condition) };
  auto budget = query::query_budget();
  const auto result = query::compute(
    query::Kind::ALTITUDE,
    observer,
    { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt },
    requests,
    0,
    budget
  );
  return {
    .condition = query::condition_state(condition, result.conditions.front(), result.composed.states.front()),
    .events = query::reported_events(result.composed),
  };
}

/**
 * @brief Find every forward-side upper meridian passage over [start,end) in JDE(TT).
 * @note No altitude level, refraction or above/below condition applies to this query.
 * @throw std::invalid_argument For malformed observer, nonphysical target or out-of-domain interval.
 * @throw std::runtime_error For unavailable certificates, unresolved passage side/count or exhausted work.
 */
[[nodiscard]] inline auto upper_transits(
  const Observer& observer,
  const ephemeris::Target target,
  const double start_jde_tt,
  const double end_jde_tt
) -> std::vector<UpperTransit> {
  namespace query = detail::altitude_events;
  auto budget = query::query_budget();
  const auto result = query::compute(
    query::Kind::TRANSITS,
    observer,
    { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt },
    {},
    static_cast<uint32_t>(target),
    budget
  );
  std::vector<UpperTransit> transits;
  transits.reserve(result.transit_times.size());
  for (const double jde_tt : result.transit_times) {
    transits.push_back({
      .jde_tt = jde_tt, .target = target, .source = ephemeris::source_of(target), .delta_t_model = delta_t::Model::ALGO5,
    });
  }
  return transits;
}

/**
 * @brief Intersect complete altitude conditions over [start,end) and retain their companion events.
 * @note Windows are maximal positive-duration operational spans, not strict point sets at isolated equalities.
 *       An empty conjunction returns the whole interval. Failure in any condition fails the whole query.
 * @throw std::invalid_argument For malformed inputs or an out-of-calendar/physical-provider interval.
 * @throw std::runtime_error For unavailable certificates, unresolved dependencies or exhausted work.
 */
[[nodiscard]] inline auto observing_windows(
  const Observer& observer,
  const std::span<const AltitudeCondition> conditions,
  const double start_jde_tt,
  const double end_jde_tt
) -> ObservingWindows {
  namespace query = detail::altitude_events;
  if (conditions.size() > query::MAX_CONDITIONS) {
    throw std::runtime_error { "The condition-list work budget is exhausted" };
  }
  std::vector<query::Request> requests;
  for (const auto& condition : conditions) {
    requests.push_back(query::request(condition));
  }
  auto budget = query::query_budget();
  const auto computed = query::compute(
    query::Kind::WINDOWS,
    observer,
    { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt },
    requests,
    0,
    budget
  );
  ObservingWindows result { .conditions = {}, .events = query::reported_events(computed.composed), .windows = {} };
  for (std::size_t index = 0; index < conditions.size(); ++index) {
    result.conditions.push_back(query::condition_state(conditions[index], computed.conditions.at(index), computed.composed.states.at(index)));
  }
  for (const auto& window : computed.composed.windows) {
    result.windows.push_back({ .start_jde_tt = window.start_jde_tt, .end_jde_tt = window.end_jde_tt });
  }
  return result;
}

} // namespace astro::observer
