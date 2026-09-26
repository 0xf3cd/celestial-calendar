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
#include <cmath>
#include <limits>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include "coord_transform.hpp"
#include "earth.hpp"
#include "geolocation.hpp"
#include "house.hpp"
#include "rise_set.hpp"
#include "toolbox.hpp"

namespace astro::house::test {

using astro::toolbox::AngleDeg;

namespace {

constexpr double FORMULA_TOLERANCE_DEG = 3e-6;
constexpr double DATE_TOLERANCE_DEG = 1e-4;

struct FormulaCase {
  std::string_view name;
  System system;
  double armc_deg;
  double latitude_deg;
  double obliquity_deg;
  double ascendant_deg;
  double midheaven_deg;
  std::array<double, 12> cusps_deg;
};

struct DateCase {
  std::string_view name;
  System system;
  double jd_ut1;
  double jde_tt;
  double latitude_deg;
  double longitude_deg;
  double ascendant_deg;
  double midheaven_deg;
  std::array<double, 12> cusps_deg;
};

[[nodiscard]] auto angular_difference(const double lhs, const double rhs) -> double {
  return std::fabs(std::remainder(lhs - rhs, 360.0));
}

} // anonymous namespace

static_assert(std::is_same_v<astro::rise_set::GeoLocation, astro::GeoLocation>);

TEST(House, AscendantMeeus14a) {
  // Meeus Example 14.a: the eastern ecliptic/horizon intersection is 169°21'30"; the printed
  // precision allows half an arcsecond. The antipodal 349°21'30" point is the setting branch.
  const auto ascendant = detail::ascendant(AngleDeg { 75.0 }, AngleDeg { 51.0 }, AngleDeg { 23.44 });
  ASSERT_NEAR(ascendant.deg(), 169.0 + (21.0 / 60.0) + (30.0 / 3600.0), 0.5 / 3600.0);
}

TEST(House, SwissFormulaLevel) {
  // Generated on 2026-09-25 by statistics/house_swiss_crawler.py from Swiss Ephemeris
  // v2.10.3bfinal `swe_houses_armc_ex2`. The separate 3e-6° tolerance covers the reference
  // solver's loss of precision immediately inside the Placidus polar limit.
  // NOLINTBEGIN(modernize-use-designated-initializers): positional columns mirror the crawler output.
  const std::array dataset {
    FormulaCase { "meeus-equal",       System::EQUAL,      75.00,   51.0000, 23.4400000, 169.358304802347,  76.188453109242, { 169.358304802347, 199.358304802347, 229.358304802347, 259.358304802347, 289.358304802347, 319.358304802347, 349.358304802347,  19.358304802347,  49.358304802347,  79.358304802347, 109.358304802347, 139.358304802347 } },
    FormulaCase { "meeus-whole",       System::WHOLE_SIGN, 75.00,   51.0000, 23.4400000, 169.358304802347,  76.188453109242, { 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000 } },
    FormulaCase { "meeus-placidus",    System::PLACIDUS,   75.00,   51.0000, 23.4400000, 169.358304802347,  76.188453109242, { 169.358304802347, 192.034797538214, 220.933748396566, 256.188453109242, 292.881090430665, 324.160283058891, 349.358304802347,  12.034797538214,  40.933748396566,  76.188453109242, 112.881090430665, 144.160283058891 } },
    FormulaCase { "southern-equal",     System::EQUAL,      123.00,  -33.8688, 23.4392911, 227.305346515834, 120.787323771477, { 227.305346515834, 257.305346515834, 287.305346515834, 317.305346515834, 347.305346515834,  17.305346515834,  47.305346515834,  77.305346515834, 107.305346515834, 137.305346515834, 167.305346515834, 197.305346515834 } },
    FormulaCase { "southern-whole",     System::WHOLE_SIGN, 123.00,  -33.8688, 23.4392911, 227.305346515834, 120.787323771477, { 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000 } },
    FormulaCase { "southern-placidus",  System::PLACIDUS,   123.00,  -33.8688, 23.4392911, 227.305346515834, 120.787323771477, { 227.305346515834, 255.092181735632, 277.875988585158, 300.787323771477, 328.059243246997,   4.055124459259,  47.305346515834,  75.092181735632,  97.875988585158, 120.787323771477, 148.059243246997, 184.055124459259 } },
    FormulaCase { "wrap-equal",         System::EQUAL,      359.90,    0.0000, 23.4392911,  89.908251779045, 359.891006064271, {  89.908251779045, 119.908251779045, 149.908251779045, 179.908251779045, 209.908251779045, 239.908251779045, 269.908251779045, 299.908251779045, 329.908251779045, 359.908251779045,  29.908251779045,  59.908251779045 } },
    FormulaCase { "wrap-whole",         System::WHOLE_SIGN, 359.90,    0.0000, 23.4392911,  89.908251779045, 359.891006064271, {  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000 } },
    FormulaCase { "wrap-placidus",      System::PLACIDUS,   359.90,    0.0000, 23.4392911,  89.908251779045, 359.891006064271, {  89.908251779045, 117.815034738398, 147.714652976104, 179.891006064271, 212.077143059379, 241.993911375181, 269.908251779045, 297.815034738398, 327.714652976104, 359.891006064271,  32.077143059379,  61.993911375181 } },
    FormulaCase { "near-polar-north",   System::PLACIDUS,  276.94,   66.5000, 23.4400000,  91.941491363794, 276.372211073616, {  91.941491363794,  93.033796867674,  94.394426330861,  96.372211073616,  99.948786226864, 110.000757535689, 271.941491363794, 273.033796867674, 274.394426330861, 276.372211073616, 279.948786226864, 290.000757535689 } },
    FormulaCase { "near-polar-south",   System::PLACIDUS,   96.94,  -66.5000, 23.4400000, 271.941491363794,  96.372211073616, { 271.941491363794, 273.033796867674, 274.394426330861, 276.372211073616, 279.948786226864, 290.000757535689,  91.941491363794,  93.033796867674,  94.394426330861,  96.372211073616,  99.948786226864, 110.000757535689 } },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  for (const auto& row : dataset) {
    SCOPED_TRACE(row.name);
    const auto actual = detail::calculate(
      AngleDeg { row.armc_deg },
      AngleDeg { row.obliquity_deg },
      AngleDeg { row.latitude_deg },
      row.system
    );
    ASSERT_NEAR(angular_difference(actual.ascendant.deg(), row.ascendant_deg), 0.0, FORMULA_TOLERANCE_DEG);
    ASSERT_NEAR(angular_difference(actual.midheaven.deg(), row.midheaven_deg), 0.0, FORMULA_TOLERANCE_DEG);
    for (std::size_t index = 0; index < actual.cusps.size(); ++index) {
      ASSERT_NEAR(
        angular_difference(actual.cusps.at(index).deg(), row.cusps_deg.at(index)),
        0.0,
        FORMULA_TOLERANCE_DEG
      );
    }
  }
}

TEST(House, SwissDateLevel) {
  // Generated on 2026-09-25 by statistics/house_swiss_crawler.py from Swiss Ephemeris
  // v2.10.3bfinal `swe_houses_ex2`. The 1e-4° tolerance covers the different apparent sidereal-time
  // and nutation models while remaining far below the errors from a UT1/TT or longitude-sign swap.
  // NOLINTBEGIN(modernize-use-designated-initializers): positional columns mirror the crawler output.
  const std::array dataset {
    DateCase { "j2000-equal",         System::EQUAL,      2451545.0000000000, 2451545.0007387605,  51.5000,    0.0000,  24.287305086198, 279.611087800285, {  24.287305086198,  54.287305086198,  84.287305086198, 114.287305086198, 144.287305086198, 174.287305086198, 204.287305086198, 234.287305086198, 264.287305086198, 294.287305086198, 324.287305086198, 354.287305086198 } },
    DateCase { "j2000-whole",         System::WHOLE_SIGN, 2451545.0000000000, 2451545.0007387605,  51.5000,    0.0000,  24.287305086198, 279.611087800285, {   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000 } },
    DateCase { "j2000-placidus",      System::PLACIDUS,   2451545.0000000000, 2451545.0007387605,  51.5000,    0.0000,  24.287305086198, 279.611087800285, {  24.287305086198,  61.161799412691,  82.030012128933,  99.611087800285, 119.053371520496, 147.687375204715, 204.287305086198, 241.161799412691, 262.030012128933, 279.611087800285, 299.053371520496, 327.687375204715 } },
    DateCase { "meeus-washington",    System::PLACIDUS,   2446896.3062499999, 2446896.3068918549,  38.9215,  -77.0669, 149.214202067669,  54.045103959580, { 149.214202067669, 172.642500506966, 201.040678275441, 234.045103959580, 268.623954728405, 300.833758397683, 329.214202067669, 352.642500506966,  21.040678275441,  54.045103959580,  88.623954728405, 120.833758397683 } },
    DateCase { "j2000-sydney",        System::PLACIDUS,   2451545.0000000000, 2451545.0007387605, -33.8688,  151.2093, 152.488909139281,  73.089186823627, { 152.488909139281, 195.677567269628, 228.131549399578, 253.089186823627, 275.563408529259, 299.979438157141, 332.488909139281,  15.677567269628,  48.131549399578,  73.089186823627,  95.563408529259, 119.979438157141 } },
    DateCase { "east-date-line",      System::PLACIDUS,   2461041.5000000000, 2461041.5007974519,  35.0000,  179.9000,  16.384568398230, 279.708040990102, {  16.384568398230,  51.871318063133,  77.373611104101,  99.708040990102, 123.504731500787, 153.898599937039, 196.384568398230, 231.871318063133, 257.373611104101, 279.708040990102, 303.504731500787, 333.898599937039 } },
    DateCase { "west-date-line",      System::PLACIDUS,   2461041.5000000000, 2461041.5007974519,  35.0000, -179.9000,  16.689296583904, 279.892538251926, {  16.689296583904,  52.098550056418,  77.563466671451,  99.892538251926, 123.711455212670, 154.158332645255, 196.689296583904, 232.098550056418, 257.563466671451, 279.892538251926, 303.711455212670, 334.158332645255 } },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  for (const auto& row : dataset) {
    SCOPED_TRACE(row.name);
    const auto actual = calculate(
      row.jd_ut1,
      row.jde_tt,
      astro::GeoLocation {
        .latitude = AngleDeg { row.latitude_deg },
        .longitude = AngleDeg { row.longitude_deg },
      },
      row.system
    );
    ASSERT_NEAR(angular_difference(actual.ascendant.deg(), row.ascendant_deg), 0.0, DATE_TOLERANCE_DEG);
    ASSERT_NEAR(angular_difference(actual.midheaven.deg(), row.midheaven_deg), 0.0, DATE_TOLERANCE_DEG);
    for (std::size_t index = 0; index < actual.cusps.size(); ++index) {
      ASSERT_NEAR(
        angular_difference(actual.cusps.at(index).deg(), row.cusps_deg.at(index)),
        0.0,
        DATE_TOLERANCE_DEG
      );
    }
  }
}

TEST(House, PrincipalAnglesMatchCoordinateGeometry) {
  const AngleDeg armc { 75.0 };
  const AngleDeg latitude { 51.0 };
  const AngleDeg obliquity { 23.44 };
  const auto result = detail::calculate(armc, obliquity, latitude, System::PLACIDUS);

  const auto mc_equatorial = astro::coords::ecliptic_to_equatorial(
    result.midheaven,
    AngleDeg { 0.0 },
    obliquity
  );
  ASSERT_NEAR(angular_difference(mc_equatorial.α.deg(), armc.deg()), 0.0, 1e-12);

  const auto asc_equatorial = astro::coords::ecliptic_to_equatorial(
    result.ascendant,
    AngleDeg { 0.0 },
    obliquity
  );
  const auto asc_horizontal = astro::coords::equatorial_to_horizontal(
    (armc - asc_equatorial.α).normalize(),
    asc_equatorial.δ,
    latitude
  );
  ASSERT_NEAR(asc_horizontal.h.deg(), 0.0, 1e-12);
  ASSERT_GT((armc - asc_equatorial.α).normalize().deg(), 180.0); // The eastern, rising intersection.
}

TEST(House, CuspsHonorSystemStructure) {
  const auto equal = detail::calculate(AngleDeg { 75.0 }, AngleDeg { 23.44 }, AngleDeg { 51.0 }, System::EQUAL);
  const auto whole = detail::calculate(AngleDeg { 75.0 }, AngleDeg { 23.44 }, AngleDeg { 51.0 }, System::WHOLE_SIGN);
  const auto placidus = detail::calculate(
    AngleDeg { 75.0 },
    AngleDeg { 23.44 },
    AngleDeg { 51.0 },
    System::PLACIDUS
  );

  for (std::size_t index = 0; index < 12; ++index) {
    ASSERT_NEAR(
      angular_difference(equal.cusps.at(index).deg(), equal.ascendant.deg() + (30.0 * index)),
      0.0,
      1e-12
    );
    ASSERT_NEAR(std::remainder(whole.cusps.at(index).deg(), 30.0), 0.0, 1e-12);
  }
  for (const Result* result : { &equal, &whole, &placidus }) {
    for (std::size_t index = 0; index < 6; ++index) {
      ASSERT_NEAR(
        angular_difference(result->cusps.at(index + 6).deg(), result->cusps.at(index).deg() + 180.0),
        0.0,
        1e-11
      );
    }
  }
}

TEST(House, WholeSignBoundaryBelongsToFollowingSign) {
  ASSERT_NEAR(detail::whole_sign_start(AngleDeg { 29.999999 }).deg(), 0.0, 1e-12);
  ASSERT_NEAR(detail::whole_sign_start(AngleDeg { 30.0 }).deg(), 30.0, 1e-12);
  ASSERT_NEAR(detail::whole_sign_start(AngleDeg { 359.999999 }).deg(), 330.0, 1e-12);
  ASSERT_NEAR(detail::whole_sign_start(AngleDeg { 360.0 }).deg(), 0.0, 1e-12);
}

TEST(House, PlacidusRootsHandleLongitudeSeam) {
  struct SeamCase {
    detail::PlacidusCusp cusp;
    double armc_deg;
  };
  const std::array cases {
    SeamCase { .cusp = detail::PlacidusCusp::TWO,     .armc_deg = 240.0 },
    SeamCase { .cusp = detail::PlacidusCusp::THREE,   .armc_deg = 210.0 },
    SeamCase { .cusp = detail::PlacidusCusp::ELEVEN,  .armc_deg = 330.0 },
    SeamCase { .cusp = detail::PlacidusCusp::TWELVE,  .armc_deg = 300.0 },
  };
  for (const auto& row : cases) {
    const auto cusp = detail::placidus_cusp(
      AngleDeg { row.armc_deg },
      AngleDeg { 0.0 },
      AngleDeg { 23.44 },
      row.cusp
    );
    ASSERT_NEAR(cusp.deg(), 0.0, 1e-12);
  }
}

TEST(House, RejectsInvalidInputs) {
  constexpr double jd_ut1 = 2451545.0;
  constexpr double jde_tt = 2451545.0007387605;
  const auto location = [](const double latitude_deg, const double longitude_deg) {
    return astro::GeoLocation {
      .latitude = AngleDeg { latitude_deg },
      .longitude = AngleDeg { longitude_deg },
    };
  };

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): exercises the invalid-enumerator contract.
  const auto invalid_system = static_cast<System>(255);
  EXPECT_THROW(
    static_cast<void>(calculate(nan, jde_tt, location(0.0, 0.0), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, infinity, location(0.0, 0.0), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(nan, 0.0), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, infinity), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, 180.000001), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, -180.000001), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, 0.0), invalid_system)),
    std::invalid_argument
  );

  for (const System system : { System::EQUAL, System::WHOLE_SIGN, System::PLACIDUS }) {
    EXPECT_THROW(
      static_cast<void>(calculate(jd_ut1, jde_tt, location(90.0, 0.0), system)),
      std::invalid_argument
    );
    EXPECT_THROW(
      static_cast<void>(calculate(jd_ut1, jde_tt, location(-90.0, 0.0), system)),
      std::invalid_argument
    );
  }
}

TEST(House, PlacidusUsesTrueObliquityDomain) {
  constexpr double jd_ut1 = 2451545.0;
  constexpr double jde_tt = 2451545.0007387605;
  const auto true_obliquity = astro::earth::obliquity::true_obliquity(jde_tt);
  const auto mean_obliquity = astro::earth::obliquity::mean(jde_tt);
  const double true_boundary_deg = 90.0 - true_obliquity.deg();
  const double between_boundaries_deg = 90.0 - ((true_obliquity.deg() + mean_obliquity.deg()) / 2.0);

  const auto at_boundary = astro::GeoLocation {
    .latitude = AngleDeg { true_boundary_deg },
    .longitude = AngleDeg { 0.0 },
  };
  const auto between_boundaries = astro::GeoLocation {
    .latitude = AngleDeg { between_boundaries_deg },
    .longitude = AngleDeg { 0.0 },
  };

  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, at_boundary, System::PLACIDUS)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(
      jd_ut1,
      jde_tt,
      astro::GeoLocation {
        .latitude = AngleDeg { true_boundary_deg + 0.01 },
        .longitude = AngleDeg { 0.0 },
      },
      System::PLACIDUS
    )),
    std::invalid_argument
  );
  ASSERT_NO_THROW(static_cast<void>(calculate(jd_ut1, jde_tt, between_boundaries, System::PLACIDUS)));
}

TEST(House, PlacidusReportsNonConvergence) {
  EXPECT_THROW(
    static_cast<void>(
      detail::placidus_cusp(
        AngleDeg { 75.0 },
        AngleDeg { 51.0 },
        AngleDeg { 23.44 },
        detail::PlacidusCusp::TWO,
        0
      )
    ),
    std::runtime_error
  );
}

} // namespace astro::house::test
