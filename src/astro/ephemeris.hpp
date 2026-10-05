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

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "julian_day.hpp"
#include "lunar_node.hpp"
#include "moon.hpp"
#include "planet.hpp"
#include "sun.hpp"

namespace astro::ephemeris {

/** @brief A body or lunar node. */
enum class Target : uint8_t {
  SUN = 0,
  MOON = 1,
  MERCURY = 2,
  VENUS = 3,
  MARS = 4,
  JUPITER = 5,
  SATURN = 6,
  URANUS = 7,
  NEPTUNE = 8,
  PLUTO = 9,
  MEAN_ASCENDING = 10,
  MEAN_DESCENDING = 11,
  TRUE_ASCENDING = 12,
  TRUE_DESCENDING = 13,
};

/** @brief A signed longitude rate; negative values represent retrograde motion. */
struct LongitudeRate {
  double deg_per_tt_day;
};

namespace detail {

inline constexpr double DIFFERENCE_STEP_TT_DAYS = 0.125;
inline constexpr double DIFFERENCE_REACH_TT_DAYS = 3.0 * DIFFERENCE_STEP_TT_DAYS;
inline constexpr double WORKING_START_JDE_TT = astro::julian_day::jm_to_jde(-10.0);
inline constexpr double WORKING_END_JDE_TT = astro::julian_day::jm_to_jde(10.0);

[[nodiscard]] inline auto longitude_for_rate(const Target target, const double jde_tt)
  -> astro::toolbox::AngleDeg {
  using astro::lunar_node::Node;
  using astro::lunar_node::position;
  using astro::planet::Planet;
  using astro::planet::geocentric_coord::apparent;
  using enum Target;

  static constexpr std::array mappings {
    std::pair { MERCURY, Planet::MERCURY },
    std::pair { VENUS,   Planet::VENUS },
    std::pair { MARS,    Planet::MARS },
    std::pair { JUPITER, Planet::JUPITER },
    std::pair { SATURN,  Planet::SATURN },
    std::pair { URANUS,  Planet::URANUS },
    std::pair { NEPTUNE, Planet::NEPTUNE },
    std::pair { PLUTO,   Planet::PLUTO },
  };
  for (const auto& [candidate, planet] : mappings) {
    if (target == candidate) {
      return apparent(planet, jde_tt).λ;
    }
  }

  switch (target) {
    case SUN:  return astro::sun::geocentric_coord::apparent(jde_tt).λ;
    case MOON: return astro::moon::geocentric_coord::apparent(jde_tt).λ;
    case MEAN_ASCENDING:
    case MEAN_DESCENDING:
      return position(Node::MEAN_ASCENDING, jde_tt);
    case TRUE_ASCENDING:
    case TRUE_DESCENDING:
      return position(Node::TRUE_ASCENDING, jde_tt);
    default:
      throw std::invalid_argument { std::format("Unknown ephemeris target {}", static_cast<uint32_t>(target)) };
  }
}

inline auto validate_stencil(const double jde_tt) -> void {
  if (not std::isfinite(jde_tt)) [[unlikely]] {
    throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
  }
  for (const int index : std::array { -3, -2, -1, 1, 2, 3 }) {
    const double offset = static_cast<double>(index) * DIFFERENCE_STEP_TT_DAYS;
    const double sample = jde_tt + offset;
    if (not std::isfinite(sample) or sample - jde_tt != offset) [[unlikely]] {
      throw std::invalid_argument {
        std::format("JDE(TT) {} cannot represent the longitude-rate stencil", jde_tt)
      };
    }
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Names distinguish the center and the two bounds.
inline auto validate_reach(const double jde_tt, const double start_jde_tt, const double end_jde_tt) -> void {
  if (jde_tt - DIFFERENCE_REACH_TT_DAYS < start_jde_tt
      or jde_tt + DIFFERENCE_REACH_TT_DAYS >= end_jde_tt) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Longitude-rate stencil at JDE(TT) {} leaves [{}, {})", jde_tt, start_jde_tt, end_jde_tt)
    };
  }
}

inline auto validate_rate_input(const Target target, const double jde_tt) -> void {
  validate_stencil(jde_tt);
  using enum Target;
  switch (target) {
    case SUN:
    case MOON:
    case MERCURY:
    case VENUS:
    case MARS:
    case JUPITER:
    case SATURN:
    case URANUS:
    case NEPTUNE:
      validate_reach(jde_tt, WORKING_START_JDE_TT, WORKING_END_JDE_TT);
      return;
    case PLUTO:
      validate_reach(
        jde_tt,
        astro::planet::detail::pluto::APPARENT_START_JDE_TT,
        astro::planet::detail::pluto::APPARENT_END_JDE_TT
      );
      return;
    case MEAN_ASCENDING:
    case MEAN_DESCENDING:
    case TRUE_ASCENDING:
    case TRUE_DESCENDING:
      validate_reach(jde_tt, astro::lunar_node::detail::START_JDE_TT, astro::lunar_node::detail::END_JDE_TT);
      return;
    default:
      throw std::invalid_argument { std::format("Unknown ephemeris target {}", static_cast<uint32_t>(target)) };
  }
}

/** @note Each sample pair's continuously lifted change must be within (-180, 180) degrees.
 *  @see https://en.wikipedia.org/wiki/Finite_difference_coefficient#Central_finite_difference */
template <typename LongitudeProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, astro::toolbox::AngleDeg>
[[nodiscard]] inline auto differentiate(const LongitudeProvider& longitude, const double jde_tt) -> LongitudeRate {
  const auto change = [&](const double offset) -> double {
    const double before = longitude(jde_tt - offset).deg();
    const double after = longitude(jde_tt + offset).deg();
    if (not std::isfinite(before) or not std::isfinite(after)) [[unlikely]] {
      throw std::runtime_error { std::format("Non-finite longitude-rate sample at JDE(TT) {}", jde_tt) };
    }
    return std::remainder(after - before, 360.0);
  };
  const double near = change(DIFFERENCE_STEP_TT_DAYS);
  const double middle = change(2.0 * DIFFERENCE_STEP_TT_DAYS);
  const double far = change(DIFFERENCE_REACH_TT_DAYS);
  const double rate = ((45.0 * near) - (9.0 * middle) + far) / (60.0 * DIFFERENCE_STEP_TT_DAYS);
  if (not std::isfinite(rate)) [[unlikely]] {
    throw std::runtime_error { std::format("Non-finite longitude rate at JDE(TT) {}", jde_tt) };
  }
  return { .deg_per_tt_day = rate };
}

} // namespace detail

/**
 * @brief Instantaneous geocentric longitude rate using the target's position model.
 * @param jde_tt JDE(TT).
 * @return Longitude rate in the true ecliptic and equinox of date.
 * @throw std::invalid_argument For invalid arguments, out-of-domain samples or inexact sample offsets.
 * @throw std::runtime_error If numerical evaluation fails.
 * @note Seven-point centered difference at 1/8 TT day spacing, sampling up to 3/8 TT day each way.
 * @note Center domains: Pluto [2409543.875, 2488069.125); nodes [2409542.875, 2488069.125);
 *       others [-1200954.625, 6104044.625).
 * @note Descending-node rates equal ascending-node rates.
 */
[[nodiscard]] inline auto longitude_rate(const Target target, const double jde_tt) -> LongitudeRate {
  detail::validate_rate_input(target, jde_tt);

  const auto longitude = [target](const double sample_jde_tt) -> astro::toolbox::AngleDeg {
    return detail::longitude_for_rate(target, sample_jde_tt);
  };
  return detail::differentiate(longitude, jde_tt);
}

} // namespace astro::ephemeris
