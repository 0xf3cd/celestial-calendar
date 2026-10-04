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
#include <format>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include "julian_day.hpp"
#include "lunar_node.hpp"
#include "moon.hpp"
#include "planet.hpp"
#include "sun.hpp"

namespace astro::ephemeris {

/** @brief Select the Sun. */
struct Sun {};

/** @brief Select the Moon. */
struct Moon {};

/** @brief A body or lunar node. */
using Target = std::variant<Sun, Moon, astro::planet::Planet, astro::lunar_node::Node>;

/** @brief A signed longitude rate; negative values represent retrograde motion. */
struct LongitudeRate {
  double deg_per_tt_day;
};

namespace detail {

inline constexpr double DIFFERENCE_STEP_TT_DAYS = 0.125;
inline constexpr double DIFFERENCE_REACH_TT_DAYS = 3.0 * DIFFERENCE_STEP_TT_DAYS;
inline constexpr double WORKING_START_JDE_TT = astro::julian_day::jm_to_jde(-10.0);
inline constexpr double WORKING_END_JDE_TT = astro::julian_day::jm_to_jde(10.0);

[[nodiscard]] inline auto longitude_for_rate([[maybe_unused]] const Sun source, const double jde_tt)
  -> astro::toolbox::AngleDeg {
  return astro::sun::geocentric_coord::apparent(jde_tt).λ;
}

[[nodiscard]] inline auto longitude_for_rate([[maybe_unused]] const Moon source, const double jde_tt)
  -> astro::toolbox::AngleDeg {
  return astro::moon::geocentric_coord::apparent(jde_tt).λ;
}

[[nodiscard]] inline auto longitude_for_rate(const astro::planet::Planet planet, const double jde_tt)
  -> astro::toolbox::AngleDeg {
  return astro::planet::geocentric_coord::apparent(planet, jde_tt).λ;
}

[[nodiscard]] inline auto longitude_for_rate(const astro::lunar_node::Node node, const double jde_tt)
  -> astro::toolbox::AngleDeg {
  using enum astro::lunar_node::Node;
  if (node == MEAN_DESCENDING) {
    return astro::lunar_node::position(MEAN_ASCENDING, jde_tt);
  }
  if (node == TRUE_DESCENDING) {
    return astro::lunar_node::position(TRUE_ASCENDING, jde_tt);
  }
  return astro::lunar_node::position(node, jde_tt);
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
 * @brief Calculate a body's or lunar node's instantaneous longitude rate.
 * @param target The Sun, Moon, a planet, or a lunar node.
 * @param jde_tt Julian Ephemeris Day (TT).
 * @return Signed tropical longitude rate in degrees per TT day, in the true ecliptic and equinox of date.
 * @throw std::invalid_argument For an invalid target/date or a stencil outside the working domain.
 * @throw std::runtime_error If the numerical evaluation fails.
 * @note Uses seven-point centered differentiation at 1/8-day spacing, reaching 3/8 day each way.
 *       Pluto accepts `[2409543.875, 2488069.125)`; nodes accept `[2409542.875, 2488069.125)`.
 *       Other targets accept `[-1200954.625, 6104044.625)`; all stencil offsets must be representable.
 * @note Includes each position model's apparent corrections. Descending-node rates equal ascending-node rates.
 */
[[nodiscard]] inline auto longitude_rate(const Target& target, const double jde_tt) -> LongitudeRate {
  detail::validate_stencil(jde_tt);
  if (target.valueless_by_exception()) [[unlikely]] {
    throw std::invalid_argument { "Longitude-rate target has no value" };
  }

  return std::visit([jde_tt]<typename Source>(const Source source) -> LongitudeRate {
    if constexpr (std::same_as<Source, astro::planet::Planet>) {
      static_cast<void>(astro::planet::detail::validate_apparent_input(source, jde_tt));
      if (source == astro::planet::Planet::PLUTO) {
        detail::validate_reach(
          jde_tt,
          astro::planet::detail::pluto::APPARENT_START_JDE_TT,
          astro::planet::detail::pluto::APPARENT_END_JDE_TT
        );
      } else {
        detail::validate_reach(jde_tt, detail::WORKING_START_JDE_TT, detail::WORKING_END_JDE_TT);
      }
    } else if constexpr (std::same_as<Source, astro::lunar_node::Node>) {
      astro::lunar_node::detail::validate(source, jde_tt);
      detail::validate_reach(
        jde_tt,
        astro::lunar_node::detail::START_JDE_TT,
        astro::lunar_node::detail::END_JDE_TT
      );
    } else {
      detail::validate_reach(jde_tt, detail::WORKING_START_JDE_TT, detail::WORKING_END_JDE_TT);
    }

    const auto longitude = [source](const double sample_jde_tt) -> astro::toolbox::AngleDeg {
      return detail::longitude_for_rate(source, sample_jde_tt);
    };

    return detail::differentiate(longitude, jde_tt);
  }, target);
}

} // namespace astro::ephemeris
