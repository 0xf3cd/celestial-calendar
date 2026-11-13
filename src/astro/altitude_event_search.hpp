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

/** @brief One immutable provider observation; all fields except TT are dimensionless. */
struct Observation {
  double jde_tt;
  double up;
  double q;
  double forward;
  double meridian;
  double product;
  double residual;
};

/** @brief A fixed-observer objective restricted to a separately certified closed TT envelope. */
struct Altitude {
  astro::ephemeris::Target target;
  astro::observer::Observer observer;
  double level_sine;
  double residual_error;
  search::Interval envelope;
  bool meridian;

  [[nodiscard]] auto observe(const double jde_tt) const -> Observation {
    if (jde_tt < envelope.start_jde_tt or jde_tt > envelope.end_jde_tt) {
      throw std::runtime_error { "Altitude sample leaves its certified envelope" };
    }
    const auto position = astro::ephemeris::equatorial_position(target, jde_tt);
    const auto tt = astro::julian_day::jde_to_tt(jde_tt);
    const auto ut1 = astro::delta_t::tt_to_ut1(astro::delta_t::Model::ALGO5, tt);
    const double jd_ut1 = astro::julian_day::ut1_to_jd(ut1);
    const auto theta = astro::sidereal::local_apparent(
      jd_ut1,
      jde_tt,
      -observer.location.longitude,
      astro::earth::nutation::Model::IAU_1980
    );
    const auto geometry = astro::observer::detail::local_geometry(position, theta, observer);
    // The residual contract has two binary64 roundings, including a separately stored product.
    const volatile double product = level_sine * geometry.q;
    return {
      .jde_tt = jde_tt, .up = geometry.up, .q = geometry.q, .forward = geometry.B, .meridian = geometry.A,
      .product = product, .residual = geometry.up - product,
    };
  }

  [[nodiscard]] auto operator()(const double jde_tt) const -> search::Value {
    const auto sample = observe(jde_tt);
    return { .value = meridian ? sample.meridian : sample.residual, .error = residual_error };
  }
};

/** @brief Tighten robust brackets until the slope bound localizes uncertain values at the requested resolution. */
[[nodiscard]] inline auto prepare_refinement(
  const Altitude& altitude,
  search::Candidate candidate,
  const search::Bounds bounds,
  search::Budget& budget,
  const double resolution
) -> search::Candidate {
  const double required_slope = search::upper(search::upper(2.0 * altitude.residual_error) / resolution);
  for (uint32_t iteration = 0; iteration < 96; ++iteration) {
    if (search::derivative_floor(candidate.left, candidate.right, bounds) >= required_slope) {
      break;
    }
    const double middle = std::midpoint(candidate.left.jde_tt, candidate.right.jde_tt);
    if (middle == candidate.left.jde_tt or middle == candidate.right.jde_tt) {
      break;
    }
    const auto observed = search::sample(altitude, middle, budget);
    const int32_t sign = search::sign(observed.evaluated);
    if (sign == 0) {
      const double previous_left = candidate.left.jde_tt;
      const double previous_right = candidate.right.jde_tt;
      for (const double at : { std::midpoint(previous_left, middle), std::midpoint(middle, previous_right) }) {
        const bool inside_bracket = candidate.left.jde_tt < at and at < candidate.right.jde_tt;
        if (not inside_bracket) {
          continue;
        }
        const auto quarter = search::sample(altitude, at, budget);
        const int32_t quarter_sign = search::sign(quarter.evaluated);
        if (quarter_sign == search::sign(candidate.left.evaluated)) {
          candidate.left = quarter;
        } else if (quarter_sign == search::sign(candidate.right.evaluated)) {
          candidate.right = quarter;
        }
      }
      if (candidate.left.jde_tt == previous_left and candidate.right.jde_tt == previous_right) {
        break;
      }
      continue;
    }
    if (sign == search::sign(candidate.left.evaluated)) {
      candidate.left = observed;
    } else {
      candidate.right = observed;
    }
  }
  return candidate;
}

/** @brief Return the complete scalar census or fail if any retained region lacks a certificate. */
[[nodiscard]] inline auto events(
  const Altitude& altitude,
  const search::Interval cell,
  const search::Bounds bounds,
  search::Budget& budget,
  const double resolution
) -> std::vector<search::Root> {
  const auto candidates = search::isolate_cell(
    altitude,
    cell.start_jde_tt,
    cell.end_jde_tt,
    bounds,
    budget,
    search::EndpointOwnership::LEFT,
    18
  );
  for (const auto& candidate : candidates) {
    if (candidate.isolation != search::Isolation::UNIQUE) {
      throw std::runtime_error { "Altitude census contains an unresolved region" };
    }
  }

  std::vector<search::Root> result;
  for (const auto& candidate : candidates) {
    const auto prepared = prepare_refinement(altitude, candidate, bounds, budget, resolution);
    result.push_back(search::refine(altitude, prepared, bounds, budget, resolution));
  }
  return result;
}

/** @brief Exclude zero over the complete clock collar, including its exact-reference jump allowance. */
[[nodiscard]] inline auto certify_clock_collar(
  const search::Sample& left,
  const search::Sample& right,
  const double first_derivative,
  const double jump_upper
) -> int32_t {
  const bool finite_samples = std::isfinite(left.jde_tt) and std::isfinite(right.jde_tt)
    and std::isfinite(left.evaluated.value) and std::isfinite(right.evaluated.value);
  const bool valid_bounds = search::finite_nonnegative(left.evaluated.error) and search::finite_nonnegative(right.evaluated.error)
    and search::finite_nonnegative(first_derivative) and search::finite_nonnegative(jump_upper);
  if (not finite_samples or not valid_bounds or left.jde_tt >= right.jde_tt) {
    throw std::runtime_error { "Invalid complete clock-collar source certificate" };
  }
  const double width = search::upper(right.jde_tt - left.jde_tt);
  const double smooth_drift = search::upper(first_derivative * width);
  const double complete_drift = search::upper(smooth_drift + jump_upper);
  const double observed_difference = search::lower(std::fabs(right.evaluated.value - left.evaluated.value));
  const double comparison_allowance = search::upper(search::upper(complete_drift + left.evaluated.error) + right.evaluated.error);
  if (observed_difference > comparison_allowance) {
    throw std::runtime_error { "Clock-collar observations contradict their complete source certificate" };
  }
  const int32_t left_sign = search::sign(left.evaluated);
  const int32_t right_sign = search::sign(right.evaluated);
  const double margin = std::max(search::magnitude_floor(left.evaluated), search::magnitude_floor(right.evaluated));
  if (left_sign != 0 and left_sign == right_sign and margin > complete_drift) {
    return left_sign;
  }
  throw std::runtime_error { "The complete clock-collar event census is unresolved" };
}

/** @brief Resolved geometric degrees, absolute level allowance and inverse evaluation charge. */
struct Level {
  double geometric_degrees;
  double error_degrees;
  std::size_t evaluation_charge;
};

/** @brief Resolve an attainable apparent level; uncertain image endpoints fail numerically. */
[[nodiscard]] inline auto inverse(
  const double apparent,
  const refraction::Params& params,
  const double map_error,
  const double residual_error,
  const double slope_lower,
  const double resolution
) -> Level {
  refraction::detail::validate_params(params);
  const bool weather = params.temperature_c >= -90.0 and params.temperature_c <= 60.0
                       and params.pressure_hpa >= 300.0 and params.pressure_hpa <= 1100.0;
  const bool model = params.model == refraction::Model::BENNETT or params.model == refraction::Model::SAEMUNDSSON;
  if (not std::isfinite(apparent) or not model) {
    throw std::invalid_argument { "Invalid apparent level, model or supported weather" };
  }
  if (not weather) {
    throw std::runtime_error { "The weather leaves the proved inverse certificate tier" };
  }
  if (not search::finite_nonnegative(map_error) or not search::finite_nonnegative(residual_error)
      or not std::isfinite(slope_lower) or slope_lower <= 0.0 or not std::isfinite(resolution) or resolution <= 0.0) {
    throw std::runtime_error { "The inverse has no valid numerical certificate" };
  }
  if (params.model == refraction::Model::BENNETT) {
    if (apparent < 0.0 or apparent > 90.0) {
      throw std::invalid_argument { "Apparent level is outside the Bennett image" };
    }
    const double geometric = apparent - refraction::detail::apply_tp_correction(
      refraction::bennett(astro::toolbox::AngleDeg { apparent }), params
    ).deg();
    if (search::lower(geometric - map_error) > 90.0) {
      throw std::invalid_argument { "Apparent level exceeds the supported geometric upper endpoint" };
    }
    if (search::upper(geometric + map_error) >= 90.0) {
      throw std::runtime_error { "The geometric upper endpoint is unresolved" };
    }
    if (map_error > resolution) {
      throw std::runtime_error { "Refraction value uncertainty exceeds the geometric-level resolution" };
    }
    return { .geometric_degrees = geometric, .error_degrees = map_error, .evaluation_charge = 1 };
  }

  double left = -refraction::at_horizon(params).deg();
  constexpr std::size_t HORIZON_CHARGE = refraction::SAEMUNDSSON_HORIZON_MAX_ITER + 1;
  double right = 90.0;
  if (not std::isfinite(left) or left < -1.5 or left >= 0.0) {
    throw std::runtime_error { "The actual apparent horizon leaves the proved source interval" };
  }
  const auto map = [&](const double value) {
    return value + refraction::detail::apply_tp_correction(
      refraction::saemundsson(astro::toolbox::AngleDeg { value }), params
    ).deg();
  };
  const double left_value = map(left);
  const double right_value = map(right);
  if (apparent < search::lower(left_value - map_error) or apparent > search::upper(right_value + map_error)) {
    throw std::invalid_argument { "Apparent level is outside the supported Saemundsson image" };
  }
  if (apparent <= search::upper(left_value + map_error) or apparent >= search::lower(right_value - map_error)) {
    throw std::runtime_error { "The attainable apparent image endpoint is unresolved" };
  }
  for (std::size_t iteration = 0; iteration < 64; ++iteration) {
    const double middle = std::midpoint(left, right);
    const double radius = std::max(search::upper(middle - left), search::upper(right - middle));
    if (radius <= resolution) {
      return { .geometric_degrees = middle, .error_degrees = radius, .evaluation_charge = HORIZON_CHARGE + iteration + 2 };
    }
    const double residual = map(middle) - apparent;
    if (std::fabs(residual) <= residual_error) {
      const double residual_radius = search::upper(search::upper(std::fabs(residual) + residual_error) / slope_lower);
      if (residual_radius <= resolution) {
        return { .geometric_degrees = middle, .error_degrees = residual_radius, .evaluation_charge = HORIZON_CHARGE + iteration + 3 };
      }
      throw std::runtime_error { "Refraction value uncertainty exceeds the geometric-level resolution" };
    }
    if (residual < 0.0) {
      left = middle;
    } else {
      right = middle;
    }
  }
  throw std::runtime_error { "Refraction inverse work budget exhausted" };
}

} // namespace astro::observer::detail::altitude_events
