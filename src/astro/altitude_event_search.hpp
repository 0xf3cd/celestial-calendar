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
#include <numeric>
#include <stdexcept>
#include <vector>

#include "delta_t.hpp"
#include "earth/refraction.hpp"
#include "ephemeris.hpp"
#include "event_search.hpp"
#include "julian_day.hpp"
#include "observer.hpp"
#include "sidereal_time.hpp"
#include "toolbox.hpp"

namespace astro::observer::detail::altitude_events {

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
  const toolbox::GoldenSectionRules rules {
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

  std::vector<Sample> points;
  for (const auto& value : samples) {
    if (planned.start <= value.jde_tt and value.jde_tt <= planned.end) {
      points.push_back(value);
    }
  }
  for (const auto& value : turns) {
    if (planned.start < value.point.jde_tt and value.point.jde_tt < planned.end) {
      points.push_back(value.point);
    }
  }
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

} // namespace astro::observer::detail::altitude_events
