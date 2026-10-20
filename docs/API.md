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
headers. This section covers the headers beyond the Jieqi API. Of these, only the chart snapshot is also available
from C (described below), Python and JavaScript.

The [`illumination.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/illumination.hpp),
[`planet.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/planet.hpp), and
[`planet_phenomena.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/planet_phenomena.hpp)
provide planetary illumination geometry and yearly conjunction, opposition, quadrature, greatest-elongation, and
station searches. Event instants are JDE(TT); Gregorian-year ownership follows UTC boundaries, degrading to the
library's UT1 substitute before 1972. Mercury through Neptune use VSOP87D over the `[1, 32766]` computational year
domain. `Planet::PLUTO` instead uses the lower-precision Meeus Chapter 37 model: apparent position and illumination
geometry accept `[2409543.5, 2488069.5)` JDE(TT), centered retrograde checks accept `[2409544.0, 2488069.0)`, and
complete yearly event searches accept Gregorian years `[1886, 2098]`. These hard domains are not accuracy guarantees.

The [`lunar_node.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/lunar_node.hpp)
returns true-ecliptic, true-equinox-of-date longitudes for mean or Meeus five-term true ascending and descending
nodes from JDE(TT). Latitude is exactly zero by this definition, and the longitude-only API has no physical distance.

The [`ephemeris.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/ephemeris.hpp)
provides `astro::ephemeris::longitude_rate` for the Sun, Moon, Mercury through Pluto, and all four lunar nodes.
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
bounded work and the conditional floating-point contract. These interval functions are C++-only.

The [`chart.hpp`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/chart.hpp) composes
geocentric positions, longitude rates and house geometry at one explicitly scaled civil instant:

```cpp
const calendar::Datetime civil_dt { util::to_ymd(2026, 1, 1), 0.5 };
const astro::GeoLocation location {
  .latitude = astro::toolbox::AngleDeg { 51.5 },
  .longitude = astro::toolbox::AngleDeg { 0.0 },
};
const auto snapshot = astro::chart::calculate(
  civil_dt, astro::chart::Scale::UTC, location, astro::house::System::PLACIDUS
);
```

Pass `Scale::UT1` for historical civil input; UTC begins at 1972-01-01. Timezone conversion and
inserted leap seconds are outside the input domain. A final `delta_t::Model` argument selects a
model, default Algo5. The result holds `jd_ut1`/`jde_tt`, fourteen position/rate
records and four axes/twelve cusps; nodes have zero latitude and `std::nullopt` distance.
Location affects houses only. All systems reject an Ascendant direction unresolved at round-off;
Placidus also rejects its polar geometry. Failure never selects another system.

Civil years are `[1885, 2100)` and the joint JDE(TT) domain is `[2409543.875, 2488069.125)`;
Algo4 additionally requires `< 2464328.5`.
See the [complete raw replay example](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/examples/chart_snapshot/README.md).

See [core features and algorithms](https://github.com/0xf3cd/celestial-calendar#features) for the full scope.

<a id="c-chart-snapshot"></a>
## Fixed-Version C Chart Snapshot

`chart_snapshot_v1` returns an owned `ChartSnapshotV1` by value. Its fourteen `ChartBodyV1` records follow
the `CHART_TARGET_*` code order; `ChartHousesV1` holds four axes and twelve cusps. The header defines fixed-width
selector codes, units and domains. An incompatible field or roster extension requires new separately named
structures and a new function; V1 does not grow in place and needs no allocation/free protocol.

```c
#include <stdio.h>
#include "celestial.h"

int main(void) {
  ChartSnapshotV1 snapshot = chart_snapshot_v1(
    2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UTC,
    51.5, 0.0, CHART_HOUSE_SYSTEM_PLACIDUS, CHART_DELTA_T_MODEL_DEFAULT
  );
  if (!snapshot.valid) {
    fprintf(stderr, "%s\n", last_error());
    return 1;
  }
  for (unsigned i = 0; i < 14; ++i) {
    const ChartBodyV1 *body = &snapshot.bodies[i];
    printf("%u %.9f %.9f", (unsigned)body->target_code,
           body->longitude_deg, body->longitude_rate_deg_per_tt_day);
    if (body->present_fields & CHART_PRESENT_DISTANCE) {
      printf(" %.9f AU", body->distance_au);
    }
    putchar('\n');
  }
  return 0;
}
```

Compile this as C and link a native library of the same version, from a release archive or built from the same
checkout. On failure read only `valid`
and `last_error`: invalid payload and padding bytes have no contract. Presence bits identify available fields;
absent storage is not a zero or NaN sentinel. Node latitude is present and zero, while node distance is absent.
Every distance here is in AU, the Moon's included; the standalone `MoonCoordinate` result gives kilometres.

<a id="c-longitude-events"></a>
## C Interval Longitude Events

`longitude_events`, `longitude_stations` and `longitude_difference_events` search half-open JDE(TT)
intervals over the `EVENT_TARGET_*` codes — the same fourteen identities as `CHART_TARGET_*`, pinned
equal in `c_header_check.c`. Each call owns its results through one count+fill protocol: `event_count`
always receives the full total, caller-allocated `slots` receive the first `min(total, slot_count)`
records in chronological order, and the return value is how many records were written, so a truncated
fill is observable by comparing the two. A zero total is a legal no-event outcome; every failure also
leaves `*event_count` at 0 and is told apart from the empty success only by `last_error`.

```c
#include <stdio.h>
#include "celestial.h"

int main(void) {
  uint32_t total = 0;
  LongitudeEventRecord slots[8];
  const uint32_t written = longitude_events(
    EVENT_TARGET_MERCURY, 0.0, 2460735.5, 2460785.5, &total, slots, 8
  );
  if (written == 0 && total == 0 && last_error()[0] != '\0') {
    fprintf(stderr, "%s\n", last_error());
    return 1;
  }
  for (uint32_t i = 0; i < written; ++i) {
    printf("%.10f %.9f %u\n", slots[i].jde_tt, slots[i].longitude_deg, slots[i].direction_code);
  }
  return 0;
}
```

Station records carry `motion_before_code`/`motion_after_code` (`EVENT_MOTION_*`); difference records
carry both longitudes, both rates and the canonical `signed_difference_deg`. Times are JDE(TT) and rates
are degrees per TT day. The interval must be nonempty, at most 3660 TT days, and inside the target's
longitude-rate domain. See [Longitude events](EPHEMERIS_EVENTS.md) for ownership at the endpoints,
numerical contacts and the floating-point premises.

## Build This Reference

To build this reference locally, follow
[API documentation](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/BUILDING.md#api-docs)
in the build guide. The build rejects documentation syntax errors but does not require every declaration or
parameter to have a description.
