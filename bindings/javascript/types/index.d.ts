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

export type LogVerbosity = "none" | "info" | "debug";
export type MoonPhase = "new" | "firstQuarter" | "full" | "lastQuarter";
export type DeltaTModel = "default" | "algo1" | "algo2" | "algo3" | "algo4" | "algo5";
export type LunarAlgorithm = "algo1" | "algo2" | "algo3";

/** The frozen 24-term Jieqi object, indexed from Lichun. */
export const Jieqi: Readonly<{
  LICHUN: 0;
  YUSHUI: 1;
  JINGZHE: 2;
  CHUNFEN: 3;
  QINGMING: 4;
  GUYU: 5;
  LIXIA: 6;
  XIAOMAN: 7;
  MANGZHONG: 8;
  XIAZHI: 9;
  XIAOSHU: 10;
  DASHU: 11;
  LIQIU: 12;
  CHUSHU: 13;
  BAILU: 14;
  QIUFEN: 15;
  HANLU: 16;
  SHUANGJIANG: 17;
  LIDONG: 18;
  XIAOXUE: 19;
  DAXUE: 20;
  DONGZHI: 21;
  XIAOHAN: 22;
  DAHAN: 23;
}>;

export type Jieqi = (typeof Jieqi)[keyof typeof Jieqi];

/**
 * A Gregorian calendar-date label, not an instant; year is in [1, 32767].
 * Runtime record inputs read explicit own fields, without converting Date timestamps.
 * Unrelated extra fields are allowed, but date-kind tags are not.
 */
export interface GregorianDate {
  year: number;
  month: number;
  day: number;
  fraction?: never;
  isLeap?: never;
}

/** Civil fields with year in [1, 32767]; the operation names their time scale. No automatic Date conversion. */
export interface CivilDateTime {
  year: number;
  month: number;
  day: number;
  /** Fraction of the day in [0, 1). */
  fraction: number;
  isLeap?: never;
}

/** Derived clock fields accompany the day fraction; inputs continue to use fraction, not the clock fields. */
export interface CivilDateTimeResult extends CivilDateTime {
  /** Integer hour in [0, 23]. */
  hour: number;
  /** Integer minute in [0, 59]. */
  minute: number;
  /** Seconds in [0, 60), retaining the fractional-second remainder, not rounded to milliseconds. */
  second: number;
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

export interface JieqiMoment {
  jieqi: Jieqi;
  /** UT1, not an east-eight wall clock; the east-eight display date can differ. */
  momentUt1: CivilDateTimeResult;
}

export interface LunarYearRange {
  start: number;
  end: number;
}

export interface LunarYearInfo {
  /** Gregorian date label on the selected lunar algorithm's basis. */
  firstDay: GregorianDate;
  /** Traditional month number, or null for a common year. */
  leapMonth: number | null;
  /** Month lengths in calendar order; a leap month follows its ordinary namesake. */
  monthLengths: number[];
}

/** A lunar calendar-date label; month is the traditional number in [1, 12]. */
export interface LunarDate {
  year: number;
  month: number;
  day: number;
  isLeap: boolean;
  fraction?: never;
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
  jdeToUt1(jde: number): CivilDateTimeResult;
  /** Local apparent sidereal time in degrees; longitude is east-positive. */
  localApparentSiderealTime(jdUt1: number, longitudeDeg: number): number;
  /**
   * Delta T (TT - UT1), in seconds, for a finite decimal Gregorian year.
   * Default/algo5 is the current project model: algo2 before 2005, an IERS Bulletin A fit
   * through about 2026.41, then anchored Morrison et al. long-term extrapolation.
   * Algo1 (Xu Jianwei 2008), algo2 (Espenak and Meeus 2006), algo3 (Espenak 2014), and
   * algo4 (IERS Bulletin A / USNO predictions) are frozen comparison models. Algo2 also
   * remains the live pre-2005 branch of algo3, algo4, and algo5.
   * Algo1 requires year >= -4000; algo3 year < 3000; algo4 year < 2035.
   * Algo2/default/algo5 have no model-specific year bound, not an accuracy guarantee.
   */
  deltaT(year: number, model?: DeltaTModel): number;
}>;

export const sun: Readonly<{
  /** Apparent geocentric ecliptic coordinates at a TT-based JDE. */
  apparentGeocentricCoordinate(jde: number): EclipticCoordinateAu;
  /** TT-based JDEs when the Sun reaches longitudeDeg in a Gregorian year in [1, 32766]. */
  longitudeCrossings(year: number, longitudeDeg: number): number[];
  /** Equation of time in degrees of hour angle; multiply by 240 for seconds. */
  equationOfTime(jde: number): number;
  /** Convert a civil UTC moment to local apparent solar time; longitude is east-positive. */
  apparentSolarTime(utc: CivilDateTime, longitudeDeg: number): CivilDateTimeResult;
}>;

export const moon: Readonly<{
  /** Apparent geocentric ecliptic coordinates at a TT-based JDE. */
  apparentGeocentricCoordinate(jde: number): EclipticCoordinateKm;
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
  /**
   * UT1 civil moment for a Gregorian year in [401, 32766].
   * Not an east-eight wall date or UTC: establish the time-scale conversion before displaying either.
   * The returned UT1 year can differ from the requested year.
   * @throws {CelestialError} If the native calculation cannot produce the moment.
   */
  moment(year: number, index: Jieqi): JieqiMoment;
  /** Return the Chinese name of a Jieqi. */
  name(index: Jieqi): string;
}>;

/**
 * Calendar-date labels, not instants. Algo1 preserves HKO labels (1901-2099); algo2 computes
 * from VSOP87D / truncated ELP2000-82B (410-2500); algo3 is a baked hybrid (1600-2199), using HKO
 * for 1901-2099 and algo2-generated dates elsewhere. Choose table compatibility or computation,
 * not an assumed accuracy ranking. Algo2 renders TT through the library's UTC model, then +8 h:
 * UT1 proxy before 1972, the leap-second table from 1972, and frozen Delta AT = 37 s after its last entry.
 * All methods throw Error if called before init() completes.
 */
export const lunar: Readonly<{
  /**
   * Inclusive lunar-year bounds reported by the native algorithm, not Gregorian-year bounds.
   * @throws {TypeError} If algorithm is not a string.
   * @throws {RangeError} If algorithm is unknown.
   * @throws {CelestialError} If the native range query fails.
   */
  supportedYearRange(algorithm: LunarAlgorithm): LunarYearRange;
  /**
   * Metadata for a lunar year; firstDay uses the selected algorithm's Gregorian date basis.
   * @throws {TypeError} For a non-string algorithm or a non-integer year.
   * @throws {RangeError} For an unknown algorithm, non-finite/unsafe year, or year outside its native range.
   * @throws {CelestialError} If the native range query or year computation fails.
   */
  yearInfo(algorithm: LunarAlgorithm, year: number): LunarYearInfo;
  /**
   * Convert Gregorian date fields on the selected algorithm's basis, not a civil moment.
   * @throws {TypeError} For wrong types, missing own fields, fraction/isLeap tags, or non-integer date fields.
   * @throws {RangeError} For an unknown algorithm, non-finite/unsafe fields, or an invalid Gregorian date.
   * @throws {CelestialError} If the date is outside native Gregorian coverage or native conversion fails.
   */
  fromGregorian(algorithm: LunarAlgorithm, date: GregorianDate): LunarDate;
  /**
   * Convert a traditional lunar month/day, with an explicit leap flag, to a Gregorian date label.
   * @throws {TypeError} For wrong types, missing own fields, a fraction tag, or non-integer date fields.
   * @throws {RangeError} For an unknown algorithm, non-finite/unsafe fields, a year outside its native range,
   * or month/day outside [1, 12]/[1, 30].
   * @throws {CelestialError} If the native range query fails, the leap month/day does not exist,
   * or native conversion fails.
   */
  toGregorian(algorithm: LunarAlgorithm, date: LunarDate): GregorianDate;
}>;
