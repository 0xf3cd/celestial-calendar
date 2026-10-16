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

namespace astro::house::test {

// #321 UTC witness: at 2026-01-01 12:00:05 UTC the ecliptic and the horizon coincide to round-off
// at the northern site and at its antipode (negated latitude), so the Ascendant is unresolved.
// The sites depend on Algo 5's UT1; rederive them after a refit.
inline constexpr double WITNESS_LATITUDE_DEG = 66.561856230963841;
inline constexpr double WITNESS_NORTH_LONGITUDE_DEG = -11.176256364511062;
inline constexpr double WITNESS_SOUTH_LONGITUDE_DEG = 168.82374363548894;

} // namespace astro::house::test
