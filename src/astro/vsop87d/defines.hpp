/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
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

#include <span>
#include <cmath>
#include <ranges>
#include <numeric>

namespace astro::vsop87d {

#pragma region Type Defs

/** @struct Representing 3 coefficients in a VSOP87D term. */
struct Coefficients {
  double A, B, C;
};

/** @brief The VSOP87D table of coefficients. */
using Vsop87dTable = std::span<const Coefficients>;

/** @brief The VSOP87D tables. */
using Vsop87dTables = std::span<const Vsop87dTable>;

#pragma endregion


#pragma region VSOP87D Evaluation

/**
 * @brief The scaling factor used for the VSOP87D tables in this project.
 * @note In this project, the coefficient "A"s in the tables are multiplied by 1e8.
 *       So before getting the correct radians, we need to divide them by 1e8. 
 */
constexpr double SCALING_FACTOR = 1e8;


/** 
 * @brief Return the sum of all the terms in the given VSOP87D table, for the given julian millennium. 
 * @param vsop_table The VSOP87D table.
 * @param jm The julian millennium.
 * @return The sum of the terms in the table.
 * @example `evaluate_table(astro::vsop87d::earth::L0, 0.0)` means apply the Earth's L0 table on the given julian millennium 0.0.
 */
inline auto evaluate_table(const Vsop87dTable& vsop_table, const double jm) -> double {
  const auto calc_term = [jm](const auto& term) constexpr -> double {
    return term.A * std::cos(term.B + term.C * jm);
  };

  const auto evaluated = vsop_table | std::views::transform(calc_term);
  const auto terms_sum = std::reduce(cbegin(evaluated), cend(evaluated));

  return terms_sum / SCALING_FACTOR;
}


/**
 * @brief Evaluate the given VSOP87D tables on the given julian millennium.
 * @param vsop_tables The VSOP87D tables.
 * @param jm The julian millennium.
 * @return The evaluated result. As per the VSOP87D model, the result is in radians.
 * @example `evaluate_tables(astro::vsop87d::earth::L, 0.0)` means apply all Earth's L tables on the given julian millennium 0.0.
 */
inline auto evaluate_tables(const Vsop87dTables& vsop_tables, const double jm) -> double {
  // Evaluate the result for each table in `vsop_tables`.
  const auto values = vsop_tables | std::views::transform([jm](const Vsop87dTable& vsop_table) {
    return evaluate_table(vsop_table, jm);
  });

  // Create the reversed view of `table_results`.
  // In order to get the correct result, the values need to be reversed.
  const auto reversed = std::views::reverse(values);

  // Evaluate the final result.
  const auto accumulated = std::accumulate(cbegin(reversed), cend(reversed), 0.0, [jm](double a, double b) {
    return a * jm + b;
  });

  return accumulated;
}

/** @enum The planets supported by VSOP87D. */
enum class Planet : uint8_t { EAR, /* SAT, MAR, ... */ };

/** @struct The type trait for the VSOP87D tables. Expected specializations in `*_coeff.hpp`s. */
template <Planet planet>
struct PlannetTables;

/**
 * @struct The result of the VSOP87D evaluation.
 * @note This struct is expected to only hold the untouched results from the VSOP87D model.
 */
struct Evaluation {
  double λ; // In radians
  double β; // In radians
  double r; // In AU
};

/**
 * @brief Evaluate the VSOP87D tables on the given julian millennium.
 * @tparam planet The planet to evaluate.
 * @param jm The julian millennium since J2000, calculated based on JDE (julian ephemeris date).
 * @return The evaluation result. VSOP87D provides the heliocentric ecliptic spherical coordinates for the equinox of the day.
 * @example `evaluate<Planet::EAR>(0.0)` means evaluating the Earth's L, B, and R tables on the given julian millennium 0.0.
 */
template <Planet planet>
inline auto evaluate(const double jm) -> Evaluation {
  const auto& L = PlannetTables<planet>::L;
  const auto& B = PlannetTables<planet>::B;
  const auto& R = PlannetTables<planet>::R;

  return {
    .λ = evaluate_tables(L, jm), 
    .β = evaluate_tables(B, jm), 
    .r = evaluate_tables(R, jm),
  };
}

#pragma endregion

} // namespace astro::vsop87d
