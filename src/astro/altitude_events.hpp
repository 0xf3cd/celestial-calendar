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
 * @brief Sampled interval altitude events, upper transits and observing windows.
 * @note Inputs and reported times use JDE(TT); observer rotation uses fixed ALGO5 and IAU1980.
 *       Levels refer to body centres. Existing rise_set::sun geometric constants supply the
 *       fixed -50 arcminute sunrise/sunset convention and the three twilight levels.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "delta_t.hpp"
#include "earth/refraction.hpp"
#include "ephemeris.hpp"
#include "ephemeris_events.hpp"
#include "event_search.hpp"
#include "julian_day.hpp"
#include "observer.hpp"
#include "sidereal_time.hpp"
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

/** @brief One condition's altitude crossing in JDE(TT), associated by zero-based condition index. */
struct AltitudeEvent {
  double jde_tt;
  uint32_t condition_index;
  ephemeris::EventDirection direction;
};

/** @brief Single-condition state and detected crossings over the requested half-open TT interval. */
struct AltitudeEvents {
  AltitudeConditionState condition;
  std::vector<AltitudeEvent> events;
};

/** @brief Positive-duration operational interval in JDE(TT). */
struct ObservingWindow {
  double start_jde_tt;
  double end_jde_tt;
};

/** @brief Condition states, detected crossings and maximal reconstructed observing windows. */
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

namespace search = astro::detail::event_search;
namespace refraction = astro::earth::refraction;

inline constexpr double TIME_RESOLUTION_TT_DAYS = 0x1p-20;
inline constexpr double LEVEL_RESOLUTION_DEGREES = 0x1p-30;
inline constexpr double SAMPLE_STEP_TT_DAYS = 0x1p-7;
/** @brief Empirical dimensionless ambiguity threshold at a detected turn, not an error bound. */
inline constexpr double CONTACT_RESIDUAL = 0x1p-24;

/** @brief First and last evaluable TT instants on one continuous model branch, both included. */
struct Domain {
  double first;
  double last;
};

/** @brief One topocentric observation; all components are dimensionless. */
struct Observation {
  double up;
  double q;
  double forward;
  double meridian;
};

/** @brief Fixed-observer altitude or meridian objective, guarded by its evaluable domain. */
struct Altitude {
  ephemeris::Target target;
  Observer observer;
  double level_sine;
  Domain domain;
  bool meridian;

  [[nodiscard]] auto observe(const double jde_tt) const -> Observation {
    if (not std::isfinite(jde_tt) or jde_tt < domain.first or jde_tt > domain.last) {
      throw std::runtime_error { "Altitude sample leaves its evaluable domain" };
    }

    const auto position = ephemeris::equatorial_position(target, jde_tt);
    const auto tt = julian_day::jde_to_tt(jde_tt);
    const auto ut1 = delta_t::tt_to_ut1(delta_t::Model::ALGO5, tt);
    const double jd_ut1 = julian_day::ut1_to_jd(ut1);
    const auto theta = sidereal::local_apparent(
      jd_ut1,
      jde_tt,
      -observer.location.longitude,
      earth::nutation::Model::IAU_1980
    );
    const auto geometry = astro::observer::detail::local_geometry(position, theta, observer);
    return { .up = geometry.up, .q = geometry.q, .forward = geometry.B, .meridian = geometry.A };
  }

  [[nodiscard]] auto operator()(const double jde_tt) const -> double {
    const auto observed = observe(jde_tt);
    return meridian ? observed.meridian : observed.up - (level_sine * observed.q);
  }
};

/** @brief Scalar sample; zero belongs to the below side throughout the search and replay. */
struct Sample {
  double jde_tt;
  double value;

  [[nodiscard]] auto above() const -> bool { return value > 0.0; }
};

/** @brief Crossing time on the new side, with +1 for rising and -1 for falling. */
struct Event {
  double jde_tt;
  int32_t change;
};

/** @brief Fixed-grid scan bounds and neighbouring samples on a continuous model branch. */
struct Grid {
  Domain domain;
  double start;
  double end;
  std::vector<double> times;
};

/** @brief Build a query-independent dyadic grid, clipped only at fixed domain boundaries. */
[[nodiscard]] inline auto grid(
  const search::Interval query,
  const Domain domain,
  const std::size_t maximum_samples
) -> Grid {
  constexpr double EPOCH = 2451545.0;
  const double start = std::max(
    domain.first,
    EPOCH + ((std::ceil((query.start_jde_tt - EPOCH) / SAMPLE_STEP_TT_DAYS) - 1.0) * SAMPLE_STEP_TT_DAYS)
  );
  const double end = std::min(
    domain.last,
    EPOCH + (std::ceil((query.end_jde_tt - EPOCH) / SAMPLE_STEP_TT_DAYS) * SAMPLE_STEP_TT_DAYS)
  );
  const double left = std::max(domain.first, start - SAMPLE_STEP_TT_DAYS);
  const double right = std::min(domain.last, end + SAMPLE_STEP_TT_DAYS);

  const double left_index = std::floor((left - EPOCH) / SAMPLE_STEP_TT_DAYS);
  const double right_index = std::floor((right - EPOCH) / SAMPLE_STEP_TT_DAYS);
  const bool clipped_end = EPOCH + (right_index * SAMPLE_STEP_TT_DAYS) < right;
  const double count = right_index - left_index + 1.0 + static_cast<double>(clipped_end);
  if (not std::isfinite(count) or count > static_cast<double>(maximum_samples)) {
    throw std::runtime_error { "The altitude sample plan exceeds the shared evaluation budget" };
  }

  Grid result { .domain = domain, .start = start, .end = end, .times = { left } };
  const auto first = static_cast<int64_t>(left_index) + 1;
  const auto last = static_cast<int64_t>(right_index);
  for (int64_t index = first; index <= last; ++index) {
    result.times.push_back(EPOCH + (static_cast<double>(index) * SAMPLE_STEP_TT_DAYS));
  }
  if (result.times.back() < right) {
    result.times.push_back(right);
  }
  return result;
}

/** @brief Charge an attempted scalar evaluation, including one that returns a non-finite value. */
template <typename Func>
[[nodiscard]] inline auto sample(const Func& f, const double jde_tt, search::Budget& budget) -> Sample {
  search::charge_evaluation(jde_tt, budget);
  const double value = f(jde_tt);
  if (not std::isfinite(value)) {
    throw std::runtime_error { "Altitude objective is not finite" };
  }
  return { .jde_tt = jde_tt, .value = value };
}

/** @brief Reserve one turn or crossing bracket before refining it. */
inline auto charge_candidate(search::Budget& budget) -> void {
  if (budget.candidates_remaining == 0) {
    throw std::runtime_error { "The altitude candidate budget is exhausted" };
  }
  --budget.candidates_remaining;
}

/** @brief Bisect a side change and return the first retained sample on the new side. */
template <typename Func>
[[nodiscard]] inline auto crossing(const Func& f, Sample left, Sample right, search::Budget& budget) -> Event {
  charge_candidate(budget);
  if (left.jde_tt >= right.jde_tt or left.above() == right.above()) {
    throw std::runtime_error { "Altitude crossing has no directed bracket" };
  }

  for (uint32_t iteration = 0; iteration < 64; ++iteration) {
    if (right.jde_tt - left.jde_tt <= TIME_RESOLUTION_TT_DAYS) {
      return { .jde_tt = right.jde_tt, .change = right.above() ? 1 : -1 };
    }
    const auto middle = sample(f, std::midpoint(left.jde_tt, right.jde_tt), budget);
    if (middle.above() == left.above()) {
      left = middle;
    } else {
      right = middle;
    }
  }
  throw std::runtime_error { "Altitude crossing did not converge" };
}

/** @brief Refined extremum and its orientation, for partitioning neighbouring grid intervals. */
struct Turn {
  Sample point;
  bool minimum;
};

/** @brief Refine a sampled turn with the shared golden-section primitive. */
template <typename Func>
[[nodiscard]] inline auto turn(
  const Func& f,
  const Sample left,
  const Sample right,
  const bool minimum,
  search::Budget& budget
) -> Turn {
  charge_candidate(budget);
  const auto objective = [&](const double at) {
    const double value = sample(f, at, budget).value;
    return minimum ? value : -value;
  };
  constexpr toolbox::GoldenSectionRules rules {
    .lower_probe = [](const double lo, const double hi) { return lo + (toolbox::INV_PHI_SQ * (hi - lo)); },
    .move_lower = [](const double lhs, const double rhs) { return not (lhs < rhs); },
    .finished = [](const double width, const double tolerance) { return width <= tolerance; },
  };
  const auto bracket = toolbox::golden_section_bracket(
    objective,
    left.jde_tt,
    right.jde_tt,
    TIME_RESOLUTION_TT_DAYS,
    64,
    rules
  );
  if (not bracket.converged) {
    throw std::runtime_error { "Altitude extremum did not converge" };
  }
  return {
    .point = sample(f, std::midpoint(bracket.lower, bracket.upper), budget),
    .minimum = minimum,
  };
}

/** @brief Retain a detected turn only when its side is numerically distinguishable from contact. */
inline auto append_turn(const Turn& refined, std::vector<Turn>& turns) -> void {
  if (std::abs(refined.point.value) <= CONTACT_RESIDUAL) {
    throw std::runtime_error { "Altitude search encountered an unresolved near contact" };
  }
  turns.push_back(refined);
}

/** @brief Search a fixed domain-edge interval where a three-sample turn test has no outer neighbour. */
template <typename Func>
inline auto edge_turns(
  const Func& f,
  const Sample left,
  const Sample right,
  std::vector<Turn>& turns,
  search::Budget& budget
) -> void {
  for (const bool minimum : { true, false }) {
    const bool already_found = std::ranges::any_of(turns, [&](const Turn& value) {
      return value.minimum == minimum and left.jde_tt <= value.point.jde_tt and value.point.jde_tt <= right.jde_tt;
    });
    if (already_found) {
      continue;
    }

    const auto refined = turn(f, left, right, minimum, budget);
    const bool interior = refined.point.jde_tt - left.jde_tt > TIME_RESOLUTION_TT_DAYS
      and right.jde_tt - refined.point.jde_tt > TIME_RESOLUTION_TT_DAYS;
    const bool beats_ends = minimum
      ? refined.point.value < std::min(left.value, right.value)
      : refined.point.value > std::max(left.value, right.value);
    if (interior and beats_ends) {
      append_turn(refined, turns);
    }
  }
}

/** @brief Detected crossings and endpoint sides of one fixed-grid scan. */
struct Scan {
  bool initially_above;
  bool finally_above;
  std::vector<Event> events;
};

/** @brief Sample, partition at detected turns, and refine directed crossings on one continuous branch. */
template <typename Func>
[[nodiscard]] inline auto scan(
  const Func& f,
  const Grid& planned,
  const bool find_turns,
  search::Budget& budget
) -> Scan {
  if (planned.times.size() > budget.evaluations_remaining) {
    throw std::runtime_error { "The altitude sample plan exceeds the shared evaluation budget" };
  }
  std::vector<Sample> samples;
  samples.reserve(planned.times.size());
  for (const double time : planned.times) {
    samples.push_back(sample(f, time, budget));
  }

  std::vector<Turn> turns;
  if (find_turns) {
    for (std::size_t index = 1; index + 1 < samples.size(); ++index) {
      const auto left = samples.at(index - 1);
      const auto middle = samples.at(index);
      const auto right = samples.at(index + 1);
      if (middle.jde_tt < planned.start or middle.jde_tt > planned.end) {
        continue;
      }
      const bool minimum = middle.value <= left.value and middle.value < right.value;
      const bool maximum = middle.value >= left.value and middle.value > right.value;
      if (minimum or maximum) {
        append_turn(turn(f, left, right, minimum, budget), turns);
      }
    }

    if (samples.size() >= 2 and samples.front().jde_tt == planned.domain.first) {
      edge_turns(f, samples.front(), samples.at(1), turns, budget);
    }
    if (samples.size() >= 2 and samples.back().jde_tt == planned.domain.last) {
      edge_turns(f, samples.at(samples.size() - 2), samples.back(), turns, budget);
    }
  }

  const auto in_scan = [&planned](const Sample& value) {
    return planned.start <= value.jde_tt and value.jde_tt <= planned.end;
  };
  const auto interior = [&planned](const Sample& value) {
    return planned.start < value.jde_tt and value.jde_tt < planned.end;
  };
  std::vector<Sample> points;
  std::ranges::copy_if(
    samples,
    std::back_inserter(points),
    in_scan
  );
  std::ranges::copy_if(
    turns | std::views::transform(&Turn::point),
    std::back_inserter(points),
    interior
  );

  std::ranges::sort(points, {}, &Sample::jde_tt);
  const auto duplicates = std::ranges::unique(points, {}, &Sample::jde_tt);
  points.erase(duplicates.begin(), duplicates.end());

  Scan result { .initially_above = points.front().above(), .finally_above = points.back().above(), .events = {} };
  for (std::size_t index = 1; index < points.size(); ++index) {
    const auto left = points.at(index - 1);
    const auto right = points.at(index);
    if (left.above() != right.above()) {
      result.events.push_back(crossing(f, left, right, budget));
    }
  }
  return result;
}

/** @brief Resolve an apparent threshold to geometric degrees within the supported weather window. */
[[nodiscard]] inline auto inverse(
  const double apparent,
  const refraction::Params& params,
  search::Budget& budget
) -> double {
  refraction::detail::validate_params(params);
  const bool weather = params.temperature_c >= -90.0 and params.temperature_c <= 60.0
    and params.pressure_hpa >= 300.0 and params.pressure_hpa <= 1100.0;
  const bool model = params.model == refraction::Model::BENNETT or params.model == refraction::Model::SAEMUNDSSON;
  if (not weather or not model or not std::isfinite(apparent) or apparent < 0.0 or apparent > 90.0) {
    throw std::invalid_argument { "Unsupported apparent altitude, refraction model or weather" };
  }

  const auto correction = [&](const double degrees) {
    search::charge_evaluation(0.0, budget);
    const auto native = params.model == refraction::Model::BENNETT
      ? refraction::bennett(toolbox::AngleDeg { degrees }) : refraction::saemundsson(toolbox::AngleDeg { degrees });
    return refraction::detail::apply_tp_correction(native, params).deg();
  };

  if (params.model == refraction::Model::BENNETT) {
    const double geometric = apparent - correction(apparent);
    if (geometric >= 90.0) {
      throw std::invalid_argument { "Apparent level exceeds the supported geometric upper endpoint" };
    }
    return geometric;
  }

  // Meeus (16.4) is monotone here for the supported weather window; -1.5° brackets its apparent horizon.
  double left = -1.5;
  double right = 90.0;
  const auto map = [&](const double value) { return value + correction(value); };
  if (map(left) > apparent) {
    throw std::runtime_error { "Saemundsson apparent horizon is not bracketed" };
  }
  if (apparent > map(right)) {
    throw std::invalid_argument { "Apparent level is outside Saemundsson's supported range" };
  }
  for (uint32_t iteration = 0; iteration < 64; ++iteration) {
    if (right - left <= LEVEL_RESOLUTION_DEGREES) {
      return std::midpoint(left, right);
    }
    const double middle = std::midpoint(left, right);
    if (map(middle) < apparent) {
      left = middle;
    } else {
      right = middle;
    }
  }
  throw std::runtime_error { "Refraction inverse did not converge" };
}

/** @brief One condition's detected crossings, scan-start side and resolved geometric level. */
struct Condition {
  bool seed_above;
  bool above;
  std::vector<Event> events;
  double geometric_degrees;
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

/** @brief Replay detected crossings before clipping; start events are reflected in the initial state. */
[[nodiscard]] inline auto windows(
  const std::span<const Condition> conditions,
  const double start_jde_tt,
  const double end_jde_tt,
  const double scan_start_jde_tt,
  const double scan_end_jde_tt
) -> Result {
  const bool covered = scan_start_jde_tt <= start_jde_tt and start_jde_tt < end_jde_tt
    and end_jde_tt <= scan_end_jde_tt;
  if (not std::isfinite(start_jde_tt) or not std::isfinite(end_jde_tt) or not covered) {
    throw std::invalid_argument { "The window query leaves its sampled interval" };
  }

  std::vector<SideState> states;
  std::vector<ReportedEvent> reported;
  std::vector<std::size_t> positions(conditions.size(), 0);
  std::size_t unsatisfied = 0;
  std::size_t companion_events = 0;
  for (std::size_t index = 0; index < conditions.size(); ++index) {
    const auto& condition = conditions[index];
    bool above = condition.seed_above;
    double previous = astro::detail::event_search::lower(scan_start_jde_tt);
    for (std::size_t position = 0; position < condition.events.size(); ++position) {
      const auto& event = condition.events.at(position);
      if (not std::isfinite(event.jde_tt) or event.jde_tt <= previous or event.jde_tt > scan_end_jde_tt
          or (event.change != -1 and event.change != 1)) {
        throw std::runtime_error { "A condition has an invalid crossing" };
      }
      previous = event.jde_tt;
      if (event.jde_tt <= start_jde_tt) {
        above = event.change > 0;
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
    states.push_back({
      .initially_satisfied = static_cast<uint8_t>(satisfied),
      .currently_satisfied = static_cast<uint8_t>(satisfied),
    });
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
        result.reported_events.push_back({
          .jde_tt = event.jde_tt, .condition = static_cast<uint32_t>(index), .change = event.change,
        });
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
    // Earlier ALGO5 joins lie outside the calendar's supported TT range.
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

/** @brief Attach physical-source and fixed-clock metadata to a resolved condition. */
[[nodiscard]] inline auto condition_state(
  const AltitudeCondition& requested,
  const Condition& resolved,
  const SideState state
) -> AltitudeConditionState {
  return {
    .condition = requested, .source = ephemeris::source_of(requested.target), .delta_t_model = delta_t::Model::ALGO5,
    .geometric_level = toolbox::AngleDeg { resolved.geometric_degrees },
    .initially_satisfied = state.initially_satisfied != 0,
  };
}

/** @brief Translate a crossing's side change to the shared event direction. */
[[nodiscard]] inline auto event_direction(const int32_t change) -> ephemeris::EventDirection {
  switch (change) {
    case -1: return ephemeris::EventDirection::DECREASING;
    case 1: return ephemeris::EventDirection::INCREASING;
    default: throw std::runtime_error { "A completed altitude event has an invalid side change" };
  }
}

/** @brief Project detected crossings in chronological then condition-index order. */
[[nodiscard]] inline auto reported_events(const Result& composed) -> std::vector<AltitudeEvent> {
  const auto event = [](const ReportedEvent& value) -> AltitudeEvent {
    return {
      .jde_tt = value.jde_tt, .condition_index = value.condition,
      .direction = event_direction(value.change),
    };
  };
  return composed.reported_events | std::views::transform(event) | std::ranges::to<std::vector>();
}

} // namespace detail::altitude_events

/**
 * @brief Find centre-altitude crossings for one physical target over [start,end) in TT.
 * @note Events at start are included and already reflected in initially_satisfied; events at end are excluded.
 *       Sampling can miss events without an error; success does not guarantee completeness.
 *       No TANGENT events are returned. Detected near contacts fail the whole query.
 * @throw std::invalid_argument For malformed inputs or queries outside the calendar/physical-provider domain.
 *       Refraction requires weather in [-90,60] degrees Celsius and [300,1100] hPa.
 * @throw std::runtime_error For detected numerical ambiguity, failed refinement or exhausted work.
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
 * @brief Find upper meridian passages over [start,end) in JDE(TT).
 * @note No altitude level, refraction or above/below condition applies to this query.
 *       Sampling can miss passages without an error, as with @ref altitude_events.
 * @throw std::invalid_argument For malformed observer, nonphysical target or out-of-domain interval.
 * @throw std::runtime_error For an unresolved passage side, failed refinement or exhausted work.
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
  const auto transit = [target](const double jde_tt) -> UpperTransit {
    return {
      .jde_tt = jde_tt, .target = target, .source = ephemeris::source_of(target), .delta_t_model = delta_t::Model::ALGO5,
    };
  };
  return result.transit_times | std::views::transform(transit) | std::ranges::to<std::vector>();
}

/**
 * @brief Intersect altitude conditions over [start,end) and retain their detected crossings.
 * @note Windows are maximal positive-duration operational spans, not strict point sets at isolated equalities.
 *       An empty conjunction returns the whole interval. Failure in any condition fails the whole query.
 *       Windows are reconstructed from detected crossings; @ref altitude_events sampling limits apply.
 * @throw std::invalid_argument For malformed inputs or an out-of-calendar/physical-provider interval.
 * @throw std::runtime_error For detected numerical ambiguity, failed refinement or exhausted work.
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
  const auto requests = conditions | std::views::transform(query::request) | std::ranges::to<std::vector>();
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
    result.conditions.push_back(
      query::condition_state(
        conditions[index],
        computed.conditions.at(index),
        computed.composed.states.at(index)
      )
    );
  }

  const auto window = [](const query::Window& value) -> ObservingWindow {
    return { .start_jde_tt = value.start_jde_tt, .end_jde_tt = value.end_jde_tt };
  };
  result.windows = computed.composed.windows | std::views::transform(window) | std::ranges::to<std::vector>();
  return result;
}

} // namespace astro::observer
