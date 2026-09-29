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
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "earth/precession.hpp"
#include "julian_day.hpp"
#include "sun.hpp"
#include "toolbox.hpp"

namespace astro::planet::detail::pluto {

struct Term {
  double i;
  double j;
  double k;
  double lon_a;
  double lon_b;
  double lat_a;
  double lat_b;
  double radius_a;
  double radius_b;
};

inline constexpr double RAW_START_JDE_TT = 2409542.5;
inline constexpr double RAW_END_JDE_TT = 2488069.5;
inline constexpr double APPARENT_START_JDE_TT = 2409543.5;
inline constexpr double APPARENT_END_JDE_TT = 2488069.5;
inline constexpr double RETROGRADE_START_JDE_TT = 2409544.0;
inline constexpr double RETROGRADE_END_JDE_TT = 2488069.0;
inline constexpr int32_t EVENT_START_YEAR = 1886;
inline constexpr int32_t EVENT_END_YEAR = 2098;
inline constexpr std::size_t FK5_INVERSE_MAX_ITERATIONS = 8;
// Below binary64 angular resolution at Pluto's longitudes; convergence is exact fixed-point stability.
inline constexpr double FK5_INVERSE_ANGLE_TOLERANCE_DEG = 1e-15;

// Retained material boundary (R38/#298): this evaluator and its 43 Chapter 37 terms were ported
// from Sonia Keys' `v3/pluto/pluto.go` at bfbd9ac2c7f709c94f1dee190c633d24a723f628. They remain
// under Sonia Keys' MIT terms and outside the project MIT grant.
// Copyright 2013 Sonia Keys.
// @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 37, Table 37.A.
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense source table reads by column.
inline constexpr std::array<Term, 43> TERMS {{
  { 0, 0,  1, -19.799805, 19.850055, -5.452852, -14.974862,  6.6865439,  6.8951812 },
  { 0, 0,  2,   0.897144, -4.954829,  3.527812,   1.672790, -1.1827535, -0.0332538 },
  { 0, 0,  3,   0.611149,  1.211027, -1.050748,   0.327647,  0.1593179, -0.1438890 },
  { 0, 0,  4,  -0.341243, -0.189585,  0.178690,  -0.292153, -0.0018444,  0.0483220 },
  { 0, 0,  5,   0.129287, -0.034992,  0.018650,   0.100340, -0.0065977, -0.0085431 },
  { 0, 0,  6,  -0.038164,  0.030893, -0.030697,  -0.025823,  0.0031174, -0.0006032 },
  { 0, 1, -1,   0.020442, -0.009987,  0.004878,   0.011248, -0.0005794,  0.0022161 },
  { 0, 1,  0,  -0.004063, -0.005071,  0.000226,  -0.000064,  0.0004601,  0.0004032 },
  { 0, 1,  1,  -0.006016, -0.003336,  0.002030,  -0.000836, -0.0001729,  0.0000234 },
  { 0, 1,  2,  -0.003956,  0.003039,  0.000069,  -0.000604, -0.0000415,  0.0000702 },
  { 0, 1,  3,  -0.000667,  0.003572, -0.000247,  -0.000567,  0.0000239,  0.0000723 },
  { 0, 2, -2,   0.001276,  0.000501, -0.000057,   0.000001,  0.0000067, -0.0000067 },
  { 0, 2, -1,   0.001152, -0.000917, -0.000122,   0.000175,  0.0001034, -0.0000451 },
  { 0, 2,  0,   0.000630, -0.001277, -0.000049,  -0.000164, -0.0000129,  0.0000504 },
  { 1, -1, 0,   0.002571, -0.000459, -0.000197,   0.000199,  0.0000480, -0.0000231 },
  { 1, -1, 1,   0.000899, -0.001449, -0.000025,   0.000217,  0.0000002, -0.0000441 },
  { 1, 0, -3,  -0.001016,  0.001043,  0.000589,  -0.000248, -0.0003359,  0.0000265 },
  { 1, 0, -2,  -0.002343, -0.001012, -0.000269,   0.000711,  0.0007856, -0.0007832 },
  { 1, 0, -1,   0.007042,  0.000788,  0.000185,   0.000193,  0.0000036,  0.0045763 },
  { 1, 0,  0,   0.001199, -0.000338,  0.000315,   0.000807,  0.0008663,  0.0008547 },
  { 1, 0,  1,   0.000418, -0.000067, -0.000130,  -0.000043, -0.0000809, -0.0000769 },
  { 1, 0,  2,   0.000120, -0.000274,  0.000005,   0.000003,  0.0000263, -0.0000144 },
  { 1, 0,  3,  -0.000060, -0.000159,  0.000002,   0.000017, -0.0000126,  0.0000032 },
  { 1, 0,  4,  -0.000082, -0.000029,  0.000002,   0.000005, -0.0000035, -0.0000016 },
  { 1, 1, -3,  -0.000036, -0.000029,  0.000002,   0.000003, -0.0000019, -0.0000004 },
  { 1, 1, -2,  -0.000040,  0.000007,  0.000003,   0.000001, -0.0000015,  0.0000008 },
  { 1, 1, -1,  -0.000014,  0.000022,  0.000002,  -0.000001, -0.0000004,  0.0000012 },
  { 1, 1,  0,   0.000004,  0.000013,  0.000001,  -0.000001,  0.0000005,  0.0000006 },
  { 1, 1,  1,   0.000005,  0.000002,  0.000000,  -0.000001,  0.0000003,  0.0000001 },
  { 1, 1,  3,  -0.000001,  0.000000,  0.000000,   0.000000,  0.0000006, -0.0000002 },
  { 2, 0, -6,   0.000002,  0.000000,  0.000000,  -0.000002,  0.0000002,  0.0000002 },
  { 2, 0, -5,  -0.000004,  0.000005,  0.000002,   0.000002, -0.0000002, -0.0000002 },
  { 2, 0, -4,   0.000004, -0.000007, -0.000007,   0.000000,  0.0000014,  0.0000013 },
  { 2, 0, -3,   0.000014,  0.000024,  0.000010,  -0.000008, -0.0000063,  0.0000013 },
  { 2, 0, -2,  -0.000049, -0.000034, -0.000003,   0.000020,  0.0000136, -0.0000236 },
  { 2, 0, -1,   0.000163, -0.000048,  0.000006,   0.000005,  0.0000273,  0.0001065 },
  { 2, 0,  0,   0.000009, -0.000024,  0.000014,   0.000017,  0.0000251,  0.0000149 },
  { 2, 0,  1,  -0.000004,  0.000001, -0.000002,   0.000000, -0.0000025, -0.0000009 },
  { 2, 0,  2,  -0.000003,  0.000001,  0.000000,   0.000000,  0.0000009, -0.0000002 },
  { 2, 0,  3,   0.000001,  0.000003,  0.000000,   0.000000, -0.0000008,  0.0000007 },
  { 3, 0, -2,  -0.000003, -0.000001,  0.000000,   0.000001,  0.0000002, -0.0000010 },
  { 3, 0, -1,   0.000005, -0.000003,  0.000000,   0.000000,  0.0000019,  0.0000035 },
  { 3, 0,  0,   0.000000,  0.000000,  0.000001,   0.000000,  0.0000010,  0.0000003 },
}};
// NOLINTEND(modernize-use-designated-initializers)

inline auto validate_jde(
  const double jde_tt,
  const double start_jde_tt,
  const double end_jde_tt,
  const char* const domain
) -> void {
  if (not std::isfinite(jde_tt)) [[unlikely]] {
    throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
  }
  if (jde_tt < start_jde_tt or jde_tt >= end_jde_tt) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Pluto {} JDE(TT) {} is out of range [{}, {}).", domain, jde_tt, start_jde_tt, end_jde_tt)
    };
  }
}

inline auto validate_raw_jde(const double jde_tt) -> void {
  validate_jde(jde_tt, RAW_START_JDE_TT, RAW_END_JDE_TT, "Chapter 37 model");
}

inline auto validate_apparent_jde(const double jde_tt) -> void {
  validate_jde(jde_tt, APPARENT_START_JDE_TT, APPARENT_END_JDE_TT, "apparent-position");
}

inline auto validate_retrograde_jde(const double jde_tt) -> void {
  validate_jde(jde_tt, RETROGRADE_START_JDE_TT, RETROGRADE_END_JDE_TT, "centered-retrograde");
}

/** @brief Evaluate Pluto's geometric heliocentric J2000 ecliptic position. */
[[nodiscard]] inline auto heliocentric_j2000(const double jde_tt)
  -> astro::toolbox::SphericalCoordinate {
  validate_raw_jde(jde_tt);
  const double T = astro::julian_day::jde_to_jc(jde_tt);

  const astro::toolbox::AngleDeg J { 34.35 + (3034.9057 * T) }; // Jupiter mean longitude
  const astro::toolbox::AngleDeg S { 50.08 + (1222.1138 * T) }; // Saturn mean longitude
  const astro::toolbox::AngleDeg P { 238.96 + (144.96 * T) };   // Pluto mean longitude

  double longitude_correction_deg = 0.0;
  double latitude_correction_deg = 0.0;
  double radius_correction_au = 0.0;
  for (const auto& term : TERMS) {
    const double argument =
      (term.i * J.rad()) + (term.j * S.rad()) + (term.k * P.rad());
    const double sine = std::sin(argument);
    const double cosine = std::cos(argument);

    longitude_correction_deg += (term.lon_a * sine) + (term.lon_b * cosine);
    latitude_correction_deg += (term.lat_a * sine) + (term.lat_b * cosine);
    radius_correction_au += (term.radius_a * sine) + (term.radius_b * cosine);
  }

  return {
    .λ = astro::toolbox::AngleDeg {
      238.958116 + (144.96 * T) + longitude_correction_deg
    }.normalize(),
    .β = astro::toolbox::AngleDeg { -3.908239 + latitude_correction_deg },
    .r = astro::toolbox::DistanceAu { 40.7241346 + radius_correction_au },
  };
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, (32.3). */
[[nodiscard]] inline auto inverse_fk5(
  const astro::toolbox::SphericalCoordinate& fk5_coordinate,
  const double observation_jde_tt
) -> astro::toolbox::SphericalCoordinate {
  auto dynamic = fk5_coordinate;
  for (std::size_t iteration = 0; iteration < FK5_INVERSE_MAX_ITERATIONS; ++iteration) {
    const auto correction = astro::sun::geocentric_coord::fk5_correction(observation_jde_tt, dynamic);
    const astro::toolbox::SphericalCoordinate updated {
      .λ = (fk5_coordinate.λ - correction.Δλ).normalize(),
      .β = fk5_coordinate.β - correction.Δβ,
      .r = fk5_coordinate.r,
    };
    const double lon_change = std::fabs(std::remainder(updated.λ.deg() - dynamic.λ.deg(), 360.0));
    const double lat_change = std::fabs(updated.β.deg() - dynamic.β.deg());
    if (lon_change <= FK5_INVERSE_ANGLE_TOLERANCE_DEG
        and lat_change <= FK5_INVERSE_ANGLE_TOLERANCE_DEG) {
      return updated;
    }
    dynamic = updated;
  }
  throw std::runtime_error {
    std::format("Inverse FK5 correction did not converge for Pluto at JDE(TT) {}", observation_jde_tt)
  };
}

/** @brief Return observation-date mean-ecliptic coordinates ready for VSOP87D Earth subtraction. */
[[nodiscard]] inline auto heliocentric_for_subtraction(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): epoch names distinguish emission from observation.
  const double retarded_jde_tt,
  const double observation_jde_tt
) -> astro::toolbox::SphericalCoordinate {
  const auto j2000 = heliocentric_j2000(retarded_jde_tt);
  const auto precessed = astro::earth::precession::ecliptic(
    j2000.λ,
    j2000.β,
    astro::julian_day::J2000,
    observation_jde_tt
  );
  return inverse_fk5(
    {
      .λ = precessed.λ,
      .β = precessed.β,
      .r = j2000.r,
    },
    observation_jde_tt
  );
}

} // namespace astro::planet::detail::pluto
