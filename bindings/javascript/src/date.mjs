/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import {
  MAX_CIVIL_YEAR,
  MILLISECONDS_PER_DAY,
  civilDateTime,
  civilDateTimeResult,
  finiteNumber,
  rangedInteger,
} from "./validation.mjs";

const MILLISECONDS_PER_MINUTE = 60_000;

export const dateToCivilAtOffset = (date, offsetMinutesEast) => {
  const timestamp = finiteNumber(Date.prototype.getTime.call(date), "date");
  const offset = rangedInteger(offsetMinutesEast, "offsetMinutesEast", -1439, 1439);
  const local = timestamp + offset * MILLISECONDS_PER_MINUTE;
  const carrier = new Date(local);
  const year = rangedInteger(carrier.getUTCFullYear(), "civil.year", 1, MAX_CIVIL_YEAR);
  const milliseconds = ((local % MILLISECONDS_PER_DAY) + MILLISECONDS_PER_DAY) % MILLISECONDS_PER_DAY;
  return civilDateTimeResult(
    {
      year,
      month: carrier.getUTCMonth() + 1,
      day: carrier.getUTCDate(),
      fraction: milliseconds / MILLISECONDS_PER_DAY,
    },
    milliseconds / 1000,
  );
};

export const civilAtOffsetToDate = (civil, offsetMinutesEast) => {
  const value = civilDateTime(civil, "civil");
  const offset = rangedInteger(offsetMinutesEast, "offsetMinutesEast", -1439, 1439);
  const carrier = new Date(0);
  // Date.UTC maps years 0-99 into 1900-1999; setUTCFullYear preserves the supplied year.
  carrier.setUTCFullYear(value.year, value.month - 1, value.day);
  const milliseconds = Math.round(value.fraction * MILLISECONDS_PER_DAY);
  if (milliseconds === MILLISECONDS_PER_DAY) {
    carrier.setUTCDate(carrier.getUTCDate() + 1);
  }
  rangedInteger(carrier.getUTCFullYear(), "rounded civil.year", 1, MAX_CIVIL_YEAR);
  return new Date(
    carrier.getTime() + milliseconds % MILLISECONDS_PER_DAY - offset * MILLISECONDS_PER_MINUTE,
  );
};

export const dateToCivilUtc = (date) => dateToCivilAtOffset(date, 0);
export const civilUtcToDate = (civil) => civilAtOffsetToDate(civil, 0);
