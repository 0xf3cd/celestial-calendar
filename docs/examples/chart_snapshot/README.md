# Fixed-instant chart snapshot replay

`data.json` records the complete output for 2026-01-01 12:00 UTC, north-positive latitude
51.5 degrees, east-positive longitude 0 degrees, Placidus and the default Algo5 Delta T model.
It contains UT1 and TT, all fourteen positions and longitude rates, four axes and twelve cusps.
Node distance is `null` and latitude is zero.

This is a chart snapshot replay example, not an independent physical-accuracy oracle.
This snapshot API leaves sign, house-placement and aspect calculations to the consumer.

Generate the values from the repository root:

```sh
clang++ -std=c++23 -O2 -Isrc/astro -Isrc/calendar -Isrc/util \
  statistics/chart_snapshot_fixture.cpp -o chart_snapshot_example
./chart_snapshot_example > docs/examples/chart_snapshot/data.json
```
