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
returns its state plus events. `upper_transits(observer, target, start_jde_tt, end_jde_tt)`
takes a physical target without a level or weather and returns a tuple of `UpperTransit` values.
`observing_windows` alone accepts a condition iterable. Its result retains every condition
state and companion event, even when the intersection is empty. Values are frozen dataclasses
and tuples.

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
Angles and elevation retain the library's physical types. The equivalent program is:

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
- Condition state retains the original target, sense, level convention and weather, the resolved
  geometric threshold, source and clock identity. Event `condition_index`/`conditionIndex` refers to
  that ordered condition list. Transit records carry their own target, source and clock.
- Thresholds refer to body centres, with no solar/lunar limb or geocentric-parallax offset.
  Geometric levels must be strictly inside `(−90°,90°)`. Refraction resolves an attainable apparent
  threshold with the selected model; it does not extrapolate through an uncertain image endpoint.
  The proved weather tier is `[−90,60]°C` and `[300,1100] hPa`. Outside the proof tier, a query can
  fail numerically rather than return an extrapolated threshold.
- At an exact geographic pole, upper passage uses the meridian defined by the supplied longitude.
  An unavailable azimuth does not by itself prevent an altitude or transit query.

## Admission and bounded work

The calendar admits `[1867522.5,13689325.5)` in JDE(TT); an interval may end at the exclusive
upper bound. Only selected physical providers constrain it. In particular, Pluto requires
`[2409543.5,2488069.5)`, but selecting the Sun, Moon or a VSOP87 planet does not inherit Pluto's
window or a chart/longitude-rate domain. Additional proof samples and closed-end certificates
must also be available. Admission alone therefore does not promise a successful query.

Each complete query shares these fixed work limits across conditions, clock pieces, inverse levels
and event searches:

| Limit | Value |
|---|---:|
| Charged evaluations | 8,192 |
| Candidate/proof-piece slots | 2,048 |
| Input conditions | 2,048 |
| Event time resolution | `2^-20` TT day (about 0.0824 second) |
| Inverse-level resolution | `2^-30` degree |

These are work and isolation resolutions, **not physical accuracy bounds**. Canonical quarter-day
cells, any preceding-cell companion, clock partitions and root candidates share the candidate
allowance. There is no guaranteed successful span or condition count below these ceilings:
an ordinary long interval may exhaust work, and a short near-contact may remain unresolved.
The resolutions are not relaxed to make a query fit. Python limits result storage to 64 MiB;
JavaScript limits caller-owned WASM buffers to 16 MiB. Their condition collections are bounded
before native execution.

Successful queries certify the complete event census at the declared resolution. An unresolved
count, indistinguishable close pair, unavailable certificate or exhausted work fails the **whole
query**, including any window intersection. A known false condition does not permit skipping an
unresolved companion. Failure never returns a partial result or an uncertified empty success.

Malformed/out-of-domain inputs throw `std::invalid_argument` in C++, `ValueError` in Python and
`RangeError` in JavaScript; wrong high-level types/shapes use `TypeError`. Unresolved numerical work
throws `std::runtime_error` in C++ and `CelestialError` in the bindings. Binding-side resource
failures have `recorded=false`; native-reported failures have `recorded=true`. Error-kind identity
remains authoritative even if no message could be allocated.

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

Unresolved work sets `CELESTIAL_ERROR_COMPUTATION`; allocation failure sets
`CELESTIAL_ERROR_RESOURCE`. Both leave total zero and slots untouched. Read
`celestial_last_error_kind()` to distinguish an empty successful transit census from failure.
