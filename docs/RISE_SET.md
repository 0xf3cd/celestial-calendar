# Rise, transit and set

Use `solar_rise_set`, `lunar_rise_set` and `planet_rise_set` in Python,
`riseSet.solar/lunar/planet` in JavaScript, or the corresponding
`celestial_*_rise_set` C exports. They return the existing C++ daily summaries,
with TT event times, explicit absence and independent polar topology.

## Choose the date and altitude convention

| Query | Meaning of the Gregorian UT1 date | Coordinates and rise/set altitude |
|---|---|---|
| Solar | Select the transit nearest local mean noon. Rise/set may lie outside this UT1 date. | Geocentric centre; standard h0 = −50′, twilight −6°/−12°/−18°. |
| Lunar | Events in [0h,24h) UT1. | Geocentric centre; h0 = 0.7275 Π − horizon refraction, with Π evaluated at midday. |
| Planetary | Events in [0h,24h) UT1. | Airless topocentric centre from a WGS84 site; standard h0 = −34′ includes horizon refraction. |

Longitude is east-positive. For the solar query, +180° and −180° can select
transits one day apart. A UT1 date is neither a UTC-midnight boundary nor the
observer's local calendar date. To display an event, convert its TT time first,
then apply the application's time-zone rules.

Solar queries take h0 directly; the C++ `sun::h0_from` weather helper has no C,
Python or JavaScript counterpart. Lunar queries take Bennett or Saemundsson
horizon refraction, defaulting in the bindings to Bennett at 15°C/1013.25 hPa.
Their weather domain is finite temperature > −273°C and finite positive pressure,
wider than the observer position/interval-altitude application window.
The lunar result retains these actual inputs, not an effective numeric h0.

Only planets take elevation, in metres above the WGS84 ellipsoid. It changes
the site vector, not the horizon plane. Planetary parallax is already in the
coordinates; do not apply the lunar geocentric threshold to that direction.

## Python: display a polar-day result

```python
from datetime import datetime, timedelta, timezone
from celestial_calendar import GregorianDate, GeoLocation, jde_to_utc, solar_rise_set

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
```

Use `lunar_rise_set(date_ut1, location, refraction=RefractionParams(...))` for
the Moon. For a planet, use
`planet_rise_set(Target.MARS, date_ut1, Observer(latitude_deg, longitude_deg, elevation_m))`.
Python result dataclasses are frozen, with `None` for missing events.

## JavaScript: display the same result

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
```

The `/date` helper constructs an ECMAScript `Date` from the library's civil UTC
result. Use `Intl.DateTimeFormat` with the desired time zone for local display.
It does not change which native UT1 window was queried. Results and their
atmosphere objects are frozen and own their values.

`riseSet.lunar(dateUt1, location, { refraction: { model: "bennett" } })`
and `riseSet.planet("mars", dateUt1, site, { h0Deg: -34 / 60 })` select the other
two operations. TypeScript distinguishes the three result types; a solar
transit is required, while a lunar or planetary transit can be `null`.

## C

The standalone [C11 example](examples/rise_set/main.c) calls all three exports,
handles missing events before reading slots, and converts TT with
`celestial_jde_to_utc`. It prints UTC calendar fields and day fractions;
formatting and time zones belong to the application.

Compile it against `celestial.h` and the built shared library. The native test
suite compiles that same file as C and executes it through `RiseSetCabi`.
Each return value has a `valid` flag. On failure, read
`celestial_last_error_kind` and `celestial_last_error` before another recording
call clears them. A successful record uses `CELESTIAL_PRESENT_RISE`,
`CELESTIAL_PRESENT_TRANSIT` and `CELESTIAL_PRESENT_SET`; absent slots are zero.

## Missing events, selection and domains

`none`, `day` and `night` describe polar topology relative to the selected
altitude. Missing rise, set or transit alone does not establish polar day/night.
On lunar and planetary days with two crossings of the same kind, the **later**
crossing is returned; the first selected transit is retained. Native strict
crossing comparisons omit an event landing exactly on a UT1 midnight from both
adjacent days. The bindings preserve this boundary exception.

For lists rather than one representative of each kind, use the
[interval-event queries](ALTITUDE_EVENTS.md), whose sampling can miss events.
Daily summaries are not lists of every event.

All daily queries use fixed IAU1980 axes and the default Algo5 ΔT model. Each
result reports concrete position-source, ΔT and nutation identities. C capability
text (`CELESTIAL_OPERATION_RISE_SET`) and Python
`capability_text(CapabilityOperation.RISE_SET)` describe this policy. Use
EQUATORIAL capabilities for the position providers' TT domains. Pluto has the
hard interval [2409543.875,2488069.125) TT; every internal sample must fit,
including transit estimates and derivative samples outside the input day.
Midnight inside that domain does not guarantee a successful day query.

Gregorian input years are [1,32767], subject to all internal time conversions
and provider admission. The library's civil UTC conversion starts at
1972-01-01; earlier event displays can use `jde_to_ut1` instead. Future UTC
holds the last known leap-table offset. The Python/JavaScript date adapters
also have their host date-range limits.

Invalid inputs raise Python `ValueError` or JavaScript `RangeError`; wrong
binding argument types raise `TypeError`. Numerical/resource failures and
malformed successful native records raise `CelestialError`. Missing events
are successful results, not failures.
