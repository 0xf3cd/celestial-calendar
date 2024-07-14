/*
 * CelestialCalendar: 
 *   A C++23-style library for date conversions and astronomical calculations for various calendars,
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

#include <vector>
#include <ranges>
#include <numeric>

#include <functional>
#include <type_traits>

namespace astro::vsop87d {

#pragma region Type Defs

/** @struct Representing 3 coefficients in a VSOP87D term. */
struct Coefficients {
  double A, B, C;
};

/** @brief The VSOP87D table of coefficients. */
using Vsop87dTable = std::vector<Coefficients>;

/** @brief The VSOP87D tables. */
using Vsop87dTables = std::vector<Vsop87dTable>;



#pragma region VSOP87D Evaluation

/**
 * @brief The scaling factor used for the VSOP87D tables in this project.
 * @note In this project, the coefficient As in the tables are multiplied by 1e8.
 *       So before getting the correct radians, we need to divide them by 1e8. 
 */
constexpr double SCALING_FACTOR = 1e8;


/** 
 * @brief Return the sum of all the terms in the given VSOP87D table, for the given julian millennium. 
 * @param vsop_table The VSOP87D table.
 * @param jm The julian millennium.
 * @return The sum of the terms in the table.
 * @example `evaluate_single(astro::vsop87d::earth::L0, 0.0)` means apply the Earth's L0 table on the given julian millennium 0.0.
 */
double evaluate_single(const Vsop87dTable& vsop_table, const double jm) {
  const auto calc_term = [jm](const auto& term) constexpr -> double {
    return term.A * std::cos(term.B + term.C * jm);
  };

  const auto evaluated = vsop_table | std::views::transform(calc_term);
  const auto terms_sum = std::reduce(begin(evaluated), end(evaluated));

  return terms_sum / SCALING_FACTOR;
}


/**
 * @brief Evaluate the given VSOP87D tables on the given julian millennium.
 * @param vsop_tables The VSOP87D tables.
 * @param jm The julian millennium.
 * @return The evaluated result. As per the VSOP87D model, the result is in radians.
 * @example `evaluate(astro::vsop87d::earth::L, 0.0)` means apply all Earth's L tables on the given julian millennium 0.0.
 */
double evaluate(const Vsop87dTables& vsop_tables, const double jm) {
  // Evaluate the result for each table in `vsop_tables`.
  const auto values = vsop_tables | std::views::transform([jm](const Vsop87dTable& vsop_table) {
    return evaluate_single(vsop_table, jm);
  });

  // Create the reversed view of `table_results`.
  // In order to get the correct result, the values need to be reversed.
  const auto reversed = std::views::reverse(values);

  // Evaluate the final result.
  const auto accumulated = std::accumulate(begin(reversed), end(reversed), 0.0, [jm](double a, double b) {
    return a * jm + b;
  });

  return accumulated;
}

} // namespace astro::vsop87d
