# Design

This document is for contributors and for readers who want to see how the library fits together: how the source
directories depend on each other, where a namespace differs from its file, how time scales are handled, and which
failure gets which mechanism. Day-to-day conventions are in [`AGENTS.md`](../AGENTS.md).

<a id="layers"></a>
## Layers

A header includes only from its own level or a lower one. From the bottom:

| Level | Headers | Project includes |
|---|---|---|
| 0 | `util/`: `ymd.hpp`, `hash.hpp`, `cache.hpp` | other `util/` headers |
| 1 | `calendar/datetime.hpp` | `ymd.hpp` |
| 2 | `astro/` | other `astro/` headers, `ymd.hpp`, `datetime.hpp` |
| 3 | `calendar/jieqi.hpp`, `calendar/lunar/algo2.hpp` | `astro/` headers, `datetime.hpp`, `cache.hpp`; `algo2.hpp` also `jieqi.hpp` and `lunar/common.hpp` |
| top | `shared_lib/` (the C ABI), `test/`, `bench/` | anything below; no library header includes them |

`calendar/lunar/{common,converter,algo1,algo3}.hpp` include only `ymd.hpp` and `lunar/common.hpp`. They live under
`calendar/` but sit just above `util/` by dependency and never reach `astro/`.

Time-bearing `astro/` headers use `calendar/datetime.hpp`. That is not an inversion. `calendar::Datetime` is a date
and a time of day with no time scale or time zone attached, a util-level type that happens to live under `calendar/`, and
`datetime.hpp` itself includes only `ymd.hpp`. No `astro/` header includes anything else from `calendar/`, directly
or transitively, so the order `util` → `datetime.hpp` → `astro` → `jieqi.hpp` / `algo2.hpp` has no backward edge.

`src/CMakeLists.txt` puts `astro/`, `calendar/` and `util/` on one include path, so an include names a file, not a
level. No check enforces this order; read the includes when you add one.

Within `astro/`, `ephemeris::TARGET_FACTS` owns target membership, sources and provider domains. Position dispatch
and `equatorial_position` live with those facts; chart composes them with rates and house geometry. The typed
`capability.hpp` facade combines domain facts with the lightweight `ephemeris_event_facts.hpp`, without pulling
the event search kernel/bounds tables into chart or independent position queries. `julian_day` owns physical time
conversion; `chart::resolve_civil_times` adds the shared civil admission policy. Raw C selectors, sentinel encoding,
record shaping and runtime model/text registries belong to `shared_lib`; adapters use public core operations,
not core `detail` symbols.

<a id="namespaces"></a>
## Files and namespaces

Most headers open the namespace their path suggests: `astro/earth/precession.hpp` is `astro::earth::precession`,
`calendar/lunar/algo2.hpp` is `calendar::lunar::algo2`. These do not:

| File | Namespace | Difference |
|---|---|---|
| `astro/coord_transform.hpp` | `astro::coords` | shorter name |
| `astro/sidereal_time.hpp` | `astro::sidereal` | shorter name |
| `astro/geolocation.hpp` | `astro` | `GeoLocation` sits directly in `astro` |
| `astro/pluto.hpp` | `astro::planet::detail::pluto` | an implementation detail of `planet.hpp` |
| `astro/planet_phenomena.hpp` | `astro::planet::phenomena` | one file name, two namespace levels |
| `calendar/datetime.hpp` | `calendar` | no `calendar::datetime`; the type is `calendar::Datetime` |
| `util/ymd.hpp` | `util`, `util::ymd_operator` | no `util::ymd`; date arithmetic operators are opt-in through `util::ymd_operator` |

`astro/astro.hpp` and `astro/vsop87d/vsop87d.hpp` only include other headers and open no namespace. Outside the
library headers, `src/test/random.hpp` keeps `util` and `util::detail`, `src/test/test_helper.hpp` uses
`util::test`, and `src/shared_lib/` keeps its C++ helpers in `lib` and `lib::chart`, with the exported functions
`extern "C"` at global scope.

The names stay. The design ledger in [`AGENTS.md`](../AGENTS.md) rules out a one-off namespace rename: new code lands
in the intended shape, and old names change incidentally or not at all.

<a id="time-scales"></a>
## Time scales

Ephemerides run on TT. UT1 enters where Earth's rotation matters (sidereal time, houses, rise and set), and UTC only
at civil inputs and outputs. Other documents state the conclusion and link here.

- UT1 follows Earth's rotation; no clock keeps it.
- TT is TAI + 32.184 s exactly, where TAI is atomic time (`TT_MINUS_TAI_SEC` in `leap_second.hpp`).
- UTC is TAI − ΔAT, where ΔAT is the leap-second count that IERS steps to keep |UT1 − UTC| < 0.9 s. Together
  with the line above, TT − UTC = ΔAT + 32.184 s exactly, with no model involved (`leap_second.hpp`).
- ΔT is TT − UT1. It is observed, not defined, so the library takes it from a model (`delta_t.hpp`).
- DUT1 is UT1 − UTC. This document uses that sign throughout.

A JD counts days on UT1 and a JDE on TT (`julian_day.hpp`). A `calendar::Datetime` carries no scale of its own;
the parameter name says which one it holds (`jd_ut1`, `jde_tt`, `ymd_ut1`, `utc_dt`).

The library's UTC begins on 1972-01-01 (`MODERN_UTC_START`); it does not model the rubber-second UTC of 1960–1971.
The explicitly scaled `julian_day::civil_to_tt` / `resolve_times` and chart/civil-house inputs reject UTC before
that date. Legacy `leap_second::utc_to_tt` / `tt_to_utc` and their Julian-day convenience wrappers instead read
pre-1972 UTC-labelled values as UT1, with fixed Algo5. Their seam at 1972-01-01 is ΔT(1972.0) − 42.184 s,
under a second. From 1972 the conversion uses the ΔAT table (`LEAP_SECOND_TABLE`), whose last step is 2017-01-01.
After the last step ΔAT is held at 37 s.
Whether real UTC keeps that value depends on future leap seconds or their successor under CGPM 2022 Resolution 4,
which the library cannot know (`calendar/lunar/algo2.hpp`, the comment on its year ceiling).

DUT1 is not modelled: the library never reads an observed UT1 − UTC. Its UT1 is TT − ΔT from the selected model and
its UTC is TT − (ΔAT + 32.184 s), so for one instant UT1 − UTC = (ΔAT + 32.184 s) − ΔT. By era:

| Era | UT1 − UTC in the library |
|---|---|
| Before 1972 | Zero by construction in legacy convenience conversions: UT1 stands in for UTC. Explicitly scaled UTC input is rejected. |
| 1972 to the last ΔAT step (2017) | (ΔAT + 32.184 s) − ΔT. While leap seconds are applied, IERS keeps the real \|UT1 − UTC\| < 0.9 s; the library's value is as close to it as the ΔT model is to observed ΔT. |
| After the last ΔAT step | (37 s + 32.184 s) − ΔT = 69.184 s − ΔT. It turns negative once ΔT exceeds 69.184 s and is no longer bounded by 0.9 s. With Algo5 it is −2.57 s at 2050 (`delta_t.hpp`) and about 21 h in magnitude by year 6772, UT1 behind UTC (`get_start_jde` in `sun.hpp`). |

So a UT1 result is not UTC, and reading one as UTC carries the gap in the table above. The leap-second-aware
`leap_second::utc_to_tt` / `tt_to_utc`, added in v0.4.0 (#84), convert between UTC and TT through the ΔAT table;
they supply no UT1 − UTC, so they do not make a UT1 output usable as UTC.

Where the library derives UT1 from a UTC input, UTC goes to TT through the ΔAT table, then TT − ΔT with one ΔT
evaluation at TT. `julian_day::Scale`, `Times { jd_ut1, jde_tt }`, `civil_to_tt` and `resolve_times` own this
conversion. The general resolver has no chart/house civil-year or derived-TT admission policy. Model guards apply
at the evaluation instant: UT1 for the forward shift, TT for the reverse. Algo4 UT1 input may derive TT beyond its
TT-evaluation end; reversing that TT through Algo4 may fail. UTC-to-TT alone does not evaluate ΔT and must not be
routed through the both-scales resolver.

`chart::resolve_civil_times` adds the shared chart/civil-house policy: civil years `[1885, 2100)`, strict modern
UTC, and Algo4 derived JDE(TT) `< 2464328.5`, checked before reverse conversion. UT1 input passes through unchanged.
Chart then checks its joint target/rate window; the civil house query does not. Numeric `house::axes` and
`house::calculate` take same-instant UT1/TT values without this civil policy. Future UTC holds the last known leap
offset, and derived UT1 is modelled, not observed DUT1.

| API | Time scale |
|---|---|
| Positions and rates (`sun`, `moon`, `planet`, `ephemeris`, `lunar_node`) | take JDE(TT) |
| Interval longitude events and stations (`ephemeris_events`) | take JDE(TT) endpoints; return JDE(TT) |
| Searches by year: `jieqi_jde`, `moon_phase` moments, planetary events | bound the year by `[Jan 1, next Jan 1)` in UTC, UT1 before 1972; return JDE(TT) |
| `jieqi_ut1_moment`, C `celestial_query_jieqi_moment` and the package Jieqi moments | the same Jieqi converted to UT1 through fixed Algo5, not UTC, even if a selectable-model default changes |
| Lunar Algo2 | assigns date labels by rendering TT moments through the library's UTC, then a fixed +8 h offset |
| Lunar Algo1 and Algo3 | calendar-date labels in and out; no instant |
| Rise and set | take a UT1 date (`ymd_ut1`); return JDE(TT) |
| Sidereal time, houses | take JD(UT1); apparent sidereal time and houses also take the JDE(TT) of the same instant |
| C `celestial_apparent_solar_time`, Python `apparent_solar_time` | takes civil UTC in place of UT1, so the result keeps the UT1 − UTC gap (under 0.9 s while leap seconds are applied) |
| Chart snapshot and civil house query | takes civil UTC (from 1972-01-01) or UT1, chosen explicitly; returns JD(UT1) and JDE(TT), with UT1 from UTC modelled |
| `delta_t(year)` | takes a decimal year; TT and UT1 are not distinguished |

The C, Python and JavaScript UTC/JDE convenience conversions follow the same historical substitute,
1972 seam and leap-table policy as C++. Their separate input domains and noninvertible boundaries,
with a UTC-input Jieqi example, are in [UTC conversions](UTC_CONVERSIONS.md).

ΔT defaults to Algo5. Each model's source, status, hard year domain and role are tabulated in the
[Python](../bindings/python/README.md#delta-t-models) and [JavaScript](../bindings/javascript/README.md#delta-t-models)
package guides; the per-model contracts and the Algo5 construction are in `src/astro/delta_t.hpp`.

Selectable ΔT operations follow `delta_t::DEFAULT_MODEL`; fixed-Algo5 convenience conversions stay fixed.
House nutation follows `house::DEFAULT_NUTATION_MODEL`. Nutation/true-obliquity primitives and omitted sidereal
model arguments retain fixed IAU 1980; positions, equatorial projection, solar time and rise/set also retain that
fixed convention. A chart's returned house-nutation key does not claim its body corrections follow the house default.

<a id="failures"></a>
## Failures

Each failure is sorted by what happened to the answer (#97):

| The answer | Mechanism | Examples |
|---|---|---|
| Cannot be produced correctly: the input is malformed or outside a declared window | `throw std::invalid_argument` | `rise_set::detail::validate`; a chart civil year outside [1885, 2100) |
| Cannot be produced correctly: no convergence, or a consistency check that runs in every build fails | `throw std::runtime_error` | the Jieqi root search does not find exactly one root; two leap months in one Algo2 lunar year |
| Was produced, and it is "none" | `std::optional` | no sunrise on a polar night; no leap month in a common year |
| Is beside the point: the library's own bookkeeping is broken | `assert` | internal invariants only |

A declared model window is the contract's domain, so leaving it is a bad argument like any other. An input guard is
never an `assert`.

The `runtime_error` check and the `assert` differ in when they run. An `assert` states an invariant the code
guarantees by construction and is checked only without `NDEBUG`: in debug builds and the test binaries, never in
the Release library. A `runtime_error` check guards a result that depends on numerics or data, runs in every build,
and stops a wrong answer from reaching the caller.

`optional` never carries an error; it carries a legitimate "none". `std::expected` was evaluated for the core and
declined (#97). The C ABI catches failures without throwing across the boundary: records return `valid = false`,
and count/sized-string calls return zero. Every export except `celestial_last_error` and
`celestial_last_error_kind` clears or records the calling thread's error kind/message. Both readers preserve
state. The kind is authoritative even when the message is empty because message allocation failed.

| Native kind | Python | JavaScript |
|---|---|---|
| Invalid input, including a declared-domain rejection | `ValueError` | `RangeError` |
| Computation, resource or internal failure | `CelestialError` | `CelestialError` |

Binding type/shape errors use `TypeError`; malformed native success payloads use the library error class.
Coordinate checks after finite input can fail computation without declaring a new input interval. No coordinate
validity check establishes physical plausibility or accuracy. Deliberate invalid-input error-class/message
changes are listed in [Migrating to 1.0](MIGRATING_1_0.md).
