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
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <stdexcept>
#include <tuple>

#include "delta_t.hpp"
#include "ephemeris.hpp"
#include "geolocation.hpp"
#include "house.hpp"
#include "julian_day.hpp"
#include "toolbox.hpp"

namespace astro::chart {

/** @brief Adopted civil-year policy for charts and civil house queries. */
inline constexpr int32_t CIVIL_START_YEAR = 1885;
inline constexpr int32_t CIVIL_END_YEAR_EXCLUSIVE = 2100;

/** @brief Geocentric position and longitude rate in the true ecliptic and equinox of date. */
struct BodyState {
  astro::ephemeris::Target target;
  toolbox::AngleDeg longitude;
  toolbox::AngleDeg latitude;
  std::optional<toolbox::DistanceAu> distance;
  astro::ephemeris::LongitudeRate longitude_rate;
};

inline constexpr std::size_t BODY_COUNT = astro::ephemeris::TARGETS.size();

/** @brief An independent fixed-instant chart value; bodies follow the ephemeris target roster. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): This aggregate has no default constructor; all result members are supplied.
struct Snapshot {
  astro::julian_day::Times times;
  std::array<BodyState, BODY_COUNT> bodies;
  astro::house::Result houses;
};

/** @brief Exclusive derived-TT cutoff for this civil policy, only where the selected model binds it. */
[[nodiscard]] constexpr auto civil_derived_tt_cutoff(const astro::delta_t::Model model) -> std::optional<double> {
  using enum astro::delta_t::Model;
  switch (model) {
    case ALGO1:
    case ALGO2:
    case ALGO3:
    case ALGO5:
      return std::nullopt;
    case ALGO4:
      return astro::delta_t::ALGO4_END_JDE_TT;
  }
  throw std::invalid_argument { std::format("Unknown Delta T model {}", static_cast<uint32_t>(model)) };
}

namespace detail {

inline constexpr auto JOINT_DOMAIN = std::invoke([] {
  auto domain = astro::ephemeris::longitude_rate_domain(astro::ephemeris::TARGETS.front());
  for (const auto target : astro::ephemeris::TARGETS) {
    const auto next = astro::ephemeris::longitude_rate_domain(target);
    domain.front() = std::max(domain.front(), next.front());
    domain.back() = std::min(domain.back(), next.back());
  }
  return domain;
});
inline constexpr double START_JDE_TT = JOINT_DOMAIN.front();
inline constexpr double END_JDE_TT = JOINT_DOMAIN.back();

/** @brief Add model-domain context to the Algo4 forward conversion from civil UT1. */
inline auto with_delta_t_domain_context(
  const astro::delta_t::Model model,
  const astro::julian_day::Scale scale,
  const auto& convert
) -> decltype(convert()) {
  try {
    return convert();
  } catch (const std::invalid_argument& error) {
    const bool algo4_ut1_forward = model == astro::delta_t::Model::ALGO4 and scale == astro::julian_day::Scale::UT1;
    if (not algo4_ut1_forward) {
      throw;
    }
    throw std::invalid_argument {
      std::format("Civil Delta T datetime leaves its model domain: {}", error.what())
    };
  }
}

/** @brief Evaluate positions then rates in target-roster order at one admitted chart JDE(TT). */
[[nodiscard]] inline auto evaluate_bodies(const double jde_tt) -> decltype(Snapshot::bodies) {
  const auto evaluate = [jde_tt](const astro::ephemeris::Target target) -> BodyState {
    const auto coordinate = astro::ephemeris::position(target, jde_tt);
    return {
      .target = target,
      .longitude = coordinate.longitude,
      .latitude = coordinate.latitude,
      .distance = coordinate.distance,
      .longitude_rate = astro::ephemeris::longitude_rate(target, jde_tt),
    };
  };

  return std::apply([&evaluate](const auto... targets) {
    return std::array { evaluate(targets)... };
  }, astro::ephemeris::TARGETS);
}

} // namespace detail

/**
 * @brief Resolve a civil instant under the adopted chart/civil-house admission policy.
 * @param civil_dt A valid civil datetime in [CIVIL_START_YEAR, CIVIL_END_YEAR_EXCLUSIVE).
 * @param scale UTC from 1972-01-01, or explicitly labelled UT1.
 * @param model The Delta T model used for time conversion.
 * @return Modelled UT1 and TT; UT1 input passes through unchanged.
 * @throw std::invalid_argument For invalid input or a civil/model policy rejection.
 * @note Applies the derived-TT cutoff before any TT-to-UT1 conversion, but no joint target/rate window.
 *       Future UTC holds the last known leap-table offset; UT1 is modelled, not observed DUT1.
 */
[[nodiscard]] inline auto resolve_civil_times(
  const calendar::Datetime& civil_dt,
  const astro::julian_day::Scale scale,
  const astro::delta_t::Model model = astro::delta_t::DEFAULT_MODEL
) -> astro::julian_day::Times {
  if (not civil_dt.ok()) {
    throw std::invalid_argument {
      std::format(
        "Invalid civil datetime: {}-{}-{}, time-of-day {} ns",
        civil_dt.year(), civil_dt.month(), civil_dt.day(), civil_dt.time_of_day.to_duration().count()
      )
    };
  }
  if (civil_dt.year() < CIVIL_START_YEAR or civil_dt.year() >= CIVIL_END_YEAR_EXCLUSIVE) {
    throw std::invalid_argument {
      std::format("Civil year {} leaves [{}, {})", civil_dt.year(), CIVIL_START_YEAR, CIVIL_END_YEAR_EXCLUSIVE)
    };
  }

  const auto tt = detail::with_delta_t_domain_context(model, scale, [&] {
    return astro::julian_day::civil_to_tt(civil_dt, scale, model);
  });
  const double jde_tt = astro::julian_day::tt_to_jde(tt);
  if (const auto cutoff = civil_derived_tt_cutoff(model); cutoff.has_value() and jde_tt >= *cutoff) {
    throw std::invalid_argument {
      std::format("Civil Delta T requires JDE(TT) before {}", *cutoff)
    };
  }

  const double jd_ut1 = scale == astro::julian_day::Scale::UT1
    ? astro::julian_day::ut1_to_jd(civil_dt)
    : astro::julian_day::ut1_to_jd(astro::delta_t::tt_to_ut1(model, tt));
  return { .jd_ut1 = jd_ut1, .jde_tt = jde_tt };
}

/**
 * @brief Calculate geocentric positions, rates and house geometry at an explicitly scaled civil instant.
 * @param civil_dt A valid civil datetime; timezone conversion and inserted leap seconds are outside the input.
 * @param scale UTC from 1972-01-01, or explicitly labelled UT1.
 * @param location North-positive latitude in (-90, 90) degrees; east-positive longitude in [-180, 180].
 * @param system Equal, Whole Sign or Placidus.
 * @param model The Delta T model used for time conversion, defaulting to delta_t::DEFAULT_MODEL.
 * @return Time values, all target states, four axes and twelve cusps.
 * @throw std::invalid_argument If any input or the complete snapshot domain is invalid.
 * @throw std::runtime_error If numerical evaluation fails.
 * @note Civil admission follows resolve_civil_times; chart additionally intersects every target's rate domain.
 *       Location affects houses only; house nutation follows house::DEFAULT_NUTATION_MODEL.
 */
[[nodiscard]] inline auto calculate(
  const calendar::Datetime& civil_dt,
  const astro::julian_day::Scale scale,
  const astro::GeoLocation& location,
  const astro::house::System system,
  const astro::delta_t::Model model = astro::delta_t::DEFAULT_MODEL
) -> Snapshot {
  const auto times = resolve_civil_times(civil_dt, scale, model);
  if (times.jde_tt < detail::START_JDE_TT or times.jde_tt >= detail::END_JDE_TT) {
    throw std::invalid_argument {
      std::format("Chart JDE(TT) {} leaves [{}, {})", times.jde_tt, detail::START_JDE_TT, detail::END_JDE_TT)
    };
  }

  const auto houses = astro::house::calculate(times.jd_ut1, times.jde_tt, location, system);
  const auto bodies = detail::evaluate_bodies(times.jde_tt);
  return { .times = times, .bodies = bodies, .houses = houses };
}

} // namespace astro::chart
