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
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>

#include "delta_t.hpp"
#include "ephemeris.hpp"
#include "geolocation.hpp"
#include "house.hpp"
#include "julian_day.hpp"
#include "leap_second.hpp"
#include "toolbox.hpp"

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
  toolbox::AngleDeg longitude;
  toolbox::AngleDeg latitude;
  std::optional<toolbox::DistanceAu> distance;
  astro::ephemeris::LongitudeRate longitude_rate;
};

inline constexpr std::size_t BODY_COUNT = 14;

/** @brief An owned fixed-instant chart ephemeris; bodies follow Target declaration order. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): This aggregate has no default constructor; all result members are supplied.
struct Snapshot {
  Times times;
  std::array<BodyState, BODY_COUNT> bodies;
  astro::house::Result houses;
};

namespace detail {

inline constexpr double START_JDE_TT = std::max({
  astro::ephemeris::longitude_rate_domain(astro::ephemeris::Target::PLUTO).front(),
  astro::ephemeris::longitude_rate_domain(astro::ephemeris::Target::TRUE_ASCENDING).front(),
  astro::ephemeris::longitude_rate_domain(astro::ephemeris::Target::SUN).front(),
});
inline constexpr double END_JDE_TT = std::min({
  astro::ephemeris::longitude_rate_domain(astro::ephemeris::Target::PLUTO).back(),
  astro::ephemeris::longitude_rate_domain(astro::ephemeris::Target::TRUE_ASCENDING).back(),
  astro::ephemeris::longitude_rate_domain(astro::ephemeris::Target::SUN).back(),
});
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
    case ALGO5:
      return;
    case ALGO4:
      if (jde_tt >= ALGO4_END_JDE_TT) {
        throw std::invalid_argument { "Chart Algo4 requires JDE(TT) before 2035-01-01" };
      }
      return;
    default:
      throw std::invalid_argument { std::format("Unknown Delta T model {}", static_cast<uint32_t>(model)) };
  }
}

[[nodiscard]] inline auto resolve_times(
  const calendar::Datetime& civil_dt,
  const Scale scale,
  const astro::delta_t::Model model
) -> Times {
  validate_civil(civil_dt, scale);
  try {
    const auto tt = scale == Scale::UTC
      ? astro::leap_second::utc_to_tt(civil_dt)
      : astro::delta_t::ut1_to_tt(model, civil_dt);
    const double jde_tt = astro::julian_day::tt_to_jde(tt);
    validate_domain(jde_tt, model);

    const auto ut1 = scale == Scale::UTC
      ? astro::delta_t::tt_to_ut1(model, tt)
      : civil_dt;
    return { .jd_ut1 = astro::julian_day::ut1_to_jd(ut1), .jde_tt = jde_tt };
  } catch (const std::out_of_range& error) {
    throw std::invalid_argument { std::format("Chart Delta T datetime leaves its model domain: {}", error.what()) };
  }
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Angle members require values; every aggregate construction supplies them.
struct Position {
  toolbox::AngleDeg longitude;
  toolbox::AngleDeg latitude;
  std::optional<toolbox::DistanceAu> distance;
};

[[nodiscard]] inline auto position(const astro::ephemeris::Target target, const double jde_tt) -> Position {
  using astro::lunar_node::Node;
  using astro::planet::geocentric_coord::apparent;
  using enum astro::ephemeris::Target;

  const auto with_distance = [](const toolbox::SphericalCoordinate& coordinate) -> Position {
    return { .longitude = coordinate.λ, .latitude = coordinate.β, .distance = coordinate.r };
  };

  const auto node_position = [jde_tt](const Node node) -> Position {
    return {
      .longitude = astro::lunar_node::position(node, jde_tt),
      .latitude = toolbox::AngleDeg { 0.0 },
      .distance = std::nullopt,
    };
  };

  if (const auto planet = astro::ephemeris::planet_for_target(target); planet.has_value()) {
    return with_distance(apparent(*planet, jde_tt));
  }

  switch (target) {
    case SUN: return with_distance(astro::sun::geocentric_coord::apparent(jde_tt));
    case MOON: return with_distance(astro::moon::geocentric_coord::apparent(jde_tt));
    case MEAN_ASCENDING: return node_position(Node::MEAN_ASCENDING);
    case MEAN_DESCENDING: return node_position(Node::MEAN_DESCENDING);
    case TRUE_ASCENDING: return node_position(Node::TRUE_ASCENDING);
    case TRUE_DESCENDING: return node_position(Node::TRUE_DESCENDING);
    default:
      throw std::invalid_argument { std::format("Unknown chart target {}", static_cast<uint32_t>(target)) };
  }
}

[[nodiscard]] inline auto evaluate_bodies(const double jde_tt) -> decltype(Snapshot::bodies) {
  using enum astro::ephemeris::Target;

  const auto evaluate = [jde_tt](const astro::ephemeris::Target target) -> BodyState {
    const auto coordinate = position(target, jde_tt);
    return {
      .target = target,
      .longitude = coordinate.longitude,
      .latitude = coordinate.latitude,
      .distance = coordinate.distance,
      .longitude_rate = astro::ephemeris::longitude_rate(target, jde_tt),
    };
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
 * @param system Equal, Whole Sign or Placidus.
 * @param model The Delta T model used for time conversion, default Algo5.
 * @return Time values, all fourteen target states, four axes and twelve cusps.
 * @throw std::invalid_argument If any input or the complete snapshot domain is invalid.
 * @throw std::runtime_error If numerical evaluation fails.
 * @note Civil years are [1885, 2100); JDE(TT) is [2409543.875, 2488069.125).
 *       Algo4 additionally requires JDE(TT) < 2464328.5. Exact sample-offset checks apply.
 * @note TT-to-UT1 uses one selected Delta T evaluation at TT;
 *       this is modelled UT1, not observed DUT1. Location affects houses only.
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
