/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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

import * as celestial from "@0xf3cd/celestial";
import type { DeltaTModel, LunarAlgorithm, MoonPhase } from "@0xf3cd/celestial";

const phase: MoonPhase = "full";
const deltaTModel: DeltaTModel = "algo5";
const lunarAlgorithm: LunarAlgorithm = "algo3";

await celestial.init();
celestial.config.setLogVerbosity("none");
celestial.time.ut1ToJd({ year: 2000, month: 1, day: 1, fraction: 0.5 });
celestial.time.ut1ToJde({ year: 2000, month: 1, day: 1, fraction: 0.5 });
celestial.time.jdeToUt1(2451545.0);
celestial.time.localApparentSiderealTime(2451545.0, 0);
celestial.time.deltaT(2024.5, deltaTModel);
celestial.sun.apparentGeocentricCoordinates(2451545.0);
celestial.sun.longitudeCrossings(2024, 0);
celestial.sun.equationOfTime(2451545.0);
celestial.sun.apparentSolarTime({ year: 2024, month: 6, day: 1, fraction: 0.5 }, 116.4);
celestial.moon.apparentGeocentricCoordinates(2451545.0);
celestial.moon.illumination(2451545.0);
celestial.moon.brightLimbPositionAngle(2451545.0);
celestial.moon.phaseMoments(2024, phase);
celestial.moon.newMoonsAfter(2451545.0, 2);
celestial.moon.newMoonsInYear(2024);
celestial.jieqi.moment(2024, 0);
celestial.jieqi.name(0);
celestial.lunar.supportedYearRange(lunarAlgorithm);
celestial.lunar.yearInfo(lunarAlgorithm, 2024);
celestial.lunar.fromGregorian(lunarAlgorithm, { year: 2024, month: 2, day: 10 });
celestial.lunar.toGregorian(lunarAlgorithm, { year: 2024, month: 1, day: 1, isLeap: false });

// @ts-expect-error no numeric phase entry
celestial.moon.phaseMoments(2024, 0);
// @ts-expect-error no runtime enum-object shape
celestial.time.deltaT(2024, { algo: "algo5" });
// @ts-expect-error no Date convenience API
celestial.time.ut1ToJd(new Date());
// @ts-expect-error no solarTerms alias
celestial.solarTerms.moment(2024, 0);
// @ts-expect-error LunarDate requires isLeap
celestial.lunar.toGregorian("algo1", { year: 2024, month: 1, day: 1 });
