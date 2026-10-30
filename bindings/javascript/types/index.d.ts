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

export type MoonPhase = "new" | "firstQuarter" | "full" | "lastQuarter";
export type DeltaTModel = "default" | "algo1" | "algo2" | "algo3" | "algo4" | "algo5";
export type LunarAlgorithm = "algo1" | "algo2" | "algo3";
export type CivilScale = "utc" | "ut1";
export type HouseSystem = "equal" | "wholeSign" | "placidus";
export type ChartTarget =
  | "sun" | "moon" | "mercury" | "venus" | "mars" | "jupiter" | "saturn"
  | "uranus" | "neptune" | "pluto"
  | "meanAscending" | "meanDescending" | "trueAscending" | "trueDescending";

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
 * A Gregorian calendar-date label, not an instant.
 * Native results use astronomical years in [-32767, 32767], with 0 denoting 1 BCE.
 * Input-year admission depends on the operation; /date bridges require civil years in [1, 32767].
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

/**
 * Civil fields on the time scale named by the operation. No automatic Date conversion.
 * Input-year admission depends on the operation; native results use GregorianDate's astronomical-year range.
 */
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
  distanceAu: number;
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

export interface GeoLocation {
  /** Finite, north-positive degrees in (-90, 90). */
  latitudeDeg: number;
  /** Finite, east-positive degrees in [-180, 180]. */
  longitudeDeg: number;
}

export interface ChartTimes {
  jdUt1: number;
  jdeTt: number;
}

export interface BodyState {
  target: ChartTarget;
  longitudeDeg: number;
  latitudeDeg: number;
  /** AU for all bodies, including Moon; null for nodes. */
  distanceAu: number | null;
  /** Instantaneous rate; negative means the longitude is decreasing at that instant. */
  longitudeRateDegPerTtDay: number;
}

export interface ChartHouses {
  ascendantDeg: number;
  midheavenDeg: number;
  descendantDeg: number;
  imumCoeliDeg: number;
  /** Twelve longitudes in [0, 360) degrees, in house order. */
  cuspsDeg: number[];
}

/** Fresh owned values, independent of later calls and WASM memory growth. */
export interface ChartSnapshot {
  times: ChartTimes;
  /** Fourteen targets in ChartTarget declaration order. */
  bodies: BodyState[];
  houses: ChartHouses;
}

/** Only these four own keys are accepted; inherited model is rejected. Location fields must also be own. */
export interface ChartSnapshotOptions {
  scale: CivilScale;
  location: GeoLocation;
  system: HouseSystem;
  /** Omitted or own undefined selects Algo5. */
  model?: DeltaTModel | undefined;
}

/** An unavailable module, computation, resource or protocol failure; invalid native input throws RangeError.
 * recorded is true when native reported a recognized error kind, even with an empty message.
 * Caller-owned WASM output buffers are limited to 16 MiB by this binding.
 */
export class CelestialError extends Error {
  constructor(operation: string, message: string, recorded: boolean);
  operation: string;
  recorded: boolean;
}

/**
 * Load the package-owned WASM module, resolving to undefined. Concurrent and completed calls share one promise.
 * A failed load preserves its original rejection and can be retried.
 * Namespace methods throw CelestialError with their operation and recorded=false until initialization completes,
 * before argument validation, including zero-count calls.
 */
export function init(): Promise<void>;

export const time: Readonly<{
  /** Convert a UT1 civil moment to JD (UT1). */
  ut1ToJd(ut1: CivilDateTime): number;
  /** Convert a UT1 civil moment to JDE (TT). */
  ut1ToJde(ut1: CivilDateTime): number;
  /** Convert a TT-based JDE to a UT1 civil moment. */
  jdeToUt1(jde: number): CivilDateTimeResult;
  /**
   * Convert UTC civil fields to JDE(TT); before 1972 uses the UT1 substitute with fixed Algo5.
   * Input years are in [1, 32767], and the shifted TT instant must stay representable.
   * Uses the fixed leap-second table and retains its final offset after the last entry.
   * Leap seconds and the 1972 model seam are not generally invertible.
   */
  utcToJde(utc: CivilDateTime): number;
  /**
   * Convert finite JDE(TT) in [1867522.5, 13689325.5) to UTC civil fields; year 400 is possible.
   * Before 1972 uses the UT1 substitute with fixed Algo5; retains the final offset after the last leap-table entry.
   * Leap seconds, leap-step midnight round-off and the 1972 seam do not promise an exact inverse.
   */
  jdeToUtc(jde: number): CivilDateTimeResult;
  /**
   * Local apparent sidereal time in [0, 360) degrees, at a finite JD on UT1 in Gregorian years [401, 32766].
   * Geographic longitude is finite, east-positive, in [-180, 180] degrees.
   * @throws {RangeError} For non-finite numbers, invalid longitude or a JD outside the native domain.
   * @throws {CelestialError} If the calculation fails or native success is malformed.
   */
  localApparentSiderealTime(jdUt1: number, longitudeDeg: number): number;
  /**
   * Delta T (TT - UT1), in seconds, for a finite decimal Gregorian year.
   * Default/algo5 is the current project model: algo2 before 2005, an IERS Bulletin A fit
   * through about 2026.41, then anchored Morrison et al. long-term extrapolation.
   * Algo1 (Xu Jianwei 2008), algo2 (Espenak and Meeus 2006), algo3 (Espenak 2014), and
   * algo4 (IERS Bulletin A / USNO predictions) are frozen comparison models. Algo2 also
   * remains the live pre-2005 branch of algo3, algo4, and algo5.
   * Algo1 requires year >= -4000; algo3 year < 3000; algo4 year < 2035.
   * Algo2, default and algo5 accept any finite year; that input bound is not an accuracy guarantee.
   */
  deltaT(year: number, model?: DeltaTModel): number;
}>;

export const sun: Readonly<{
  /** Apparent geocentric ecliptic coordinates at a TT-based JDE. */
  apparentGeocentricCoordinate(jde: number): EclipticCoordinateAu;
  /**
   * TT-based JDEs when the Sun reaches a finite apparent geocentric longitude in [0, 360) degrees.
   * The Gregorian year is in [1, 32766]; returns [] when that year has no crossing.
   */
  longitudeCrossings(year: number, longitudeDeg: number): number[];
  /** Equation of time in degrees of hour angle; multiply by 240 for seconds. */
  equationOfTime(jde: number): number;
  /** Convert civil UTC to local apparent solar time; geographic longitude is east-positive in [-180, 180] degrees. */
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
  /** The next count New Moon JDEs; the binding caps synchronous work at 4096 results. */
  newMoonsAfter(jde: number, count: number): number[];
  /** New Moon JDEs in a Gregorian year in [1, 32766]. */
  newMoonsInYear(year: number): number[];
}>;

export const jieqi: Readonly<{
  /**
   * UT1 civil moment for a Gregorian year in [401, 32766].
   * Not an east-eight wall date or UTC: establish the time-scale conversion before displaying either.
   * The returned UT1 year can differ from the requested year.
   * @throws {RangeError} For inputs outside the native domain.
   * @throws {CelestialError} If the calculation fails or native success is malformed.
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
 * All methods throw CelestialError with recorded=false before init() completes, before argument validation.
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
   * @throws {CelestialError} If computation fails or native success is malformed.
   */
  yearInfo(algorithm: LunarAlgorithm, year: number): LunarYearInfo;
  /**
   * Convert Gregorian date fields on the selected algorithm's basis, not a civil moment.
   * @throws {TypeError} For wrong types, missing own fields, fraction/isLeap tags, or non-integer date fields.
   * @throws {RangeError} For an unknown algorithm, non-finite/unsafe fields, an invalid Gregorian date,
   * or a date outside native Gregorian coverage.
   * @throws {CelestialError} If computation fails or native success is malformed.
   */
  fromGregorian(algorithm: LunarAlgorithm, date: GregorianDate): LunarDate;
  /**
   * Convert a traditional lunar month/day, with an explicit leap flag, to a Gregorian date label.
   * @throws {TypeError} For wrong types, missing own fields, a fraction tag, or non-integer date fields.
   * @throws {RangeError} For an unknown algorithm, non-finite/unsafe fields, a year outside its native range,
   * month/day outside [1, 12]/[1, 30], or a nonexistent leap month/day.
   * @throws {CelestialError} If computation fails or native success is malformed.
   */
  toGregorian(algorithm: LunarAlgorithm, date: LunarDate): GregorianDate;
}>;

export const chart: Readonly<{
  /**
   * Fixed-instant geocentric chart on the true ecliptic and equinox of date: apparent body positions
   * and mean/true node longitudes. Civil years 1885-2099; pre-1972 input requires scale="ut1".
   * Core requires JDE(TT) in [2409543.875, 2488069.125); Algo4 also requires JDE < 2464328.5.
   * UTC-derived UT1 is modelled, not observed DUT1. Ascendant round-off singularities and
   * Placidus polar limits are rejected. No failure returns a partial snapshot.
   * @throws {TypeError} For wrong types, missing own fields, inherited model or unknown option keys.
   * @throws {RangeError} For invalid civil dates, fractions, locations, selections or native domain rejection.
   * @throws {CelestialError} Before init, on allocation/computation failure or malformed native success.
   */
  snapshot(civil: CivilDateTime, options: ChartSnapshotOptions): ChartSnapshot;
}>;
