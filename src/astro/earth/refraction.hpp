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
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <optional>
#include <stdexcept>

#include "toolbox.hpp"


namespace astro::earth::refraction {

#pragma region Constants

/**
 * @brief Native temperature for the refraction models, in degrees Celsius.
 * @note Both Bennett (Meeus 16.3) and Saemundsson (Meeus 16.4) are tabulated for 10°C/1010 hPa.
 */
inline constexpr double NATIVE_TEMPERATURE_C = 10.0;

/**
 * @brief Native pressure for the refraction models, in hectopascals.
 * @note Both Bennett (Meeus 16.3) and Saemundsson (Meeus 16.4) are tabulated for 10°C/1010 hPa.
 */
inline constexpr double NATIVE_PRESSURE_HPA = 1010.0;

/**
 * @brief The 283 K numerator in the T/P correction factor, derived from 10°C = 273 + 10 K.
 * @note The correction scales refraction by `(P/1010) * (283/(273+T_c))`.
 */
inline constexpr double NATIVE_TEMPERATURE_K = 273.0 + NATIVE_TEMPERATURE_C;

/** @brief Iteration tolerance for Saemundsson's horizon refraction, in degrees. */
inline constexpr double SAEMUNDSSON_HORIZON_TOL_DEG = 1e-9;

/** @brief Maximum number of iterations for Saemundsson's horizon solve. */
inline constexpr std::size_t SAEMUNDSSON_HORIZON_MAX_ITER = 20;

/** @brief Forward residual required by the geometric-to-apparent Bennett inverse, in degrees. */
inline constexpr double BENNETT_INVERSE_TOL_DEG = 1e-12;

/** @brief Bisection cap; 90 / 2^47 degrees is already below the required forward residual. */
inline constexpr std::size_t BENNETT_INVERSE_MAX_ITER = 64;

#pragma endregion


#pragma region Types

/** @brief Atmospheric refraction model selector. */
enum class Model : uint8_t { BENNETT, SAEMUNDSSON };

/**
 * @brief Atmospheric conditions for refraction calculations.
 * @note Defaults (15°C / 1013.25 hPa / Bennett) reproduce the historical −34′ horizon refraction
 *       used by `rise_set::sun::STANDARD_ALTITUDE` to within 0.02′.
 */
struct Params {
  double temperature_c = 15.0;   // °C
  double pressure_hpa  = 1013.25; // hPa
  Model model = Model::BENNETT;
};

#pragma endregion


namespace detail {

/**
 * @brief Apply the standard temperature/pressure correction to a native refraction value.
 * @param r_native The refraction at 10°C/1010 hPa, in degrees.
 * @param params   The actual atmospheric conditions.
 * @return The corrected refraction, in degrees.
 * @note Meeus scales refraction by `(P/1010) * (283/(273+T_c))`.
 */
[[nodiscard]] inline auto apply_tp_correction(
  const toolbox::AngleDeg& r_native,
  const Params& params
) -> toolbox::AngleDeg {
  const double factor = (params.pressure_hpa / NATIVE_PRESSURE_HPA)
                      * (NATIVE_TEMPERATURE_K / (273.0 + params.temperature_c));
  return r_native * factor;
}

/**
 * @brief Validate refraction parameters are finite and above the numerical pole at −273°C.
 * @throw std::invalid_argument If temperature is not finite or ≤ −273°C, or pressure is not finite
 *        or non-positive.
 */
inline auto validate_params(const Params& params) -> void {
  if (not std::isfinite(params.temperature_c) or not std::isfinite(params.pressure_hpa)) {
    throw std::invalid_argument {
      std::format("Refraction params must be finite, got T={}°C, P={} hPa",
                  params.temperature_c, params.pressure_hpa)
    };
  }
  if (params.temperature_c <= -273.0) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Refraction temperature must be above -273°C, got {}°C", params.temperature_c)
    };
  }
  if (params.pressure_hpa <= 0.0) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Refraction pressure must be positive, got {} hPa", params.pressure_hpa)
    };
  }
}

} // namespace detail


/**
 * @brief Bennett's formula for atmospheric refraction from apparent altitude.
 * @param apparent_alt The apparent altitude of the body (what an observer sees), in degrees.
 * @return The refraction angle in degrees (the formula yields a negligible negative value at
 *         exactly 90°).
 * @note Meeus (16.3) gives the result in arcminutes for 10°C/1010 hPa. The returned value is the
 *       native (10°C/1010 hPa) refraction; `at_horizon(Params)` computes the T/P-corrected
 *       horizon refraction. The formula is valid for apparent altitudes in [0°, 90°] and becomes
 *       numerically unstable below about −2°.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 16, Formula (16.3).
 */
[[nodiscard]] inline auto bennett(const toolbox::AngleDeg& apparent_alt) -> toolbox::AngleDeg {
  using toolbox::AngleDeg;

  const double h_deg = apparent_alt.deg();
  const double denominator = h_deg + 4.4;

  // Meeus (16.3): R = 1 / tan(h + 7.31/(h + 4.4)), with R in arcminutes.
  const double inner = h_deg + (7.31 / denominator);
  const double r_arcmin = 1.0 / std::tan(AngleDeg { inner }.rad());

  return AngleDeg::from_arcmin(r_arcmin);
}


/**
 * @brief Saemundsson's formula for atmospheric refraction from true (geometric) altitude.
 * @param true_alt The true geometric altitude of the body, in degrees.
 * @return The refraction angle in degrees (the formula yields a negligible negative value at
 *         exactly 90°).
 * @note Meeus (16.4) gives the result in arcminutes for 10°C/1010 hPa. The returned value is the
 *       native (10°C/1010 hPa) refraction; `at_horizon(Params)` computes the T/P-corrected
 *       horizon refraction. The input is the geometric altitude (before refraction), opposite to
 *       Bennett. The formula is valid for true altitudes in [0°, 90°] and becomes numerically
 *       unstable below about −2°.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 16, Formula (16.4).
 */
[[nodiscard]] inline auto saemundsson(const toolbox::AngleDeg& true_alt) -> toolbox::AngleDeg {
  using toolbox::AngleDeg;

  const double h_deg = true_alt.deg();
  const double denominator = h_deg + 5.11;

  // Meeus (16.4): R = 1.02 / tan(h + 10.3/(h + 5.11)), with R in arcminutes.
  const double inner = h_deg + (10.3 / denominator);
  const double r_arcmin = 1.02 / std::tan(AngleDeg { inner }.rad());

  return AngleDeg::from_arcmin(r_arcmin);
}


/**
 * @brief Compute the refraction at the apparent horizon (apparent altitude = 0°).
 * @param params The atmospheric conditions. Defaults to 15°C/1013.25 hPa/Bennett.
 * @return The horizon refraction, positive in degrees.
 * @note For Bennett the apparent altitude is already 0°, so the formula can be evaluated directly.
 *       For Saemundsson the input is the true altitude, which at the apparent horizon equals
 *       −R; this function iterates `R = apply_tp_correction(saemundsson(−R), params)` to
 *       convergence.
 */
[[nodiscard]] inline auto at_horizon(const Params& params = {}) -> toolbox::AngleDeg {
  using toolbox::AngleDeg;

  detail::validate_params(params);

  if (params.model == Model::BENNETT) {
    const auto r_native = bennett(AngleDeg { 0.0 });
    return detail::apply_tp_correction(r_native, params);
  }

  // Saemundsson: solve R = apply_tp_correction(saemundsson(−R), params), where R is the
  // refraction at apparent altitude 0°. Start from Bennett's value for the same conditions — it
  // is the right order of magnitude.
  auto r = bennett(AngleDeg { 0.0 });
  r = detail::apply_tp_correction(r, params);

  double delta_deg = 0.0;
  for (std::size_t i = 0; i < SAEMUNDSSON_HORIZON_MAX_ITER; ++i) {
    const auto r_next = saemundsson(-r);
    const auto r_next_corrected = detail::apply_tp_correction(r_next, params);

    delta_deg = std::fabs((r_next_corrected - r).deg());
    if (delta_deg < SAEMUNDSSON_HORIZON_TOL_DEG) {
      return r_next_corrected;
    }
    r = r_next_corrected;
  }

  throw std::runtime_error {
    std::format("refraction::at_horizon: Saemundsson iteration did not converge in {} iterations, "
                "last |Δ| = {} deg", SAEMUNDSSON_HORIZON_MAX_ITER, delta_deg)
  };
}

/**
 * @brief Apply refraction to a geometric centre altitude, or return none below the apparent horizon.
 * @param geometric_alt Finite geometric altitude in degrees; unsupported altitudes return none.
 * @param params Model and weather: temperature in [-90, 60]°C and pressure in [300, 1100] hPa.
 * @return The computed apparent altitude, without clamping, when supported.
 * @note The weather interval is an implementation support window. The geometric lower bound is
 *       the selected model's apparent horizon; the upper bound is 90°. Saemundsson's inclusive
 *       lower endpoint can return a signed residual down to -SAEMUNDSSON_HORIZON_TOL_DEG.
 * @throw std::invalid_argument For non-finite altitude, unknown model or unsupported weather.
 * @throw std::runtime_error If the inverse fails to converge or the result is inconsistent.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, formulas (16.3), (16.4).
 */
[[nodiscard]] inline auto apparent_altitude(
  const toolbox::AngleDeg geometric_alt,
  const Params& params = {}
) -> std::optional<toolbox::AngleDeg> {
  using toolbox::AngleDeg;

  detail::validate_params(params);
  const bool supported_weather = params.temperature_c >= -90.0 and params.temperature_c <= 60.0
    and params.pressure_hpa >= 300.0 and params.pressure_hpa <= 1100.0;
  if (not supported_weather) {
    throw std::invalid_argument { "Refraction application requires T in [-90, 60] C and P in [300, 1100] hPa" };
  }
  if (params.model != Model::BENNETT and params.model != Model::SAEMUNDSSON) {
    throw std::invalid_argument { "Unknown refraction model" };
  }
  const double g = geometric_alt.deg();
  if (not std::isfinite(g)) {
    throw std::invalid_argument { "Geometric altitude must be finite" };
  }

  if (params.model == Model::SAEMUNDSSON) {
    if (g < -at_horizon(params).deg() or g > 90.0) {
      return std::nullopt;
    }
    const double apparent = g + detail::apply_tp_correction(saemundsson(geometric_alt), params).deg();
    if (not std::isfinite(apparent) or apparent < -SAEMUNDSSON_HORIZON_TOL_DEG or apparent > 90.0) {
      throw std::runtime_error { "Saemundsson apparent altitude is outside its supported range" };
    }
    return AngleDeg { apparent };
  }

  const auto geometric_from_apparent = [&](const double apparent) {
    return apparent - detail::apply_tp_correction(bennett(AngleDeg { apparent }), params).deg();
  };
  if (g < geometric_from_apparent(0.0) or g > 90.0) {
    return std::nullopt;
  }

  double lower = 0.0;
  double upper = 90.0;
  for (std::size_t iteration = 0; iteration < BENNETT_INVERSE_MAX_ITER; ++iteration) {
    const double apparent = std::midpoint(lower, upper);
    const double residual = geometric_from_apparent(apparent) - g;
    if (std::abs(residual) <= BENNETT_INVERSE_TOL_DEG) {
      return AngleDeg { apparent };
    }
    if (residual < 0.0) {
      lower = apparent;
    } else {
      upper = apparent;
    }
  }
  throw std::runtime_error { "Bennett geometric-to-apparent inverse did not converge" };
}

} // namespace astro::earth::refraction
