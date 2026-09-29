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
#include <cstdint>
#include <format>
#include <stdexcept>

#include "earth.hpp"
#include "elp2000_82b.hpp"
#include "julian_day.hpp"
#include "toolbox.hpp"

namespace astro::lunar_node {

/** @brief A mean or Meeus five-term true lunar node. */
enum class Node : uint8_t {
  MEAN_ASCENDING,
  MEAN_DESCENDING,
  TRUE_ASCENDING,
  TRUE_DESCENDING,
};

namespace detail {

inline constexpr double START_JDE_TT = 2409542.5;
inline constexpr double END_JDE_TT = 2488069.5;

inline auto validate(const Node node, const double jde_tt) -> void {
  switch (node) {
    case Node::MEAN_ASCENDING:
    case Node::MEAN_DESCENDING:
    case Node::TRUE_ASCENDING:
    case Node::TRUE_DESCENDING: break;
    default:
      throw std::invalid_argument {
        std::format("Unknown lunar node {}", static_cast<uint32_t>(node))
      };
  }

  if (not std::isfinite(jde_tt)) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `jde_tt` is not finite, got {}", jde_tt)
    };
  }
  if (jde_tt < START_JDE_TT or jde_tt >= END_JDE_TT) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `jde_tt` out of range [{}, {}), got {}", START_JDE_TT, END_JDE_TT, jde_tt)
    };
  }
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, Equation (47.7). */
[[nodiscard]] inline auto mean_ascending(const double jc) -> astro::toolbox::AngleDeg {
  const double jc2 = jc * jc;
  const double jc3 = jc2 * jc;
  const double jc4 = jc3 * jc;
  return astro::toolbox::AngleDeg {
    125.0445479
      - (1934.1362891 * jc)
      + (0.0020754 * jc2)
      + (jc3 / 467441)
      - (jc4 / 60616000)
  };
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, printed p. 344. */
[[nodiscard]] inline auto true_ascending(const double jc) -> astro::toolbox::AngleDeg {
  const auto ctx = astro::elp2000_82b::create_context(jc);
  const double correction_deg =
    (-1.4979 * std::sin(((ctx.D - ctx.F) * 2.0).rad()))
    - (0.1500 * std::sin(ctx.M.rad()))
    - (0.1226 * std::sin((ctx.D * 2.0).rad()))
    + (0.1176 * std::sin((ctx.F * 2.0).rad()))
    - (0.0801 * std::sin(((ctx.Mp - ctx.F) * 2.0).rad()));
  return mean_ascending(jc) + astro::toolbox::AngleDeg { correction_deg };
}

} // namespace detail

/**
 * @brief Calculate a mean or Meeus five-term true lunar-node longitude.
 * @param node The lunar node to calculate.
 * @param jde_tt The Julian Ephemeris Day on the TT scale, in `[2409542.5, 2488069.5)`.
 * @return Tropical longitude in the true ecliptic and equinox of date, normalized to `[0°, 360°)`.
 * @throw std::invalid_argument If `node` is not a named enumerator or `jde_tt` is non-finite or
 *        outside `[2409542.5, 2488069.5)`.
 * @note Mean ascending uses Meeus equation (47.7); true ascending adds exactly the five periodic
 *       terms on printed p. 344. Both then receive the full IAU 1980 nutation in longitude.
 *       Descending nodes are their exact normalized antipodes.
 * @note The half-open domain is project-selected to match Pluto's raw domain; Meeus gives no
 *       accuracy interval for these formulas.
 * @note The selected ecliptic-node definition fixes latitude at exactly zero. This longitude-only
 *       API has no latitude field and no physical distance.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Equation (47.7) and printed p. 344.
 * @see astro::earth::nutation::longitude
 */
[[nodiscard]] inline auto position(const Node node, const double jde_tt) -> astro::toolbox::AngleDeg {
  detail::validate(node, jde_tt);

  const double jc = astro::julian_day::jde_to_jc(jde_tt);
  const bool is_true = node == Node::TRUE_ASCENDING or node == Node::TRUE_DESCENDING;
  const bool is_descending = node == Node::MEAN_DESCENDING or node == Node::TRUE_DESCENDING;
  const auto raw_ascending = is_true ? detail::true_ascending(jc) : detail::mean_ascending(jc);
  const auto ascending = (
    raw_ascending
    + astro::earth::nutation::longitude(jde_tt, astro::earth::nutation::Model::IAU_1980)
  ).normalize();

  if (is_descending) {
    return (ascending + astro::toolbox::AngleDeg { 180.0 }).normalize();
  }
  return ascending;
}

} // namespace astro::lunar_node
