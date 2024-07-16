#include <gtest/gtest.h>
#include "toolbox.hpp"
#include "util.hpp"

namespace astro::toolbox {

TEST(AstroMath, normalize_deg) {
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

TEST(AstroMath, rad_deg_conversion) {
  for (auto i = 0; i < 1000; ++i) {
    const double random_deg = util::random(-720.0, 720.0);
    const double random_rad = deg_to_rad(random_deg);

    const auto rad = deg_to_rad(random_deg);
    const auto deg = rad_to_deg(random_rad);

    ASSERT_FLOAT_EQ(deg, random_deg);
    ASSERT_FLOAT_EQ(rad, random_rad);
  }
}

TEST(AstroMath, angle) {
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

TEST(AstroMath, angle_operator) {
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

} // namespace astro::math
