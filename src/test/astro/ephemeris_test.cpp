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

#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#include <gtest/gtest.h>

#include "ephemeris.hpp"

// Retained material boundaries: V44 identifies AA+ v2.63 output, V20 the SOFA IAU 1980
// correction, and V15/R38 the corrected PyMeeus/Sonia Keys Pluto reference components.
// The numerical rows remain under their source terms and outside the project MIT grant.

namespace astro::ephemeris::test {

namespace {

using astro::lunar_node::Node;
using astro::planet::Planet;
using astro::toolbox::AngleDeg;

static_assert(std::same_as<std::underlying_type_t<Target>, uint8_t>);
static_assert(not std::is_convertible_v<Target, int>);
static_assert(not std::is_convertible_v<Planet, Target>);
static_assert(not std::is_convertible_v<Node, Target>);
static_assert(std::same_as<decltype(&longitude_rate), LongitudeRate (*)(Target, double)>);
static_assert(std::same_as<decltype(LongitudeRate::deg_per_tt_day), double>);
static_assert(not std::is_convertible_v<LongitudeRate, double>);
static_assert(not std::is_convertible_v<LongitudeRate, AngleDeg>);
static_assert(not std::is_constructible_v<Target, int>);
static_assert(not std::is_constructible_v<Target, AngleDeg>);

constexpr std::array<Target, 9> WORKING_WINDOW_TARGETS {
  Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS, Target::JUPITER,
  Target::SATURN, Target::URANUS, Target::NEPTUNE,
};

struct GoldenRow {
  Target target;
  double jde_tt;
  double deg_per_tt_day;
  std::string_view role;
};

// Collected 2026-10-03: independent 11-point reference, h=1/8 TT day; seeds 306/30602.
// Source pins, regeneration and comparison tolerances: src/test/provenance/longitude-rate/ATTRIBUTION.md.
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
constexpr std::array GOLDEN_ROWS {
  // Target, JDE(TT), rate (degrees/TT day), reference role.
  GoldenRow { Target::SUN            , 2409545           , 1.0189947437412421     , "early"                     },
  GoldenRow { Target::SUN            , 2451545           , 1.0194337304241066     , "J2000"                     },
  GoldenRow { Target::SUN            , 2488067.5         , 1.0186999774276886     , "late"                      },
  GoldenRow { Target::SUN            , 2426902.8825072921, 0.95358781516615654    , "seed30602-first"           },
  GoldenRow { Target::MOON           , 2409545           , 14.643378026367806     , "early"                     },
  GoldenRow { Target::MOON           , 2451545           , 12.021193502742651     , "J2000"                     },
  GoldenRow { Target::MOON           , 2488067.5         , 13.834433464609255     , "late"                      },
  GoldenRow { Target::MOON           , 2426902.8825072921, 14.444974870600037     , "seed30602-first"           },
  GoldenRow { Target::MERCURY        , 2409545           , -1.3410116739041711    , "early"                     },
  GoldenRow { Target::MERCURY        , 2451545           , 1.5562523359351419     , "J2000"                     },
  GoldenRow { Target::MERCURY        , 2488067.5         , 1.6154354957908246     , "late"                      },
  GoldenRow { Target::MERCURY        , 2426902.8825072921, 1.2369849593358857     , "seed30602-first"           },
  GoldenRow { Target::VENUS          , 2409545           , 1.2402294789858916     , "early"                     },
  GoldenRow { Target::VENUS          , 2451545           , 1.2090418196647108     , "J2000"                     },
  GoldenRow { Target::VENUS          , 2488067.5         , 1.205132423090191      , "late"                      },
  GoldenRow { Target::VENUS          , 2426902.8825072921, -0.2564433587347969    , "seed30602-first"           },
  GoldenRow { Target::MARS           , 2409545           , 0.77894993860238115    , "early"                     },
  GoldenRow { Target::MARS           , 2451545           , 0.77567415478420121    , "J2000"                     },
  GoldenRow { Target::MARS           , 2488067.5         , 0.39573536839623347    , "late"                      },
  GoldenRow { Target::MARS           , 2426902.8825072921, 0.69034030328639917    , "seed30602-first"           },
  GoldenRow { Target::JUPITER        , 2409545           , -0.042701484876254171  , "early"                     },
  GoldenRow { Target::JUPITER        , 2451545           , 0.04075887452225286    , "J2000"                     },
  GoldenRow { Target::JUPITER        , 2488067.5         , 0.1112281242245209     , "late"                      },
  GoldenRow { Target::JUPITER        , 2426902.8825072921, 0.19928695791546139    , "seed30602-first"           },
  GoldenRow { Target::SATURN         , 2409545           , -0.070953415462241404  , "early"                     },
  GoldenRow { Target::SATURN         , 2451545           , -0.019946186914668561  , "J2000"                     },
  GoldenRow { Target::SATURN         , 2488067.5         , 0.061031831699355756   , "late"                      },
  GoldenRow { Target::SATURN         , 2426902.8825072921, -0.072259247173846547  , "seed30602-first"           },
  GoldenRow { Target::URANUS         , 2409545           , 0.0032047113106323699  , "early"                     },
  GoldenRow { Target::URANUS         , 2451545           , 0.050343690526310762   , "J2000"                     },
  GoldenRow { Target::URANUS         , 2488067.5         , 0.002845662560201597   , "late"                      },
  GoldenRow { Target::URANUS         , 2426902.8825072921, 0.012201887602618595   , "seed30602-first"           },
  GoldenRow { Target::NEPTUNE        , 2409545           , -0.014673614100103788  , "early"                     },
  GoldenRow { Target::NEPTUNE        , 2451545           , 0.035570485847211064   , "J2000"                     },
  GoldenRow { Target::NEPTUNE        , 2488067.5         , -0.006056761186810416  , "late"                      },
  GoldenRow { Target::NEPTUNE        , 2426902.8825072921, 0.028318081999340296   , "seed30602-first"           },
  GoldenRow { Target::PLUTO          , 2409545           , -0.013178659764448307  , "early"                     },
  GoldenRow { Target::PLUTO          , 2451545           , 0.035153993021122515   , "J2000"                     },
  GoldenRow { Target::PLUTO          , 2488067.5         , -0.0060921149580399703 , "late"                      },
  GoldenRow { Target::PLUTO          , 2426902.8825072921, 0.026679976558030005   , "seed30602-first"           },
  GoldenRow { Target::MEAN_ASCENDING , 2409545           , -0.052943230807125118  , "early"                     },
  GoldenRow { Target::MEAN_ASCENDING , 2451545           , -0.05295165246865232   , "J2000"                     },
  GoldenRow { Target::MEAN_ASCENDING , 2488067.5         , -0.052927552653230833  , "late"                      },
  GoldenRow { Target::MEAN_ASCENDING , 2426902.8825072921, -0.052911264352908063  , "seed30602-first"           },
  GoldenRow { Target::MEAN_DESCENDING, 2409545           , -0.052943230807125118  , "early"                     },
  GoldenRow { Target::MEAN_DESCENDING, 2451545           , -0.05295165246865232   , "J2000"                     },
  GoldenRow { Target::MEAN_DESCENDING, 2488067.5         , -0.052927552653230833  , "late"                      },
  GoldenRow { Target::MEAN_DESCENDING, 2426902.8825072921, -0.052911264352908063  , "seed30602-first"           },
  GoldenRow { Target::TRUE_ASCENDING , 2409545           , -0.16744958561552245   , "early"                     },
  GoldenRow { Target::TRUE_ASCENDING , 2451545           , -0.044514694546148412  , "J2000"                     },
  GoldenRow { Target::TRUE_ASCENDING , 2488067.5         , -0.1045827665892711    , "late"                      },
  GoldenRow { Target::TRUE_ASCENDING , 2426902.8825072921, -0.11470319296500217   , "seed30602-first"           },
  GoldenRow { Target::TRUE_DESCENDING, 2409545           , -0.16744958561552245   , "early"                     },
  GoldenRow { Target::TRUE_DESCENDING, 2451545           , -0.044514694546148412  , "J2000"                     },
  GoldenRow { Target::TRUE_DESCENDING, 2488067.5         , -0.1045827665892711    , "late"                      },
  GoldenRow { Target::TRUE_DESCENDING, 2426902.8825072921, -0.11470319296500217   , "seed30602-first"           },
  GoldenRow { Target::MEAN_ASCENDING , 2453905.5         , -0.052972465986243014  , "negative-wrap"             },
  GoldenRow { Target::MEAN_ASCENDING , 2453906.5         , -0.052966120409169898  , "negative-wrap"             },
  GoldenRow { Target::MEAN_DESCENDING, 2453905.5         , -0.052972465986243014  , "negative-wrap"             },
  GoldenRow { Target::MEAN_DESCENDING, 2453906.5         , -0.052966120409169898  , "negative-wrap"             },
  GoldenRow { Target::TRUE_ASCENDING , 2453908.5         , -0.088632140478598381  , "negative-wrap"             },
  GoldenRow { Target::TRUE_ASCENDING , 2453909.5         , -0.13531528219801778   , "negative-wrap"             },
  GoldenRow { Target::TRUE_DESCENDING, 2453908.5         , -0.088632140478598381  , "negative-wrap"             },
  GoldenRow { Target::TRUE_DESCENDING, 2453909.5         , -0.13531528219801778   , "negative-wrap"             },
  GoldenRow { Target::SUN            , 2460754.856816418 , 0.9935182911006224     , "Sun wrap offset -0.02"     },
  GoldenRow { Target::SUN            , 2460754.876816418 , 0.99350870837234873    , "Sun wrap offset 0.0"       },
  GoldenRow { Target::SUN            , 2460754.896816418 , 0.99349912665834328    , "Sun wrap offset 0.02"      },
  GoldenRow { Target::MOON           , 2460681.273070456 , 14.010827394702979     , "Moon wrap offset -0.02"    },
  GoldenRow { Target::MOON           , 2460681.293070456 , 14.012038943393113     , "Moon wrap offset 0.0"      },
  GoldenRow { Target::MOON           , 2460681.313070456 , 14.013246861908073     , "Moon wrap offset 0.02"     },
  GoldenRow { Target::MERCURY        , 2460749.5328680002, 0.035572283076531701   , "station-retrograde-before" },
  GoldenRow { Target::MERCURY        , 2460750.0328680002, -0.035362976011225615  , "station-retrograde-after"  },
  GoldenRow { Target::MERCURY        , 2460749.7828680002, -8.0933592936759627e-06, "station-retrograde-center" },
  GoldenRow { Target::MERCURY        , 2460772.714472    , -0.022661841585230085  , "station-direct-before"     },
  GoldenRow { Target::MERCURY        , 2460773.214472    , 0.022537504323557528   , "station-direct-after"      },
  GoldenRow { Target::MERCURY        , 2460772.964472    , 4.1013927214422338e-06 , "station-direct-center"     },
  GoldenRow { Target::VENUS          , 2460736.2758499999, 0.010168344826338592   , "station-retrograde-before" },
  GoldenRow { Target::VENUS          , 2460736.7758499999, -0.010206608903308049  , "station-retrograde-after"  },
  GoldenRow { Target::VENUS          , 2460736.5258499999, 1.1944363015526867e-06 , "station-retrograde-center" },
  GoldenRow { Target::VENUS          , 2460778.2941089999, -0.0099630917990907644 , "station-direct-before"     },
  GoldenRow { Target::VENUS          , 2460778.7941089999, 0.0099284365819929908  , "station-direct-after"      },
  GoldenRow { Target::VENUS          , 2460778.5441089999, 1.3525387307781212e-06 , "station-direct-center"     },
  GoldenRow { Target::MARS           , 2460730.334059    , -0.0031071545087747788 , "station-direct-before"     },
  GoldenRow { Target::MARS           , 2460730.834059    , 0.0030996856992529629  , "station-direct-after"      },
  GoldenRow { Target::MARS           , 2460730.584059    , 5.9507510123181886e-07 , "station-direct-center"     },
  GoldenRow { Target::JUPITER        , 2460990.9459930002, 0.00083792913477324536 , "station-retrograde-before" },
  GoldenRow { Target::JUPITER        , 2460991.4459930002, -0.00083681879364022182, "station-retrograde-after"  },
  GoldenRow { Target::JUPITER        , 2460991.1959930002, 7.8030413712822929e-07 , "station-retrograde-center" },
  GoldenRow { Target::JUPITER        , 2460710.6524189999, -0.0008636285492547997 , "station-direct-before"     },
  GoldenRow { Target::JUPITER        , 2460711.1524189999, 0.00085396155988064497 , "station-direct-after"      },
  GoldenRow { Target::JUPITER        , 2460710.9024189999, -4.6211594728505128e-06, "station-direct-center"     },
  GoldenRow { Target::SATURN         , 2460869.4208479999, 0.00042639859231056939 , "station-retrograde-before" },
  GoldenRow { Target::SATURN         , 2460869.9208479999, -0.00042133648884399379, "station-retrograde-after"  },
  GoldenRow { Target::SATURN         , 2460869.6708479999, 2.5527871504128622e-06 , "station-retrograde-center" },
  GoldenRow { Target::SATURN         , 2461007.412854    , -0.00044508311135478795, "station-direct-before"     },
  GoldenRow { Target::SATURN         , 2461007.912854    , 0.00044939592580379089 , "station-direct-after"      },
  GoldenRow { Target::SATURN         , 2461007.662854    , 2.0479962938138455e-06 , "station-direct-center"     },
  GoldenRow { Target::URANUS         , 2460924.4532260001, 0.00021437307573187863 , "station-retrograde-before" },
  GoldenRow { Target::URANUS         , 2460924.9532260001, -0.00021411249396947153, "station-retrograde-after"  },
  GoldenRow { Target::URANUS         , 2460924.7032260001, 8.212017993021923e-08  , "station-retrograde-center" },
  GoldenRow { Target::URANUS         , 2460705.932786    , -0.00021713718633359732, "station-direct-before"     },
  GoldenRow { Target::URANUS         , 2460706.432786    , 0.00021839311505143346 , "station-direct-after"      },
  GoldenRow { Target::URANUS         , 2460706.182786    , 5.2397472164297411e-07 , "station-direct-center"     },
  GoldenRow { Target::NEPTUNE        , 2460861.1496100002, 0.00013292695008754806 , "station-retrograde-before" },
  GoldenRow { Target::NEPTUNE        , 2460861.6496100002, -0.0001322569922693465 , "station-retrograde-after"  },
  GoldenRow { Target::NEPTUNE        , 2460861.3996100002, 2.4591743229146395e-07 , "station-retrograde-center" },
  GoldenRow { Target::NEPTUNE        , 2461019.7669520001, -0.00014153017515823992, "station-direct-before"     },
  GoldenRow { Target::NEPTUNE        , 2461020.2669520001, 0.00014074002172687224 , "station-direct-after"      },
  GoldenRow { Target::NEPTUNE        , 2461020.0169520001, -6.4051828736583491e-07, "station-direct-center"     },
  GoldenRow { Target::PLUTO          , 2460799.9288070002, 0.00010447022631296932 , "station-retrograde-before" },
  GoldenRow { Target::PLUTO          , 2460800.4288070002, -0.00013734969845664017, "station-retrograde-after"  },
  GoldenRow { Target::PLUTO          , 2460800.1788070002, -1.6553790857278007e-05, "station-retrograde-center" },
  GoldenRow { Target::PLUTO          , 2460962.4163409998, -9.467352246795759e-05 , "station-direct-before"     },
  GoldenRow { Target::PLUTO          , 2460962.9163409998, 0.00013616213715511639 , "station-direct-after"      },
  GoldenRow { Target::PLUTO          , 2460962.6663409998, 2.078300449320234e-05  , "station-direct-center"     },
  GoldenRow { Target::TRUE_ASCENDING , 2460681.5799983721, -5.0070247256251958e-05, "station 0 offset -0.01"    },
  GoldenRow { Target::TRUE_ASCENDING , 2460681.5899983719, 1.064522260996821e-11  , "station 0 offset 0.0"      },
  GoldenRow { Target::TRUE_ASCENDING , 2460681.5999983717, 4.8048529576308544e-05 , "station 0 offset 0.01"     },
  GoldenRow { Target::TRUE_ASCENDING , 2460682.0649247896, 4.7905484238062107e-05 , "station 1 offset -0.01"    },
  GoldenRow { Target::TRUE_ASCENDING , 2460682.0749247894, -1.9497236399164397e-12, "station 1 offset 0.0"      },
  GoldenRow { Target::TRUE_ASCENDING , 2460682.0849247891, -4.9908402318796574e-05, "station 1 offset 0.01"     },
  GoldenRow { Target::TRUE_DESCENDING, 2460681.5799983721, -5.0070247256251958e-05, "station 0 offset -0.01"    },
  GoldenRow { Target::TRUE_DESCENDING, 2460681.5899983719, 1.064522260996821e-11  , "station 0 offset 0.0"      },
  GoldenRow { Target::TRUE_DESCENDING, 2460681.5999983717, 4.8048529576308544e-05 , "station 0 offset 0.01"     },
  GoldenRow { Target::TRUE_DESCENDING, 2460682.0649247896, 4.7905484238062107e-05 , "station 1 offset -0.01"    },
  GoldenRow { Target::TRUE_DESCENDING, 2460682.0749247894, -1.9497236399164397e-12, "station 1 offset 0.0"      },
  GoldenRow { Target::TRUE_DESCENDING, 2460682.0849247891, -4.9908402318796574e-05, "station 1 offset 0.01"     },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

inline constexpr std::array<Target, 14> ALL_TARGETS {
  Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS, Target::JUPITER,
  Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO,
  Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING,
};

auto comparison_tolerance(const Target target) -> double {
  // Same-definition implementation/reference discrepancies, not physical accuracy.
  using enum Target;
  switch (target) {
    case SUN: case MOON: return 1e-8;
    case MERCURY: case VENUS: return 1e-7;
    case MARS: return 5e-8;
    case JUPITER: return 5e-9;
    case SATURN: return 2e-9;
    case URANUS: return 1e-9;
    case NEPTUNE: case PLUTO: case TRUE_ASCENDING: case TRUE_DESCENDING: return 5e-10;
    case MEAN_ASCENDING: case MEAN_DESCENDING: return 1e-10;
    default: throw std::logic_error { "Unknown golden target" };
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - Names distinguish the two domain bounds.
auto check_complete_stencil_bounds(const Target target, const double lower_jde_tt, const double upper_jde_tt) -> void {
  const double infinity = std::numeric_limits<double>::infinity();
  EXPECT_THROW(std::ignore = longitude_rate(target, std::nextafter(lower_jde_tt, -infinity)), std::invalid_argument);
  EXPECT_TRUE(std::isfinite(longitude_rate(target, lower_jde_tt).deg_per_tt_day));
  EXPECT_TRUE(std::isfinite(longitude_rate(target, std::nextafter(lower_jde_tt, infinity)).deg_per_tt_day));
  EXPECT_TRUE(std::isfinite(longitude_rate(target, std::nextafter(upper_jde_tt, -infinity)).deg_per_tt_day));
  EXPECT_THROW(std::ignore = longitude_rate(target, upper_jde_tt), std::invalid_argument);
  EXPECT_THROW(std::ignore = longitude_rate(target, std::nextafter(upper_jde_tt, infinity)), std::invalid_argument);
}

} // namespace

TEST(Ephemeris, IndependentLongitudeRates) {
  for (const auto& row : GOLDEN_ROWS) {
    SCOPED_TRACE(::testing::Message() << row.role << " target=" << static_cast<uint32_t>(row.target) << " JDE(TT)=" << row.jde_tt);
    const double actual = longitude_rate(row.target, row.jde_tt).deg_per_tt_day;
    const double tolerance = comparison_tolerance(row.target);
    ASSERT_NEAR(actual, row.deg_per_tt_day, tolerance);
    if (row.deg_per_tt_day > tolerance) {
      EXPECT_GT(actual, 0.0);
    } else if (row.deg_per_tt_day < -tolerance) {
      EXPECT_LT(actual, 0.0);
    }
  }
}

TEST(Ephemeris, IndependentStationNeighborhoods) {
  for (const auto& row : GOLDEN_ROWS) {
    SCOPED_TRACE(::testing::Message() << row.role << " JDE(TT)=" << row.jde_tt);
    if (row.role == "station 0 offset 0.0" or row.role == "station 1 offset 0.0") {
      EXPECT_NEAR(longitude_rate(row.target, row.jde_tt).deg_per_tt_day, 0.0, 5e-10);
    } else if (row.role == "station-retrograde-before" or row.role == "station-direct-after"
               or row.role == "station 0 offset 0.01" or row.role == "station 1 offset -0.01") {
      EXPECT_GT(longitude_rate(row.target, row.jde_tt).deg_per_tt_day, 0.0);
    } else if (row.role == "station-retrograde-after" or row.role == "station-direct-before"
               or row.role == "station 0 offset -0.01" or row.role == "station 1 offset 0.01") {
      EXPECT_LT(longitude_rate(row.target, row.jde_tt).deg_per_tt_day, 0.0);
    } else if (row.role.ends_with("-center")) {
      EXPECT_LT(std::abs(longitude_rate(row.target, row.jde_tt).deg_per_tt_day), 3e-5);
    }
  }
}

TEST(Ephemeris, PositiveAndNegativeWrappedRamps) {
  constexpr double center_jde_tt = 2451545.0;
  for (const double speed : std::array { -13.25, -0.5, 0.5, 13.25 }) {
    for (const double phase : std::array { 0.0, 180.0, 359.984375 }) {
      const auto longitude = [speed, phase](const double jde_tt) {
        return AngleDeg { phase + (speed * (jde_tt - center_jde_tt)) }.normalize();
      };
      for (const double offset : std::array { -0.25, 0.0, 0.25 }) {
        SCOPED_TRACE(::testing::Message() << "speed=" << speed << " phase=" << phase << " offset=" << offset);
        EXPECT_NEAR(detail::differentiate(longitude, center_jde_tt + offset).deg_per_tt_day, speed, 1e-12);
      }
    }
  }
}

TEST(Ephemeris, DegreeFivePolynomialDerivative) {
  constexpr double center_jde_tt = 2451545.0;
  const auto longitude = [](const double jde_tt) {
    const double x = jde_tt - center_jde_tt;
    // p(x)=17+2x-3x^2+4x^3-5x^4+6x^5; the analytic derivative is independent of stencil weights.
    return AngleDeg { 17.0 + (x * (2.0 + (x * (-3.0 + (x * (4.0 + (x * (-5.0 + (6.0 * x))))))))) };
  };
  for (const double x : std::array { -0.5, -0.25, 0.0, 0.25, 0.5 }) {
    const double derivative = 2.0 - (6.0 * x) + (12.0 * x * x) - (20.0 * x * x * x) + (30.0 * x * x * x * x);
    EXPECT_NEAR(detail::differentiate(longitude, center_jde_tt + x).deg_per_tt_day, derivative, 1e-12);
  }
}

TEST(Ephemeris, ConstantLongitudeHasZeroRate) {
  for (const double phase : std::array { 0.0, 180.0, 359.75 }) {
    const auto longitude = [phase](const double) { return AngleDeg { phase }; };
    EXPECT_EQ(detail::differentiate(longitude, 2451545.0).deg_per_tt_day, 0.0);
  }
}

TEST(Ephemeris, NonFiniteSamplesAreEvaluationFailures) {
  for (const double failure : std::array {
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(),
  }) {
    for (const double offset : std::array { -0.375, -0.25, -0.125, 0.125, 0.25, 0.375 }) {
      const auto longitude = [failure, offset](const double jde_tt) {
        return AngleDeg { jde_tt == 2451545.0 + offset ? failure : 10.0 };
      };
      EXPECT_THROW(std::ignore = detail::differentiate(longitude, 2451545.0), std::runtime_error);
    }
  }
  const auto overflowing_difference = [](const double jde_tt) {
    return AngleDeg { jde_tt < 2451545.0 ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max() };
  };
  EXPECT_THROW(std::ignore = detail::differentiate(overflowing_difference, 2451545.0), std::runtime_error);
}

TEST(Ephemeris, UnknownTargets) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) - Exercises the invalid-enumerator contract.
  const auto first_unknown = static_cast<Target>(14);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) - Exercises the invalid-enumerator contract.
  const auto last_unknown = static_cast<Target>(255);
  EXPECT_THROW(std::ignore = longitude_rate(first_unknown, 2451545.0), std::invalid_argument);
  EXPECT_THROW(std::ignore = longitude_rate(last_unknown, 2451545.0), std::invalid_argument);
}

TEST(Ephemeris, NonFiniteDates) {
  for (const auto& target : ALL_TARGETS) {
    for (const double jde_tt : std::array {
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
    }) {
      EXPECT_THROW(std::ignore = longitude_rate(target, jde_tt), std::invalid_argument);
    }
  }
}

TEST(Ephemeris, FiniteDatesWithCollapsedOffsets) {
  for (const auto& target : ALL_TARGETS) {
    for (const double jde_tt : std::array { 0x1p50, -0x1p50, std::numeric_limits<double>::max(), -std::numeric_limits<double>::max() }) {
      EXPECT_THROW(std::ignore = longitude_rate(target, jde_tt), std::invalid_argument);
    }
  }
}

TEST(Ephemeris, CompletePlutoStencilBoundaries) {
  check_complete_stencil_bounds(Target::PLUTO, 2409543.875, 2488069.125);
  EXPECT_THROW(std::ignore = longitude_rate(Target::PLUTO, 2409543.5), std::invalid_argument);
  EXPECT_THROW(std::ignore = longitude_rate(Target::PLUTO, 2488069.25), std::invalid_argument);
}

TEST(Ephemeris, CompleteNodeStencilBoundaries) {
  for (const Target node : std::array { Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING }) {
    check_complete_stencil_bounds(node, 2409542.875, 2488069.125);
    EXPECT_THROW(std::ignore = longitude_rate(node, 2409542.5), std::invalid_argument);
    EXPECT_THROW(std::ignore = longitude_rate(node, 2488069.25), std::invalid_argument);
  }
}

TEST(Ephemeris, CompleteWorkingStencilBoundaries) {
  for (const auto& target : WORKING_WINDOW_TARGETS) {
    check_complete_stencil_bounds(target, -1200954.625, 6104044.625);
    EXPECT_THROW(std::ignore = longitude_rate(target, -1200955.0), std::invalid_argument);
    EXPECT_THROW(std::ignore = longitude_rate(target, 6104045.0), std::invalid_argument);
    EXPECT_TRUE(std::isfinite(longitude_rate(target, 2409543.25).deg_per_tt_day));
    EXPECT_TRUE(std::isfinite(longitude_rate(target, 2488069.25).deg_per_tt_day));
  }
}

TEST(Ephemeris, AliasedDatesLeaveWorkingDomain) {
  for (const auto& target : WORKING_WINDOW_TARGETS) {
    for (const double jde_tt : std::array { -1e8, 1e8, -0x1p30, 0x1p30, 4e9 }) {
      EXPECT_THROW(std::ignore = longitude_rate(target, jde_tt), std::invalid_argument);
    }
  }
}

TEST(Ephemeris, RepresentabilityAcrossBinades) {
  constexpr double transition_jde_tt = 0x1p22;
  const double below = std::nextafter(transition_jde_tt, 0.0);
  const double above = std::nextafter(transition_jde_tt, std::numeric_limits<double>::infinity());
  for (const Target target : std::array { Target::SUN, Target::MOON }) {
    EXPECT_THROW(std::ignore = longitude_rate(target, -524287.7), std::invalid_argument);
    EXPECT_TRUE(std::isfinite(longitude_rate(target, -0x1p19).deg_per_tt_day));
    EXPECT_TRUE(std::isfinite(longitude_rate(target, -0x1p19 + 0.5).deg_per_tt_day));
    EXPECT_THROW(std::ignore = longitude_rate(target, 4194303.6999999997), std::invalid_argument);
    EXPECT_THROW(std::ignore = longitude_rate(target, below), std::invalid_argument);
    EXPECT_TRUE(std::isfinite(longitude_rate(target, transition_jde_tt).deg_per_tt_day));
    EXPECT_TRUE(std::isfinite(longitude_rate(target, above).deg_per_tt_day));
    EXPECT_TRUE(std::isfinite(longitude_rate(target, transition_jde_tt - 0.5).deg_per_tt_day));
    EXPECT_TRUE(std::isfinite(longitude_rate(target, transition_jde_tt + 0.5).deg_per_tt_day));
  }
}

TEST(Ephemeris, DescendingRatesAreBitExactAscendingRates) {
  for (const auto& row : GOLDEN_ROWS) {
    if (row.target == Target::MEAN_ASCENDING or row.target == Target::TRUE_ASCENDING) {
      const Target descending = row.target == Target::MEAN_ASCENDING ? Target::MEAN_DESCENDING : Target::TRUE_DESCENDING;
      EXPECT_EQ(
        std::bit_cast<uint64_t>(longitude_rate(row.target, row.jde_tt).deg_per_tt_day),
        std::bit_cast<uint64_t>(longitude_rate(descending, row.jde_tt).deg_per_tt_day)
      );
    }
  }
}

} // namespace astro::ephemeris::test
