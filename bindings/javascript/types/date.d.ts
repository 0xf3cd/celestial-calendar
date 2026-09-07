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

import type { CivilDateTime, CivilDateTimeResult } from "@0xf3cd/celestial";

/**
 * Render a Date as UTC civil fields, exactly at its millisecond resolution; no init() or WASM.
 * This is a calendar bridge, not a UT1/TT conversion.
 * @throws {TypeError} If date is not a Date.
 * @throws {RangeError} If date is invalid or its UTC civil year is outside [1, 32767].
 */
export function dateToCivilUtc(date: Date): CivilDateTimeResult;

/**
 * Convert UTC civil fields to a Date; fraction is the time-of-day input, not hour/minute/second.
 * Round to the nearest millisecond, with exact half milliseconds toward the next instant and day carry.
 * A same-UTC Date -> civil -> Date round trip preserves getTime() exactly for in-domain civil years.
 * No init() or WASM; this does not convert UT1 or TT to UTC.
 * @throws {TypeError} For wrong types, missing own fields, isLeap, or non-integer year/month/day.
 * @throws {RangeError} For invalid dates, non-finite/unsafe fields, fraction outside [0, 1),
 * or an input or rounded/carry-adjusted civil year outside [1, 32767].
 */
export function civilUtcToDate(civil: CivilDateTime): Date;

/**
 * Render a Date at a fixed offset, exactly at its millisecond resolution; no init() or WASM.
 * offsetMinutesEast is a safe integer in [-1439, 1439], positive east of UTC; no IANA zone or DST rules.
 * The resulting local civil year must be in [1, 32767]; a UTC carrier year of 0 or 32768 is allowed.
 * @throws {TypeError} If date is not a Date or the offset is not an integer number.
 * @throws {RangeError} If date is invalid, the offset is non-finite/unsafe/out of range,
 * or the resulting local civil year is outside [1, 32767].
 */
export function dateToCivilAtOffset(date: Date, offsetMinutesEast: number): CivilDateTimeResult;

/**
 * Convert fixed-offset civil fields to a Date; fraction is the time-of-day input, not hour/minute/second.
 * offsetMinutesEast is a safe integer in [-1439, 1439], positive east of UTC; no IANA zone or DST rules.
 * Round locally to the nearest millisecond, with exact half milliseconds toward the next instant and day carry.
 * Input and rounded local years must be in [1, 32767]; the returned UTC carrier may have year 0 or 32768.
 * A same-offset Date -> civil -> Date round trip preserves getTime() exactly for in-domain local civil years.
 * No init() or WASM; this does not convert UT1 or TT to UTC.
 * @throws {TypeError} For wrong types, missing own fields, isLeap, or non-integer date/offset fields.
 * @throws {RangeError} For invalid dates, non-finite/unsafe fields, fraction outside [0, 1),
 * an out-of-range offset, or an input or rounded/carry-adjusted local year outside [1, 32767].
 */
export function civilAtOffsetToDate(civil: CivilDateTime, offsetMinutesEast: number): Date;
