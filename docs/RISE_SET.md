# Rise, transit and set

Use `solar_rise_set`, `lunar_rise_set` and `planet_rise_set` in Python,
`riseSet.solar/lunar/planet` in JavaScript, or the corresponding
`celestial_*_rise_set` C exports. Each result gives rise, upper transit and set
as Julian ephemeris day numbers on the TT scale. Missing events are `None` in
Python and `null` in JavaScript; a successful solar query always has a transit.
The separate `polar` field reports polar day, polar night or neither.

## Choose the date and altitude convention

The threshold h0 is the altitude of the body's centre at rise and set.
Π is the Moon's horizontal parallax.

| Query | Meaning of the Gregorian UT1 date | Coordinates and rise/set altitude |
|---|---|---|
| Solar | The query picks the transit nearest 12h local mean time on this date. Rise/set may lie outside this UT1 date. | Geocentric centre; standard h0 = −50′, twilight −6°/−12°/−18°. |
| Lunar | Events in [0h,24h) UT1. | Geocentric centre; h0 = 0.7275 Π − horizon refraction, with Π evaluated at midday. |
| Planetary | Events in [0h,24h) UT1. | Airless topocentric centre from a WGS84 site; horizon refraction is included in the standard threshold h0 = −34′. |

Longitude is east-positive. For the solar query, +180° and −180° can select
transits one day apart: 12h local mean time is 0h UT1 at +180° and 24h UT1 at −180°.
A UT1 date is neither a UTC-midnight boundary nor the
observer's local calendar date. To display an event, convert its TT time first,
then apply the application's time-zone rules.

Solar queries take h0 directly, not weather parameters; the weather-to-h0
helper is C++-only. Lunar queries take Bennett or Saemundsson
horizon refraction, defaulting in the bindings to Bennett at 15°C/1013.25 hPa.
Their weather domain is finite temperature > −273°C and finite positive pressure,
whereas observer position and interval-altitude queries restrict weather to
[−90,60]°C and [300,1100] hPa.
The lunar result echoes the model, temperature and pressure used.

Only planets take elevation, in metres above the WGS84 ellipsoid. It changes
the observer's position; no horizon-dip correction is applied. Planetary parallax
is already in the coordinates, so do not reuse the Moon's geocentric h0 formula.

## Python: display a polar-day result

```python
from datetime import datetime, timedelta, timezone
from celestial_calendar import (
  GregorianDate,
  GeoLocation,
  Observer,
  RefractionModel,
  RefractionParams,
  Target,
  jde_to_utc,
  lunar_rise_set,
  planet_rise_set,
  solar_rise_set,
)

result = solar_rise_set(GregorianDate(2025, 6, 21), GeoLocation(69.65, 18.96))
print(result.polar.value)
for label, instant in (("rise", result.rise_jde_tt), ("transit", result.transit_jde_tt), ("set", result.set_jde_tt)):
  if instant is None:
    print(label, "absent")
    continue
  utc = jde_to_utc(instant)
  clock = datetime(utc.year, utc.month, utc.day, tzinfo=timezone.utc) + timedelta(days=utc.fraction)
  # Fixed UTC+02 is the display choice for this summer example.
  print(label, clock.astimezone(timezone(timedelta(hours=2))).isoformat())

date = GregorianDate(2025, 6, 21)
location = GeoLocation(69.65, 18.96)
twilight = solar_rise_set(date, location, h0_deg=-6.0)
moon = lunar_rise_set(date, location, refraction=RefractionParams(RefractionModel.BENNETT, 10, 1000))
mars = planet_rise_set(Target.MARS, date, Observer(69.65, 18.96, 100))
assert twilight.h0_deg == -6.0 and moon.refraction.pressure_hpa == 1000
assert mars.h0_deg == -34.0 / 60
```

The displayed result is `day`, with absent rise and set and a transit at
`2025-06-21T12:46:00.296234+02:00`. `jde_to_utc` converts each TT instant before
the application applies its display offset. The last three calls show civil
twilight (−6°), lunar weather at 10°C/1000 hPa, and Mars from a site 100 m high.
Python result dataclasses are frozen.

## JavaScript: display in UTC

```js
import { init, riseSet, time } from "@0xf3cd/celestial";
import { civilUtcToDate } from "@0xf3cd/celestial/date";

await init();
const result = riseSet.solar(
  { year: 2025, month: 6, day: 21 },
  { latitudeDeg: 69.65, longitudeDeg: 18.96 },
);
console.log(result.polar);
for (const [label, instant] of [
  ["rise", result.riseJdeTt], ["transit", result.transitJdeTt], ["set", result.setJdeTt],
]) {
  console.log(label, instant === null ? "absent" : civilUtcToDate(time.jdeToUtc(instant)).toISOString());
}

const date = { year: 2025, month: 6, day: 21 };
const location = { latitudeDeg: 69.65, longitudeDeg: 18.96 };
const twilight = riseSet.solar(date, location, { h0Deg: -6 });
const moon = riseSet.lunar(date, location, {
  refraction: { model: "bennett", temperatureC: 10, pressureHpa: 1000 },
});
const mars = riseSet.planet("mars", date, { ...location, elevationM: 100 });
if (twilight.h0Deg !== -6 || moon.refraction.pressureHpa !== 1000 || mars.h0Deg !== -34 / 60) {
  throw new Error("unexpected altitude or weather");
}
```

This prints the same transit instant as `2025-06-21T10:46:00.296Z`.
The `/date` helper constructs an ECMAScript `Date` from the library's civil UTC
result. Use `Intl.DateTimeFormat` with the desired time zone for local display.
The display time zone does not change the query's UT1 date or selected events. Results and their
atmosphere objects are frozen and own their values.

The last three calls use the same altitude, weather and elevation choices as
the Python example. TypeScript distinguishes the three result types; a solar
transit is required, while a lunar or planetary transit can be `null`.

## C

The standalone [C11 example](examples/rise_set/main.c) calls all three exports,
handles missing events before reading slots, and converts TT with
`celestial_jde_to_utc`. It prints UTC calendar fields and day fractions;
formatting and time zones belong to the application.

See [Building](BUILDING.md) for the shared library and header.
Each return value has a `valid` flag. On failure, read
`celestial_last_error_kind` and `celestial_last_error` before another recording
call clears them. On success, test `present_fields & CELESTIAL_PRESENT_RISE`
before reading `rise_jde_tt`; use `CELESTIAL_PRESENT_TRANSIT` and
`CELESTIAL_PRESENT_SET` for the other slots. Absent slots are zero.
The example prints `Sun polar topology: 1`, where 1 is `CELESTIAL_RISE_SET_POLAR_DAY`.

## Missing events, selection and domains

`none`, `day` and `night` describe polar topology relative to the selected
altitude. Missing rise, set or transit alone does not establish polar day/night.
For lunar and planetary queries, if the day contains two rises or two sets,
the later one is returned; if it contains two transits, the earlier one is
returned. A rise or set exactly at 0h UT1 is returned for neither neighbouring
day.

For lists rather than one representative of each kind, use the
[interval-event queries](ALTITUDE_EVENTS.md), whose sampling can miss events.
Daily summaries are not lists of every event.

All daily queries use fixed IAU1980 axes and the default Algo5 ΔT model. Each
result reports concrete position-source, ΔT and nutation identities. C capability
text (`CELESTIAL_OPERATION_RISE_SET`) and Python
`capability_text(CapabilityOperation.RISE_SET)` describe this policy. Use
EQUATORIAL capabilities for the position providers' TT domains; JavaScript has
no capability-query API. Pluto has the
hard interval [2409543.875,2488069.125) TT; every internal sample must fit,
including transit estimates and derivative samples outside the input day.
Midnight inside that domain does not guarantee a successful day query.
A date whose samples leave a provider's domain raises Python `ValueError` or
JavaScript `RangeError`.

Gregorian input years are [1,32767], subject to all internal time conversions
and provider admission. Event display has separate time-conversion limits:
`jde_to_utc` and `jde_to_ut1` accept TT in [1867522.5,13689325.5).
Before the leap-second table begins at 1972-01-01, `jde_to_utc` uses an Algo5
UT1 approximation for its civil fields; use `jde_to_ut1` to label that display
as UT1. JavaScript names these helpers `time.jdeToUtc` and `time.jdeToUt1`.
Future UTC holds the last known leap-table offset. The Python/JavaScript date
adapters also have their host date-range limits.

Invalid inputs raise Python `ValueError` or JavaScript `RangeError`; wrong
binding argument types raise `TypeError`. Numerical/resource failures and
malformed successful native records raise `CelestialError`. Missing events
are successful results, not failures.
