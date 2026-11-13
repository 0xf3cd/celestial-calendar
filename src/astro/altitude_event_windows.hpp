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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "altitude_event_search.hpp"
#include "event_search.hpp"

namespace astro::observer::detail::altitude_events {

/** @brief Reported crossing/contact and the originating closed proof piece's start. */
struct Event {
  double jde_tt;
  int32_t change;
  double origin_start_jde_tt;
};

/** @brief One condition's complete canonical census, seed, sense and resolved level. */
struct Condition {
  bool complete;
  bool seed_above;
  bool above;
  std::vector<Event> events;
  Level level {};
  double residual_error {};
};

/** @brief Positive-duration operational span in JDE(TT). */
struct Window {
  double start_jde_tt;
  double end_jde_tt;
};

/** @brief One event associated with its condition, in chronological then condition-index order. */
struct ReportedEvent {
  double jde_tt;
  uint32_t condition;
  int32_t change;
};

/** @brief Condition truth immediately after start replay and after the processed reported events. */
struct SideState {
  uint8_t initially_satisfied;
  uint8_t currently_satisfied;
};

/** @brief Operational intersection, with companion events and each condition's initial state. */
struct Result {
  std::vector<SideState> states;
  std::vector<Window> windows;
  std::size_t companion_events;
  std::vector<ReportedEvent> reported_events;
};

/** @brief Replay complete censuses before clipping; start events are already reflected in the initial state. */
[[nodiscard]] inline auto windows(
  const std::span<const Condition> conditions,
  const double start_jde_tt,
  const double end_jde_tt,
  const double census_start_jde_tt,
  const double census_end_jde_tt
) -> Result {
  const bool covered = census_start_jde_tt <= start_jde_tt and start_jde_tt < end_jde_tt and end_jde_tt <= census_end_jde_tt;
  if (not std::isfinite(start_jde_tt) or not std::isfinite(end_jde_tt) or not covered) {
    throw std::invalid_argument { "The window query leaves the complete canonical cell" };
  }

  std::vector<SideState> states;
  std::vector<ReportedEvent> reported;
  std::vector<std::size_t> positions(conditions.size(), 0);
  std::size_t unsatisfied = 0;
  std::size_t companion_events = 0;
  for (std::size_t index = 0; index < conditions.size(); ++index) {
    const auto& condition = conditions[index];
    if (not condition.complete) {
      throw std::runtime_error { "A condition has an unresolved census" };
    }
    bool above = condition.seed_above;
    double previous = astro::detail::event_search::lower(census_start_jde_tt);
    for (std::size_t position = 0; position < condition.events.size(); ++position) {
      const auto& event = condition.events.at(position);
      if (not std::isfinite(event.jde_tt) or event.jde_tt <= previous or event.jde_tt > census_end_jde_tt
          or event.change < -1 or event.change > 1) {
        throw std::runtime_error { "A condition census has an invalid event" };
      }
      previous = event.jde_tt;
      if (event.jde_tt <= start_jde_tt) {
        if (event.change != 0 and event.origin_start_jde_tt >= census_start_jde_tt) {
          above = event.change > 0;
        }
        positions[index] = position + 1;
      }
      if (start_jde_tt <= event.jde_tt and event.jde_tt < end_jde_tt) {
        ++companion_events;
        if (event.jde_tt == start_jde_tt) {
          reported.push_back({ .jde_tt = event.jde_tt, .condition = static_cast<uint32_t>(index), .change = event.change });
        }
      }
    }
    const bool satisfied = condition.above ? above : not above;
    states.push_back({ .initially_satisfied = static_cast<uint8_t>(satisfied), .currently_satisfied = static_cast<uint8_t>(satisfied) });
    unsatisfied += static_cast<std::size_t>(not satisfied);
  }
  Result result {
    .states = std::move(states), .windows = {}, .companion_events = companion_events, .reported_events = std::move(reported)
  };

  double opening = start_jde_tt;
  while (true) {
    bool found = false;
    double time = end_jde_tt;
    for (std::size_t index = 0; index < conditions.size(); ++index) {
      const auto& events = conditions[index].events;
      if (positions[index] < events.size() and events.at(positions[index]).jde_tt < time) {
        time = events.at(positions[index]).jde_tt;
        found = true;
      }
    }
    if (not found) {
      break;
    }
    const bool previously_active = unsatisfied == 0;
    for (std::size_t index = 0; index < conditions.size(); ++index) {
      const auto& condition = conditions[index];
      while (positions[index] < condition.events.size() and condition.events.at(positions[index]).jde_tt == time) {
        const auto& event = condition.events.at(positions[index]++);
        result.reported_events.push_back({ .jde_tt = event.jde_tt, .condition = static_cast<uint32_t>(index), .change = event.change });
        if (event.change == 0) {
          continue;
        }
        const bool above = event.change > 0;
        const bool satisfied = condition.above ? above : not above;
        const bool previous = result.states[index].currently_satisfied != 0;
        if (previous != satisfied) {
          if (satisfied) {
            --unsatisfied;
          } else {
            ++unsatisfied;
          }
          result.states[index].currently_satisfied = static_cast<uint8_t>(satisfied);
        }
      }
    }
    const bool active = unsatisfied == 0;
    if (previously_active and not active and opening < time) {
      result.windows.push_back({ .start_jde_tt = opening, .end_jde_tt = time });
    } else if (not previously_active and active) {
      opening = time;
    }
  }
  if (unsatisfied == 0 and opening < end_jde_tt) {
    result.windows.push_back({ .start_jde_tt = opening, .end_jde_tt = end_jde_tt });
  }
  return result;
}

} // namespace astro::observer::detail::altitude_events
