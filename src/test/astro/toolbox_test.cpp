#include <gtest/gtest.h>
#include <cmath>
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

    const Angle<DEG> angle { deg };

    ASSERT_FLOAT_EQ(angle.as<DEG>(), deg);
    ASSERT_FLOAT_EQ(angle.as<RAD>(), deg_to_rad(deg));

    ASSERT_FLOAT_EQ(angle.normalize().as<DEG>(), normalize_deg(deg));
    ASSERT_FLOAT_EQ(angle.normalize().as<RAD>(), normalize_rad(deg_to_rad(deg)));

    const Angle<RAD> angle_rad { angle }; // Test implicit conversion.

    ASSERT_FLOAT_EQ(angle_rad.as<DEG>(), deg);
    ASSERT_FLOAT_EQ(angle_rad.as<RAD>(), deg_to_rad(deg));
  }

  for (auto i = 0; i < 1000; ++i) {
    const double rad = util::random(-2 * std::numbers::pi, 2 * std::numbers::pi);

    const Angle<RAD> angle { rad };

    ASSERT_FLOAT_EQ(angle.as<DEG>(), rad_to_deg(rad));
    ASSERT_FLOAT_EQ(angle.as<RAD>(), rad);

    ASSERT_FLOAT_EQ(angle.normalize().as<DEG>(), normalize_deg(rad_to_deg(rad)));
    ASSERT_FLOAT_EQ(angle.normalize().as<RAD>(), normalize_rad(rad));

    const Angle<DEG> angle_deg { angle }; // Test implicit conversion.

    ASSERT_FLOAT_EQ(angle_deg.as<DEG>(), rad_to_deg(rad));
    ASSERT_FLOAT_EQ(angle_deg.as<RAD>(), rad);
  }
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

    ASSERT_EQ(angle.as<DEG>(), (angle + 0.0).as<DEG>());
    ASSERT_EQ(angle.as<DEG>(), (angle - 0.0).as<DEG>());
    ASSERT_EQ(angle.as<DEG>(), (angle + 0.0_deg).as<DEG>());
    ASSERT_EQ(angle.as<DEG>(), (angle - 0.0_deg).as<DEG>());
    ASSERT_EQ(angle.as<DEG>() * 2.0, (angle * 2.0).as<DEG>());
    ASSERT_EQ(angle.as<DEG>() / 2.0, (angle / 2.0).as<DEG>());
  }

  {
    const auto angle = 1.0_rad;

    ASSERT_EQ(angle.as<RAD>(), (angle + 0.0).as<RAD>());
    ASSERT_EQ(angle.as<RAD>(), (angle - 0.0).as<RAD>());
    ASSERT_EQ(angle.as<RAD>(), (angle + 0.0_rad).as<RAD>());
    ASSERT_EQ(angle.as<RAD>(), (angle - 0.0_rad).as<RAD>());
    ASSERT_EQ(angle.as<RAD>() * 2.0, (angle * 2.0).as<RAD>());
    ASSERT_EQ(angle.as<RAD>() / 2.0, (angle / 2.0).as<RAD>());
  }
}


TEST(AstroMath, Ulp) {
  // A double carries 52 fraction bits, so the ulp is 2^(exponent-52) and doubles with the exponent.
  ASSERT_EQ(ulp(2451545.0), std::pow(2.0, -31)); // J2000.0 sits in [2^21, 2^22): 4.66e-10 day.
  ASSERT_EQ(ulp(4194303.0), std::pow(2.0, -31)); // Just below 2^22.
  ASSERT_EQ(ulp(4194305.0), std::pow(2.0, -30)); // Just above: 9.31e-10 day, i.e. 6771-07-07.

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
  // Reproduces the failure mode measured on issue #76: once the iterate lands on a stretch where
  // `f` reads flat, both difference samples come out equal, f' collapses to exactly zero, and the
  // next candidate is +/-inf — which `pull_back` then clamps to an interval edge. The solver must
  // hand back the closest approach it already made, not whatever the last round produced.
  const double root = 2451545.5;
  const auto f = [&](const double jde) -> double {
    const double d = jde - root;
    return (std::fabs(d) < 1e-6) ? 1e-6 : d; // flat plateau, far wider than the difference step
  };

  constexpr double start_jde = 2451545.0;
  constexpr double end_jde   = 2451546.0;
  const double found = newton_method(f, start_jde, end_jde, 1.0);

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
