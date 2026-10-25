# Migrating to 1.0

This guide describes the 1.0 development API, not a release announcement or final freeze. Rebuild C/FFI consumers
against the matching header and library; update Python/JavaScript callers with the matching package. There are no
0.x forwarding exports. No accepted numerical value changes, but invalid-input acceptance, error classes and some
messages intentionally change.

## C and FFI names and layouts

Every public function uses `celestial_`, every record `Celestial`, and every public code `CELESTIAL_`.
Initial declarations have no universal `_v1` / `V1` suffix. Existing layouts and signatures stay fixed within a
major; a genuinely incompatible change needs an explicitly named parallel function/record or a new major.
The fixed chart roster/array is part of that layout, even if a selected-target query later supports more targets.

| Previous name or family | 1.0 name or action |
|---|---|
| `ut1_to_jd`, `query_jieqi_moment`, other unprefixed exports | `celestial_ut1_to_jd`, `celestial_query_jieqi_moment`, corresponding `celestial_` exports |
| `chart_snapshot_v1`, `ChartSnapshotV1` | `celestial_chart_snapshot`, `CelestialChartSnapshot` |
| `ChartBodyV1`, `ChartHousesV1` | `CelestialChartBody`, `CelestialChartHouses` |
| `position_query_v1`, `PositionRecordV1` | `celestial_positions`, `CelestialPositionRecord` |
| `house_query_v1`, `HouseQueryResultV1` | `celestial_house_query`, `CelestialHouseQueryResult` |
| `CHART_TARGET_*`, `EVENT_TARGET_*`, capability target codes | One `CELESTIAL_TARGET_*` family |
| Six Delta T exports: default and algo1–algo5 | `celestial_delta_t(year, model_code)` |
| `set_log_verbosity` | Removed; the library does not print errors |

Read [`celestial.h`](../src/shared_lib/celestial.h) for the complete declarations. Do not retain a copied 0.x layout
and merely rename its symbols: the snapshot now contains resolved Delta T/house-nutation identities and per-body
source identities. `CelestialDeltaT` also records the resolved model. `CelestialPositionRecord` is a separate
shape from a chart body, and equatorial records have distinct RA/declination fields.

A model key is the exact `(kind_code, identity_code)` pair, not a flat registry index. Explicit Delta T values are
1–5, Meeus/IAU 1980 nutation values are 1/2, and position sources are 1–6. Selectable-model identity values are also
valid explicit selectors within the matching kind. DEFAULT (0) is input-only where supported, never a returned
identity; its meaning belongs to the operation. Use `celestial_model_identity_text(kind, identity, buffer, capacity)`
for model text, not `celestial_capability_text` with a registry ordinal. The latter describes an operation; target
operations need a concrete target, while HOUSES, CHART and DELTA_T use `CELESTIAL_TARGET_ALL`.

## Errors and caller-owned buffers

Check `celestial_last_error_kind()` after each recording call, including count and fill passes. It is authoritative
even if allocating a message failed. `celestial_last_error()` and `celestial_last_error_kind()` both preserve state;
every other export clears or records the calling thread's kind/message. A legal empty result has kind NONE.
Check `valid` before reading a returned record; invalid payload and padding bytes have no contract.

| Failure | C kind / binding exception |
|---|---|
| Wrong binding type or record shape | Python/JavaScript `TypeError` |
| Malformed or out-of-declared-domain input, wherever detected | `CELESTIAL_ERROR_INVALID_INPUT`; Python `ValueError`, JavaScript `RangeError` |
| Computation, resource or internal failure | Corresponding C error kind; binding `CelestialError` |
| Malformed native success, unknown returned codes or inconsistent fill totals | Binding `CelestialError` |

For example, an out-of-domain native sidereal query or a date label outside the selected lunar algorithm's coverage
now raises `ValueError` / `RangeError`, not the library error used by 0.x. After a finite Sun/Moon input, an internal non-finite angle or
invalid computed coordinate is a computation failure; a malformed/non-finite input remains invalid input.
Applications should branch on the error kind/class, not match message text or test whether the message is nonempty.

Returned Gregorian/civil labels use astronomical years in `[-32767, 32767]`, including year 0 (1 BCE).
A valid year-1 solar-time input can cross into year 0. The JavaScript `/date` bridge keeps its separate
`[1, 32767]` civil domain; Python's `GregorianDate.to_date()` still requires `[1, 9999]`.

Sized UTF-8 calls return the required byte count **including NUL**. Pass a null buffer and zero capacity to size;
with positive capacity, calls write at most capacity minus one payload bytes and a terminating NUL, truncating if
needed. Allocate the reported size and check the error kind after both passes. Jieqi-name callers must not depend
on a fixed 16-byte allocation. The complete [C quickstart](../README.md#c-abi) shows dynamic allocation.

Count/fill queries report the total separately from the number written. A truncated fill is a successful prefix,
not a partial result from a failed computation. Check errors after both passes and reject changed totals when
replaying count then fill. Buffers are caller-owned; result values copied by the packages remain owned values.

## Python

Use `Target` in place of `ChartTarget`, `EventTarget` and `CapabilitySubject`. Positions, charts, events and TT
capabilities share that enum. Position/body `.source` values and snapshot/house `.delta_t_model` /
`.nutation_model` values are `ModelKey(kind, identity)` pairs. `ModelIdentity.key` supplies the same pair;
`model_identity_text(key)` looks it up without depending on registry order. Matching-kind keys can be reused as
model selections. `PositionSource` and registry-wide default-alias fields are removed.

`positions(targets, jde_tt)` remains ecliptic. `equatorial_positions(targets, jde_tt)` returns distinct
`EquatorialPosition` values with `right_ascension_deg`, `declination_deg`, optional `distance_au` and `.source`.
Both preserve caller order, reject empty/duplicate/unknown selections, evaluate only selected targets, and fail
the entire query if a target fails. They evaluate no rates, civil conversion or houses. All fourteen targets are
supported; mathematical nodes have no distance. Read civil policy through the separate
`house_capability(delta_t_model=...)`, not a TT target-domain query.

```python
import celestial_calendar as celestial

targets = (celestial.Target.SUN, celestial.Target.TRUE_ASCENDING)
ecliptic = celestial.positions(targets, 2451545.0)
equatorial = celestial.equatorial_positions(targets, 2451545.0)
print(equatorial[0].right_ascension_deg, equatorial[0].declination_deg)
print(celestial.model_identity_text(equatorial[0].source))

when = celestial.CivilDateTime(2026, 1, 1, 0.5)
site = celestial.GeoLocation(51.5, 0.0)
houses = celestial.house_query(when, celestial.CivilScale.UTC, site, celestial.HouseSystem.EQUAL)
policy = celestial.house_capability(delta_t_model=houses.delta_t_model)
again = celestial.house_query(
  when,
  celestial.CivilScale.UTC,
  site,
  celestial.HouseSystem.EQUAL,
  delta_t_model=houses.delta_t_model,
  nutation_model=houses.nutation_model,
)
print(policy.civil_start_year, policy.civil_end_year_exclusive, again.times)
```

`house_query` keeps `delta_t_model=` and `nutation_model=`; `delta_t` and `chart_snapshot` keep `model=`.
`delta_t` still returns a scalar. Reusing resolved keys preserves the chosen model, not coefficients across library
versions. Python `set_log_verbosity` and `LogVerbosity` are removed.

## JavaScript and TypeScript

Remove `config`, logging types/setters and initialization configuration. Use `await init()` without arguments.
The public namespaces remain `time`, `sun`, `moon`, `jieqi`, `lunar` and `chart`; no independent position,
equatorial, house, event, capability or model-text query is added. The Sun coordinate's `radiusAu` becomes
`distanceAu`; Moon distance remains `distanceKm`, and chart distances remain `distanceAu` (null for nodes).
The `celestial-calendar` npm alias forwards the same API; use matching primary/alias versions.

Wrong types/shapes use `TypeError`; invalid input found by JavaScript or native code uses `RangeError`.
Computation/resource/internal failures and corrupt native success records use `CelestialError`. Initialization
and loader retry behavior are unchanged. In both bindings, `CelestialError.recorded` is true when a recognized
native error kind was reported, even if its message is empty; binding-local failures set it to false.

## C++ ownership, time and model conventions

Use `astro::ephemeris::position` instead of `astro::chart::position`; use its distinct `equatorial_position`
for RA/declination. `ephemeris::TARGET_FACTS` owns targets, sources and provider domains. The C++ capability facade
uses typed enums and optional intervals; raw C codes, NaN encoding, model registries and runtime text stay in
`shared_lib`. Adapters use public core operations, not `detail` helpers.

`astro::julian_day` owns `Scale`, `Times`, `civil_to_tt` and the general `resolve_times`. Replace `chart::Scale` /
`chart::Times` uses with the Julian-day types. The general resolver has no chart/house admission policy; model
guards apply at the evaluation instant, UT1 forward and TT reverse. Algo4 UT1 input can resolve to TT beyond its
TT-evaluation end, then fail if that TT is reversed. UTC-to-TT alone does not evaluate Delta T.

`chart::resolve_civil_times` applies the shared chart/civil-house policy: `[1885, 2100)` civil years, strict UTC
from 1972-01-01 and Algo4 derived JDE(TT) `< 2464328.5` before reverse conversion. This deliberately excludes the
last roughly 71.5 UT1 seconds of 2034 under Algo4. Chart adds its joint target/rate window; house does not.
Common time/model rejections may now precede a chart joint-window rejection, so multiply-invalid messages can
change. Legacy convenience conversions keep their pre-1972 UT1 substitute and fixed Algo5; Jieqi TT-to-UT1 also
stays fixed Algo5 even if the selectable default changes.

`house::axes(jd_ut1, jde_tt, location, model)` is a standalone C++ operation without cusps; no C axes export is added.
Numeric axes/houses have no civil-year or chart target/rate window. House nutation follows
`house::DEFAULT_NUTATION_MODEL`, currently IAU 1980. Nutation/true-obliquity primitives, omitted sidereal model
arguments, positions, projection, solar time and rise/set retain fixed IAU 1980 conventions. A snapshot's
house-nutation key does not describe its body corrections.

For the duration constructor, replace `Datetime{day, hh_mm_ss{elapsed}}` with `Datetime{day, elapsed}`;
an existing `hh_mm_ss` can supply `.to_duration()`. Fraction and time-point constructors remain.

## Scientific limits that still apply

Finite-input evaluation can fail computation. Coordinate validity establishes neither physical plausibility
nor accuracy at remote epochs; no universal position-year or obliquity cutoff is introduced. Pluto/node hard
windows remain unchanged. Equatorial projection uses fixed IAU 1980 longitude nutation and true obliquity;
finite rotations do not validate the model's far-epoch orientation.

The mean-node polynomial is tropical and referred to the mean equinox of date before the library adds longitude
nutation. The true node is Meeus' five-term approximation, not an osculating node. Pluto uses Meeus Chapter 37,
despite the stable source code name containing `PLUTO95`.

Algo1–Algo4 Delta T models are frozen comparison models. Algo5 uses Algo2 before 2005, the Bulletin A fitted
segment through the inclusive `algo5::LAST_OBSERVATION_YEAR`, then the anchored corrected integrated-LOD
extrapolation; the whole model is not a trained polynomial. See the [model sources](../bindings/javascript/README.md#delta-t-models).
Existing goldens and tolerances are unchanged. The new equatorial tests separate same-input projection/node
conformance from physical Horizons regression checks; their thresholds and sample limits are in
[Validation](VALIDATION.md#equatorial-reference-layers). Final release-candidate/platform validation remains separate.
