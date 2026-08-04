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

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include "toolbox.hpp"
#include "util.hpp"

namespace astro::toolbox::test {

using namespace astro::toolbox;

TEST(AstroMath, NormalizedDeg) {
  {
    const auto x = normalize_deg(361);
    ASSERT_EQ(x, 1);
  }
  {
    const auto x = normalize_deg(-1);
    ASSERT_EQ(x, 359);
  }
  {
    const auto x = normalize_deg(355);
    ASSERT_EQ(x, 355);
  }
  {
    const auto x = normalize_deg(0);
    ASSERT_EQ(x, 0);
  }
  {
    const auto x = normalize_deg(180);
    ASSERT_EQ(x, 180);
  }
  {
    const auto x = normalize_deg(-180.05);
    ASSERT_FLOAT_EQ(x, 179.95);
  }
}

TEST(AstroMath, NormalizedPm180) {
  {
    const auto x = normalize_pm180(0);
    ASSERT_EQ(x, 0);
  }
  {
    // Half-open range [-180, 180): 180 wraps to -180.
    const auto x = normalize_pm180(180);
    ASSERT_EQ(x, -180);
  }
  {
    const auto x = normalize_pm180(-180);
    ASSERT_EQ(x, -180);
  }
  {
    const auto x = normalize_pm180(270);
    ASSERT_EQ(x, -90);
  }
  {
    const auto x = normalize_pm180(-90);
    ASSERT_EQ(x, -90);
  }
  {
    const auto x = normalize_pm180(450);
    ASSERT_EQ(x, 90);
  }
  {
    const auto x = normalize_pm180(-360.05);
    ASSERT_FLOAT_EQ(x, -0.05);
  }

  for (auto i = 0; i < 1000; ++i) {
    const double random_deg = util::random(-720.0, 720.0);
    const double x = normalize_pm180(random_deg);

    ASSERT_GE(x, -180.0);
    ASSERT_LT(x, 180.0);
    // Result is congruent to the input modulo 360.
    ASSERT_FLOAT_EQ(normalize_deg(x), normalize_deg(random_deg));
  }
}

TEST(AstroMath, NormalizedDegHalfOpenNearZero) {
  // The wrap `rem + 360.0` rounds up to exactly 360.0 for any `rem` within ulp(360)/2 = 2^-45 of
  // zero, so all of [-2^-45, 0) used to land on the end the range excludes (#88). The last two
  // inputs sit past that threshold and normalize the ordinary way — they are here so that an
  // over-correction pushing them out of range fails too.
  for (const double deg : { -1e-16, -1e-15, -1e-14, -2.0e-14, -2.8e-14, -2.9e-14, -1e-13 }) {
    const double x = normalize_deg(deg);
    ASSERT_GE(x, 0.0) << "deg = " << deg;
    ASSERT_LT(x, 360.0) << "deg = " << deg;
  }
}

TEST(AstroMath, NormalizedRad) {
  constexpr double TWO_PI = 2.0 * std::numbers::pi;

  // `ASSERT_DOUBLE_EQ` here and in `Distance`, where the older tests in this file use
  // `ASSERT_FLOAT_EQ`: these results land within ~1 ulp of double, and a float-scale tolerance
  // would wave through an error nine orders of magnitude larger. The neighbours keep the looser
  // one for a reason — the deg↔rad round trips they assert drift past 4 ulp and fail under this.

  {
    const auto x = normalize_rad(0.0);
    ASSERT_EQ(x, 0.0);
  }
  {
    const auto x = normalize_rad(std::numbers::pi);
    ASSERT_DOUBLE_EQ(x, std::numbers::pi);
  }
  {
    const auto x = normalize_rad(TWO_PI + 1.0);
    ASSERT_DOUBLE_EQ(x, 1.0);
  }
  {
    const auto x = normalize_rad(-1.0);
    ASSERT_DOUBLE_EQ(x, TWO_PI - 1.0);
  }

  // Same edge as `NormalizedDegHalfOpenNearZero`, one unit down: ulp(2π)/2 = 2^-51.
  for (const double rad : { -1e-18, -1e-17, -1e-16, -4.0e-16, -5.0e-16, -1e-15 }) {
    const double x = normalize_rad(rad);
    ASSERT_GE(x, 0.0) << "rad = " << rad;
    ASSERT_LT(x, TWO_PI) << "rad = " << rad;
  }

  for (auto i = 0; i < 1000; ++i) {
    const double x = normalize_rad(util::random(-2.0 * TWO_PI, 2.0 * TWO_PI));
    ASSERT_GE(x, 0.0);
    ASSERT_LT(x, TWO_PI);
  }
}

TEST(AstroMath, NormalizedWholeTurnsGivePositiveZero) {
  // `rem < 0.0` is false for -0.0, so whole turns used to skip the wrap and keep their sign bit.
  // `ASSERT_EQ(x, 0.0)` alone cannot see this — -0.0 compares equal to 0.0 (#88).
  for (const double deg : { -0.0, -360.0, -720.0, -1080.0 }) {
    const double x = normalize_deg(deg);
    ASSERT_EQ(x, 0.0) << "deg = " << deg;
    ASSERT_FALSE(std::signbit(x)) << "deg = " << deg;
  }

  constexpr double TWO_PI = 2.0 * std::numbers::pi;

  for (const double rad : { -0.0, -TWO_PI, -2.0 * TWO_PI }) {
    const double x = normalize_rad(rad);
    ASSERT_EQ(x, 0.0) << "rad = " << rad;
    ASSERT_FALSE(std::signbit(x)) << "rad = " << rad;
  }
}

TEST(AstroMath, NormalizeRejectsNonFinite) {
  constexpr double INF_D = std::numeric_limits<double>::infinity();
  constexpr double NAN_D = std::numeric_limits<double>::quiet_NaN();

  for (const double bad : { INF_D, -INF_D, NAN_D }) {
    ASSERT_THROW(std::ignore = normalize_deg(bad), std::invalid_argument);
    ASSERT_THROW(std::ignore = normalize_rad(bad), std::invalid_argument);
    ASSERT_THROW(std::ignore = normalize_pm180(bad), std::invalid_argument);
    ASSERT_THROW(std::ignore = Angle<AngleUnit::DEG> { bad }.normalize(), std::invalid_argument);
    ASSERT_THROW(std::ignore = Angle<AngleUnit::RAD> { bad }.normalize(), std::invalid_argument);
  }
}

TEST(AstroMath, RadDegConversion) {
  for (auto i = 0; i < 1000; ++i) {
    const double random_deg = util::random(-720.0, 720.0);
    const double random_rad = deg_to_rad(random_deg);

    const auto rad = deg_to_rad(random_deg);
    const auto deg = rad_to_deg(random_rad);

    ASSERT_FLOAT_EQ(deg, random_deg);
    ASSERT_FLOAT_EQ(rad, random_rad);
  }
}

TEST(AstroMath, Angle) {
  using AngleUnit::DEG;
  using AngleUnit::RAD;

  for (auto i = 0; i < 1000; ++i) {
    const double deg = util::random(-720.0, 720.0);

    const AngleDeg angle { deg };

    ASSERT_FLOAT_EQ(angle.as<DEG>(), deg);
    ASSERT_FLOAT_EQ(angle.as<RAD>(), deg_to_rad(deg));

    ASSERT_FLOAT_EQ(angle.normalize().as<DEG>(), normalize_deg(deg));
    ASSERT_FLOAT_EQ(angle.normalize().as<RAD>(), normalize_rad(deg_to_rad(deg)));

    const AngleRad angle_rad { angle };

    ASSERT_FLOAT_EQ(angle_rad.as<DEG>(), deg);
    ASSERT_FLOAT_EQ(angle_rad.as<RAD>(), deg_to_rad(deg));
  }

  for (auto i = 0; i < 1000; ++i) {
    const double rad = util::random(-2 * std::numbers::pi, 2 * std::numbers::pi);

    const AngleRad angle { rad };

    ASSERT_FLOAT_EQ(angle.as<DEG>(), rad_to_deg(rad));
    ASSERT_FLOAT_EQ(angle.as<RAD>(), rad);

    ASSERT_FLOAT_EQ(angle.normalize().as<DEG>(), normalize_deg(rad_to_deg(rad)));
    ASSERT_FLOAT_EQ(angle.normalize().as<RAD>(), normalize_rad(rad));

    const AngleDeg angle_deg { angle };

    ASSERT_FLOAT_EQ(angle_deg.as<DEG>(), rad_to_deg(rad));
    ASSERT_FLOAT_EQ(angle_deg.as<RAD>(), rad);
  }
}

TEST(AstroMath, AngleFromArcSubdivisions) {
  // Arcminutes and arcseconds subdivide the degree, but the angle they name is carried in
  // whichever unit it is asked of.
  static_assert(std::is_same_v<decltype(AngleDeg::from_arcmin(1.0)), AngleDeg>);
  static_assert(std::is_same_v<decltype(AngleRad::from_arcmin(1.0)), AngleRad>);
  static_assert(std::is_same_v<decltype(AngleDeg::from_arcsec(1.0)), AngleDeg>);
  static_assert(std::is_same_v<decltype(AngleRad::from_arcsec(1.0)), AngleRad>);

  ASSERT_DOUBLE_EQ(AngleDeg::from_arcmin(60.0).deg(), 1.0);
  ASSERT_DOUBLE_EQ(AngleRad::from_arcmin(60.0).deg(), 1.0);
  ASSERT_DOUBLE_EQ(AngleRad::from_arcmin(60.0).rad(), deg_to_rad(1.0));

  ASSERT_DOUBLE_EQ(AngleDeg::from_arcsec(3600.0).deg(), 1.0);
  ASSERT_DOUBLE_EQ(AngleRad::from_arcsec(3600.0).deg(), 1.0);
  ASSERT_DOUBLE_EQ(AngleRad::from_arcsec(3600.0).rad(), deg_to_rad(1.0));
}

TEST(AstroMath, literals) {
  using namespace literals;
  using AngleUnit::DEG;
  using AngleUnit::RAD;

  {
    const auto angle = 360.0_deg;
    ASSERT_FLOAT_EQ(angle.as<DEG>(), 360.0);
    ASSERT_FLOAT_EQ(angle.as<RAD>(), 2.0 * std::numbers::pi);
  }

  {
    const auto angle = 0.3141592653589793_rad;
    ASSERT_FLOAT_EQ(angle.as<RAD>(), 0.3141592653589793);
  }

  {
    const auto angle = 1.0_arcmin;
    ASSERT_FLOAT_EQ(angle.as<DEG>(), arcmin_to_deg(1.0));
  }

  {
    const auto angle = 1.0_arcsec;
    ASSERT_FLOAT_EQ(angle.as<DEG>(), arcsec_to_deg(1.0));
  }
}

TEST(AstroMath, AngleOperators) {
  using namespace literals;
  using AngleUnit::DEG;
  using AngleUnit::RAD;

  {
    const auto angle = 360.0_deg;

    ASSERT_EQ(angle.as<DEG>(), (angle + 0.0_deg).as<DEG>());
    ASSERT_EQ(angle.as<DEG>(), (angle - 0.0_deg).as<DEG>());
    ASSERT_EQ(angle.as<DEG>() * 2.0, (angle * 2.0).as<DEG>());
    ASSERT_EQ(angle.as<DEG>() / 2.0, (angle / 2.0).as<DEG>());
  }

  {
    const auto angle = 1.0_rad;

    ASSERT_EQ(angle.as<RAD>(), (angle + 0.0_rad).as<RAD>());
    ASSERT_EQ(angle.as<RAD>(), (angle - 0.0_rad).as<RAD>());
    ASSERT_EQ(angle.as<RAD>() * 2.0, (angle * 2.0).as<RAD>());
    ASSERT_EQ(angle.as<RAD>() / 2.0, (angle / 2.0).as<RAD>());
  }
}

TEST(AstroMath, AngleDivisionByZeroThrows) {
  using namespace literals;

  // The contract (#48): a zero divisor is a caller mistake, not a state to hand on as ±inf.
  const auto angle = 30.0_deg;
  ASSERT_THROW(std::ignore = (angle / 0.0), std::runtime_error);
  ASSERT_THROW(std::ignore = (angle / -0.0), std::runtime_error); // IEEE says -0.0 == 0.0; so does the guard.

  // The guard tests for zero, not for smallness — a denormal divisor still divides.
  ASSERT_NO_THROW(std::ignore = (angle / std::numeric_limits<double>::denorm_min()));
}


TEST(AstroMath, Distance) {
  using DistanceUnit::AU;
  using DistanceUnit::KM;

  static_assert(std::is_same_v<DistanceAu, Distance<AU>>);
  static_assert(std::is_same_v<DistanceKm, Distance<KM>>);
  // A bare double must not become a distance, and AU must not quietly become KM — the same rule
  // `Angle` follows (#48).
  static_assert(not std::is_convertible_v<double, DistanceAu>);
  static_assert(not std::is_convertible_v<DistanceAu, DistanceKm>);

  // Pinned against the constant, not the literal 149597870.691: a re-value or rename (#86) then
  // repoints this test instead of forcing its expected values to be re-derived.
  ASSERT_EQ(au_to_km(1.0), au_km_scale);
  ASSERT_EQ(km_to_au(au_km_scale), 1.0);
  ASSERT_EQ(au_to_km(0.0), 0.0);
  ASSERT_EQ(km_to_au(0.0), 0.0);

  for (auto i = 0; i < 1000; ++i) {
    const double au = util::random(-50.0, 50.0);

    const DistanceAu distance { au };

    ASSERT_DOUBLE_EQ(distance.as<AU>(), au);
    ASSERT_DOUBLE_EQ(distance.as<KM>(), au_to_km(au));
    ASSERT_DOUBLE_EQ(distance.au(), au);
    ASSERT_DOUBLE_EQ(distance.km(), au_to_km(au));

    const DistanceKm in_km { distance };

    ASSERT_DOUBLE_EQ(in_km.km(), au_to_km(au));
    ASSERT_DOUBLE_EQ(in_km.au(), au);
  }

  for (auto i = 0; i < 1000; ++i) {
    const double km = util::random(-1e9, 1e9);

    const DistanceKm distance { km };

    ASSERT_DOUBLE_EQ(distance.as<KM>(), km);
    ASSERT_DOUBLE_EQ(distance.as<AU>(), km_to_au(km));
    ASSERT_DOUBLE_EQ(distance.km(), km);
    ASSERT_DOUBLE_EQ(distance.au(), km_to_au(km));

    const DistanceAu in_au { distance };

    ASSERT_DOUBLE_EQ(in_au.km(), km);
    ASSERT_DOUBLE_EQ(in_au.au(), km_to_au(km));
  }
}

TEST(AstroMath, Ulp) {
  // A double carries 52 fraction bits, so the ulp is 2^(exponent-52) and doubles with the exponent.
  // Hex float literals rather than `std::pow`: exact by construction, so `ASSERT_EQ` does not rest
  // on libm rounding the integer exponent bit-exactly.
  ASSERT_EQ(ulp(2451545.0), 0x1p-31); // J2000.0 sits in [2^21, 2^22): 4.66e-10 day.
  ASSERT_EQ(ulp(4194303.0), 0x1p-31); // Just below 2^22.
  ASSERT_EQ(ulp(4194305.0), 0x1p-30); // Just above: 9.31e-10 day, i.e. 6771-07-07.

  // Always positive, and symmetric about zero.
  ASSERT_GT(ulp(-2451545.0), 0.0);
  ASSERT_EQ(ulp(-2451545.0), ulp(2451545.0));
}


TEST(AstroMath, NewtonMethodConverges) {
  // A smooth, slightly non-linear function, so the solver has to actually iterate.
  const double root = 2451545.25;
  const auto f = [&](const double jde) -> double {
    const double d = jde - root;
    return d + 0.001 * d * d;
  };

  const double found = newton_method(f, 2451545.0, 2451546.0, 1.0);
  ASSERT_NEAR(found, root, 1e-9);
}


TEST(AstroMath, NewtonMethodKeepsBestIterate) {
  // The contract is that the closest approach survives, so the divergence has to be one the solver
  // cannot walk back from. `f` reads a small non-zero residual on a narrow plateau at the root --
  // enough that the solver keeps iterating -- and everywhere else it pushes right hard enough to
  // overshoot `end_jde`, which from the clamped edge only ever overshoots again. Iteration 0 lands
  // on the root, iteration 1 leaves for the edge and stays: only a retained iterate can come back.
  constexpr double start_jde = 2451545.0;
  constexpr double end_jde   = 2451546.0;
  constexpr double root      = std::midpoint(start_jde, end_jde); // the solver's own first iterate

  const auto visits_root_then_diverges = [](const double jde) -> double {
    if (std::fabs(jde - root) < 1e-6) {
      return 1e-6;                 // above the residual tolerance, so this is not a stopping point
    }
    return -1.0 - (end_jde - jde); // f' == 1, and the step overshoots `end_jde` from anywhere
  };

  const double found = newton_method(visits_root_then_diverges, start_jde, end_jde, 1.0);

  ASSERT_NEAR(found, root, 1e-5); // the iterate that was kept, not the edge the run ended on
  ASSERT_GT(found, start_jde);
  ASSERT_LT(found, end_jde);
}


TEST(AstroMath, NewtonMethodSurvivesCollapsedDerivative) {
  // The mechanism behind #76: on a stretch where `f` reads flat both difference samples come out
  // equal, f' is exactly zero, and the Newton step is undefined.
  // This covers the collapse path only, not the retention contract above: from a clamped edge
  // Newton steps straight back onto the root, so which iterate the run ends on comes down to where
  // the budget runs out in that cycle -- the parity that made #76 surface in only 3% of years.
  const double root = 2451545.5;
  const auto flat_at_root = [&](const double jde) -> double {
    const double d = jde - root;
    return (std::fabs(d) < 1e-6) ? 1e-6 : d; // plateau far wider than the floored difference step
  };

  constexpr double start_jde = 2451545.0;
  constexpr double end_jde   = 2451546.0;
  const double found = newton_method(flat_at_root, start_jde, end_jde, 1.0);

  ASSERT_NEAR(found, root, 1e-5);
  ASSERT_GT(found, start_jde); // not clamped to an edge
  ASSERT_LT(found, end_jde);
}


TEST(AstroMath, NewtonMethodStepIsFlooredAtUlp) {
  // At this magnitude one ulp is 0.125, so `NEWTON_MIN_STEP_ULP * ulp(jde)` is 1.0 — three orders
  // of magnitude above `NEWTON_INITIAL_STEP_DAYS`. The floor therefore governs from the first
  // round, which is the branch that never runs at real JDE magnitudes.
  constexpr double huge_jde = 1e15;
  ASSERT_EQ(ulp(huge_jde), 0.125);
  ASSERT_GT(NEWTON_MIN_STEP_ULP * ulp(huge_jde), NEWTON_INITIAL_STEP_DAYS);

  const double root = huge_jde + 400.0;
  const auto f = [&](const double jde) -> double { return jde - root; };

  const double found = newton_method(f, huge_jde, huge_jde + 1000.0, 1.0);

  // Without the floor the samples would land on the same representable double and f' would be 0.
  ASSERT_NEAR(found, root, 1.0);
}


TEST(AstroMath, NewtonMethodStaysInRange) {
  // The half-open contract: `end_jde` itself must never be returned, however hard f pushes right.
  constexpr double start_jde = 2451545.0;
  constexpr double end_jde   = 2451546.0;

  const auto pushes_right = [](const double jde) -> double { return -1.0 - (end_jde - jde); };
  const double found = newton_method(pushes_right, start_jde, end_jde, 1.0);

  ASSERT_GE(found, start_jde);
  ASSERT_LT(found, end_jde);
}

} // namespace astro::toolbox::test
