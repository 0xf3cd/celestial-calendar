# API Reference

This reference covers C++ declarations and the C interface in `celestial.h`.
Bulk coefficient data and source listings are omitted.
Browse **Namespaces**, **Classes**, or **Files**, or search for a function name.
The lunar headers retain their English and Chinese descriptions; the rest is primarily English.

Read each function's units, time scale and supported range before using it. A civil date is not
an instant, and UT1 and TT inputs are not interchangeable.

For installation, examples and Python/JavaScript package guides, see the
[repository README](https://github.com/0xf3cd/celestial-calendar/blob/main/README.md).
Project-authored material uses MIT. The generated directory includes `LICENSE` and
`THIRD_PARTY_NOTICES.txt`; retained material keeps the source terms stated there.

<a id="cpp-guide"></a>
## C++ Guide

The README's [C++ quickstart](https://github.com/0xf3cd/celestial-calendar#cpp) shows how to compile against the
headers. This section covers the headers beyond the Jieqi API. The chart snapshot is also available from C
(described below), Python and JavaScript; the interval longitude events, the capabilities and model identities,
and the independent ecliptic/equatorial position and house queries are also available from C and Python.
For changes from 0.x, see [Migrating to 1.0](MIGRATING_1_0.md).

The [`illumination.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/illumination.hpp),
[`planet.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/planet.hpp), and
[`planet_phenomena.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/planet_phenomena.hpp)
provide planetary illumination geometry and yearly conjunction, opposition, quadrature, greatest-elongation, and
station searches. Event instants are JDE(TT); Gregorian-year ownership follows UTC boundaries, degrading to the
library's UT1 substitute before 1972. Mercury through Neptune use VSOP87D; their annual searches accept
years `[1, 32766]`, without a guarantee of successful evaluation or accuracy at remote epochs. `Planet::PLUTO`
instead uses the lower-precision Meeus Chapter 37 model: apparent position and illumination
geometry accept `[2409543.5, 2488069.5)` JDE(TT), centered retrograde checks accept `[2409544.0, 2488069.0)`, and
complete yearly event searches accept Gregorian years `[1886, 2098]`. These hard domains are not accuracy guarantees.

The [`lunar_node.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/lunar_node.hpp)
returns true-ecliptic, true-equinox-of-date longitudes for mean or Meeus five-term true ascending and descending
nodes from JDE(TT). The mean-node polynomial is tropical, referred to the mean equinox of date before the library
adds longitude nutation; the five-term true node is not an osculating node. Latitude is exactly zero by this
definition, and the longitude-only API has no physical distance.

The [`ephemeris.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/ephemeris.hpp)
owns `Target`, `TARGET_FACTS`, provider domains and position dispatch for the Sun, Moon, Mercury through Pluto,
and all four lunar nodes. `astro::ephemeris::position(target, jde_tt)` returns ecliptic longitude, latitude and
optional AU distance; `equatorial_position(target, jde_tt)` returns right ascension in `[0, 360)` degrees,
declination in `[-90, 90]` degrees and the same optional distance. These are distinct result types. They evaluate
no rate stencil, house geometry or civil conversion. Position longitude nutation and equatorial true-obliquity
projection use fixed IAU 1980, independent of the house default.

Where no hard provider interval is declared, finite input is admitted to evaluation, which can still fail.
Finite angles, valid latitude and positive physical distance establish neither physical plausibility nor
accuracy. Pluto and node hard windows remain in force; no universal position-year or obliquity cutoff is implied.

`astro::ephemeris::longitude_rate` provides the instantaneous rate for the same targets.
It accepts JDE(TT) and returns `LongitudeRate::deg_per_tt_day`, the instantaneous rate; a negative value means the
longitude is decreasing at that instant. Near a station its sign can differ from the one-day retrograde marker
`is_retrograde` in `planet.hpp`. The `Target` enum selects a body or lunar node.

```cpp
const auto rate = astro::ephemeris::longitude_rate(
  astro::ephemeris::Target::MOON,
  2451545.0
);
const double degrees_per_tt_day = rate.deg_per_tt_day;
```

Pluto rate inputs are `[2409543.875, 2488069.125)` and node inputs are `[2409542.875, 2488069.125)`.
Other rate inputs are `[-1200954.625, 6104044.625)`.

The optional [`ephemeris_events.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/ephemeris_events.hpp)
adds `longitude_events`, `stations` and `longitude_difference_events` for explicit half-open TT intervals inside
those domains. The [event guide](EPHEMERIS_EVENTS.md) covers signed target orientation, exact numerical contacts,
bounded work and the conditional floating-point contract. C and Python expose the same searches below.

The [`chart.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/chart.hpp) composes
geocentric positions, longitude rates and house geometry at one explicitly scaled civil instant:

```cpp
const calendar::Datetime civil_dt { util::to_ymd(2026, 1, 1), 0.5 };
const astro::GeoLocation location {
  .latitude = astro::toolbox::AngleDeg { 51.5 },
  .longitude = astro::toolbox::AngleDeg { 0.0 },
};
const auto snapshot = astro::chart::calculate(
  civil_dt, astro::julian_day::Scale::UTC, location, astro::house::System::PLACIDUS
);
```

Pass `julian_day::Scale::UT1` for historical civil input; UTC begins at 1972-01-01. Timezone conversion and
inserted leap seconds are outside the input domain. A final `delta_t::Model` argument selects a
model, through `delta_t::DEFAULT_MODEL` (currently Algo5). The result's `times` holds `jd_ut1`/`jde_tt`, alongside
fourteen position/rate records and four axes/twelve cusps; nodes have zero latitude and `std::nullopt` distance.
Location affects houses only. All systems reject an Ascendant direction unresolved at round-off;
Placidus also rejects its polar geometry. Failure never selects another system.

Civil years are `[1885, 2100)` and the joint JDE(TT) domain is `[2409543.875, 2488069.125)`;
Algo4 additionally requires derived JDE(TT) `< 2464328.5`, checked before reverse conversion. The shared
`chart::resolve_civil_times(civil_dt, scale, model)` owns this civil-year/UTC/model policy, but not the chart's
joint target/rate window. The numeric `house::calculate(jd_ut1, jde_tt, location, system, model)` and
`house::axes(jd_ut1, jde_tt, location, model)` accept both scales of the same instant without that civil policy.
Houses follow `house::DEFAULT_NUTATION_MODEL`, currently IAU 1980; chart bodies keep their fixed IAU 1980 convention.
There is no separate C axes export.

For conversion without chart/house admission, `julian_day.hpp` owns `Scale`, `Times { jd_ut1, jde_tt }`,
`civil_to_tt` and `resolve_times`. Model guards apply at the evaluation instant: UT1 forward, TT reverse.
UTC-to-TT alone does not evaluate ΔT. See [Time scales](DESIGN.md#time-scales) for legacy convenience conversions.
See the [complete raw replay example](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/examples/chart_snapshot/README.md).

See [core features and algorithms](https://github.com/0xf3cd/celestial-calendar#features) for the full scope.

<a id="c-chart-snapshot"></a>
## C chart snapshot and ABI policy

All exports use `celestial_`, records use `Celestial`, and public codes use `CELESTIAL_`. Existing layouts and
signatures are preserved within a major version. An incompatible change needs an explicitly named parallel
function/record or a new major; initial declarations do not carry a universal version suffix. No 0.x forwarders
are provided.

`celestial_chart_snapshot` returns an owned `CelestialChartSnapshot` by value. Its fourteen `CelestialChartBody`
records follow `CELESTIAL_TARGET_*` order; `CelestialChartHouses` holds four axes and twelve cusps. The fixed
roster/array layout cannot grow in place within 1.x. The result records resolved ΔT and house-nutation identities,
and each body carries its position-source identity. These keys describe model choice; exact reproduction also
requires the library/source version.

```c
#include <stdio.h>
#include "celestial.h"

int main(void) {
  const CelestialChartSnapshot snapshot = celestial_chart_snapshot(
    2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UTC,
    51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_PLACIDUS, CELESTIAL_DELTA_T_MODEL_DEFAULT
  );
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE || !snapshot.valid) {
    fprintf(stderr, "%s\n", celestial_last_error());
    return 1;
  }
  for (uint32_t i = 0; i < 14; ++i) {
    const CelestialChartBody *body = &snapshot.bodies[i];
    printf("%u %.9f %.9f", (unsigned)body->target_code,
           body->longitude_deg, body->longitude_rate_deg_per_tt_day);
    if (body->present_fields & CELESTIAL_PRESENT_DISTANCE) {
      printf(" %.9f AU", body->distance_au);
    }
    putchar('\n');
  }
  return 0;
}
```

Compile this as C and link a native library of the same version, from a release archive or built from the same
checkout. On failure read only `valid`
and the two error readers: invalid payload and padding bytes have no contract. Presence bits identify available fields;
absent storage is not a zero or NaN sentinel. Node latitude is present and zero, while node distance is absent.
Every distance here is in AU, the Moon's included; the standalone `CelestialMoonCoordinate` result gives kilometres.

`celestial_last_error_kind` is authoritative even if message allocation failed and `celestial_last_error` returns
an empty string. Both readers preserve the calling thread's kind/message; every other export clears or records
them. Invalid input uses `CELESTIAL_ERROR_INVALID_INPUT`; computation, resource and internal failures have distinct
kinds. Bindings translate invalid input to Python `ValueError` / JavaScript `RangeError`, wrong types or shapes to
`TypeError`, and computation/resource/internal failures or malformed successful payloads to `CelestialError`.

Sized UTF-8 calls (`celestial_get_jieqi_name`, `celestial_capability_text`, `celestial_model_identity_text`) return
required bytes including NUL. A null buffer with zero capacity queries the size. Positive capacity writes at most
capacity minus one payload bytes and a terminating NUL, truncating if necessary; zero return signals failure.
Query the size instead of assuming a fixed Jieqi-name buffer.

<a id="c-longitude-events"></a>
## C Interval Longitude Events

`celestial_longitude_events`, `celestial_longitude_stations` and `celestial_longitude_difference_events` search
half-open JDE(TT) intervals over the single `CELESTIAL_TARGET_*` family shared by charts, positions and capabilities.
Each call owns its results through one count+fill protocol: `event_count`
always receives the full total, caller-allocated `slots` receive the first `min(total, slot_count)`
records in chronological order, and the return value is how many records were written, so a truncated
fill is observable by comparing the two. A zero total is a legal no-event outcome; every failure also
leaves `*event_count` at 0 and is told apart from the empty success by `celestial_last_error_kind`.

```c
#include <stdio.h>
#include "celestial.h"

int main(void) {
  uint32_t total = 0;
  CelestialLongitudeEventRecord slots[8];
  const uint32_t written = celestial_longitude_events(
    CELESTIAL_TARGET_MERCURY, 0.0, 2460735.5, 2460785.5, &total, slots, 8
  );
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE) {
    fprintf(stderr, "%s\n", celestial_last_error());
    return 1;
  }
  for (uint32_t i = 0; i < written; ++i) {
    printf("%.10f %.9f %u\n", slots[i].jde_tt, slots[i].longitude_deg, slots[i].direction_code);
  }
  return 0;
}
```

Station records carry `motion_before_code`/`motion_after_code` (`CELESTIAL_EVENT_MOTION_*`); difference records
carry both longitudes, both rates and the canonical `signed_difference_deg`. Times are JDE(TT) and rates
are degrees per TT day. The interval must be nonempty, at most 3660 TT days, and inside the target's
longitude-rate domain. See [Longitude events](EPHEMERIS_EVENTS.md) for ownership at the endpoints,
numerical contacts and the floating-point premises.

## C Capabilities and Model Identities

`celestial_capabilities` lists TT target-operation records for POSITION, EQUATORIAL, LONGITUDE_RATE and EVENTS
under the count+fill protocol. Select one `CELESTIAL_TARGET_*` or `CELESTIAL_TARGET_ALL`; NaN endpoints mean no
declared bound. `CELESTIAL_STATUS_FINITE_INPUT_EVALUATION` admits finite inputs to evaluation, not guaranteed
successful computation. Event records carry the 3660-TT-day span limit.

Civil houses have a separate `celestial_house_capability(delta_t_model_code)` returning
`CelestialCivilHouseCapability`: civil years `[1885, 2100)`, allowed scales, UTC start `1972-01-01`, the resolved
ΔT identity and an optional exclusive derived-TT cutoff (Algo4 only). It describes civil admission, not a numeric
TT house domain or guaranteed geometric success. Python's canonical call is `house_capability(delta_t_model=...)`.

`celestial_model_identities` enumerates five ΔT models, two nutation models and six position sources. A model key
is `(kind_code, identity_code)`, never its registry position. Explicit ΔT identities/selectors are 1–5; nutation
Meeus/IAU 1980 are 1/2; position sources are 1–6. DEFAULT (0) is input-only for supported operations and is never a
returned identity. `celestial_model_identity_text(kind_code, identity_code, buffer, capacity)` describes that exact
pair. The Pluto source's stable code name `CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT` does not change its
runtime algorithm: Meeus Chapter 37, not PLUTO95.

`celestial_capability_text` describes operation-specific sources, frames, units, corrections and model use. Target
operations require one concrete target; HOUSES, CHART and DELTA_T require `CELESTIAL_TARGET_ALL`. Model text is a
separate lookup, not another interpretation of the subject argument. Model applicability guards act at evaluation
time; they are not universal converted-TT bounds. See [Time scales](DESIGN.md#time-scales).

The C++ `capability::domain_record` / `domain_records` facade composes typed operation, target, status and source
values with optional intervals/span limits. Raw NaN/code encoding and runtime model/text registries belong to
`shared_lib`, not the core facade. All these domains describe admission rules, not accuracy guarantees.

## C Independent Position and House Queries

`celestial_positions` evaluates the caller's selected targets at one finite JDE(TT) under the
count+fill protocol (#355). Output records follow the caller's target order; duplicates, an empty
selection and unknown codes are rejected before any position is computed, and one failed target
fails the whole query — no partial list is ever returned. The query evaluates no rate stencil and
applies no snapshot joint window: each target's provider window stands alone (see the POSITION
capability records), so a subset succeeds where a complete snapshot rejects an excluded target's
window. `CelestialPositionRecord::source_code` carries the
resolved position-source identity (`CELESTIAL_POSITION_SOURCE_*`), and `CELESTIAL_PRESENT_DISTANCE` marks
the AU distance the nodes never carry.

`celestial_equatorial_positions` uses the same selection/count+fill contract and returns distinct
`CelestialEquatorialPositionRecord` values: right ascension/declination in degrees, optional AU distance and
resolved source. It projects all fourteen targets with fixed IAU 1980 longitude nutation and true obliquity;
mathematical nodes have no distance. Neither query evaluates rates, houses or civil conversion.

`celestial_house_query` returns `CelestialHouseQueryResult` by value using the public
`astro::chart::resolve_civil_times` composer and numeric `astro::house::calculate`. It follows the shared civil
admission policy, including Algo4's derived-TT cutoff, without the chart joint window. `CELESTIAL_NUTATION_MODEL_*`
selects house nutation; DEFAULT follows `house::DEFAULT_NUTATION_MODEL`, currently IAU 1980. The result records both
resolved ΔT and nutation identities, never DEFAULT. This house key does not describe the fixed body corrections.

The C++ position entry points are `astro::ephemeris::position` and `equatorial_position`. Python uses one `Target`
enum across positions, events, charts and capabilities; position/body sources and snapshot/house model selections
are `ModelKey` pairs, accepted by matching-kind model selectors and `model_identity_text(key)`.

## Build This Reference

To build this reference locally, follow
[API documentation](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/BUILDING.md#api-docs)
in the build guide. The build rejects documentation syntax errors but does not require every declaration or
parameter to have a description.
