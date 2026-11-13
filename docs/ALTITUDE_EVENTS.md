# Interval altitude events and observing windows

Use these queries to find every centre-altitude crossing in a half-open TT interval,
all upper meridian passages, or the times when several altitude conditions hold together.
They support the Sun, Moon, Mercury, Venus, Mars, Jupiter, Saturn, Uranus, Neptune and Pluto.
Mathematical lunar nodes are not physical targets.

## Python

This example asks when Mars is above 20° and the Sun is below −18°. The site uses WGS84
latitude and east-positive longitude in degrees, with ellipsoidal height in metres.
Each Python or JavaScript block on this page is a standalone program.

```python
from celestial_calendar import (
  AltitudeCondition,
  AltitudeSense,
  GeometricLevel,
  Observer,
  Target,
  observing_windows,
)

conditions = (
  AltitudeCondition(Target.MARS, GeometricLevel(20.0), AltitudeSense.ABOVE),
  AltitudeCondition(Target.SUN, GeometricLevel(-18.0), AltitudeSense.BELOW),
)
result = observing_windows(Observer(0.0, 95.0, 0.0), conditions, 2451545.0, 2451545.25)
assert len(result.conditions) == 2
for window in result.windows:
  print(window.start_jde_tt, window.end_jde_tt)
```

`altitude_events(observer, condition, start_jde_tt, end_jde_tt)` takes one condition and
returns its state in `result.condition` and its events in `result.events`. Read the initial
truth as `result.condition.initially_satisfied`. `upper_transits(observer, target, start_jde_tt, end_jde_tt)`
takes a physical target without a level or weather and returns a tuple of `UpperTransit` values.
`observing_windows` is the only query that accepts a condition collection. Read its ordered
states, companion events and intersected spans through `result.conditions`, `result.events`
and `result.windows`, respectively, even when the intersection is empty. Values are frozen
dataclasses and tuples.

For an apparent threshold, replace `GeometricLevel(20.0)` with
`RefractedLevel(20.0, RefractionParams(RefractionModel.BENNETT))`, importing those types
from `celestial_calendar`. `SAEMUNDSSON` is also supported. Weather defaults are 15°C and
1013.25 hPa; specify temperature and pressure in `RefractionParams` to change them.

## JavaScript and TypeScript

```js
import { init, observer } from "@0xf3cd/celestial";

await init();
const site = { latitudeDeg: 0, longitudeDeg: 95, elevationM: 0 };
const result = observer.observingWindows(site, [
  { target: "mars", level: { kind: "geometric", altitudeDeg: 20 }, sense: "above" },
  { target: "sun", level: { kind: "geometric", altitudeDeg: -18 }, sense: "below" },
], 2451545, 2451545.25);
if (result.conditions.length !== 2) throw new Error("missing condition state");
for (const window of result.windows) console.log(window.startJdeTt, window.endJdeTt);
```

`observer.altitudeEvents(site, condition, startJdeTt, endJdeTt)` and
`observer.upperTransits(site, target, startJdeTt, endJdeTt)` have the corresponding single-query
shapes. TypeScript uses `GeometricLevel | RefractedLevel` and readonly result collections.
For refraction, use a level such as
`{ kind: "refracted", altitudeDeg: 20, refraction: { model: "bennett" } }`.
A geometric level cannot carry refraction fields. Returned objects, nested conditions and arrays
are frozen copies, independent of later queries, mutable inputs and WASM memory growth.

## C++

Include `altitude_events.hpp`; the functions and condition types belong to `astro::observer`.
Angles use `toolbox::AngleDeg`; elevation uses `toolbox::DistanceKm`, in kilometres.
C, Python and JavaScript take elevation in metres. The equivalent program is:

```cpp
#include <array>
#include <cassert>
#include "altitude_events.hpp"

auto main() -> int {
  using namespace astro;
  const observer::Observer site {
    .location = { .latitude = toolbox::AngleDeg { 0.0 }, .longitude = toolbox::AngleDeg { 95.0 } },
    .elevation = toolbox::DistanceKm { 0.0 },
  };
  const std::array conditions {
    observer::AltitudeCondition {
      .target = ephemeris::Target::MARS,
      .level = observer::GeometricLevel { .altitude = toolbox::AngleDeg { 20.0 } },
      .sense = observer::AltitudeSense::ABOVE,
    },
    observer::AltitudeCondition {
      .target = ephemeris::Target::SUN,
      .level = observer::GeometricLevel { .altitude = toolbox::AngleDeg { -18.0 } },
      .sense = observer::AltitudeSense::BELOW,
    },
  };
  const auto result = observer::observing_windows(site, conditions, 2451545.0, 2451545.25);
  assert(result.conditions.size() == 2);
  for (const auto& window : result.windows) {
    assert(window.start_jde_tt < window.end_jde_tt);
  }
}
```

The other functions are `altitude_events(site, condition, start_jde_tt, end_jde_tt)` and
`upper_transits(site, target, start_jde_tt, end_jde_tt)`. `AltitudeLevel` is a variant of
`GeometricLevel` and `RefractedLevel`; geometric input has no unused weather. Conditions and
transits retain their position source and fixed Delta T model. The daily rise/set APIs are unchanged.

## Time, levels and result meaning

- Times are **JDE(TT)**, not JD(UT1), civil UTC or local clock fields. The site is fixed throughout
  the query. The computation uses fixed ALGO5 TT-to-UT1 conversion and IAU 1980 nutation.
- Intervals are `[start,end)`: start events are included, end events excluded. `initially_satisfied`
  (JavaScript `initiallySatisfied`) already includes start-event replay; do not apply those events twice.
- Events are chronological, with condition index breaking equal-time ties. The direction is
  increasing/decreasing **altitude**, irrespective of the condition's above/below sense. A certified
  tangent does not change the side state; rounded residual zero alone does not certify contact.
- Windows are maximal positive-duration operational spans. They are not strict point sets with
  holes at isolated equalities. Equal-time events apply atomically; irrelevant crossings and tangents
  do not fragment a window. An empty conjunction returns the whole requested interval.
- Event `condition_index`/`conditionIndex` selects its state in `result.conditions` (or
  `result.condition` for a single altitude query). Transit records carry their own target,
  source and clock.
- Thresholds refer to body centres, with no solar/lunar limb or geocentric-parallax offset.
  Geometric levels must be strictly inside `(−90°,90°)`. Refraction resolves an attainable apparent
  threshold with the selected model.
  The proved weather tier is `[−90,60]°C` and `[300,1100] hPa`. Outside the proof tier, a query can
  fail numerically rather than return an extrapolated threshold.
- At an exact geographic pole, upper passage uses the meridian defined by the supplied longitude.

Bennett accepts apparent inputs from 0° to 90°, subject to a resolved geometric upper endpoint.
Saemundsson's apparent endpoints are computed using the selected weather at the model's horizon
and at 90° geometric altitude. Values definitely outside the selected model's range are invalid
input; uncertain endpoints fail numerically. Keep twilight thresholds such as −6° geometric
rather than passing them to either refraction model.

## Sunrise, twilight and civil-clock display

The conventional Sun thresholds are geometric centre altitudes. Civil, nautical and astronomical
twilight use −6°, −12° and −18°. An approximate upper-limb sunrise/sunset convention uses −50′
(`-50.0 / 60.0` degrees), combining 34′ of standard horizon refraction with a fixed 16′ solar
semidiameter, as in Meeus Chapter 15 and the existing
[`rise_set::sun` constants](../src/astro/rise_set.hpp). Pass that combined threshold as a
`GeometricLevel`; applying a refraction model again would count the atmospheric correction twice.
These conventions assume an unobstructed horizon and do not model terrain, horizon dip or a
changing solar semidiameter. A lunar geocentric rise/set threshold already contains parallax and
must not be applied unchanged to this topocentric direction.

Convert each civil boundary to TT separately. This example searches between two UTC midnights
and converts the returned TT moments back to UTC for display at a fixed UTC+08:00 offset.
`CivilDateTime.fraction` is a fraction of the day, with 0.0 denoting midnight:

```python
from datetime import datetime, timedelta, timezone

from celestial_calendar import (
  AltitudeCondition,
  AltitudeSense,
  CivilDateTime,
  GeometricLevel,
  Observer,
  Target,
  altitude_events,
  jde_to_utc,
  utc_to_jde,
)

start = utc_to_jde(CivilDateTime(2026, 11, 7, 0.0))
end = utc_to_jde(CivilDateTime(2026, 11, 8, 0.0))
condition = AltitudeCondition(Target.SUN, GeometricLevel(-6.0), AltitudeSense.ABOVE)
result = altitude_events(Observer(30.0, -77.0, 0.0), condition, start, end)
assert len(result.events) == 2
for event in result.events:
  utc = jde_to_utc(event.jde_tt)
  instant = datetime(utc.year, utc.month, utc.day, tzinfo=timezone.utc) + timedelta(days=utc.fraction)
  print(event.direction.value, instant.astimezone(timezone(timedelta(hours=8))).isoformat())
```

For a named local time zone, resolve its two local civil boundaries independently and convert
each to UTC before calling `utc_to_jde`. Daylight-saving changes and leap seconds mean a civil
day need not span exactly one TT day. The fixed-offset conversion above changes only the display;
the search and returned event times remain TT. See [UTC conversion rules](UTC_CONVERSIONS.md).

## Admission and bounded work

The calendar admits `[1867522.5,13689325.5)` in JDE(TT); an interval may end at the exclusive
upper bound. Only selected physical providers constrain it. In particular, Pluto requires
`[2409543.5,2488069.5)`, but selecting the Sun, Moon or a VSOP87 planet does not inherit Pluto's
window or a chart/longitude-rate domain. The library ships precomputed source and clock bounds,
called certificates; callers do not supply them. The search needs these bounds over each complete,
closed quarter-day cell. Admission alone therefore does not
promise a successful query.

The current clock certificates cover the closed TT range `[2378495.0,2524595.0]` (roughly 1800–2200).
Queries use complete quarter-day cells, plus the preceding cell when the start is exactly aligned;
all of those cells need certificates. Two additional closed clock windows are
`[10000000.5,10000000.75]` and `[13689000.5,13689000.75]`, with source certificates for Mars and Jupiter
in both, and Saturn in the first only. These are certificate-coverage ranges, not narrower provider
admission rules or guarantees that a query succeeds. The
[shipped source and clock tables](../src/astro/altitude_event_bounds.hpp) record this coverage.

Each complete query shares these fixed work limits across conditions, clock pieces, inverse levels
and event searches:

| Limit | Value |
|---|---:|
| Charged evaluations | 8,192 |
| Candidate/interval-piece slots | 2,048 |
| Input conditions | 2,048 |
| Event time resolution | `2^-20` TT day (about 0.0824 second) |
| Inverse-level resolution | `2^-30` degree |

These are work and isolation resolutions, **not physical accuracy bounds**. Fixed quarter-day
cells, any preceding-cell companion, clock partitions and root candidates share the candidate
allowance. There is no guaranteed successful span or condition count below these ceilings:
an ordinary long interval may exhaust work, and a short near-contact may remain unresolved.
Slow altitude crossings, including those near a geographic pole or culmination, can fail to
resolve at the fixed time resolution even when an ordinary mid-latitude crossing succeeds.
The resolutions are not relaxed to make a query fit. Python limits result storage to 64 MiB;
JavaScript limits caller-owned WASM buffers to 16 MiB. Their condition collections are bounded
before native execution.

Successful queries certify the complete event census at the declared resolution. An unresolved
count, indistinguishable close pair, unavailable certificate or exhausted work fails the **whole
query**, including any window intersection. A known false condition does not permit skipping an
unresolved companion. Failure never returns a partial result or an uncertified empty success.

Input checks report malformed/out-of-domain values as `std::invalid_argument` in C++, `ValueError` in Python and
`RangeError` in JavaScript; wrong high-level types/shapes use `TypeError`. Unresolved numerical work
throws `std::runtime_error` in C++ and `CelestialError` in the bindings. Binding-side resource
failures have `recorded=false`; native-reported failures have `recorded=true`. Error-kind identity
remains authoritative even if no message could be allocated.
Legal weather outside the proved tier is a numerical failure. If several aspects of a query are
invalid or unprovable, the first detected failure determines the kind; a proof failure can precede
an inverse-level input check.

## C count/fill stream

The [C header](../src/shared_lib/celestial.h) declares `celestial_altitude_events`,
`celestial_upper_transits` and `celestial_observing_windows`. Conditions are 40 bytes and tagged
records are 88 bytes on the supported native64/wasm32 ABIs. Existing refraction codes are retained:
NONE/BENNETT/SAEMUNDSSON = 0/1/2.

Call with null slots and capacity zero to obtain the total. On success, a fill writes exactly
`min(total,capacity)` records, preserves the full total and leaves the tail untouched. Condition
metadata appear first in input order, followed by chronological condition-associated events,
then windows. Transit queries return only transit records and may succeed with zero records.
Every unused field is zero. All fallible computation and preparation finish before count/slot
publication.

Invalid inputs set `CELESTIAL_ERROR_INVALID_INPUT`; unresolved work sets
`CELESTIAL_ERROR_COMPUTATION`; allocation failure sets `CELESTIAL_ERROR_RESOURCE`.
Each failure leaves total zero and slots untouched. Read
`celestial_last_error_kind()` to distinguish an empty successful transit census from failure.

```c
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "celestial.h"

int main(void) {
  const CelestialAltitudeCondition conditions[2] = {
    { .target_code = CELESTIAL_TARGET_MARS, .sense_code = CELESTIAL_ALTITUDE_SENSE_ABOVE,
      .refraction_code = CELESTIAL_REFRACTION_NONE, .level_deg = 20.0 },
    { .target_code = CELESTIAL_TARGET_SUN, .sense_code = CELESTIAL_ALTITUDE_SENSE_BELOW,
      .refraction_code = CELESTIAL_REFRACTION_NONE, .level_deg = -18.0 },
  };
  uint32_t total = 0;
  celestial_observing_windows(conditions, 2, 0.0, 95.0, 0.0, 2451545.0, 2451545.25, &total, NULL, 0);
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE) return 1;
  CelestialAltitudeRecord *slots = calloc(total, sizeof(*slots));
  if (total > 0 && slots == NULL) return 1;
  const uint32_t written = celestial_observing_windows(
    conditions, 2, 0.0, 95.0, 0.0, 2451545.0, 2451545.25, &total, slots, total
  );
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE) {
    free(slots);
    return 1;
  }
  assert(written == total);
  uint32_t windows = 0;
  for (uint32_t index = 0; index < written; ++index) {
    if (slots[index].tag == CELESTIAL_ALTITUDE_RECORD_WINDOW) {
      assert(slots[index].jde_tt < slots[index].end_jde_tt);
      ++windows;
    }
  }
  assert(windows == 1);
  free(slots);
  return 0;
}
```
