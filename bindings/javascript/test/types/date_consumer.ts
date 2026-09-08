/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import {
  civilAtOffsetToDate,
  civilUtcToDate,
  dateToCivilAtOffset,
  dateToCivilUtc,
} from "@0xf3cd/celestial/date";
import type { CivilDateTime, CivilDateTimeResult, GregorianDate, LunarDate } from "@0xf3cd/celestial";
// @ts-expect-error the date subpath has no types registry of its own
import type { CivilDateTime as DateCivilDateTime } from "@0xf3cd/celestial/date";
// @ts-expect-error no IANA-zone bridge
import { dateToCivilInZone } from "@0xf3cd/celestial/date";
// @ts-expect-error no implicit UTC-to-TT bridge
import { jdeFromDate } from "@0xf3cd/celestial/date";
// @ts-expect-error the pure bridge needs no initialization API
import { init } from "@0xf3cd/celestial/date";

const date = new Date(-1);
const utc = dateToCivilUtc(date);
const eastEight = dateToCivilAtOffset(date, 480);
const results: CivilDateTimeResult[] = [utc, eastEight];
for (const result of results) {
  const input: CivilDateTime = result;
  const clock: [number, number, number] = [result.hour, result.minute, result.second];
}
const utcDate: Date = civilUtcToDate(utc);
const offsetDate: Date = civilAtOffsetToDate(eastEight, 480);
const civil: CivilDateTime = { year: 2024, month: 1, day: 1, fraction: 0.5 };
const gregorian: GregorianDate = { year: 2024, month: 1, day: 1 };
const lunar: LunarDate = { year: 2024, month: 1, day: 1, isLeap: false };
civilUtcToDate(civil);
civilAtOffsetToDate(civil, -1439);
dateToCivilAtOffset(date, 1439);
civilUtcToDate(Object.assign(new Date(0), civil));
civilAtOffsetToDate(Object.assign(new Date(0), civil), 480);

// @ts-expect-error Date-to-civil functions require a Date, not an epoch number
dateToCivilUtc(0);
// @ts-expect-error no date-string parsing
dateToCivilAtOffset("2024-01-01", 0);
// @ts-expect-error offsets are numeric minutes, not zone names
dateToCivilAtOffset(date, "UTC");
// @ts-expect-error the fixed offset must be explicit
dateToCivilAtOffset(date);
// @ts-expect-error the fixed offset must be explicit
civilAtOffsetToDate(civil);
// @ts-expect-error offsets are numeric minutes
civilAtOffsetToDate(civil, "480");
// @ts-expect-error a Date is not a civil record
civilUtcToDate(date);
// @ts-expect-error a Date is not a civil record
civilAtOffsetToDate(date, 0);
// @ts-expect-error Gregorian date labels have no time of day
civilUtcToDate(gregorian);
// @ts-expect-error Gregorian date labels have no time of day
civilAtOffsetToDate(gregorian, 0);
// @ts-expect-error lunar dates are not civil moments
civilUtcToDate(lunar);
// @ts-expect-error lunar dates are not civil moments
civilAtOffsetToDate(lunar, 0);
// @ts-expect-error a lunar date cannot become civil merely by adding a fraction
civilUtcToDate({ ...lunar, fraction: 0 });
// @ts-expect-error the excluded date-kind tag cannot be present even as undefined
civilAtOffsetToDate({ ...civil, isLeap: undefined }, 0);
// @ts-expect-error clock fields do not replace the input day fraction
civilUtcToDate({ year: 2024, month: 1, day: 1, hour: 12, minute: 0, second: 0 });
// @ts-expect-error the bridge result is a civil moment, not a date label
const dateLabel: GregorianDate = utc;
// @ts-expect-error the bridge result is not a lunar date
const lunarLabel: LunarDate = eastEight;
