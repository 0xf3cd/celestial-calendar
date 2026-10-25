<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions between
    Gregorian and Chinese Lunar calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# @0xf3cd/celestial

Astronomical calculations and Gregorian / Chinese Lunar date conversion from
[CelestialCalendar](https://github.com/0xf3cd/celestial-calendar), as an ESM package that bundles its own
WebAssembly module.

[中文](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/javascript/README_ZH.md)

This guide describes version **1.0.0**, currently unreleased. The install command below gets the latest published
version on npm; use its matching documentation. See [Migrating to 1.0](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/MIGRATING_1_0.md)
for the removed logging API, Sun distance rename and changed error classes.

<a id="install"></a>
## Install

Install from npm in your application directory:

```sh
npm install @0xf3cd/celestial
```

For each release, the matching [GitHub Release](https://github.com/0xf3cd/celestial-calendar/releases) also
provides `celestial-wasm.zip`, which contains a tarball byte-identical to the npm publication and its SHA-256
checksum file.

Node 22 or newer is supported. Browser consumers are tested on Chrome and Firefox. Importing the package performs no
I/O; call `init()` once and await it before using the synchronous calculation APIs.

Each JavaScript block below is a standalone ES module for `example.mjs` in your application directory; run it with
`node example.mjs` from that directory.

<a id="examples"></a>
## Examples

<a id="chart-snapshot"></a>
### Chart Snapshot at a Fixed Instant

```js
import { init, chart } from "@0xf3cd/celestial";

await init();
const snapshot = chart.snapshot(
  { year: 2026, month: 1, day: 1, fraction: 0.5 },
  {
    scale: "utc",
    location: { latitudeDeg: 51.5, longitudeDeg: 0 },
    system: "placidus",
    model: "default",
  },
);
console.log(snapshot.times); // { jdUt1, jdeTt }
console.log(snapshot.bodies[2]); // Mercury: position, distance and longitude rate
console.log(snapshot.houses.cuspsDeg); // twelve house cusps in house order
```

`civil` is a Gregorian date with a day fraction in `[0, 1)`; a JavaScript `Date` is not converted automatically.
`scale` is required and is `"utc"` or `"ut1"`; input before 1972-01-01 must select `scale: "ut1"` explicitly.
Civil years are limited to 1885–2099. Latitude is north-positive degrees in `(-90, 90)`; longitude is east-positive
degrees in `[-180, 180]`. `system` is required and is `"equal"`, `"wholeSign"`, or `"placidus"`.
Omitting `model`, or setting it as an own property to `undefined` or `"default"`, selects Algo5; `"algo1"` through
`"algo5"` select a model explicitly. Input fields must be own properties: an inherited `model` throws instead of
silently falling back to the default model. `options` accepts only the four keys `scale`, `location`, `system`, and
`model`; a misspelled or extra key throws `TypeError`.

The result contains `times: { jdUt1, jdeTt }` and fourteen `bodies` in a fixed order, whose `target` values are
`sun`, `moon`, `mercury`, `venus`, `mars`, `jupiter`, `saturn`, `uranus`, `neptune`, `pluto`, `meanAscending`,
`meanDescending`, `trueAscending`, and `trueDescending`. Each member has `longitudeDeg`, `latitudeDeg`,
`distanceAu`, and `longitudeRateDegPerTtDay`. All members use the true ecliptic and equinox of date: the Sun, Moon,
and planets give apparent geocentric coordinates; the nodes give mean / true node longitudes with zero latitude and
`null` for `distanceAu`. All distances are in AU, including the Moon's; the standalone
`moon.apparentGeocentricCoordinate()` gives the Moon's distance in kilometres. `longitudeRateDegPerTtDay` is the
instantaneous longitude rate in degrees per TT day: a negative value means the longitude is decreasing at that
instant (retrograde motion).
`houses` contains `ascendantDeg`, `midheavenDeg`, `descendantDeg`, `imumCoeliDeg`, and `cuspsDeg`, with every angle
in `[0, 360)` degrees. The result is made of independent plain objects and arrays; later calls or WASM memory growth
do not change an earlier result.

The core checks the converted JDE(TT) against the chart range `[2409543.875, 2488069.125)`; Algo4 also requires
derived `JDE < 2464328.5`, checked before reverse conversion. This deliberately excludes the final roughly 71.5
UT1 seconds of 2034 under Algo4. Common civil/model errors may precede a chart joint-window error.
UT1 derived from UTC is modelled, not observed DUT1; future UTC holds the last known leap-table offset. See
[Time scales](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md#time-scales). Ascendant
directions that degenerate at round-off scale, and Placidus polar cases, fail. Houses follow the house operation's
nutation default, currently IAU 1980; body corrections remain fixed IAU 1980. Type or record-shape errors throw
`TypeError`; invalid dates, day fractions, locations, option strings or native-domain rejections throw `RangeError`; calling before
initialization, allocation failure, native calculation failure, or a malformed native success record throws
`CelestialError`, with `operation` set to `"chart.snapshot"`. `recorded` is `true` when native reported a recognized
error kind, even if its message is empty. A failure never returns a partial snapshot.

<a id="todays-lunar-date"></a>
### Today's Lunar Date

Use `/date` to choose the fixed UTC+8 civil date, then convert it with the selected algorithm; "today" here does not
depend on the host time zone. Destructure only the year, month, and day: the full civil record contains `fraction`,
which a Gregorian date input must not carry.

```js
import * as celestial from "@0xf3cd/celestial";
import { dateToCivilAtOffset } from "@0xf3cd/celestial/date";

await celestial.init();
const { year, month, day } = dateToCivilAtOffset(new Date(), 480);
const gregorian = { year, month, day };
const lunar = celestial.lunar.fromGregorian("algo3", gregorian);
console.log(gregorian, lunar);
```

The date bridge does not convert clock UTC to UT1 or TT. For the lunar date basis and algorithm year domains, see
[Lunar Algorithms](#lunar-algorithms).

<a id="next-jieqi"></a>
### Next Jieqi and Remaining Days

The supplied cutoff is **2026-12-31 00:00 UT1**. No `Date` timestamp is treated as UT1.

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const cutoffUt1 = { year: 2026, month: 12, day: 31, fraction: 0 };
const cutoffJd = celestial.time.ut1ToJd(cutoffUt1);
const upcoming = [2026, 2027]
  .flatMap((year) => Object.values(celestial.Jieqi).map((jieqi) => {
    const event = celestial.jieqi.moment(year, jieqi);
    return { ...event, jdUt1: celestial.time.ut1ToJd(event.momentUt1) };
  }))
  .filter((event) => event.jdUt1 > cutoffJd)
  .sort((a, b) => a.jdUt1 - b.jdUt1)[0];
if (upcoming === undefined) throw new Error("No later Jieqi in the queried years.");
console.log(celestial.jieqi.name(upcoming.jieqi));
console.log("UT1:", upcoming.momentUt1);
console.log("Remaining UT1 days:", upcoming.jdUt1 - cutoffJd);
```

This enumerates all terms in both years and orders them by moment, not enum value. Remaining days are fractional
UT1 days. `momentUt1` is UT1, not UTC or a UTC+8 wall clock. This is a fixed contemporary query across one year
boundary, not a search algorithm for the entire API domain; the query-year domain and failure conditions of
`jieqi.moment()` are listed under [API Contract](#api-contract).

<a id="moon-illumination-and-jieqi-moment"></a>
### Moon Illumination and a Jieqi Moment

`2448724.5` is a JDE on TT. The Jieqi result's `momentUt1` is a UT1 civil moment, not a UTC+8 display time.

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();

const moon = celestial.moon.illumination(2448724.5);
const lichun = celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN);
console.log(moon.fraction, lichun.momentUt1, celestial.jieqi.name(lichun.jieqi));
```

<a id="gregorian-lunar-round-trip"></a>
### Gregorian / Lunar Round Trip

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const result = celestial.lunar.fromGregorian("algo3", { year: 2026, month: 8, day: 15 });
console.log(result);
console.log(celestial.lunar.toGregorian("algo3", result));
```

<a id="initialization"></a>
## Initialization and Browser Deployment

Concurrent and repeated `init()` calls share one initialization promise, resolving to `undefined`. A failed load
rejects with the loader's original error, and a later explicit call may retry. Before initialization completes,
namespace methods throw `CelestialError` before argument validation, with `operation` set to the public method name
and `recorded === false`, even for zero-count requests.

For browser deployment, make sure the package's `.wasm` asset is reachable at the URL your build output points to,
and have the static server send it with `Content-Type: application/wasm`.

<a id="api-contract"></a>
## API Contract

The root entry provides the `time`, `sun`, `moon`, `jieqi`, `lunar`, and `chart` namespaces, and the
package ships TypeScript declarations. The package uses only the `jieqi` / `lunar` names, with no `solarTerms` /
`lunarCalendar` aliases. Model and Moon phase options are string-literal unions such as `"full"` and
`"algo3"`. There is no logging configuration or `init` configuration argument. Independent position, equatorial,
house, event, capability and model-text queries are not public JavaScript APIs.
`Jieqi` is a frozen object of 24 named constants, from `Jieqi.LICHUN = 0` to
`Jieqi.DAHAN = 23`, spelled as in the Python package. The TypeScript type `Jieqi` is the matching
`0 | 1 | ... | 23` union; a plain `number` variable must be narrowed before it is passed to `jieqi.moment()` or
`jieqi.name()`. `jieqi.name()` returns the Chinese name, for example `"立春"` for `Jieqi.LICHUN`.

Keep time scales and units distinct:

- Operation names for JD inputs and outputs distinguish UT1 from JDE (TT).
- `jieqi.moment()` returns `{ jieqi, momentUt1 }`, using fixed Algo5 for TT-to-UT1 even if a selectable-model default
  changes. Its civil moment is UT1, not UTC or a UTC+8 wall clock; the same
  instant shown in UTC+8 can fall on a different date. Establish the time-scale conversion before displaying it as
  UTC or a fixed-offset time. The returned UT1 year can differ from the requested year.
- `sun.apparentSolarTime()` takes a UTC civil moment and an east-positive geographic longitude in `[-180, 180]`
  degrees.
- Angle results are in degrees. Sun distance is `distanceAu` in AU. Moon distance from `moon.apparentGeocentricCoordinate()` is
  in kilometres; `chart.snapshot()` gives every distance, the Moon's included, in AU.
- `time.deltaT()` returns seconds.
- The equation of time is in degrees of hour angle; multiply by 240 for seconds of time.
- `sun.apparentGeocentricCoordinate(jde)` and `moon.apparentGeocentricCoordinate(jde)`
  each return one coordinate record for the given TT-based JDE.

`jieqi.moment(year, index)` accepts a Gregorian year in `[401, 32766]` and a Jieqi index in `[0, 23]`.
Even with in-range input, it throws `CelestialError` when the native calculation cannot produce a unique moment.
`sun.longitudeCrossings(year, longitudeDeg)`, `moon.phaseMoments(year, phase)`, and `moon.newMoonsInYear(year)`
accept Gregorian years in `[1, 32766]`.

`time.localApparentSiderealTime(jdUt1, longitudeDeg)` accepts a finite JD on UT1 whose Gregorian year is in
`[401, 32766]`, and a finite, east-positive geographic longitude in `[-180, 180]` degrees. It returns degrees in
`[0, 360)`. The native boundary checks the year range; leaving it throws `RangeError`, just as the JavaScript
finiteness and longitude checks do. This operation retains fixed Algo5 ΔT and fixed IAU 1980 nutation.

`sun.longitudeCrossings(year, longitudeDeg)` accepts a finite apparent geocentric solar longitude in `[0, 360)`
degrees. It returns an array of TT-based JDEs, or `[]` when the Sun does not reach that longitude in the year.

After initialization, wrong shapes and types throw `TypeError`. Invalid input or a declared-domain rejection
throws `RangeError`, whether found in JavaScript or native code. Computation/resource/internal failures and
malformed native success records throw `CelestialError`, whose `operation` is the public method name and whose
`recorded` is true for a recognized native error kind, even when its message is empty; binding-local failures set
it to false. A legitimate absence remains `null` or `[]`.

Finite position inputs can still fail computation; coordinate validity promises neither physical plausibility
nor accuracy at remote epochs. There is no new universal position-year or obliquity cutoff. Caller-owned WASM
output buffers are limited to 16 MiB by the binding.

`moon.newMoonsAfter(jde, count)` accepts `count` in `[0, 4096]`; zero returns `[]`.
The cap keeps the WASM output buffer within 32 KiB.

<a id="date-records"></a>
### Date Records

The three input kinds below are mutually exclusive, both in TypeScript and at runtime:

| Type | Required own fields | Excluded fields |
|---|---|---|
| `GregorianDate` | `year`, `month`, `day` | `fraction`, `isLeap` |
| `CivilDateTime` | `year`, `month`, `day`, `fraction` | `isLeap` |
| `LunarDate` | `year`, `month`, `day`, `isLeap` | `fraction` |

Civil moments and Gregorian dates need an integer year in `[1, 32767]` and a month and day that form a valid
Gregorian date; a lunar year and date must exist in the selected algorithm. `fraction` is a finite day fraction in
`[0, 1)`. The runtime allows unrelated extra properties, but rejects an excluded field even when it is inherited or
set to `undefined`. With TypeScript's `exactOptionalPropertyTypes` enabled, type checking also rejects an explicit
`undefined` on the optional `never` fields.

`CivilDateTimeResult` adds an integer `hour` and `minute` and a possibly fractional `second` to `CivilDateTime`.
`time.jdeToUt1()`, `sun.apparentSolarTime()`, and `JieqiMoment.momentUt1` use this output.
These fields are derived from the day fraction and are not rounded to milliseconds; `second` counts seconds, not
milliseconds. A result can still be passed as a civil-moment input: functions read `fraction`, not the derived clock
fields. The operation name, not this shared record type, says which time scale applies.

Record inputs are recognized by shape: explicit own fields and excluded tags decide the kind, not the constructor,
and `Date` methods or timestamps are never read. A plain `Date` lacks the required fields; use the date bridge below
to turn a timestamp into civil fields. A civil-moment result or a lunar date cannot be passed directly to
`lunar.fromGregorian()`; choose the date basis first, then build a `GregorianDate` explicitly.

<a id="date-bridge"></a>
### Fixed-Offset Date Bridge

`@0xf3cd/celestial/date` is a pure calendar bridge with no runtime dependencies. It does not import the root entry or
load WASM, and needs no `init()`. Offsets are minutes east of UTC; `480` selects fixed UTC+8.

```js
import { civilAtOffsetToDate, dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const date = new Date("2024-02-09T16:00:00.123Z");
const eastEight = dateToCivilAtOffset(date, 480);
const restored = civilAtOffsetToDate(eastEight, 480);
console.log(eastEight);
console.log(restored.toISOString(), restored.getTime() === date.getTime());
```

Here `2024-02-09T16:00:00.123Z` becomes the UTC+8 civil date 2024-02-10, 00:00:00.123. The same-offset round trip
preserves the `Date` millisecond timestamp, so the last line ends with `true`.

| Function | Input | Output |
|---|---|---|
| `dateToCivilUtc(date)` | `Date` | `CivilDateTimeResult` in UTC |
| `civilUtcToDate(civil)` | `CivilDateTime` in UTC | `Date` |
| `dateToCivilAtOffset(date, offsetMinutesEast)` | `Date`, fixed offset | `CivilDateTimeResult` at that offset |
| `civilAtOffsetToDate(civil, offsetMinutesEast)` | `CivilDateTime` at a fixed offset | `Date` |

`offsetMinutesEast` is a safe integer in `[-1439, 1439]`, in minutes, positive east of UTC.
Conversion uses epoch arithmetic and UTC fields, never the host's local time zone, `Intl`, locale-dependent parsing,
IANA zones, or DST rules.

Converting a `Date` to a civil moment keeps the host value's millisecond resolution; the fractional `second` carries
no extra sub-millisecond information. Converting a civil moment to a `Date` rounds the non-negative local time of day
to the nearest millisecond, rounds an exact half millisecond toward the later civil moment, and handles carry into
the next day explicitly. For any representable `Date` whose civil year at the chosen offset is in `[1, 32767]`, a
`Date -> civil -> Date` round trip at the same offset preserves `getTime()` exactly.

The year bounds constrain the local civil record, not necessarily the UTC value that carries it. Both the input local
year and the local year after rounding carry must be in `[1, 32767]`; rounding the end of local year 32767 into 32768
is rejected. At the offset edges, the returned UTC `Date` can have year 0 or 32768 and still converts back with the
same offset. The UTC-only functions apply the year domain to the UTC civil fields.

Wrong types, missing civil fields, mixed date kinds, and non-integer date or offset fields throw `TypeError`.
An invalid `Date`, an invalid Gregorian date, non-finite or unsafe numbers, an out-of-range day fraction or offset,
and a local year outside the domain throw `RangeError`.

The bridge converts no time scales and does not handle leap seconds; it provides neither `Date` to JDE nor UTC to UT1
conversion. Do not pass `momentUt1` to `civilUtcToDate()` as though it were UTC: UT1 is not UTC, and the library
does not model observed DUT1.
[Time scales](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md#time-scales) explains how far
apart the two can be.

<a id="lunar-algorithms"></a>
### Lunar Algorithms

Gregorian inputs and outputs are calendar-date labels on the selected algorithm's basis, not instants.
Choose the source and compatibility role you need, not an assumed accuracy ranking:

| Algorithm | Source and status | Lunar-year domain | Selection role |
|---|---|---|---|
| `"algo1"` | Published Hong Kong Observatory date labels, stored as a table | `[1901, 2099]` | HKO almanac compatibility |
| `"algo2"` | Computed from VSOP87D and truncated ELP2000-82B | `[410, 2500]` | Astronomical construction over the wider computed window |
| `"algo3"` | Baked hybrid: algo1/HKO in 1901-2099, algo2-generated dates elsewhere | `[1600, 2199]` | Table lookup with HKO compatibility in the overlapping years |

Algo2 assigns dates by rendering TT moments in the library's modelled UTC
([time scales](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md#time-scales)) and then applying
a fixed eight-hour eastward offset. Algo1 preserves HKO's published labels rather than imposing this rule on their
history; algo3 inherits the basis of each source slice. The 2500 ceiling is the computed algorithm's enforced
civil-date domain, not a physical limit of VSOP87D or ELP2000-82B.

`lunar.supportedYearRange(algorithm)` queries the native algorithm's inclusive range of lunar years, not Gregorian
January-to-December bounds. `lunar.yearInfo(algorithm, year)` returns the Gregorian `firstDay`, the traditional
`leapMonth` number or `null`, and `monthLengths` in calendar order, with a leap month directly after its ordinary
namesake. `lunar.toGregorian()` takes a traditional month number in `[1, 12]` and an explicit `isLeap` boolean, not a
positional month index. The native operation validates the selected algorithm's lunar year and date;
the JavaScript package keeps no separate range table or cache.

All four lunar methods require `await init()` first; a call before initialization completes throws `CelestialError`
with `recorded === false`, before argument validation. After initialization:

- `TypeError` means a wrong input type, a missing own field, an excluded date-kind tag, a non-integer date field, or
  a non-boolean `isLeap`.
- `RangeError` means an unknown algorithm, a non-finite or unsafe number, an invalid Gregorian date (including a year
  outside `[1, 32767]`), a lunar year outside the native range, or a lunar month/day outside `[1, 12]` / `[1, 30]`.
- `RangeError` also covers native invalid-input rejections, including a Gregorian date label outside the selected
  algorithm's coverage or a lunar leap-month/day combination that does not exist.
- `CelestialError` means computation/resource/internal failure or a malformed native success record, with
  `operation` naming the caller, such as `"lunar.yearInfo"`, rather than an internal range query.

For example, `lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 })` passes Gregorian validation, but algo1
cannot represent that date, so it throws `RangeError`; `lunar.yearInfo("algo1", 1900)` also throws `RangeError`.

<a id="delta-t-models"></a>
### Delta T Models

`time.deltaT(year, model)` returns TT minus UT1 in seconds for a finite decimal Gregorian year.
The default is the current project model; the named older models allow historical comparison:

| Model | Source and status | Hard year domain | Selection role |
|---|---|---|---|
| `"default"`, `"algo5"` | Current project model: algo2 before 2005; IERS Bulletin A fit from 2005 through the inclusive `algo5::LAST_OBSERVATION_YEAR` (about 2026.41); then anchored, corrected Morrison et al. (2021) integrated-LOD extrapolation | No model-specific bound | Current project calculations |
| `"algo1"` | Xu Jianwei's 2008 model, frozen | `year >= -4000` | Comparison with that historical model |
| `"algo2"` | Espenak and Meeus, NASA/TP-2006-214141, frozen | No model-specific bound | NASA polynomial comparison; live pre-2005 branch of algo3, algo4, and algo5 |
| `"algo3"` | Fred Espenak's 2014 eclipse canon, frozen | `year < 3000` | Comparison with that later polynomial model |
| `"algo4"` | IERS Bulletin A observations and USNO predictions, frozen | `year < 2035` | Comparison with the previous project fit/prediction model |

"Frozen" describes model maintenance, not dead code: algo2 still supplies the pre-2005 values used by the current
default. A hard domain is an input contract, not an accuracy claim. Fitted residuals and test tolerances are not
statistical error guarantees, and an unbounded model does not promise useful accuracy at arbitrary years.

<a id="license"></a>
## License

Project-authored package material is licensed under MIT.

Bundled third-party components retain their own terms in `THIRD_PARTY_NOTICES.txt`.
