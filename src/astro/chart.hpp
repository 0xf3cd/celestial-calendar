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
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>

#include "delta_t.hpp"
#include "ephemeris.hpp"
#include "house.hpp"
#include "julian_day.hpp"
#include "leap_second.hpp"

namespace astro::chart {

/** @brief The explicitly declared scale of a civil datetime. */
enum class Scale : uint8_t { UTC, UT1 };

/** @brief Modelled UT1 and TT at the same instant, in Julian days. */
struct Times {
  double jd_ut1;
  double jde_tt;
};

/** @brief Geocentric position and longitude rate in the true ecliptic and equinox of date. */
struct BodyState {
  astro::ephemeris::Target target;
  astro::toolbox::AngleDeg longitude;
  astro::toolbox::AngleDeg latitude;
  std::optional<astro::toolbox::DistanceAu> distance;
  astro::ephemeris::LongitudeRate longitude_rate;
};

/** @brief An owned fixed-instant chart ephemeris; bodies follow Target declaration order. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): This aggregate has no default constructor; all result members are supplied.
struct Snapshot {
  Times times;
  std::array<BodyState, 14> bodies;
  astro::house::Result houses;
};

namespace detail {

inline constexpr double START_JDE_TT = astro::planet::detail::pluto::APPARENT_START_JDE_TT
                                      + astro::ephemeris::detail::DIFFERENCE_REACH_TT_DAYS;
inline constexpr double END_JDE_TT = astro::planet::detail::pluto::APPARENT_END_JDE_TT
                                    - astro::ephemeris::detail::DIFFERENCE_REACH_TT_DAYS;
inline constexpr double ALGO4_END_JDE_TT = 2464328.5;

inline auto validate_civil(const calendar::Datetime& civil_dt, const Scale scale) -> void {
  if (not civil_dt.ok()) {
    throw std::invalid_argument { "Invalid chart civil datetime" };
  }
  if (civil_dt.year() < 1885 or civil_dt.year() >= 2100) {
    throw std::invalid_argument { std::format("Chart civil year {} leaves [1885, 2100)", civil_dt.year()) };
  }
  switch (scale) {
    case Scale::UT1: return;
    case Scale::UTC:
      if (civil_dt.ymd < astro::leap_second::MODERN_UTC_START) {
        throw std::invalid_argument { "Chart UTC input begins at 1972-01-01" };
      }
      return;
    default:
      throw std::invalid_argument { std::format("Unknown chart time scale {}", static_cast<uint32_t>(scale)) };
  }
}

inline auto validate_domain(const double jde_tt, const astro::delta_t::Model model) -> void {
  astro::ephemeris::detail::validate_stencil(jde_tt);
  if (jde_tt < START_JDE_TT or jde_tt >= END_JDE_TT) {
    throw std::invalid_argument {
      std::format("Chart JDE(TT) {} leaves [{}, {})", jde_tt, START_JDE_TT, END_JDE_TT)
    };
  }
  using enum astro::delta_t::Model;
  switch (model) {
    case ALGO1:
    case ALGO2:
    case ALGO3:
    case ALGO5: return;
    case ALGO4:
      if (jde_tt >= ALGO4_END_JDE_TT) {
        throw std::invalid_argument { "Chart Algo4 requires JDE(TT) before 2035-01-01" };
      }
      return;
    default:
      throw std::invalid_argument { std::format("Unknown Delta T model {}", static_cast<uint32_t>(model)) };
  }
}

[[nodiscard]] inline auto delta_t_seconds(const astro::delta_t::Model model, const calendar::Datetime& dt) -> double {
  if (model == astro::delta_t::Model::ALGO4 and dt.year() >= 2035) {
    throw std::invalid_argument { "Chart Algo4 datetime leaves its model-year domain" };
  }
  const double seconds = astro::delta_t::compute(model, dt);
  if (not std::isfinite(seconds)) {
    throw std::runtime_error { "Non-finite chart Delta T evaluation" };
  }
  return seconds;
}

[[nodiscard]] inline auto resolve_times(
  const calendar::Datetime& civil_dt,
  const Scale scale,
  const astro::delta_t::Model model
) -> Times {
  validate_civil(civil_dt, scale);
  const auto tt = scale == Scale::UTC
    ? astro::leap_second::utc_to_tt(civil_dt)
    : calendar::add_seconds(civil_dt, delta_t_seconds(model, civil_dt));
  const double jde_tt = astro::julian_day::tt_to_jde(tt);
  validate_domain(jde_tt, model);

  const auto ut1 = scale == Scale::UT1
    ? civil_dt
    : calendar::add_seconds(tt, -delta_t_seconds(model, tt));
  return { .jd_ut1 = astro::julian_day::ut1_to_jd(ut1), .jde_tt = jde_tt };
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Angle members require values; every aggregate construction supplies them.
struct Position {
  astro::toolbox::AngleDeg longitude;
  astro::toolbox::AngleDeg latitude;
  std::optional<astro::toolbox::DistanceAu> distance;
};

[[nodiscard]] inline auto position(const astro::ephemeris::Target target, const double jde_tt) -> Position {
  using astro::lunar_node::Node;
  using astro::planet::Planet;
  using astro::planet::geocentric_coord::apparent;
  using enum astro::ephemeris::Target;
  const auto with_distance = [](const astro::toolbox::SphericalCoordinate& coordinate) -> Position {
    return { .longitude = coordinate.λ, .latitude = coordinate.β, .distance = coordinate.r };
  };
  const auto node_position = [jde_tt](const Node node) -> Position {
    return { .longitude = astro::lunar_node::position(node, jde_tt),
             .latitude = astro::toolbox::AngleDeg { 0.0 }, .distance = std::nullopt };
  };
  switch (target) {
    case SUN:     return with_distance(astro::sun::geocentric_coord::apparent(jde_tt));
    case MOON:    return with_distance(astro::moon::geocentric_coord::apparent(jde_tt));
    case MERCURY: return with_distance(apparent(Planet::MERCURY, jde_tt));
    case VENUS:   return with_distance(apparent(Planet::VENUS, jde_tt));
    case MARS:    return with_distance(apparent(Planet::MARS, jde_tt));
    case JUPITER: return with_distance(apparent(Planet::JUPITER, jde_tt));
    case SATURN:  return with_distance(apparent(Planet::SATURN, jde_tt));
    case URANUS:  return with_distance(apparent(Planet::URANUS, jde_tt));
    case NEPTUNE: return with_distance(apparent(Planet::NEPTUNE, jde_tt));
    case PLUTO:   return with_distance(apparent(Planet::PLUTO, jde_tt));
    case MEAN_ASCENDING: return node_position(Node::MEAN_ASCENDING);
    case MEAN_DESCENDING: return node_position(Node::MEAN_DESCENDING);
    case TRUE_ASCENDING: return node_position(Node::TRUE_ASCENDING);
    case TRUE_DESCENDING: return node_position(Node::TRUE_DESCENDING);
    default:
      throw std::invalid_argument { std::format("Unknown chart target {}", static_cast<uint32_t>(target)) };
  }
}

[[nodiscard]] inline auto evaluate_bodies(const double jde_tt) -> std::array<BodyState, 14> {
  using enum astro::ephemeris::Target;
  const auto evaluate = [jde_tt](const astro::ephemeris::Target target) -> BodyState {
    const auto coordinate = position(target, jde_tt);
    return { .target = target, .longitude = coordinate.longitude, .latitude = coordinate.latitude,
             .distance = coordinate.distance, .longitude_rate = astro::ephemeris::longitude_rate(target, jde_tt) };
  };
  return {
    evaluate(SUN), evaluate(MOON), evaluate(MERCURY), evaluate(VENUS), evaluate(MARS), evaluate(JUPITER),
    evaluate(SATURN), evaluate(URANUS), evaluate(NEPTUNE), evaluate(PLUTO),
    evaluate(MEAN_ASCENDING), evaluate(MEAN_DESCENDING), evaluate(TRUE_ASCENDING), evaluate(TRUE_DESCENDING),
  };
}

} // namespace detail

/**
 * @brief Calculate geocentric positions, rates and house geometry at an explicitly scaled civil instant.
 * @param civil_dt A valid civil datetime; timezone conversion and inserted leap seconds are outside the input.
 * @param scale UTC from 1972-01-01, or explicitly labelled UT1.
 * @param location North-positive latitude in (-90, 90) degrees; east-positive longitude in [-180, 180].
 * @param system Equal, Whole Sign or Placidus; Placidus's polar failure is preserved.
 * @param model The retained Delta T model used for time conversion, default Algo5.
 * @return Owned time values, all fourteen target states, four axes and twelve cusps.
 * @throw std::invalid_argument If any input or the complete snapshot domain is invalid.
 * @throw std::runtime_error If numerical evaluation fails.
 * @note Civil years are [1885, 2100); JDE(TT) is [2409543.875, 2488069.125).
 *       Algo4 additionally requires JDE(TT) < 2464328.5. Exact sample-offset checks still apply.
 * @note TT-to-UT1 uses one selected Delta T evaluation at TT, as in the existing conversion policy;
 *       this is modelled UT1, not measured DUT1. Location affects houses only.
 */
[[nodiscard]] inline auto calculate(
  const calendar::Datetime& civil_dt,
  const Scale scale,
  const astro::GeoLocation& location,
  const astro::house::System system,
  const astro::delta_t::Model model = astro::delta_t::Model::ALGO5
) -> Snapshot {
  const auto times = detail::resolve_times(civil_dt, scale, model);
  const auto houses = astro::house::calculate(times.jd_ut1, times.jde_tt, location, system);
  const auto bodies = detail::evaluate_bodies(times.jde_tt);
  return { .times = times, .bodies = bodies, .houses = houses };
}

} // namespace astro::chart
