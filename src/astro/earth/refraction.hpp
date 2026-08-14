/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
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

#pragma endregion


#pragma region Types

/** @brief Atmospheric refraction model selector. */
enum class Model : uint8_t { BENNETT, SAEMUNDSSON };

/**
 * @brief Atmospheric conditions for refraction calculations.
 * @note Defaults (15°C / 1013.25 hPa / Bennett) reproduce the historical −34′ horizon refraction
 *       used by `sunrise_sunset::STANDARD_ALTITUDE`.
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
  const astro::toolbox::AngleDeg& r_native,
  const Params& params
) -> astro::toolbox::AngleDeg {
  const double factor = (params.pressure_hpa / NATIVE_PRESSURE_HPA)
                      * (NATIVE_TEMPERATURE_K / (273.0 + params.temperature_c));
  return r_native * factor;
}

/**
 * @brief Validate refraction parameters are physically admissible and finite.
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
 * @return The refraction angle, positive in degrees.
 * @note Meeus (16.3) gives the result in arcminutes for 10°C/1010 hPa. The returned value is the
 *       native (10°C/1010 hPa) refraction; pass it through `at_horizon(Params)` to apply a T/P
 *       correction. The formula is valid for apparent altitudes in [0°, 90°] and becomes
 *       numerically unstable below about −2°.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 16, Formula (16.3).
 */
[[nodiscard]] inline auto bennett(const astro::toolbox::AngleDeg& apparent_alt) -> astro::toolbox::AngleDeg {
  using astro::toolbox::AngleDeg;

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
 * @return The refraction angle, positive in degrees.
 * @note Meeus (16.4) gives the result in arcminutes for 10°C/1010 hPa. The returned value is the
 *       native (10°C/1010 hPa) refraction; pass it through `at_horizon(Params)` to apply a T/P
 *       correction. The input is the geometric altitude (before refraction), opposite to Bennett.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 16, Formula (16.4).
 */
[[nodiscard]] inline auto saemundsson(const astro::toolbox::AngleDeg& true_alt) -> astro::toolbox::AngleDeg {
  using astro::toolbox::AngleDeg;

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
[[nodiscard]] inline auto at_horizon(const Params& params = {}) -> astro::toolbox::AngleDeg {
  using astro::toolbox::AngleDeg;

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

} // namespace astro::earth::refraction
