# Fixed-instant chart snapshot replay

`data.json` records the complete output for 2026-01-01 12:00 UTC, north-positive latitude
51.5 degrees, east-positive longitude 0 degrees, Placidus and the default Algo5 Delta T model.
It contains UT1 and TT, all fourteen positions and longitude rates, four axes and twelve cusps.
Node distance is `null` and latitude is zero.

The installed Python `chart_snapshot` consumer uses this same input.
The [Python guide](../../../bindings/python/README.md) and [C example](../../API.md#c-chart-snapshot)
show how to request it; Python represents absent distance as `None`, and C uses `CELESTIAL_PRESENT_DISTANCE`.
The C/Python results also record resolved Delta T and house-nutation keys and each body's position-source key;
the stored numerical replay is not a model-identity registry. The house key does not describe body corrections,
which remain fixed IAU 1980.

This is a chart snapshot replay example, not an independent physical-accuracy oracle; its `replay_margins` are
acceptance thresholds, explained in [Validation](../../VALIDATION.md#replay-margins).
[`astro::chart::calculate`](../../../src/astro/chart.hpp) leaves sign, house-placement and aspect calculations to the consumer.

Generate the values from the repository root; the binary goes to the ignored `build/` directory, which must exist:

```sh
clang++ -std=c++23 -O2 -Isrc/astro -Isrc/calendar -Isrc/util \
  docs/examples/chart_snapshot/chart_snapshot_fixture.cpp -o build/chart_snapshot_example
```

Run this Python block from the repository root to update the computed fields while keeping the hand-maintained
`replay_margins`:

```python
import json
import subprocess
from pathlib import Path

path = Path("docs/examples/chart_snapshot/data.json")
stored = json.loads(path.read_text(encoding="utf-8"))
computed = json.loads(subprocess.check_output(["build/chart_snapshot_example"], text=True))
assert computed.keys() == stored.keys() - {"replay_margins"}
computed["replay_margins"] = stored["replay_margins"]
path.write_text(json.dumps(computed, indent=2) + "\n", encoding="utf-8")
```
