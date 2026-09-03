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

export type LogVerbosity = "none" | "info" | "debug";
export type MoonPhase = "new" | "firstQuarter" | "full" | "lastQuarter";
export type DeltaTModel = "default" | "algo1" | "algo2" | "algo3" | "algo4" | "algo5";
export type LunarAlgorithm = "algo1" | "algo2" | "algo3";

/** Runtime record inputs require the declared fields as own properties and may include additional fields. */
export interface CivilDate {
  year: number;
  month: number;
  day: number;
}

export interface CivilDateTime extends CivilDate {
  /** Fraction of the day in [0, 1). */
  fraction: number;
}

export interface EclipticCoordinateAu {
  longitudeDeg: number;
  latitudeDeg: number;
  radiusAu: number;
}

export interface EclipticCoordinateKm {
  longitudeDeg: number;
  latitudeDeg: number;
  distanceKm: number;
}

export interface MoonIllumination {
  /** Illuminated fraction in [0, 1]. */
  fraction: number;
  /** Apparent ecliptic longitude difference Moon - Sun, in degrees. */
  elongationDeg: number;
}

export interface JieqiMoment extends CivilDateTime {
  index: number;
}

export interface LunarYearRange {
  start: number;
  end: number;
}

export interface LunarYearInfo {
  firstDay: CivilDate;
  /** Traditional month number, or null for a common year. */
  leapMonth: number | null;
  /** Month lengths in calendar order; a leap month follows its ordinary namesake. */
  monthLengths: number[];
}

export interface LunarDate extends CivilDate {
  isLeap: boolean;
}

export class CelestialError extends Error {
  constructor(operation: string, message: string, recorded: boolean);
  operation: string;
  recorded: boolean;
}

/** Load the package-owned WASM module. Concurrent calls share one promise; a failed load can be retried. */
export function init(): Promise<void>;

export const config: Readonly<{
  setLogVerbosity(level: LogVerbosity): void;
}>;

export const time: Readonly<{
  /** Convert a UT1 civil moment to JD (UT1). */
  ut1ToJd(ut1: CivilDateTime): number;
  /** Convert a UT1 civil moment to JDE (TT). */
  ut1ToJde(ut1: CivilDateTime): number;
  /** Convert a TT-based JDE to a UT1 civil moment. */
  jdeToUt1(jde: number): CivilDateTime;
  /** Local apparent sidereal time in degrees; longitude is east-positive. */
  localApparentSiderealTime(jdUt1: number, longitudeDeg: number): number;
  /**
   * Delta T (TT - UT1), in seconds.
   * Algo1 requires year >= -4000; algo3 requires year < 3000; algo4 requires year < 2035.
   * The other models have no model-specific year bound.
   */
  deltaT(year: number, model?: DeltaTModel): number;
}>;

export const sun: Readonly<{
  /** Apparent geocentric ecliptic coordinates at a TT-based JDE. */
  apparentGeocentricCoordinates(jde: number): EclipticCoordinateAu;
  /** TT-based JDEs when the Sun reaches longitudeDeg in a Gregorian year in [1, 32766]. */
  longitudeCrossings(year: number, longitudeDeg: number): number[];
  /** Equation of time in degrees of hour angle; multiply by 240 for seconds. */
  equationOfTime(jde: number): number;
  /** Convert a civil UTC moment to local apparent solar time; longitude is east-positive. */
  apparentSolarTime(utc: CivilDateTime, longitudeDeg: number): CivilDateTime;
}>;

export const moon: Readonly<{
  /** Apparent geocentric ecliptic coordinates at a TT-based JDE. */
  apparentGeocentricCoordinates(jde: number): EclipticCoordinateKm;
  illumination(jde: number): MoonIllumination;
  /** Position angle of the bright limb, in degrees eastward from north. */
  brightLimbPositionAngle(jde: number): number;
  /** TT-based JDEs of the selected phase in a Gregorian year in [1, 32766]. */
  phaseMoments(year: number, phase: MoonPhase): number[];
  /** The next count New Moon JDEs; count is in [0, 4096]. */
  newMoonsAfter(jde: number, count: number): number[];
  /** New Moon JDEs in a Gregorian year in [1, 32766]. */
  newMoonsInYear(year: number): number[];
}>;

export const jieqi: Readonly<{
  /** UT1 civil moment for a Gregorian year in [401, 32766] and a Jieqi index in [0, 23]. */
  moment(year: number, index: number): JieqiMoment;
  name(index: number): string;
}>;

export const lunar: Readonly<{
  supportedYearRange(algorithm: LunarAlgorithm): LunarYearRange;
  yearInfo(algorithm: LunarAlgorithm, year: number): LunarYearInfo;
  fromGregorian(algorithm: LunarAlgorithm, date: CivilDate): LunarDate;
  toGregorian(algorithm: LunarAlgorithm, date: LunarDate): CivilDate;
}>;
