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

#include <stdio.h>
#include "celestial.h"

/* Print the current thread's error before another recording call clears it. */
static int32_t failure(void) {
  fputs(celestial_last_error(), stderr);
  fputc('\n', stderr);
  return 1;
}

/* The native date selects a UT1 window, not local midnight. Display each TT event separately. */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): The mask/bit pair selects this event's optional slot.
static int32_t display(const char *label, const uint32_t mask, const uint32_t bit, const double jde_tt) {
  if (!(mask & bit)) {
    printf("%s: absent\n", label);
    return 0;
  }
  const CelestialUTCTime utc = celestial_jde_to_utc(jde_tt);
  if (!utc.valid) {
    return failure();
  }
  printf("%s: %d-%02u-%02u + %.6f day UTC\n", label, utc.year, utc.month, utc.day, utc.fraction);
  return 0;
}

int main(void) {
  const CelestialSolarRiseSet sun = celestial_solar_rise_set(2025, 6, 21, 69.65, 18.96, -50.0 / 60.0);
  if (!sun.valid) {
    return failure();
  }
  printf("Sun polar topology: %u\n", sun.polar_code);
  if (display("sunrise", sun.present_fields, CELESTIAL_PRESENT_RISE, sun.rise_jde_tt)
      || display("solar transit", sun.present_fields, CELESTIAL_PRESENT_TRANSIT, sun.transit_jde_tt)
      || display("sunset", sun.present_fields, CELESTIAL_PRESENT_SET, sun.set_jde_tt)) {
    return 1;
  }

  const CelestialLunarRiseSet moon = celestial_lunar_rise_set(2025, 6, 21, 51.5, 0.0, CELESTIAL_REFRACTION_BENNETT, 15.0, 1013.25);
  if (!moon.valid) {
    return failure();
  }
  if (display("lunar transit", moon.present_fields, CELESTIAL_PRESENT_TRANSIT, moon.transit_jde_tt)) {
    return 1;
  }
  const CelestialPlanetRiseSet mars = celestial_planet_rise_set(CELESTIAL_TARGET_MARS, 2025, 6, 21, 51.5, 0.0, 50.0, -34.0 / 60.0);
  if (!mars.valid) {
    return failure();
  }
  return display("Mars rise", mars.present_fields, CELESTIAL_PRESENT_RISE, mars.rise_jde_tt);
}
