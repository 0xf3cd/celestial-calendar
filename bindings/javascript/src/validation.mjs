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

export const MAX_CIVIL_YEAR = 32_767;
export const MILLISECONDS_PER_DAY = 86_400_000;
const SECONDS_PER_DAY = 86_400;

export const finiteNumber = (value, name) => {
  if (typeof value !== "number") throw new TypeError(`${name} must be a number.`);
  if (!Number.isFinite(value)) throw new RangeError(`${name} must be finite.`);
  return value;
};

const integer = (value, name) => {
  finiteNumber(value, name);
  if (!Number.isInteger(value)) throw new TypeError(`${name} must be an integer.`);
  if (!Number.isSafeInteger(value)) throw new RangeError(`${name} must be a safe integer.`);
  return value;
};

export const rangedInteger = (value, name, minimum, maximum) => {
  integer(value, name);
  if (value < minimum || value > maximum) {
    throw new RangeError(`${name} must be in [${minimum}, ${maximum}].`);
  }
  return value;
};

export const rangedNumber = (value, name, minimum, maximum, includeMaximum = true) => {
  finiteNumber(value, name);
  if (value < minimum || (includeMaximum ? value > maximum : value >= maximum)) {
    const closing = includeMaximum ? "]" : ")";
    throw new RangeError(`${name} must be in [${minimum}, ${maximum}${closing}.`);
  }
  return value;
};

export const requiredRecord = (value, name, fields, excluded = []) => {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object.`);
  }
  if (fields.some((field) => !Object.hasOwn(value, field))) {
    throw new TypeError(`${name} must contain: ${fields.join(", ")}.`);
  }
  if (excluded.some((field) => field in value)) {
    throw new TypeError(`${name} must not contain: ${excluded.join(", ")}.`);
  }
  return value;
};

const gregorianMonthLength = (year, month) => {
  if (month === 2) {
    const leap = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
    return leap ? 29 : 28;
  }
  return [4, 6, 9, 11].includes(month) ? 30 : 31;
};

const civilDate = (value, name) => {
  const year = rangedInteger(value.year, `${name}.year`, 1, MAX_CIVIL_YEAR);
  const month = rangedInteger(value.month, `${name}.month`, 1, 12);
  const day = rangedInteger(value.day, `${name}.day`, 1, gregorianMonthLength(year, month));
  return { year, month, day };
};

export const gregorianDate = (value, name) => {
  requiredRecord(value, name, ["year", "month", "day"], ["fraction", "isLeap"]);
  return civilDate(value, name);
};

export const civilDateTime = (value, name) => {
  requiredRecord(value, name, ["year", "month", "day", "fraction"], ["isLeap"]);
  const date = civilDate(value, name);
  const fraction = rangedNumber(value.fraction, `${name}.fraction`, 0, 1, false);
  return { ...date, fraction };
};

// Date callers supply their exact millisecond-derived seconds; native callers retain the day fraction.
export const civilDateTimeResult = (
  { year, month, day, fraction },
  secondsOfDay = fraction * SECONDS_PER_DAY,
) => {
  const hour = Math.floor(secondsOfDay / 3600);
  const minute = Math.floor((secondsOfDay - hour * 3600) / 60);
  const second = secondsOfDay - hour * 3600 - minute * 60;
  return { year, month, day, fraction, hour, minute, second };
};

export const enumValue = (value, name, values) => {
  if (typeof value !== "string") throw new TypeError(`${name} must be a string.`);
  if (!Object.hasOwn(values, value)) {
    throw new RangeError(`${name} must be one of: ${Object.keys(values).join(", ")}.`);
  }
  return values[value];
};

export const booleanValue = (value, name) => {
  if (typeof value !== "boolean") throw new TypeError(`${name} must be a boolean.`);
  return value;
};
