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
#include <format>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "datetime.hpp"
#include "illumination.hpp"
#include "julian_day.hpp"
#include "planet.hpp"
#include "sun.hpp"
#include "ymd.hpp"

namespace astro::planet::phenomena {

/** @brief An observer-facing planetary event. */
enum class Kind : uint8_t {
  INFERIOR_CONJUNCTION,
  SUPERIOR_CONJUNCTION,
  CONJUNCTION,
  OPPOSITION,
  EASTERN_QUADRATURE,
  WESTERN_QUADRATURE,
  GREATEST_EASTERN_ELONGATION,
  GREATEST_WESTERN_ELONGATION,
  STATION_RETROGRADE,
  STATION_DIRECT,
};

/** @brief A planetary event and its TT-scale instant. */
struct Event {
  Kind kind;
  double jde_tt;
};

namespace detail {

inline constexpr double COARSE_STEP_DAYS = 1.0;
inline constexpr double TIME_TOLERANCE_DAYS = 1e-8;
inline constexpr double ANGLE_RESIDUAL_TOLERANCE_DEG = 1e-6;
inline constexpr double WITNESS_HALF_WIDTH_DAYS = 0.01;
inline constexpr double DEDUPLICATION_TOLERANCE_DAYS = 1e-6;
inline constexpr std::size_t MAX_REFINEMENT_ITERATIONS = 96;

enum class Category : uint8_t { INNER, OUTER };

struct Sample {
  double jde_tt;
  double planet_λ;
  double difference;
  double separation;
};

struct SamplingWindow {
  double start_jde_tt;
  double end_jde_tt;
  double step_days;
};

struct CrossingSpec {
  double target_difference = 0.0;
  std::size_t max_iterations = MAX_REFINEMENT_ITERATIONS;
};

[[nodiscard]] inline auto category(const Planet planet) -> Category {
  switch (planet) {
    case Planet::MERCURY:
    case Planet::VENUS:
      return Category::INNER;
    case Planet::MARS:
    case Planet::JUPITER:
    case Planet::SATURN:
    case Planet::URANUS:
    case Planet::NEPTUNE:
      return Category::OUTER;
  }
  throw std::invalid_argument {
    std::format("Unknown planet {}", static_cast<uint32_t>(planet))
  };
}

[[nodiscard]] inline auto unwrap_near(const double wrapped, const double reference) -> double {
  return wrapped + (360.0 * std::round((reference - wrapped) / 360.0));
}

[[nodiscard]] inline auto raw_difference(const Planet planet, const double jde_tt) -> double {
  const auto source = astro::sun::geocentric_coord::apparent(jde_tt);
  const auto target = astro::planet::geocentric_coord::apparent(planet, jde_tt);
  return std::remainder(target.λ.deg() - source.λ.deg(), 360.0);
}

[[nodiscard]] inline auto sample(const Planet planet, const double jde_tt) -> Sample {
  const auto source = astro::sun::geocentric_coord::apparent(jde_tt);
  const auto target = astro::planet::geocentric_coord::apparent(planet, jde_tt);
  return {
    .jde_tt = jde_tt,
    .planet_λ = target.λ.deg(),
    .difference = std::remainder(target.λ.deg() - source.λ.deg(), 360.0),
    .separation = astro::illumination::geometry(source, target).angular_separation.deg(),
  };
}

[[nodiscard]] inline auto samples(
  const Planet planet,
  const SamplingWindow window
) -> std::vector<Sample> {
  if (not std::isfinite(window.step_days) or window.step_days <= 0.0) {
    throw std::invalid_argument {
      std::format("Planetary event scan step must be positive, got {}", window.step_days)
    };
  }
  // A global lattice makes adjacent year searches refine a shared extremum from the same bracket.
  const double scan_start = std::floor((window.start_jde_tt - window.step_days) / window.step_days)
                          * window.step_days;
  const double scan_end = std::ceil((window.end_jde_tt + window.step_days) / window.step_days)
                        * window.step_days;
  const auto count = static_cast<std::size_t>(std::ceil((scan_end - scan_start) / window.step_days)) + 1;
  std::vector<Sample> result;
  result.reserve(count);

  for (std::size_t index = 0; index < count; ++index) {
    auto current = sample(planet, scan_start + (window.step_days * static_cast<double>(index)));
    if (not result.empty()) {
      const auto& previous = result.back();
      current.planet_λ = previous.planet_λ
                       + std::remainder(current.planet_λ - previous.planet_λ, 360.0);
      current.difference = previous.difference
                         + std::remainder(current.difference - previous.difference, 360.0);
    }
    result.push_back(current);
  }
  return result;
}

[[nodiscard]] inline auto refine_crossing(
  const Planet planet,
  const Sample& left_sample,
  const Sample& right_sample,
  const CrossingSpec spec
) -> double {
  double left = left_sample.jde_tt;
  double right = right_sample.jde_tt;
  double left_residual = left_sample.difference - spec.target_difference;
  const double right_residual = right_sample.difference - spec.target_difference;
  if (left_residual == 0.0) {
    return left;
  }
  if (right_residual == 0.0) {
    return right;
  }
  if ((left_residual < 0.0) == (right_residual < 0.0)) {
    throw std::runtime_error {
      std::format(
        "Planetary event bracket does not straddle its target for {} between JDE(TT) {} and {}",
        astro::planet::detail::name(planet),
        left,
        right
      )
    };
  }

  for (std::size_t iteration = 0; iteration < spec.max_iterations; ++iteration) {
    const double middle = std::midpoint(left, right);
    const double middle_residual = unwrap_near(raw_difference(planet, middle), spec.target_difference)
                                 - spec.target_difference;
    if (std::fabs(middle_residual) <= ANGLE_RESIDUAL_TOLERANCE_DEG
        and (right - left) <= TIME_TOLERANCE_DAYS) {
      return middle;
    }
    if ((left_residual < 0.0) == (middle_residual < 0.0)) {
      left = middle;
      left_residual = middle_residual;
    } else {
      right = middle;
    }
  }

  throw std::runtime_error {
    std::format(
      "Planetary longitude event did not converge for {} between JDE(TT) {} and {}",
      astro::planet::detail::name(planet),
      left_sample.jde_tt,
      right_sample.jde_tt
    )
  };
}

template <typename Objective>
[[nodiscard]] inline auto refine_extremum(
  const Planet planet,
  const double left_jde_tt,
  const double right_jde_tt,
  const bool maximize,
  const Objective& objective
) -> double {
  constexpr double golden_ratio = 0.6180339887498948482;
  double left = left_jde_tt;
  double right = right_jde_tt;
  double lhs = right - (golden_ratio * (right - left));
  double rhs = left + (golden_ratio * (right - left));
  double lhs_value = objective(lhs);
  double rhs_value = objective(rhs);

  for (std::size_t iteration = 0; iteration < MAX_REFINEMENT_ITERATIONS; ++iteration) {
    if ((right - left) <= TIME_TOLERANCE_DAYS) {
      const double result = std::midpoint(left, right);
      const double width = std::min(WITNESS_HALF_WIDTH_DAYS, (right_jde_tt - left_jde_tt) / 4.0);
      const double center_value = objective(result);
      const double before_value = objective(result - width);
      const double after_value = objective(result + width);
      const bool bracketed = maximize
        ? center_value > before_value and center_value > after_value
        : center_value < before_value and center_value < after_value;
      if (bracketed) {
        return result;
      }
      break;
    }

    const bool move_left = maximize ? lhs_value < rhs_value : lhs_value > rhs_value;
    if (move_left) {
      left = lhs;
      lhs = rhs;
      lhs_value = rhs_value;
      rhs = left + (golden_ratio * (right - left));
      rhs_value = objective(rhs);
    } else {
      right = rhs;
      rhs = lhs;
      rhs_value = lhs_value;
      lhs = right - (golden_ratio * (right - left));
      lhs_value = objective(lhs);
    }
  }

  throw std::runtime_error {
    std::format(
      "Planetary event extremum did not converge for {} between JDE(TT) {} and {}",
      astro::planet::detail::name(planet),
      left_jde_tt,
      right_jde_tt
    )
  };
}

[[nodiscard]] inline auto crossing_samples(
  const Planet planet,
  const std::vector<Sample>& grid
) -> std::vector<Sample> {
  // A coarse interval can hide two near-tangent crossings; insert each refined reversal first.
  auto result = grid;
  for (std::size_t index = 1; index + 1 < grid.size(); ++index) {
    const auto& before = grid.at(index - 1);
    const auto& current = grid.at(index);
    const auto& after = grid.at(index + 1);
    const bool maximum = current.difference > before.difference and current.difference > after.difference;
    const bool minimum = current.difference < before.difference and current.difference < after.difference;
    if (not maximum and not minimum) {
      continue;
    }

    const auto difference = [planet, reference = current.difference](const double jde_tt) {
      return unwrap_near(raw_difference(planet, jde_tt), reference);
    };
    const double jde_tt = refine_extremum(planet, before.jde_tt, after.jde_tt, maximum, difference);
    auto extremum = sample(planet, jde_tt);
    extremum.planet_λ = unwrap_near(extremum.planet_λ, current.planet_λ);
    extremum.difference = unwrap_near(extremum.difference, current.difference);
    result.push_back(extremum);
  }
  std::ranges::sort(result, {}, &Sample::jde_tt);
  return result;
}

[[nodiscard]] inline auto outer_kind(const int64_t level_index) -> Kind {
  const int64_t remainder = ((level_index % 4) + 4) % 4;
  switch (remainder) {
    case 0: return Kind::CONJUNCTION;
    case 1: return Kind::EASTERN_QUADRATURE;
    case 2: return Kind::OPPOSITION;
    case 3: return Kind::WESTERN_QUADRATURE;
    default: throw std::runtime_error { "Invalid planetary-event grid remainder" };
  }
}

inline auto append_crossings(
  const Planet planet,
  const Category planet_category,
  const std::vector<Sample>& grid,
  std::vector<Event>& result
) -> void {
  const double spacing = planet_category == Category::INNER ? 360.0 : 90.0;
  for (std::size_t index = 1; index < grid.size(); ++index) {
    const auto& left = grid.at(index - 1);
    const auto& right = grid.at(index);
    const double low = std::min(left.difference, right.difference);
    const double high = std::max(left.difference, right.difference);
    const auto first_level = static_cast<int64_t>(std::ceil(low / spacing));
    const auto last_level = static_cast<int64_t>(std::floor(high / spacing));

    for (int64_t level_index = first_level; level_index <= last_level; ++level_index) {
      const double target_difference = spacing * static_cast<double>(level_index);
      const double jde_tt = refine_crossing(
        planet,
        left,
        right,
        {.target_difference = target_difference}
      );
      Kind kind = outer_kind(level_index);
      if (planet_category == Category::INNER) {
        const auto geometry = astro::planet::observation::geometry(planet, jde_tt);
        kind = geometry.phase_angle.deg() >= 90.0
          ? Kind::INFERIOR_CONJUNCTION
          : Kind::SUPERIOR_CONJUNCTION;
      }
      result.push_back({ .kind = kind, .jde_tt = jde_tt });
    }
  }
}

inline auto append_extrema(
  const Planet planet,
  const Category planet_category,
  const std::vector<Sample>& grid,
  std::vector<Event>& result
) -> void {
  for (std::size_t index = 1; index + 1 < grid.size(); ++index) {
    const auto& before = grid.at(index - 1);
    const auto& current = grid.at(index);
    const auto& after = grid.at(index + 1);

    if (planet_category == Category::INNER
        and current.separation > before.separation
        and current.separation > after.separation) {
      const auto separation = [planet](const double jde_tt) {
        return astro::planet::observation::geometry(planet, jde_tt).angular_separation.deg();
      };
      const double jde_tt = refine_extremum(planet, before.jde_tt, after.jde_tt, true, separation);
      const Kind kind = raw_difference(planet, jde_tt) > 0.0
        ? Kind::GREATEST_EASTERN_ELONGATION
        : Kind::GREATEST_WESTERN_ELONGATION;
      result.push_back({ .kind = kind, .jde_tt = jde_tt });
    }

    if (current.planet_λ > before.planet_λ and current.planet_λ > after.planet_λ) {
      const auto longitude = [planet, reference = current.planet_λ](const double jde_tt) {
        const double wrapped = astro::planet::geocentric_coord::apparent(planet, jde_tt).λ.deg();
        return unwrap_near(wrapped, reference);
      };
      result.push_back({
        .kind = Kind::STATION_RETROGRADE,
        .jde_tt = refine_extremum(planet, before.jde_tt, after.jde_tt, true, longitude),
      });
    }
    if (current.planet_λ < before.planet_λ and current.planet_λ < after.planet_λ) {
      const auto longitude = [planet, reference = current.planet_λ](const double jde_tt) {
        const double wrapped = astro::planet::geocentric_coord::apparent(planet, jde_tt).λ.deg();
        return unwrap_near(wrapped, reference);
      };
      result.push_back({
        .kind = Kind::STATION_DIRECT,
        .jde_tt = refine_extremum(planet, before.jde_tt, after.jde_tt, false, longitude),
      });
    }
  }
}

[[nodiscard]] inline auto in_year(
  const double jde_tt,
  const double start_jde_tt,
  const double end_jde_tt
) -> bool {
  return jde_tt >= start_jde_tt and jde_tt < end_jde_tt;
}

[[nodiscard]] inline auto search(
  const Planet planet,
  const double start_jde_tt,
  const double end_jde_tt,
  const double step_days
) -> std::vector<Event> {
  const auto planet_category = category(planet);
  const auto grid = samples(
    planet,
    {.start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt, .step_days = step_days}
  );
  const auto crossing_grid = crossing_samples(planet, grid);
  std::vector<Event> candidates;
  append_crossings(planet, planet_category, crossing_grid, candidates);
  append_extrema(planet, planet_category, grid, candidates);

  std::ranges::sort(candidates, {}, &Event::jde_tt);
  std::vector<Event> result;
  result.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (not in_year(candidate.jde_tt, start_jde_tt, end_jde_tt)) {
      continue;
    }
    if (not result.empty()
        and candidate.kind == result.back().kind
        and std::fabs(candidate.jde_tt - result.back().jde_tt) <= DEDUPLICATION_TOLERANCE_DAYS) {
      continue;
    }
    if (not result.empty() and candidate.jde_tt <= result.back().jde_tt) {
      throw std::runtime_error {
        std::format("Planetary events are not strictly ordered for {}", astro::planet::detail::name(planet))
      };
    }
    result.push_back(candidate);
  }
  return result;
}

} // namespace detail

/**
 * @brief Find all applicable observer-facing events for a major planet in a Gregorian year.
 * @param planet The planet to search, from Mercury through Neptune.
 * @param year The Gregorian year, in [1, 32766].
 * @return Events in strictly increasing JDE(TT) order.
 * @details Searches apparent geocentric ecliptic-of-date positions on a one-day candidate grid,
 *          then refines each event independently.
 * @throw std::invalid_argument If `year` is outside [1, 32766] or `planet` is not a named enumerator.
 * @throw std::runtime_error If an event cannot be refined or an internal event-consistency check fails.
 * @note The year is the half-open interval [Jan 1, next Jan 1) in UTC; before 1972, UTC degrades
 *       to the library's UT1 substitute. The declared year range is a computational input domain,
 *       not an accuracy guarantee for the underlying ephemeris at remote dates.
 * @note Inner planets receive inferior/superior conjunctions, greatest elongations, and stations.
 *       Outer planets receive conjunctions, oppositions, quadratures, and stations. The exhaustive
 *       `Planet` dispatch in this function's implementation is the applicability source of truth.
 * @note Conjunction, opposition, and quadrature solve the unwrapped apparent longitude difference
 *       planet minus Sun at 360 deg k, 180 deg + 360 deg k, and +/-90 deg + 360 deg k. Conjunction
 *       means equal longitude, not minimum separation, distance, or equal right ascension.
 * @note Greatest elongations are local maxima of great-circle Sun-planet separation, east or west by
 *       wrapped longitude difference. `STATION_RETROGRADE` marks the direct-to-retrograde turn and
 *       `STATION_DIRECT` the reverse; latitude may still be changing. Inferior conjunctions have
 *       phase angle at least 90 deg.
 */
[[nodiscard]] inline auto events(const Planet planet, const int32_t year) -> std::vector<Event> {
  if (year < 1 or year > 32766) {
    throw std::invalid_argument { std::format("Year {} is out of range [1, 32766].", year) };
  }
  const calendar::Datetime start_utc { util::to_ymd(year, 1, 1), 0.0 };
  const calendar::Datetime end_utc { util::to_ymd(year + 1, 1, 1), 0.0 };
  const double start_jde_tt = astro::julian_day::utc_to_jde(start_utc);
  const double end_jde_tt = astro::julian_day::utc_to_jde(end_utc);
  return detail::search(planet, start_jde_tt, end_jde_tt, detail::COARSE_STEP_DAYS);
}

} // namespace astro::planet::phenomena
