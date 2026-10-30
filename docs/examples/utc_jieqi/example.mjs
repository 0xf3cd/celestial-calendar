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

import { init, time, jieqi, Jieqi } from "@0xf3cd/celestial";
import { dateToCivilUtc, civilUtcToDate } from "@0xf3cd/celestial/date";

await init();
const cutoffUtc = dateToCivilUtc(new Date("2026-10-28T19:35:37Z"));
const cutoffJde = time.utcToJde(cutoffUtc);
const candidates = [cutoffUtc.year, cutoffUtc.year + 1].flatMap((year) =>
  Object.values(Jieqi).map((index) => {
    const event = jieqi.moment(year, index);
    // Jieqi moments are UT1; compare only after conversion to TT.
    return { jde: time.ut1ToJde(event.momentUt1), index };
  }),
);
const upcoming = candidates.filter(({ jde }) => jde > cutoffJde).sort((a, b) => a.jde - b.jde)[0];
if (upcoming === undefined) throw new Error("No later Jieqi in the queried years.");
const utc = time.jdeToUtc(upcoming.jde);

const utcDisplay = civilUtcToDate(utc);
const localDisplay = new Date(utcDisplay.getTime() + 8 * 3600 * 1000);
const minuteLabel = (date) => date.toISOString().slice(0, 16).replace("T", " ");
console.log(jieqi.name(upcoming.index));
console.log("UTC:", minuteLabel(utcDisplay));
console.log("UTC+8:", minuteLabel(localDisplay));
