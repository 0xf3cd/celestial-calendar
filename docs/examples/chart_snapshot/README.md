# Fixed-instant chart snapshot replay

`data.json` records the complete output for 2026-01-01 12:00 UTC, north-positive latitude
51.5 degrees, east-positive longitude 0 degrees, Placidus and the default Algo5 Delta T model.
It contains UT1 and TT, all fourteen positions and longitude rates, four axes and twelve cusps.
Node distance is `null` and latitude is zero.

The installed Python `chart_snapshot` consumer uses this same input.
The [Python guide](../../../bindings/python/README.md) and [C example](../../API.md#fixed-version-c-chart-snapshot)
show how to request it; Python represents absent distance as `None`, and C uses `CHART_PRESENT_DISTANCE`.

This is a chart snapshot replay example, not an independent physical-accuracy oracle.
[`astro::chart::calculate`](../../../src/astro/chart.hpp) leaves sign, house-placement and aspect calculations to the consumer.

Generate the values from the repository root:

```sh
clang++ -std=c++23 -O2 -Isrc/astro -Isrc/calendar -Isrc/util \
  src/bench/chart_snapshot_fixture.cpp -o chart_snapshot_example
./chart_snapshot_example > docs/examples/chart_snapshot/data.json
```
