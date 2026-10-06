# Fixed-instant chart snapshot replay

`data.json` records the complete output for 2026-01-01 12:00 UTC, north-positive latitude
51.5 degrees, east-positive longitude 0 degrees, Placidus and the default Algo5 Delta T model.
It contains both time scales, all fourteen positions and longitude rates, four axes and twelve cusps.
Node distance is `null`; its defined zero latitude is a value.

This is a facade replay example, not an independent physical-accuracy oracle. Existing position,
rate and house tests retain their reference datasets and comparison tolerances. The stored raw
longitudes, rates and cusps are sufficient for a consumer to reproduce its own sign, house and
aspect calculations; this library does not assign those interpretations.

Generate the values from the repository root:

```sh
clang++ -std=c++23 -O2 -Isrc/astro -Isrc/calendar -Isrc/util \
  statistics/chart_snapshot_fixture.cpp -o chart_snapshot_example
./chart_snapshot_example > docs/examples/chart_snapshot/data.json
```

The JSON names current C++ targets. It does not freeze a C ABI layout or numeric identity protocol.
Cross-platform floating-point differences are evaluated through the underlying tests, not by
treating every printed digit in this example as a universal accuracy guarantee.
