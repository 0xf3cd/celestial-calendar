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
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "coord_transform.hpp"
#include "earth.hpp"
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

/** @brief The computation supplying a target's apparent position. */
enum class PositionSource : uint8_t {
  SUN_VSOP87D = 0,
  MOON_ELP2000_82B_TRUNC = 1,
  PLANET_VSOP87D = 2,
  PLUTO_PLUTO95_APPARENT = 3,
  NODE_MEAN = 4,
  NODE_TRUE_MEEUS_FIVE_TERM = 5, ///< Meeus' five-term approximation, not an osculating node.
};

/** @brief Origin of a bounded position domain; ignored when no position domain is declared. */
enum class DomainOrigin : uint8_t { SOURCE, IMPLEMENTATION };

/** @brief Target identity and provider domains; rate_node preserves the paired ascending trajectory. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Every facts row supplies all provider fields.
struct TargetFacts {
  Target target;
  PositionSource source;
  std::optional<astro::planet::Planet> planet;
  std::optional<astro::lunar_node::Node> node;
  std::optional<astro::lunar_node::Node> rate_node;
  std::optional<std::array<double, 2>> position_domain;
  std::array<double, 2> rate_provider_domain;
  DomainOrigin position_domain_origin;
};

/** @brief Target facts in chart order; absent position domains admit finite-input evaluation, which may fail. */
inline constexpr auto TARGET_FACTS = std::invoke([] {
  using enum Target;
  using enum PositionSource;
  using astro::planet::Planet;
  using astro::lunar_node::Node;

  constexpr std::array working { astro::julian_day::jm_to_jde(-10.0), astro::julian_day::jm_to_jde(10.0) };
  constexpr std::array pluto {
    astro::planet::geocentric_coord::PLUTO_START_JDE_TT,
    astro::planet::geocentric_coord::PLUTO_END_JDE_TT,
  };
  constexpr std::array nodes { astro::lunar_node::START_JDE_TT, astro::lunar_node::END_JDE_TT };

  // NOLINTBEGIN(modernize-use-designated-initializers): Keep the identity columns aligned across the complete roster.
  return std::array {
    TargetFacts { SUN,             SUN_VSOP87D,              {},              {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { MOON,            MOON_ELP2000_82B_TRUNC,    {},              {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { MERCURY,         PLANET_VSOP87D,           Planet::MERCURY, {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { VENUS,           PLANET_VSOP87D,           Planet::VENUS,   {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { MARS,            PLANET_VSOP87D,           Planet::MARS,    {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { JUPITER,         PLANET_VSOP87D,           Planet::JUPITER, {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { SATURN,          PLANET_VSOP87D,           Planet::SATURN,  {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { URANUS,          PLANET_VSOP87D,           Planet::URANUS,  {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { NEPTUNE,         PLANET_VSOP87D,           Planet::NEPTUNE, {},                    {},                   {},    working, DomainOrigin::IMPLEMENTATION },
    TargetFacts { PLUTO,           PLUTO_PLUTO95_APPARENT,    Planet::PLUTO,   {},                    {},                   pluto, pluto,   DomainOrigin::SOURCE         },
    TargetFacts { MEAN_ASCENDING,  NODE_MEAN,                {},              Node::MEAN_ASCENDING,  Node::MEAN_ASCENDING, nodes, nodes,   DomainOrigin::IMPLEMENTATION },
    TargetFacts { MEAN_DESCENDING, NODE_MEAN,                {},              Node::MEAN_DESCENDING, Node::MEAN_ASCENDING, nodes, nodes,   DomainOrigin::IMPLEMENTATION },
    TargetFacts { TRUE_ASCENDING,  NODE_TRUE_MEEUS_FIVE_TERM, {},              Node::TRUE_ASCENDING,  Node::TRUE_ASCENDING, nodes, nodes,   DomainOrigin::IMPLEMENTATION },
    TargetFacts { TRUE_DESCENDING, NODE_TRUE_MEEUS_FIVE_TERM, {},              Node::TRUE_DESCENDING, Node::TRUE_ASCENDING, nodes, nodes,   DomainOrigin::IMPLEMENTATION },
  };
  // NOLINTEND(modernize-use-designated-initializers)
});

/** @brief Recognize named Target values; new distinct enumerators must also enter TARGET_FACTS. */
[[nodiscard]] constexpr auto is_target(const Target target) -> bool {
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
    case PLUTO:
    case MEAN_ASCENDING:
    case MEAN_DESCENDING:
    case TRUE_ASCENDING:
    case TRUE_DESCENDING:
      return true;
  }
  return false;
}

namespace detail {

/** @brief Keep each source's dispatch operands consistent before any runtime lookup. */
[[nodiscard]] constexpr auto consistent_provider(const TargetFacts& facts) -> bool {
  using enum PositionSource;
  const bool physical = not facts.node.has_value() and not facts.rate_node.has_value();
  switch (facts.source) {
    case SUN_VSOP87D:
    case MOON_ELP2000_82B_TRUNC:
      return physical and not facts.planet.has_value();
    case PLANET_VSOP87D:
      return physical and facts.planet.has_value() and *facts.planet != astro::planet::Planet::PLUTO;
    case PLUTO_PLUTO95_APPARENT:
      return physical and facts.planet == astro::planet::Planet::PLUTO;
    case NODE_MEAN:
      return not facts.planet.has_value() and facts.rate_node == astro::lunar_node::Node::MEAN_ASCENDING
        and (facts.node == astro::lunar_node::Node::MEAN_ASCENDING or facts.node == astro::lunar_node::Node::MEAN_DESCENDING);
    case NODE_TRUE_MEEUS_FIVE_TERM:
      return not facts.planet.has_value() and facts.rate_node == astro::lunar_node::Node::TRUE_ASCENDING
        and (facts.node == astro::lunar_node::Node::TRUE_ASCENDING or facts.node == astro::lunar_node::Node::TRUE_DESCENDING);
  }
  return false;
}

/** @brief Check membership, provider operands and shared-source windows; enum aliases are indistinguishable. */
[[nodiscard]] consteval auto complete_target_facts() -> bool {
  for (const auto& facts : TARGET_FACTS) {
    if (not consistent_provider(facts)) {
      return false;
    }
    for (const auto& other : TARGET_FACTS) {
      const bool same_position_window = facts.position_domain == other.position_domain
        and (not facts.position_domain.has_value() or facts.position_domain_origin == other.position_domain_origin);
      if (facts.source == other.source and not same_position_window) {
        return false;
      }
    }
  }

  for (uint16_t code = 0; code <= std::numeric_limits<uint8_t>::max(); ++code) {
    const auto target = static_cast<Target>(code);
    std::size_t rows = 0;
    for (const auto& facts : TARGET_FACTS) {
      rows += facts.target == target ? std::size_t { 1 } : std::size_t { 0 };
    }
    if (rows != (is_target(target) ? std::size_t { 1 } : std::size_t { 0 })) {
      return false;
    }
  }
  return true;
}

static_assert(complete_target_facts(), "Every Target needs one consistent provider row; shared sources need one position domain");

} // namespace detail

/** @brief The target roster, derived from the facts table in chart order. */
inline constexpr auto TARGETS = std::invoke([] {
  std::array<Target, TARGET_FACTS.size()> targets {};
  for (std::size_t index = 0; index < targets.size(); ++index) {
    targets.at(index) = TARGET_FACTS.at(index).target;
  }
  return targets;
});

/** @brief Look up a named target; unknown values are invalid arguments. */
[[nodiscard]] constexpr auto target_facts(const Target target) -> const TargetFacts& {
  if (is_target(target)) {
    for (const auto& facts : TARGET_FACTS) {
      if (facts.target == target) {
        return facts;
      }
    }
  }
  throw std::invalid_argument { std::format("Unknown ephemeris target {}", static_cast<uint32_t>(target)) };
}

/** @brief The source of a named target's apparent position. */
[[nodiscard]] constexpr auto source_of(const Target target) -> PositionSource {
  return target_facts(target).source;
}

/** @brief Half-open provider domain, or no hard interval; neither implies accuracy or successful evaluation. */
[[nodiscard]] constexpr auto position_domain(const Target target) -> std::optional<std::array<double, 2>> {
  return target_facts(target).position_domain;
}

/** @brief Apparent geocentric coordinates in the true ecliptic and equinox of date. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Angle members require values; every aggregate construction supplies them.
struct Position {
  toolbox::AngleDeg longitude;
  toolbox::AngleDeg latitude;
  std::optional<toolbox::DistanceAu> distance;
};

/** @brief Apparent geocentric equatorial coordinates of date; nodes have no physical distance. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Angle members require values; every aggregate construction supplies them.
struct EquatorialPosition {
  toolbox::AngleDeg right_ascension;
  toolbox::AngleDeg declination;
  std::optional<toolbox::DistanceAu> distance;
};

/**
 * @brief A signed instantaneous longitude rate; a negative value means longitude is decreasing at that instant.
 * @note Near a station its sign can differ from `astro::planet::geocentric_coord::is_retrograde` (planet.hpp),
 *       which tests the longitude change across one day.
 */
struct LongitudeRate {
  double deg_per_tt_day;
};

namespace detail {

inline constexpr double DIFFERENCE_STEP_TT_DAYS = 0.125;
inline constexpr double DIFFERENCE_REACH_TT_DAYS = 3.0 * DIFFERENCE_STEP_TT_DAYS;

/**
 * @brief Return an apparent geocentric ecliptic position, including AU distance.
 * @throws std::logic_error If the source is nonphysical or a required planet operand is missing.
 */
[[nodiscard]] inline auto physical_apparent(const TargetFacts& facts, const double jde_tt) -> toolbox::SphericalCoordinate {
  using enum PositionSource;
  switch (facts.source) {
    case SUN_VSOP87D:
      return astro::sun::geocentric_coord::apparent(jde_tt);
    case MOON_ELP2000_82B_TRUNC:
      return astro::moon::geocentric_coord::apparent(jde_tt);
    case PLANET_VSOP87D:
    case PLUTO_PLUTO95_APPARENT:
      if (facts.planet.has_value()) {
        return astro::planet::geocentric_coord::apparent(*facts.planet, jde_tt);
      }
      break;
    case NODE_MEAN:
    case NODE_TRUE_MEEUS_FIVE_TERM:
      break;
  }
  throw std::logic_error { "Target source has no physical position provider" };
}

[[nodiscard]] inline auto longitude_for_rate(const Target target, const double jde_tt) -> toolbox::AngleDeg {
  const auto& facts = target_facts(target);
  if (facts.rate_node.has_value()) {
    return astro::lunar_node::position(*facts.rate_node, jde_tt);
  }
  return physical_apparent(facts, jde_tt).λ;
}

/** @brief Reject impossible computed coordinates without clamping latitude or distance. */
inline auto validate_position(const Position& coordinate) -> void {
  if (not std::isfinite(coordinate.longitude.deg()) or not std::isfinite(coordinate.latitude.deg())) {
    throw std::runtime_error { "Computed position has non-finite angles" };
  }
  if (coordinate.latitude.deg() < -90.0 or coordinate.latitude.deg() > 90.0) {
    throw std::runtime_error { "Computed position latitude leaves [-90, 90] degrees" };
  }
  if (coordinate.distance.has_value()) {
    const double distance_au = coordinate.distance->au();
    if (not std::isfinite(distance_au) or distance_au <= 0.0) {
      throw std::runtime_error { "Computed position requires finite positive physical distance" };
    }
  }
}

inline auto validate_stencil(const double jde_tt) -> void {
  if (not std::isfinite(jde_tt)) [[unlikely]] {
    throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
  }
  for (const int32_t index : std::array<int32_t, 6> { -3, -2, -1, 1, 2, 3 }) {
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
  const auto [start_jde_tt, end_jde_tt] = target_facts(target).rate_provider_domain;
  validate_reach(jde_tt, start_jde_tt, end_jde_tt);
}

/** @note Each sample pair's continuously lifted change must be within (-180, 180) degrees.
 *  @see https://en.wikipedia.org/wiki/Finite_difference_coefficient#Central_finite_difference */
template <typename LongitudeProvider>
requires std::invocable<const LongitudeProvider&, double>
     and std::same_as<std::invoke_result_t<const LongitudeProvider&, double>, toolbox::AngleDeg>
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
 * @brief Evaluate one target's apparent position without a rate stencil or civil policy.
 * @param target A named ephemeris target.
 * @param jde_tt Finite JDE(TT), within the provider window where one is declared.
 * @return Longitude in [0, 360), latitude in [-90, 90] degrees, and positive distance where physical.
 * @throw std::invalid_argument For malformed or out-of-provider-domain input.
 * @throw std::runtime_error If computation fails or produces invalid coordinates.
 * @note Where no hard input interval is declared, finite-input evaluation gives no accuracy or plausibility guarantee.
 */
[[nodiscard]] inline auto position(const Target target, const double jde_tt) -> Position {
  const auto& facts = target_facts(target);
  if (not std::isfinite(jde_tt)) {
    throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
  }

  const auto coordinate = std::invoke([&]() -> Position {
    if (facts.node.has_value()) {
      return {
        .longitude = astro::lunar_node::position(*facts.node, jde_tt),
        .latitude = toolbox::AngleDeg { 0.0 },
        .distance = std::nullopt,
      };
    }

    const auto physical = detail::physical_apparent(facts, jde_tt);
    return { .longitude = physical.λ, .latitude = physical.β, .distance = physical.r };
  });

  detail::validate_position(coordinate);
  return coordinate;
}

/**
 * @brief Project one apparent ecliptic position to geocentric apparent RA/Dec of date.
 * @param target A named ephemeris target.
 * @param jde_tt Finite JDE(TT), within the target's declared provider window.
 * @return RA in [0, 360) and declination in [-90, 90] degrees, with physical distance where present.
 * @throw std::invalid_argument For malformed or out-of-provider-domain input.
 * @throw std::runtime_error If position evaluation or projection produces invalid coordinates.
 * @note Longitude nutation and true obliquity use fixed IAU1980; no rate stencil or civil policy applies.
 *       Coordinate validity establishes neither accuracy nor physical plausibility at remote epochs.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, formulas (13.3), (13.4) and (22.2).
 */
[[nodiscard]] inline auto equatorial_position(const Target target, const double jde_tt) -> EquatorialPosition {
  const auto ecliptic = position(target, jde_tt);
  const auto equatorial = std::invoke([&] {
    try {
      const auto ε = earth::obliquity::true_obliquity(jde_tt, earth::nutation::Model::IAU_1980);
      return coords::ecliptic_to_equatorial(ecliptic.longitude, ecliptic.latitude, ε);
    } catch (const std::invalid_argument& error) {
      throw std::runtime_error { std::format("Equatorial projection failed: {}", error.what()) };
    }
  });

  detail::validate_position({
    .longitude = equatorial.α,
    .latitude = equatorial.δ,
    .distance = ecliptic.distance,
  });
  return {
    .right_ascension = equatorial.α,
    .declination = equatorial.δ,
    .distance = ecliptic.distance,
  };
}

/**
 * @brief Return the half-open center domain of a target's longitude-rate operation.
 * @param target A named ephemeris target.
 * @return Inclusive start and exclusive end in JDE(TT), allowing the full sampled reach.
 * @throw std::invalid_argument If the target is unknown.
 * @note This window bounds the rate operation, not the target's position model.
 *       Exact sample-offset representability is checked separately by @ref longitude_rate.
 */
[[nodiscard]] constexpr auto longitude_rate_domain(const Target target) -> std::array<double, 2> {
  const auto [start_jde_tt, end_jde_tt] = target_facts(target).rate_provider_domain;
  return {
    start_jde_tt + detail::DIFFERENCE_REACH_TT_DAYS,
    end_jde_tt - detail::DIFFERENCE_REACH_TT_DAYS,
  };
}

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

  const auto longitude = [target](const double sample_jde_tt) -> toolbox::AngleDeg {
    return detail::longitude_for_rate(target, sample_jde_tt);
  };
  return detail::differentiate(longitude, jde_tt);
}

} // namespace astro::ephemeris
