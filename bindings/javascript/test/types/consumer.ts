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

import * as celestial from "@0xf3cd/celestial";
import type {
  CivilDateTime,
  CivilDateTimeResult,
  DeltaTModel,
  GregorianDate,
  Jieqi,
  JieqiMoment,
  LunarAlgorithm,
  LunarDate,
  MoonPhase,
} from "@0xf3cd/celestial";
// @ts-expect-error CivilDate was renamed, without an alias
import type { CivilDate } from "@0xf3cd/celestial";

const phase: MoonPhase = "full";
const deltaTModel: DeltaTModel = "algo5";
const lunarAlgorithm: LunarAlgorithm = "algo3";
const civil: CivilDateTime = { year: 2024, month: 6, day: 1, fraction: 0.5 };
const gregorian: GregorianDate = { year: 2024, month: 2, day: 10 };
const lunar: LunarDate = { year: 2024, month: 1, day: 1, isLeap: false };
const jieqiValues: readonly [
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
] = [
  celestial.Jieqi.LICHUN,
  celestial.Jieqi.YUSHUI,
  celestial.Jieqi.JINGZHE,
  celestial.Jieqi.CHUNFEN,
  celestial.Jieqi.QINGMING,
  celestial.Jieqi.GUYU,
  celestial.Jieqi.LIXIA,
  celestial.Jieqi.XIAOMAN,
  celestial.Jieqi.MANGZHONG,
  celestial.Jieqi.XIAZHI,
  celestial.Jieqi.XIAOSHU,
  celestial.Jieqi.DASHU,
  celestial.Jieqi.LIQIU,
  celestial.Jieqi.CHUSHU,
  celestial.Jieqi.BAILU,
  celestial.Jieqi.QIUFEN,
  celestial.Jieqi.HANLU,
  celestial.Jieqi.SHUANGJIANG,
  celestial.Jieqi.LIDONG,
  celestial.Jieqi.XIAOXUE,
  celestial.Jieqi.DAXUE,
  celestial.Jieqi.DONGZHI,
  celestial.Jieqi.XIAOHAN,
  celestial.Jieqi.DAHAN,
];
const jieqiKinds: readonly Jieqi[] = jieqiValues;
const constructedError = new celestial.CelestialError("consumer", "consumer failure", false);
constructedError.operation;
constructedError.recorded;

await celestial.init();
celestial.config.setLogVerbosity("none");
celestial.time.ut1ToJd({ year: 2000, month: 1, day: 1, fraction: 0.5 });
celestial.time.ut1ToJde({ year: 2000, month: 1, day: 1, fraction: 0.5 });
const ut1 = celestial.time.jdeToUt1(2451545.0);
celestial.time.localApparentSiderealTime(2451545.0, 0);
celestial.time.deltaT(2024.5, deltaTModel);
celestial.sun.apparentGeocentricCoordinate(2451545.0);
celestial.sun.longitudeCrossings(2024, 0);
celestial.sun.equationOfTime(2451545.0);
const solarTime = celestial.sun.apparentSolarTime(civil, 116.4);
celestial.moon.apparentGeocentricCoordinate(2451545.0);
celestial.moon.illumination(2451545.0);
celestial.moon.brightLimbPositionAngle(2451545.0);
celestial.moon.phaseMoments(2024, phase);
celestial.moon.newMoonsAfter(2451545.0, 2);
celestial.moon.newMoonsInYear(2024);
const lichun = celestial.jieqi.moment(2024, celestial.Jieqi.LICHUN);
const jieqiIndex: (typeof jieqiValues)[number] = lichun.jieqi;
const moment: JieqiMoment = lichun;
celestial.jieqi.name(lichun.jieqi);
for (const kind of jieqiKinds) {
  celestial.jieqi.moment(2024, kind);
  celestial.jieqi.name(kind);
}
celestial.jieqi.name(23);
celestial.lunar.supportedYearRange(lunarAlgorithm);
const yearInfo = celestial.lunar.yearInfo(lunarAlgorithm, 2024);
const lunarDate = celestial.lunar.fromGregorian(lunarAlgorithm, gregorian);
const gregorianDate = celestial.lunar.toGregorian(lunarAlgorithm, lunar);

const civilOutputs: CivilDateTimeResult[] = [ut1, solarTime, lichun.momentUt1];
for (const output of civilOutputs) {
  const input: CivilDateTime = output;
  const clock: [number, number, number] = [output.hour, output.minute, output.second];
}
celestial.time.ut1ToJd(ut1);
celestial.time.ut1ToJd(lichun.momentUt1);
celestial.time.ut1ToJde(lichun.momentUt1);
celestial.lunar.fromGregorian(lunarAlgorithm, yearInfo.firstDay);
celestial.lunar.fromGregorian(lunarAlgorithm, gregorianDate);
celestial.lunar.toGregorian(lunarAlgorithm, lunarDate);

celestial.lunar.fromGregorian(lunarAlgorithm, Object.assign(new Date(0), gregorian));
celestial.lunar.toGregorian(lunarAlgorithm, Object.assign(new Date(0), lunar));
celestial.time.ut1ToJd(Object.assign(new Date(0), civil));
celestial.time.ut1ToJde(Object.assign(new Date(0), civil));
celestial.sun.apparentSolarTime(Object.assign(new Date(0), civil), 0);

// @ts-expect-error GregorianDate is not a civil moment
const gregorianAsCivil: CivilDateTime = gregorian;
// @ts-expect-error GregorianDate is not a lunar date
const gregorianAsLunar: LunarDate = gregorian;
// @ts-expect-error CivilDateTime is not a Gregorian date label
const civilAsGregorian: GregorianDate = civil;
// @ts-expect-error CivilDateTime is not a lunar date
const civilAsLunar: LunarDate = civil;
// @ts-expect-error LunarDate is not a Gregorian date label
const lunarAsGregorian: GregorianDate = lunar;
// @ts-expect-error LunarDate is not a civil moment
const lunarAsCivil: CivilDateTime = lunar;
// @ts-expect-error Gregorian dates exclude even an undefined fraction property
const undefinedFraction: GregorianDate = { ...gregorian, fraction: undefined };
// @ts-expect-error Gregorian dates exclude even an undefined isLeap property
const undefinedGregorianLeap: GregorianDate = { ...gregorian, isLeap: undefined };
// @ts-expect-error civil moments exclude even an undefined isLeap property
const undefinedCivilLeap: CivilDateTime = { ...civil, isLeap: undefined };
// @ts-expect-error lunar dates exclude even an undefined fraction property
const undefinedLunarFraction: LunarDate = { ...lunar, fraction: undefined };
// @ts-expect-error result clock fields are required, but input clock fields are not
const inputAsResult: CivilDateTimeResult = civil;

// @ts-expect-error civil results cannot be silently reduced to date labels
celestial.lunar.fromGregorian(lunarAlgorithm, ut1);
// @ts-expect-error a Jieqi moment is UT1, not a Gregorian date label
celestial.lunar.fromGregorian(lunarAlgorithm, lichun.momentUt1);
// @ts-expect-error lunar outputs cannot be treated as Gregorian dates
celestial.lunar.fromGregorian(lunarAlgorithm, lunarDate);
// @ts-expect-error a Gregorian date has no civil fraction
celestial.time.ut1ToJd(gregorianDate);
// @ts-expect-error a lunar date is not a civil moment
celestial.time.ut1ToJde(lunarDate);
// @ts-expect-error a civil moment is not a lunar date
celestial.lunar.toGregorian(lunarAlgorithm, civil);

// @ts-expect-error Jieqi moments have no flat index
lichun.index;
// @ts-expect-error Jieqi moments have no flat year
lichun.year;
// @ts-expect-error Jieqi moments have no flat month
lichun.month;
// @ts-expect-error Jieqi moments have no flat day
lichun.day;
// @ts-expect-error Jieqi moments have no flat fraction
lichun.fraction;
// @ts-expect-error select momentUt1 explicitly
celestial.time.ut1ToJd(lichun);
// @ts-expect-error singular coordinate method only
celestial.sun.apparentGeocentricCoordinates(2451545.0);
// @ts-expect-error singular coordinate method only
celestial.moon.apparentGeocentricCoordinates(2451545.0);
// @ts-expect-error Jieqi constants are readonly
celestial.Jieqi.LICHUN = 0;
// @ts-expect-error no reverse enum lookup
celestial.Jieqi[0];
// @ts-expect-error Jieqi is the literal 0-through-23 union
const invalidJieqi: Jieqi = 24;
// @ts-expect-error Jieqi indices are non-negative
celestial.jieqi.name(-1);
// @ts-expect-error Jieqi indices end at 23
celestial.jieqi.moment(2024, 24);
const generalNumber: number = 0;
// @ts-expect-error callers must narrow a general number to Jieqi
celestial.jieqi.name(generalNumber);
// @ts-expect-error callers must narrow a general number to Jieqi
celestial.jieqi.moment(2024, generalNumber);

// @ts-expect-error no numeric phase entry
celestial.moon.phaseMoments(2024, 0);
// @ts-expect-error no runtime enum-object shape
celestial.time.deltaT(2024, { algo: "algo5" });
// @ts-expect-error the root entry does not treat Date as UT1
celestial.time.ut1ToJd(new Date());
// @ts-expect-error the root entry does not treat Date as UT1
celestial.time.ut1ToJde(new Date());
// @ts-expect-error the root entry requires explicit civil UTC fields
celestial.sun.apparentSolarTime(new Date(), 0);
// @ts-expect-error Date is not a Gregorian calendar-date label
celestial.lunar.fromGregorian(lunarAlgorithm, new Date());
// @ts-expect-error Date is not a lunar date
celestial.lunar.toGregorian(lunarAlgorithm, new Date());
// @ts-expect-error no solarTerms alias
celestial.solarTerms.moment(2024, 0);
// @ts-expect-error LunarDate requires isLeap
celestial.lunar.toGregorian("algo1", { year: 2024, month: 1, day: 1 });
