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

#include <algorithm>

#include "lib.hpp"
#include "celestial.h"

#include "astro.hpp"
#include "util.hpp"
#include "datetime.hpp"


extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

#pragma region Julian Days

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d/fraction is the published civil-date order.
auto ut1_to_jd(const int32_t y, const uint32_t m, const uint32_t d, const double fraction) -> JulianDay {
  return lib::wrap_export("ut1_to_jd", [=]() -> JulianDay {
    const auto ymd = util::to_ymd(y, m, d);
    const auto ut1_dt = calendar::Datetime(ymd, fraction);
    const auto jd = astro::julian_day::ut1_to_jd(ut1_dt);

    return {
      .valid = true,
      .value = jd,
    };
  });
}


// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d/fraction is the published civil-date order.
auto ut1_to_jde(const int32_t y, const uint32_t m, const uint32_t d, const double fraction) -> JulianDay {
  return lib::wrap_export("ut1_to_jde", [=]() -> JulianDay {
    const auto ymd = util::to_ymd(y, m, d);
    const auto ut1_dt = calendar::Datetime(ymd, fraction);
    const auto jde = astro::julian_day::ut1_to_jde(ut1_dt);

    return {
      .valid = true,
      .value = jde,
    };
  });
}


auto jde_to_ut1(const double jde) -> UT1Time {
  return lib::wrap_export("jde_to_ut1", [=]() -> UT1Time {
    const auto ut1_dt = astro::julian_day::jde_to_ut1(jde);

    const auto [y, m, d] = util::from_ymd(ut1_dt.ymd);
    const double fraction = ut1_dt.fraction();

    return {
      .valid      = true,
      .year       = y,
      .month      = m,
      .day        = d,
      .fraction   = fraction,
    };
  });
}


#pragma endregion


#pragma region Solar Apparent Geocentric Position

auto sun_apparent_geocentric_coord(const double jde) -> SunCoordinate {
  return lib::wrap_export("sun_apparent_geocentric_coord", [=]() -> SunCoordinate {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, got {}", jde)
      };
    }

    const auto coord = astro::sun::geocentric_coord::apparent(jde);

    return {
      .valid = true,
      .lon   = coord.λ.deg(),
      .lat   = coord.β.deg(),
      .r     = coord.r.au(),
    };
  });
}

#pragma endregion


#pragma region Moon Apparent Geocentric Position

auto moon_apparent_geocentric_coord(const double jde) -> MoonCoordinate {
  return lib::wrap_export("moon_apparent_geocentric_coord", [=]() -> MoonCoordinate {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, got {}", jde)
      };
    }

    const auto coord = astro::moon::geocentric_coord::apparent(jde);

    return {
      .valid = true,
      .lon   = coord.λ.deg(),
      .lat   = coord.β.deg(),
      .r     = coord.r.km(),
    };
  });
}

#pragma endregion


#pragma region Moon Illumination

auto moon_illumination(const double jde) -> MoonIllumination {
  return lib::wrap_export("moon_illumination", [=]() -> MoonIllumination {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, got {}", jde)
      };
    }

    // Both fields come from the same position pair — evaluate VSOP87D/ELP2000-82B once.
    const auto sun_pos = astro::sun::geocentric_coord::apparent(jde);
    const auto moon_pos = astro::moon::geocentric_coord::apparent(jde);

    return {
      .valid          = true,
      .illumination   = astro::moon_phase::illumination::fraction(
        astro::moon_phase::illumination::phase_angle(sun_pos, moon_pos)
      ),
      .elongation_deg = (moon_pos.λ - sun_pos.λ).normalize().deg(),
    };
  });
}

#pragma endregion


#pragma region Moon Position Angle

auto moon_position_angle(const double jde) -> MoonPositionAngle {
  return lib::wrap_export("moon_position_angle", [=]() -> MoonPositionAngle {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, got {}", jde)
      };
    }

    const auto angle = astro::moon_phase::illumination::position_angle(jde);

    return {
      .valid    = true,
      .angle_deg = angle.deg(),
    };
  });
}

#pragma endregion


#pragma region Moon Phase Moments

auto moon_phase_moments(
  const int32_t year, // NOLINT(bugprone-easily-swappable-parameters) -- year/phase is the published query order.
  const uint8_t phase_kind,
  uint32_t * const root_count,
  double * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("moon_phase_moments", [=] {
    if (root_count == nullptr) {
      throw std::invalid_argument { "Argument `root_count` is null." };
    }

    // Deterministic out-parameter state: every failure path leaves *root_count == 0.
    *root_count = 0;

    if (slots == nullptr and slot_count > 0) {
      throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
    }

    if (phase_kind > 3) {
      throw std::invalid_argument {
        std::format("Argument `phase_kind` must be in [0, 3], got {}", phase_kind)
      };
    }

    const auto kind = static_cast<astro::moon_phase::phase_moments::PhaseKind>(phase_kind);
    const auto roots = astro::moon_phase::phase_moments::moments(year, kind);

    *root_count = static_cast<uint32_t>(roots.size());

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(cbegin(roots), cbegin(roots) + num_written, slots);

    return num_written;
  });
}

#pragma endregion


#pragma region Solar Longitude Roots

auto solar_lon_root_discriminant(const int32_t year, const double longitude) -> Discriminant {
  return lib::wrap_export("solar_lon_root_discriminant", [=]() -> Discriminant {
    if (not std::isfinite(longitude)) {
      throw std::invalid_argument {
        std::format("Argument `longitude` is not finite, got {}", longitude)
      };
    }

    return {
      .valid = true,
      .count = astro::sun::geocentric_coord::math::discriminant(year, longitude),
    };
  });
}


auto solar_lon_roots(
  const int32_t year, 
  const double longitude, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("solar_lon_roots", [=] {
    using namespace astro::sun::geocentric_coord::math;

    if (slots == nullptr and slot_count > 0) {
      throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
    }

    if (not std::isfinite(longitude)) {
      throw std::invalid_argument {
        std::format("Argument `longitude` is not finite, got {}", longitude)
      };
    }

    auto roots = find_roots(year, longitude);

    // Some sanity check...
    const auto root_count = discriminant(year, longitude);
    if (roots.size() != root_count) [[unlikely]] {
      throw std::runtime_error {
        std::format("Root count mismatch: found {}, expected {}", roots.size(), root_count)
      };
    }

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(cbegin(roots), cbegin(roots) + num_written, slots);

    return num_written;
  });
}

#pragma endregion


#pragma region Sun Moon Conjunction

auto new_moons_after_jde(
  const double jde, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("new_moons_after_jde", [=] {
    if (slots == nullptr and slot_count > 0) {
      throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
    }

    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, got {}", jde)
      };
    }

    std::vector<double> roots;
    roots.reserve(slot_count);

    astro::moon_phase::new_moon::RootGenerator gen(jde);
    std::generate_n(std::back_inserter(roots), slot_count, [&] { return gen.next(); });

    std::ranges::copy(roots, slots);
    return static_cast<uint32_t>(slot_count);
  });
}


auto new_moons_in_year(
  const int32_t year, 
  uint32_t * const root_count,
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export("new_moons_in_year", [=] {
    if (root_count == nullptr) {
      throw std::invalid_argument { "Argument `root_count` is null." };
    }

    // Deterministic out-parameter state: every failure path leaves *root_count == 0.
    *root_count = 0;

    if (slots == nullptr and slot_count > 0) {
      throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
    }

    const auto roots = astro::moon_phase::new_moon::moments(year);

    *root_count = static_cast<uint32_t>(roots.size());

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(cbegin(roots), cbegin(roots) + num_written, slots);

    return num_written;
  });
}

#pragma endregion


#pragma region Solar Time

auto equation_of_time(const double jde) -> EquationOfTime {
  return lib::wrap_export("equation_of_time", [=]() -> EquationOfTime {
    if (not std::isfinite(jde)) {
      throw std::invalid_argument {
        std::format("Argument `jde` is not finite, got {}", jde)
      };
    }

    return {
      .valid = true,
      .value = astro::solar_time::equation_of_time(jde).deg(),
    };
  });
}


auto apparent_solar_time(
  const int32_t y,
  const uint32_t m,
  const uint32_t d, // NOLINT(bugprone-easily-swappable-parameters) -- civil date then longitude is the published API order.
  const double fraction,
  const double longitude
) -> ApparentSolarTime {
  return lib::wrap_export("apparent_solar_time", [=]() -> ApparentSolarTime {
    const auto ymd = util::to_ymd(y, m, d);
    const auto utc_dt = calendar::Datetime(ymd, fraction);
    const auto lon = astro::toolbox::AngleDeg { longitude };
    const auto apparent_dt = astro::solar_time::apparent(utc_dt, lon);

    const auto [ay, am, ad] = util::from_ymd(apparent_dt.ymd);

    return {
      .valid    = true,
      .year     = ay,
      .month    = am,
      .day      = ad,
      .fraction = apparent_dt.fraction(),
    };
  });
}

#pragma endregion


#pragma region Sidereal Time

auto local_apparent_sidereal_time(const double jd_ut1, const double longitude) -> SiderealTime {
  return lib::wrap_export("local_apparent_sidereal_time", [=]() -> SiderealTime {
    if (not std::isfinite(jd_ut1)) {
      throw std::invalid_argument {
        std::format("Argument `jd_ut1` is not finite, got {}", jd_ut1)
      };
    }
    if (not std::isfinite(longitude) or longitude < -180.0 or longitude > 180.0) {
      throw std::invalid_argument {
        std::format("Argument `longitude` out of range [-180, 180], got {}", longitude)
      };
    }

    // Nutation needs the instant on TT. jd_to_ut1 guards the floor (401); the ceiling is
    // declared as 32766 — late 32767 would survive jd_to_ut1 only to have the ΔT shift
    // (~34.8 days at that era) push the TT date past the representable years.
    const auto ut1_dt = astro::julian_day::jd_to_ut1(jd_ut1);
    const auto year = static_cast<int32_t>(ut1_dt.ymd.year());
    if (year > 32766) {
      throw std::out_of_range {
        std::format("Year {} is outside the declared domain [401, 32766].", year)
      };
    }
    const double jde_tt = astro::julian_day::ut1_to_jde(ut1_dt);

    // The boundary speaks east-positive (like `apparent_solar_time`); the core's
    // `local_apparent` takes west-positive, hence the negation (#127/D2).
    const auto last = astro::sidereal::local_apparent(jd_ut1, jde_tt, astro::toolbox::AngleDeg { -longitude });

    return {
      .valid = true,
      .value = last.deg(),
    };
  });
}

#pragma endregion

}
