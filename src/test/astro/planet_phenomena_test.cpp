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
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "planet_phenomena.hpp"

// Retained material boundary: V06 identifies the JPL Horizons rows. They remain under their
// source terms and outside the project MIT grant.

namespace astro::planet::phenomena::test {

namespace {

struct GoldenRow {
  Planet planet;
  int32_t year;
  Kind kind;
  double jde_tt;
};

struct Window {
  Planet planet;
  int32_t year;
  std::size_t expected_count;
};

struct ApproximationRow {
  Planet planet;
  int32_t year;
  Kind kind;
  double jde_tt;
};

// JPL Horizons API v1.2, collected 2026-09-26 by `statistics/planet_horizons_crawler.py`.
// The complete 2025 census comes from geocentric TT scans of quantity 31 at independent one-day,
// six-hour, and three-hour meshes. All meshes produce the same counts and Kind order. Longitude
// crossings use apparent ecliptic-of-date coordinates; extrema use quantity-31 geometry, with
// quantities 23, 43, and 10 checking separation, true phase angle, and illuminated fraction.
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
constexpr std::array HORIZONS_ROWS {
  // Planet           Year  Kind                                  JDE(TT)
  GoldenRow { Planet::MERCURY, 2025, Kind::SUPERIOR_CONJUNCTION,           2460716.006423 },
  GoldenRow { Planet::MERCURY, 2025, Kind::GREATEST_EASTERN_ELONGATION,    2460742.757104 },
  GoldenRow { Planet::MERCURY, 2025, Kind::STATION_RETROGRADE,             2460749.782868 },
  GoldenRow { Planet::MERCURY, 2025, Kind::INFERIOR_CONJUNCTION,           2460759.326018 },
  GoldenRow { Planet::MERCURY, 2025, Kind::STATION_DIRECT,                 2460772.964472 },
  GoldenRow { Planet::MERCURY, 2025, Kind::GREATEST_WESTERN_ELONGATION,    2460787.285006 },
  GoldenRow { Planet::MERCURY, 2025, Kind::SUPERIOR_CONJUNCTION,           2460825.676322 },
  GoldenRow { Planet::MERCURY, 2025, Kind::GREATEST_EASTERN_ELONGATION,    2460860.694360 },
  GoldenRow { Planet::MERCURY, 2025, Kind::STATION_RETROGRADE,             2460874.698717 },
  GoldenRow { Planet::MERCURY, 2025, Kind::INFERIOR_CONJUNCTION,           2460888.487809 },
  GoldenRow { Planet::MERCURY, 2025, Kind::STATION_DIRECT,                 2460898.813188 },
  GoldenRow { Planet::MERCURY, 2025, Kind::GREATEST_WESTERN_ELONGATION,    2460906.909259 },
  GoldenRow { Planet::MERCURY, 2025, Kind::SUPERIOR_CONJUNCTION,           2460931.953458 },
  GoldenRow { Planet::MERCURY, 2025, Kind::GREATEST_EASTERN_ELONGATION,    2460978.418839 },
  GoldenRow { Planet::MERCURY, 2025, Kind::STATION_RETROGRADE,             2460989.293526 },
  GoldenRow { Planet::MERCURY, 2025, Kind::INFERIOR_CONJUNCTION,           2460999.891940 },
  GoldenRow { Planet::MERCURY, 2025, Kind::STATION_DIRECT,                 2461009.235991 },
  GoldenRow { Planet::MERCURY, 2025, Kind::GREATEST_WESTERN_ELONGATION,    2461017.378182 },
  GoldenRow { Planet::VENUS,   2025, Kind::GREATEST_EASTERN_ELONGATION,    2460685.710192 },
  GoldenRow { Planet::VENUS,   2025, Kind::STATION_RETROGRADE,             2460736.525850 },
  GoldenRow { Planet::VENUS,   2025, Kind::INFERIOR_CONJUNCTION,           2460757.547678 },
  GoldenRow { Planet::VENUS,   2025, Kind::STATION_DIRECT,                 2460778.544109 },
  GoldenRow { Planet::VENUS,   2025, Kind::GREATEST_WESTERN_ELONGATION,    2460827.645728 },
  GoldenRow { Planet::MARS,    2025, Kind::OPPOSITION,                     2460691.610932 },
  GoldenRow { Planet::MARS,    2025, Kind::STATION_DIRECT,                 2460730.584059 },
  GoldenRow { Planet::MARS,    2025, Kind::EASTERN_QUADRATURE,             2460786.566021 },
  GoldenRow { Planet::JUPITER, 2025, Kind::STATION_DIRECT,                 2460710.902419 },
  GoldenRow { Planet::JUPITER, 2025, Kind::EASTERN_QUADRATURE,             2460737.263733 },
  GoldenRow { Planet::JUPITER, 2025, Kind::CONJUNCTION,                    2460851.137716 },
  GoldenRow { Planet::JUPITER, 2025, Kind::WESTERN_QUADRATURE,             2460965.738963 },
  GoldenRow { Planet::JUPITER, 2025, Kind::STATION_RETROGRADE,             2460991.195993 },
  GoldenRow { Planet::SATURN,  2025, Kind::CONJUNCTION,                    2460746.937588 },
  GoldenRow { Planet::SATURN,  2025, Kind::WESTERN_QUADRATURE,             2460849.275579 },
  GoldenRow { Planet::SATURN,  2025, Kind::STATION_RETROGRADE,             2460869.670848 },
  GoldenRow { Planet::SATURN,  2025, Kind::OPPOSITION,                     2460939.740832 },
  GoldenRow { Planet::SATURN,  2025, Kind::STATION_DIRECT,                 2461007.662854 },
  GoldenRow { Planet::SATURN,  2025, Kind::EASTERN_QUADRATURE,             2461026.690773 },
  GoldenRow { Planet::URANUS,  2025, Kind::STATION_DIRECT,                 2460706.182786 },
  GoldenRow { Planet::URANUS,  2025, Kind::EASTERN_QUADRATURE,             2460718.313434 },
  GoldenRow { Planet::URANUS,  2025, Kind::CONJUNCTION,                    2460813.481522 },
  GoldenRow { Planet::URANUS,  2025, Kind::WESTERN_QUADRATURE,             2460911.802732 },
  GoldenRow { Planet::URANUS,  2025, Kind::STATION_RETROGRADE,             2460924.703226 },
  GoldenRow { Planet::URANUS,  2025, Kind::OPPOSITION,                     2461001.018453 },
  GoldenRow { Planet::NEPTUNE, 2025, Kind::CONJUNCTION,                    2460754.476545 },
  GoldenRow { Planet::NEPTUNE, 2025, Kind::WESTERN_QUADRATURE,             2460849.854366 },
  GoldenRow { Planet::NEPTUNE, 2025, Kind::STATION_RETROGRADE,             2460861.399610 },
  GoldenRow { Planet::NEPTUNE, 2025, Kind::OPPOSITION,                     2460942.038016 },
  GoldenRow { Planet::NEPTUNE, 2025, Kind::STATION_DIRECT,                 2461020.016952 },
  GoldenRow { Planet::NEPTUNE, 2025, Kind::EASTERN_QUADRATURE,             2461030.543835 },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

constexpr std::array WINDOWS {
  Window { .planet = Planet::MERCURY, .year = 2025, .expected_count = 18 },
  Window { .planet = Planet::VENUS,   .year = 2025, .expected_count =  5 },
  Window { .planet = Planet::MARS,    .year = 2025, .expected_count =  3 },
  Window { .planet = Planet::JUPITER, .year = 2025, .expected_count =  5 },
  Window { .planet = Planet::SATURN,  .year = 2025, .expected_count =  6 },
  Window { .planet = Planet::URANUS,  .year = 2025, .expected_count =  6 },
  Window { .planet = Planet::NEPTUNE, .year = 2025, .expected_count =  6 },
};

// Measured 2025 maxima (library-vs-Horizons event epoch / 6h-vs-3h source mesh), in days:
// Mercury .000376/.000567, Venus .000132/.000288, Mars .000031/.000048,
// Jupiter .001297/.000087, Saturn .001497/.000066, Uranus .000635/.000105,
// Neptune .001131/.000360. Tolerances are about 3x the larger value, rounded outward.
// Near-Sun conjunction maxima are smaller: .000035 day for Mercury and .000002 for Venus.
constexpr std::array EVENT_TOLERANCES_DAYS {
  0.0020,
  0.0010,
  0.0002,
  0.0040,
  0.0045,
  0.0020,
  0.0035,
};

// PyMeeus 0.5.12, collected 2026-09-26 from the named event methods with a nearby Epoch.
// This Meeus-derived approximation layer does not provide quadratures.
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense reference rows read by column.
constexpr std::array PYMEEUS_ROWS {
  ApproximationRow { Planet::MERCURY, 2025, Kind::SUPERIOR_CONJUNCTION,           2460715.998770695 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::GREATEST_EASTERN_ELONGATION,    2460742.750095003 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::STATION_RETROGRADE,             2460749.792523804 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::INFERIOR_CONJUNCTION,           2460759.325354451 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::STATION_DIRECT,                 2460772.934269383 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::GREATEST_WESTERN_ELONGATION,    2460787.284430798 },
  ApproximationRow { Planet::MARS,    2025, Kind::OPPOSITION,                     2460691.554210103 },
  ApproximationRow { Planet::MARS,    2026, Kind::CONJUNCTION,                    2461049.915897439 },
};

// AA+ v2.55 `CAAPlanetaryPhenomena2::Calculate`, high-precision VSOP87, collected 2026-09-26.
// Source archive: web.archive.org snapshot 20240702184835, SHA-256
// 7a2d026d131e30d4b02e7c5efd41fce1bc84eda60f599a22abdef2f6439907de. Greatest-elongation
// directions are reclassified by wrapped planet-Sun longitude because v2.55 compares raw longitudes.
constexpr std::array AAPLUS_ROWS {
  ApproximationRow { Planet::MERCURY, 2025, Kind::SUPERIOR_CONJUNCTION,           2460715.999392680 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::GREATEST_EASTERN_ELONGATION,    2460742.757156140 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::STATION_RETROGRADE,             2460749.782817850 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::INFERIOR_CONJUNCTION,           2460759.332019020 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::STATION_DIRECT,                 2460772.964434800 },
  ApproximationRow { Planet::MERCURY, 2025, Kind::GREATEST_WESTERN_ELONGATION,    2460787.284990720 },
  ApproximationRow { Planet::MARS,    2025, Kind::OPPOSITION,                     2460691.610720970 },
  ApproximationRow { Planet::MARS,    2025, Kind::EASTERN_QUADRATURE,             2460786.576245680 },
  ApproximationRow { Planet::JUPITER, 2025, Kind::CONJUNCTION,                    2460851.147514530 },
  ApproximationRow { Planet::JUPITER, 2025, Kind::WESTERN_QUADRATURE,             2460965.745281630 },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense reference rows read by column.

[[nodiscard]] auto expected_for(const Window& window) -> std::vector<GoldenRow> {
  std::vector<GoldenRow> result;
  for (const auto& row : HORIZONS_ROWS) {
    if (row.planet == window.planet and row.year == window.year) {
      result.push_back(row);
    }
  }
  return result;
}

[[nodiscard]] auto closest(
  const std::vector<Event>& actual,
  const Kind kind,
  const double jde_tt
) -> std::vector<Event>::const_iterator {
  auto best = actual.end();
  double best_distance = std::numeric_limits<double>::infinity();
  for (auto candidate = actual.begin(); candidate != actual.end(); ++candidate) {
    const double distance = std::fabs(candidate->jde_tt - jde_tt);
    if (candidate->kind == kind and distance < best_distance) {
      best = candidate;
      best_distance = distance;
    }
  }
  return best;
}

} // namespace

TEST(PlanetPhenomena, HorizonsCompleteWindows) {
  for (const auto& window : WINDOWS) {
    const auto expected = expected_for(window);
    const auto actual = events(window.planet, window.year);
    const double tolerance = EVENT_TOLERANCES_DAYS.at(static_cast<std::size_t>(window.planet));
    ASSERT_EQ(expected.size(), window.expected_count);
    ASSERT_EQ(actual.size(), expected.size()) << static_cast<int>(window.planet);
    for (std::size_t index = 0; index < expected.size(); ++index) {
      ASSERT_EQ(actual.at(index).kind, expected.at(index).kind)
        << static_cast<int>(window.planet) << ", index " << index;
      ASSERT_NEAR(actual.at(index).jde_tt, expected.at(index).jde_tt, tolerance)
        << static_cast<int>(window.planet) << ", index " << index;
      if (index > 0) {
        ASSERT_GT(actual.at(index).jde_tt, actual.at(index - 1).jde_tt);
      }
    }
  }
}

TEST(PlanetPhenomena, EventDefinitionsHoldAfterRefinement) {
  const auto mercury = events(Planet::MERCURY, 2025);
  for (const auto& event : mercury) {
    const auto geometry = astro::planet::observation::geometry(Planet::MERCURY, event.jde_tt);
    switch (event.kind) {
      case Kind::INFERIOR_CONJUNCTION:
        ASSERT_GE(geometry.phase_angle.deg(), 90.0);
        break;
      case Kind::SUPERIOR_CONJUNCTION:
        ASSERT_LT(geometry.phase_angle.deg(), 90.0);
        break;
      case Kind::GREATEST_EASTERN_ELONGATION:
      case Kind::GREATEST_WESTERN_ELONGATION:
        ASSERT_GT(
          geometry.angular_separation.deg(),
          astro::planet::observation::geometry(Planet::MERCURY, event.jde_tt - 0.01)
            .angular_separation.deg()
        );
        ASSERT_GT(
          geometry.angular_separation.deg(),
          astro::planet::observation::geometry(Planet::MERCURY, event.jde_tt + 0.01)
            .angular_separation.deg()
        );
        break;
      case Kind::STATION_RETROGRADE:
        ASSERT_FALSE(astro::planet::geocentric_coord::is_retrograde(Planet::MERCURY, event.jde_tt - 0.1));
        ASSERT_TRUE(astro::planet::geocentric_coord::is_retrograde(Planet::MERCURY, event.jde_tt + 0.1));
        break;
      case Kind::STATION_DIRECT:
        ASSERT_TRUE(astro::planet::geocentric_coord::is_retrograde(Planet::MERCURY, event.jde_tt - 0.1));
        ASSERT_FALSE(astro::planet::geocentric_coord::is_retrograde(Planet::MERCURY, event.jde_tt + 0.1));
        break;
      default:
        FAIL() << "inner planet returned an inapplicable event kind";
    }
  }
}

TEST(PlanetPhenomena, MeeusDerivedApproximateEpochs) {
  const auto check = [](const auto& rows, const double tolerance_days) {
    Planet previous_planet = Planet::MERCURY;
    int32_t previous_year = 0;
    bool loaded = false;
    std::vector<Event> actual;
    for (const auto& row : rows) {
      if (not loaded or row.planet != previous_planet or row.year != previous_year) {
        actual = events(row.planet, row.year);
        previous_planet = row.planet;
        previous_year = row.year;
        loaded = true;
      }
      const auto match = closest(actual, row.kind, row.jde_tt);
      ASSERT_NE(match, actual.end());
      ASSERT_NEAR(match->jde_tt, row.jde_tt, tolerance_days)
        << static_cast<int>(row.planet) << ", Kind " << static_cast<int>(row.kind);
    }
  };

  // These approximations diagnose event-family wiring and order, not Horizons-scale accuracy.
  check(PYMEEUS_ROWS, 0.1);
  check(AAPLUS_ROWS, 0.02);
}

TEST(PlanetPhenomena, LongitudeConjunctionIsNotMinimumSeparation) {
  const auto venus = events(Planet::VENUS, 2025);
  const auto conjunction = std::ranges::find(venus, Kind::INFERIOR_CONJUNCTION, &Event::kind);
  ASSERT_NE(conjunction, venus.end());

  // Horizons' spherical minimum follows the equal-longitude event by about 5.66 hours.
  constexpr double minimum_separation_jde_tt = 2460757.78343;
  ASSERT_GT(minimum_separation_jde_tt - conjunction->jde_tt, 0.2);
  ASSERT_LT(
    astro::planet::observation::geometry(Planet::VENUS, minimum_separation_jde_tt)
      .angular_separation.deg(),
    astro::planet::observation::geometry(Planet::VENUS, conjunction->jde_tt)
      .angular_separation.deg()
  );
}

TEST(PlanetPhenomena, StationAcrossLongitudeWrap) {
  const auto neptune = events(Planet::NEPTUNE, 2025);
  const auto station = std::ranges::find(neptune, Kind::STATION_DIRECT, &Event::kind);
  ASSERT_NE(station, neptune.end());
  const double longitude = astro::planet::geocentric_coord::apparent(Planet::NEPTUNE, station->jde_tt).λ.deg();
  ASSERT_GT(longitude, 350.0);
  ASSERT_TRUE(astro::planet::geocentric_coord::is_retrograde(Planet::NEPTUNE, station->jde_tt - 0.1));
  ASSERT_FALSE(astro::planet::geocentric_coord::is_retrograde(Planet::NEPTUNE, station->jde_tt + 0.1));
}

TEST(PlanetPhenomena, HalfOpenOwnership) {
  constexpr double start_jde_tt = 100.0;
  constexpr double end_jde_tt = 200.0;
  EXPECT_FALSE(detail::in_year(std::nextafter(start_jde_tt, 0.0), start_jde_tt, end_jde_tt));
  EXPECT_TRUE(detail::in_year(start_jde_tt, start_jde_tt, end_jde_tt));
  EXPECT_TRUE(detail::in_year(std::nextafter(end_jde_tt, 0.0), start_jde_tt, end_jde_tt));
  EXPECT_FALSE(detail::in_year(end_jde_tt, start_jde_tt, end_jde_tt));
}

TEST(PlanetPhenomena, RefinerReportsNonConvergence) {
  const auto grid = detail::samples(
    Planet::MERCURY,
    {.start_jde_tt = 2460758.5, .end_jde_tt = 2460760.0, .step_days = 0.5}
  );
  bool exercised = false;
  for (std::size_t index = 1; index < grid.size(); ++index) {
    const auto& left = grid.at(index - 1);
    const auto& right = grid.at(index);
    if ((left.difference < 0.0) != (right.difference < 0.0)) {
      EXPECT_THROW(
        static_cast<void>(detail::refine_crossing(
          Planet::MERCURY,
          left,
          right,
          {.target_difference = 0.0, .max_iterations = 0}
        )),
        std::runtime_error
      );
      exercised = true;
      break;
    }
  }
  ASSERT_TRUE(exercised);
}

TEST(PlanetPhenomena, NearTangentCrossingsAreSubdivided) {
  // This extrapolated year has two conjunctions inside one coarse day (#60).
  const auto saturn = events(Planet::SATURN, 29841);
  ASSERT_EQ(std::ranges::count(saturn, Kind::CONJUNCTION, &Event::kind), 3);
  ASSERT_EQ(saturn.size(), 5);
}

TEST(PlanetPhenomena, AdjacentWindowsShareExtremumOwnership) {
  // Move a shared boundary through a 2025 Jupiter station (#60).
  constexpr double boundary_jde_tt = 2460710.903609788;
  const auto earlier = detail::search(Planet::JUPITER, boundary_jde_tt - 10.37, boundary_jde_tt, 1.0);
  const auto later = detail::search(Planet::JUPITER, boundary_jde_tt, boundary_jde_tt + 10.0, 1.0);
  const auto owns_station = [](const std::vector<Event>& window) {
    return std::ranges::count(window, Kind::STATION_DIRECT, &Event::kind);
  };
  ASSERT_EQ(owns_station(earlier) + owns_station(later), 1);
}

TEST(PlanetPhenomena, OneDayScanMatchesHalfDayAcrossComputationalDomain) {
  constexpr std::array planets {
    Planet::MERCURY,
    Planet::VENUS,
    Planet::MARS,
    Planet::JUPITER,
    Planet::SATURN,
    Planet::URANUS,
    Planet::NEPTUNE,
  };
  constexpr std::array<int32_t, 7> years { 1, 4096, 8192, 16384, 24576, 29841, 32766 };

  for (const int32_t year : years) {
    const calendar::Datetime start_utc { util::to_ymd(year, 1, 1), 0.0 };
    const calendar::Datetime end_utc { util::to_ymd(year + 1, 1, 1), 0.0 };
    const double start_jde_tt = astro::julian_day::utc_to_jde(start_utc);
    const double end_jde_tt = astro::julian_day::utc_to_jde(end_utc);
    for (const Planet planet : planets) {
      const auto one_day = events(planet, year);
      const auto half_day = detail::search(planet, start_jde_tt, end_jde_tt, 0.5);
      ASSERT_FALSE(one_day.empty()) << year << ", planet " << static_cast<int>(planet);
      ASSERT_EQ(one_day.size(), half_day.size()) << year << ", planet " << static_cast<int>(planet);
      for (std::size_t index = 0; index < one_day.size(); ++index) {
        ASSERT_EQ(one_day.at(index).kind, half_day.at(index).kind)
          << year << ", planet " << static_cast<int>(planet) << ", index " << index;
        // The measured maximum is 0.001887 day; 0.006 day is a 3.2x outward envelope for
        // identifying the same extrapolated-model event, not a remote-date accuracy claim.
        ASSERT_NEAR(one_day.at(index).jde_tt, half_day.at(index).jde_tt, 0.006)
          << year << ", planet " << static_cast<int>(planet) << ", index " << index;
      }
    }
  }
}

TEST(PlanetPhenomena, RejectsInvalidInput) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) - Exercises the invalid-enumerator contract.
  const auto invalid_planet = static_cast<Planet>(255);
  EXPECT_THROW(static_cast<void>(events(Planet::MERCURY, 0)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(events(Planet::MERCURY, 32767)), std::invalid_argument);
  EXPECT_THROW(
    static_cast<void>(events(Planet::MERCURY, std::numeric_limits<int32_t>::max())),
    std::invalid_argument
  );
  EXPECT_THROW(static_cast<void>(events(invalid_planet, 2025)), std::invalid_argument);
}

} // namespace astro::planet::phenomena::test
