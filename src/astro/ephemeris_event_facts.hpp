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

namespace astro::ephemeris {

/** @brief Maximum nonempty event-query span in TT days, independent of the search kernel. */
// About ten years per request; an admission limit, independent of evaluation/candidate exhaustion.
inline constexpr double EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS = 3660.0;

} // namespace astro::ephemeris
