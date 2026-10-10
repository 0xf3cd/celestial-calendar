# API Reference

This reference covers C++ declarations and the C interface in `celestial.h`.
Bulk coefficient data and source listings are omitted.
Browse **Namespaces**, **Classes**, or **Files**, or search for a function name.
The lunar headers retain their English and Chinese descriptions; the rest is primarily English.

Read each function's units, time scale and supported range before using it. A civil date is not
an instant, and UT1 and TT inputs are not interchangeable.

For installation, examples and Python/JavaScript package guides, see the
[repository README](https://github.com/0xf3cd/celestial-calendar#readme).
Project-authored material uses MIT. The generated directory includes `LICENSE` and
`THIRD_PARTY_NOTICES.txt`; retained material keeps the source terms stated there.

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

Compile this as C and link the native library built from the matching checkout. On failure read only `valid`
and `last_error`: invalid payload and padding bytes have no contract. Presence bits identify available fields;
absent storage is not a zero or NaN sentinel. Node latitude is present and zero, while node distance is absent.
Moon distance uses AU here, unlike the kilometre field in the older `MoonCoordinate` result.

## Build This Reference

Install Doxygen 1.15.0, then run `python project.py --docs` from the source root.
Open `build/api-docs/html/index.html`. No compiler, Graphviz or package build is required.
The same command runs in **Core Tests**, which uploads the `celestial-api-html` artifact.
Documentation is opt-in, is not part of `--all`, and is not a release artifact or hosted site.

The build rejects documentation syntax errors and checks representative generated pages.
It does not require every declaration or parameter to have a description.
