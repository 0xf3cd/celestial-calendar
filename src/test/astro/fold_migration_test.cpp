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

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

#include "earth.hpp"
#include "elp2000_82b.hpp"
#include "julian_day.hpp"
#include "lunar/algo1.hpp"
#include "vsop87d/vsop87d.hpp"

namespace astro::test {
namespace {

inline constexpr std::size_t SAMPLE_COUNT = 4096;

inline constexpr auto REDUCE = [](auto&& range, auto init, auto op) {
  return std::reduce(cbegin(range), cend(range), init, op);
};

inline constexpr auto ACCUMULATE = [](auto&& range, auto init, auto op) {
  return std::accumulate(cbegin(range), cend(range), init, op);
};


struct DifferenceStats {
  std::size_t samples = 0;
  std::size_t different = 0;
  std::size_t finite_transitions = 0;
  std::size_t nan_transitions = 0;
  std::size_t signed_zero_transitions = 0;
  uint64_t max_ulp = 0;
  double max_abs = 0.0;

  static auto ordered_bits(const double value) -> uint64_t {
    const auto bits = std::bit_cast<uint64_t>(value);
    return (bits >> 63U) != 0U ? ~bits : bits | (uint64_t { 1 } << 63U);
  }

  void observe(const double old_value, const double new_value) {
    ++samples;
    if (std::bit_cast<uint64_t>(old_value) == std::bit_cast<uint64_t>(new_value)) {
      return;
    }

    ++different;
    finite_transitions += static_cast<std::size_t>(std::isfinite(old_value) != std::isfinite(new_value));
    nan_transitions += static_cast<std::size_t>(std::isnan(old_value) != std::isnan(new_value));
    signed_zero_transitions += static_cast<std::size_t>(
      old_value == 0.0 and new_value == 0.0 and std::signbit(old_value) != std::signbit(new_value)
    );

    if (std::isfinite(old_value) and std::isfinite(new_value)) {
      max_abs = std::max(max_abs, std::abs(old_value - new_value));
      const auto old_bits = ordered_bits(old_value);
      const auto new_bits = ordered_bits(new_value);
      max_ulp = std::max(max_ulp, old_bits > new_bits ? old_bits - new_bits : new_bits - old_bits);
    }
  }

  void print(const std::string& name) const {
    std::cout << "[fold-migration] " << name
              << " samples=" << samples
              << " different=" << different
              << " max_ulp=" << max_ulp
              << " max_abs=" << std::scientific << std::setprecision(17) << max_abs
              << " finite_transitions=" << finite_transitions
              << " nan_transitions=" << nan_transitions
              << " signed_zero_transitions=" << signed_zero_transitions
              << '\n';
  }
};


auto jc_at(const std::size_t index) -> double {
  return -20.0 + (40.0 * static_cast<double>(index) / static_cast<double>(SAMPLE_COUNT - 1));
}


void measure_elp2000() {
  DifferenceStats Σl;
  DifferenceStats Σb;
  DifferenceStats Σr;

  for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
    const double jc = jc_at(i);
    const auto old_value = elp2000_82b::detail::evaluate_with(jc, REDUCE);
    const auto new_value = elp2000_82b::detail::evaluate_with(jc, std::ranges::fold_left);
    Σl.observe(old_value.Σl, new_value.Σl);
    Σb.observe(old_value.Σb, new_value.Σb);
    Σr.observe(old_value.Σr, new_value.Σr);
  }

  Σl.print("elp2000.SigmaL");
  Σb.print("elp2000.SigmaB");
  Σr.print("elp2000.SigmaR");
}


void measure_nutation_model(const earth::nutation::Model model, const std::string& name) {
  DifferenceStats Δψ_sum;
  DifferenceStats Δψ_deg;
  DifferenceStats Δε_sum;
  DifferenceStats Δε_deg;

  for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
    const double jde = julian_day::J2000 + (jc_at(i) * 36525.0);
    const double old_Δψ = earth::nutation::detail::longitude_sum(jde, model, REDUCE);
    const double new_Δψ = earth::nutation::detail::longitude_sum(jde, model, std::ranges::fold_left);
    const double old_Δε = earth::nutation::detail::obliquity_sum(jde, model, REDUCE);
    const double new_Δε = earth::nutation::detail::obliquity_sum(jde, model, std::ranges::fold_left);

    Δψ_sum.observe(old_Δψ, new_Δψ);
    Δε_sum.observe(old_Δε, new_Δε);
    Δψ_deg.observe(
      toolbox::AngleDeg::from_arcsec(old_Δψ * 0.0001).deg(),
      toolbox::AngleDeg::from_arcsec(new_Δψ * 0.0001).deg()
    );
    Δε_deg.observe(
      toolbox::AngleDeg::from_arcsec(old_Δε * 0.0001).deg(),
      toolbox::AngleDeg::from_arcsec(new_Δε * 0.0001).deg()
    );
  }

  Δψ_sum.print("nutation." + name + ".DeltaPsi.raw");
  Δψ_deg.print("nutation." + name + ".DeltaPsi.deg");
  Δε_sum.print("nutation." + name + ".DeltaEpsilon.raw");
  Δε_deg.print("nutation." + name + ".DeltaEpsilon.deg");
}


void measure_daily_variation() {
  DifferenceStats raw;
  DifferenceStats returned;

  for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
    const double jde = julian_day::J2000 + (jc_at(i) * 36525.0);
    const double old_sum = earth::aberration::detail::daily_λ_sum(jde, REDUCE);
    const double new_sum = earth::aberration::detail::daily_λ_sum(jde, std::ranges::fold_left);
    raw.observe(old_sum, new_sum);
    returned.observe(3548.330 + old_sum, 3548.330 + new_sum);
  }

  raw.print("aberration.daily_lambda.raw");
  returned.print("aberration.daily_lambda.returned");
}


void measure_vsop_tables(const std::string& name, const vsop87d::Vsop87dTables& tables) {
  for (std::size_t table_index = 0; table_index < tables.size(); ++table_index) {
    DifferenceStats table_stats;
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
      const double jm = jc_at(i) / 10.0;
      const double old_value = vsop87d::detail::evaluate_table_with(tables[table_index], jm, REDUCE);
      const double new_value = vsop87d::detail::evaluate_table_with(
        tables[table_index],
        jm,
        std::ranges::fold_left
      );
      table_stats.observe(old_value, new_value);
    }
    table_stats.print("vsop." + name + std::to_string(table_index) + ".table");
  }

  DifferenceStats final_value;
  DifferenceStats horner;
  for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
    const double jm = jc_at(i) / 10.0;
    const double old_value = vsop87d::detail::evaluate_tables_with(tables, jm, REDUCE, ACCUMULATE);
    const double new_value = vsop87d::detail::evaluate_tables_with(
      tables,
      jm,
      std::ranges::fold_left,
      ACCUMULATE
    );
    final_value.observe(old_value, new_value);

    std::vector<double> values;
    values.reserve(tables.size());
    for (const auto& table : tables) {
      values.push_back(vsop87d::evaluate_table(table, jm));
    }
    const double old_horner = vsop87d::detail::evaluate_horner_with(values, jm, ACCUMULATE);
    const double new_horner = vsop87d::detail::evaluate_horner_with(values, jm, std::ranges::fold_left);
    horner.observe(old_horner, new_horner);
  }

  final_value.print("vsop." + name + ".final");
  horner.print("vsop." + name + ".horner");
  EXPECT_EQ(horner.different, 0U);
}


void measure_lunar_integer_sums() {
  std::size_t year_comparisons = 0;
  std::size_t year_differences = 0;
  std::size_t prefix_comparisons = 0;
  std::size_t prefix_differences = 0;

  for (int32_t year = calendar::lunar::algo1::START_YEAR;
       year <= calendar::lunar::algo1::END_YEAR;
       ++year) {
    const auto info = calendar::lunar::algo1::calc_lunar_year(year);
    const auto& months = info.month_lengths;
    const uint32_t old_year = std::reduce(cbegin(months), cend(months), uint32_t { 0 });
    const uint32_t new_year = std::ranges::fold_left(months, uint32_t { 0 }, std::plus {});
    ++year_comparisons;
    year_differences += static_cast<std::size_t>(old_year != new_year);

    for (std::size_t month = 1; month <= months.size(); ++month) {
      const auto end = cbegin(months) + static_cast<std::ptrdiff_t>(month - 1);
      const uint32_t old_prefix = std::reduce(cbegin(months), end, uint32_t { 0 });
      const uint32_t new_prefix = std::ranges::fold_left(
        cbegin(months),
        end,
        uint32_t { 0 },
        std::plus {}
      );
      ++prefix_comparisons;
      prefix_differences += static_cast<std::size_t>(old_prefix != new_prefix);
    }
  }

  std::cout << "[fold-migration] lunar.year_sum samples=" << year_comparisons
            << " different=" << year_differences << '\n';
  std::cout << "[fold-migration] lunar.prefix_sum samples=" << prefix_comparisons
            << " different=" << prefix_differences << '\n';
  EXPECT_EQ(year_differences, 0U);
  EXPECT_EQ(prefix_differences, 0U);
}

} // namespace


TEST(FoldMigration, Report) {
  const std::array<double, 4> control { 1e16, 1.0, -1e16, 1.0 };
  const double strict_left = std::ranges::fold_left(control, 0.0, std::plus {});
  const double pairwise = (control[0] + control[1]) + (control[2] + control[3]);
  DifferenceStats control_stats;
  control_stats.observe(strict_left, pairwise);
  control_stats.print("control.strict_left_vs_pairwise");
  ASSERT_GT(control_stats.different, 0U);

  measure_elp2000();
  measure_nutation_model(earth::nutation::Model::MEEUS, "meeus");
  measure_nutation_model(earth::nutation::Model::IAU_1980, "iau1980");
  measure_daily_variation();
  measure_vsop_tables("L", vsop87d::earth_coeff::L);
  measure_vsop_tables("B", vsop87d::earth_coeff::B);
  measure_vsop_tables("R", vsop87d::earth_coeff::R);
  measure_lunar_integer_sums();
}

} // namespace astro::test
