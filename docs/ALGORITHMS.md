# Lunar algorithms and Delta T models

Choose by source and intended use, not by treating an algorithm number as an accuracy ranking.
The domains below describe accepted inputs, not accuracy guarantees. Calling conventions, result types and
errors belong to the [Python](../bindings/python/README.md) and [JavaScript](../bindings/javascript/README.md)
guides and the [C++ / C API reference](API.md).

<a id="lunar-algorithms"></a>
## Lunar algorithms

Gregorian / lunar conversions take calendar-date labels, not instants. Each algorithm has its own date basis
and inclusive **Lunar-year** domain:

| Algorithm | Source and status | Lunar-year domain | Selection role |
|---|---|---|---|
| Algo1 | Published Hong Kong Observatory (HKO) date labels, stored as a table | `[1901, 2099]` | HKO almanac compatibility |
| Algo2 | Computed from VSOP87D and truncated ELP2000-82B | `[410, 2500]` | Astronomical construction over the wider computed window |
| Algo3 | Baked hybrid: Algo1/HKO in 1901–2099, Algo2-generated dates elsewhere | `[1600, 2199]` | Table lookup with HKO compatibility in the overlapping years |

Gregorian coverage starts on the first day of the first supported Lunar year and ends on the last day of the
last supported Lunar year. It is not January-to-December coverage of the same Gregorian years. Query the range
through the binding's supported-year operation rather than keeping a separate range table.

Algo2 assigns dates by rendering TT moments in the library's modelled UTC, then applying a fixed eight-hour
eastward offset. Before 1972 that UTC conversion uses UT1 as a substitute; from 1972 it uses the leap-second
table, and beyond the table it holds the final offset. This is a calendar-date assignment, not an instant with
a time zone. See [Time scales](DESIGN.md#time-scales) for the three-era conversion and its limitations.
The 2500 ceiling is the algorithm's enforced civil-date domain, not a physical limit of VSOP87D or ELP2000-82B.

Algo1 preserves HKO's published labels rather than imposing a uniform historical time-zone rule on them.
Algo3 inherits the basis of each source slice; no one historical time-zone rule describes its whole table.
The non-HKO slice is rebaked when its time-scale dependencies change; it is not recomputed on each query.

Definitions and provenance are in [`algo1.hpp`](../src/calendar/lunar/algo1.hpp),
[`algo2.hpp`](../src/calendar/lunar/algo2.hpp) and [`algo3.hpp`](../src/calendar/lunar/algo3.hpp).
The retained HKO-derived material keeps its source terms; see the [license scope](../README.md#license).

<a id="delta-t-models"></a>
## Delta T models

ΔT is TT minus UT1, in seconds. Its input is a decimal Gregorian year. The selectable default is Algo5,
the current project model. Algo1–Algo4 remain callable, frozen comparison models:

| Model | Source and status | Hard year domain | Selection role |
|---|---|---|---|
| Algo5 (default) | Current project model: Algo2 before 2005, Bulletin A fit through the last observation, then anchored integrated-LOD extrapolation | No model-specific bound | Current project calculations |
| Algo1 | Xu Jianwei's 2008 model, frozen | `year >= -4000` | Comparison with that historical model |
| Algo2 | Espenak and Meeus, NASA/TP-2006-214141, frozen | No model-specific bound | NASA polynomial comparison; live pre-2005 branch of Algo3, Algo4 and Algo5 |
| Algo3 | Fred Espenak's 2014 eclipse canon, frozen | `year < 3000` | Comparison with that later polynomial model |
| Algo4 | IERS Bulletin A observations and USNO predictions, frozen | `year < 2035` | Comparison with the previous project fit/prediction model |

Algo5 has three branches, not one trained polynomial:

- Before 2005, it delegates to Algo2.
- From 2005 through the inclusive `algo5::LAST_OBSERVATION_YEAR` (about 2026.41), it uses a single fitted
  Bulletin A segment, pinned to observed medians at both edges.
- After that boundary, it uses the corrected Morrison, Stephenson, Hohenkerk and Zawilski (2021)
  integrated length-of-day (LOD) expression, anchored at the last observation and continuous there.

The source definitions, guards and citations are in [`delta_t.hpp`](../src/astro/delta_t.hpp), including the
[immutable generation record](https://github.com/0xf3cd/AstroTime-Analysis/blob/ed1cdc2fd6c5122b391a82289aa2cc060340552d/DeltaT/algo5/record.json)
and the [2021 addendum](https://doi.org/10.1098/rspa.2020.0776).

“Frozen” describes maintenance, not dead code: Algo2 still supplies the current default's pre-2005 values.
A hard domain is an input contract. Fitted residuals and test tolerances are not statistical error guarantees;
absence of a model-specific bound does not promise useful accuracy at arbitrary years.

Selectable operations follow `delta_t::DEFAULT_MODEL`. Convenience UTC/UT1/TT conversions and Jieqi TT-to-UT1
conversion retain fixed Algo5 even if that selectable default changes. Operation-specific civil and derived-TT
limits still apply; see [Time scales](DESIGN.md#time-scales), [UTC conversions](UTC_CONVERSIONS.md) and
[Validation](VALIDATION.md).
