# Observer reference fixtures

`src/test/astro/observer_reference_data.hpp` retains numeric oracle output only.
SOFA (V20), PyMeeus (V15), JPL Horizons (V06) and the printed Meeus example (V32 in
`observer_test.cpp`) remain under their source terms, outside the project MIT grant.
The [numeric receipts and attribution](../src/test/provenance/observer/ATTRIBUTION.md)
retain only the required inputs/outputs. No SOFA/PyMeeus implementation, binary or book
page is added by this dataset. PyMeeus output use remains conditional on its source
terms; the project licence does not grant rights in that output.

<a id="source-terms"></a>
## Source terms

- SOFA: the retained [2023-10-11 terms](../src/test/provenance/sofa/2023-10-11/doc/copyr.lis)
  and the SOFA section of [THIRD_PARTY_NOTICES.txt](../THIRD_PARTY_NOTICES.txt).
- PyMeeus: [LICENSE.txt at the oracle commit](https://github.com/architest/pymeeus/blob/c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92/LICENSE.txt),
  GNU GPL version 3. The [V15 disposition](../src/test/provenance/retained/record.json)
  records the conditional output boundary; it is not a conclusion about GPL output rights.
- Horizons: the [manual acknowledgements](https://ssd.jpl.nasa.gov/horizons/manual.html#acknowledgements)
  and the [JPL policies and important notices](https://www.jpl.nasa.gov/caltechjpl-privacy-policies-and-important-notices)
  linked by that manual. These are source/terms pointers, not an output-specific grant.
- Meeus: the book's reproduction reservation is recorded in the existing
  [second-edition terms record](../src/test/provenance/retained/MEEUS_R22_ATTRIBUTION.md).
  Its p.168 numeric series is separate from the observer examples; the
  [V32 disposition](../src/test/provenance/retained/record.json) covers printed worked
  examples and records unavailable permission evidence. No book prose or scan is supplied.

These records identify the retained-source boundaries; links do not grant redistribution
permission. In particular, the notices bundle is not the terms source for PyMeeus,
Horizons or the observer's Meeus examples.

## Offline regeneration from a clone

Only Python's standard library and the cloned files are needed:

```sh
python statistics/observer_reference.py --output observer_reference_data.hpp
cmp observer_reference_data.hpp src/test/astro/observer_reference_data.hpp
python statistics/observer_oracle_replay.py
```

The generator checks the three complete receipt hashes in
[SOURCE_PINS.json](../src/test/provenance/observer/SOURCE_PINS.json). Defaults are
resolved relative to the script, not the current directory; `--output` is explicit.
The replay's default is the independent offline calibration, not an external download.
It repeats the original pre-observer Cartesian translation and ENU projection from
frozen Horizons-geocentric and base-native-geocentric/GAST inputs. Each reduced component
requires finite actual and expected values and
`abs(actual - expected) <= 2 * max(math.ulp(actual), math.ulp(expected))`.
This allows twice the larger floating-point spacing, not two adjacent-value steps
across a power-of-two boundary, for runtime-library rounding differences. Receipt
hashes, full fixture bytes and both derived tolerance tables still require exact
identity. This arithmetic replay allowance does not change the astronomical tests or
their tolerances.
The native inputs came from `4d9efd52ed8ef83946208b1e2d6abe91deaeac01`,
before `observer.hpp` existed; replaying them does not recompute the base providers.
The reduction mechanism imports no production observer code.

The three numeric receipts have these schemas; all angles are degrees:

- `sofa-observer-preflight.json`: seed, semi-major axis metres, flattening, entry-source
  hash; cases contain `longitude_deg`, `latitude_deg`, `height_m`, `sofa_xyz_m`.
- `pymeeus-matched-constant-preflight.json`: seed, entry-source hash, controlled factor;
  cases contain `ra_deg`, `dec_deg`, `distance_au`, `latitude_deg`, `height_m`,
  `hour_angle_deg`, `oracle_distance_au`, `matched_ra_deg`, `matched_dec_deg`.
- `calibration-results.json`: base `source_sha`; cases contain `target_code`,
  `site` (label, east longitude, latitude, height km), `jde_tt`, `jd_ut1`,
  `tt_minus_utc_seconds`, `dut1_seconds`, `gast_deg`, two geocentric inputs
  (`library_geocentric`, `horizons_geocentric`: RA, Dec, AU) and three five-column
  outputs (`expected`, `native_reduced`, `horizons_reduced`: RA, Dec, AU, azimuth, altitude).

The frozen reduced columns supply the per-column model gaps; they are not production
observer results. Automated tests replay both reductions, regenerate the complete
fixture byte for byte and compare both derived tolerance tables.

## Exact-input replay

The SOFA and PyMeeus cases were collected on 2026-11-07, seed 42, with 72 and 128 rows.
The pins identify SOFA issue 2023-10-11, `gd2gce.c` SHA-256
`eefa9b1f6d30255c08ddd823e967d6a99d399d6f39c4b086461d8a9e85f701c9`, and PyMeeus
commit `c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92`, `pymeeus/Earth.py` SHA-256
`127b520803cb275e01031aab76f2771faada42d6ff69eee1e35862d9a30110bb`.
PyMeeus `Earth.parallax_correction` constructs `Earth()` with its **WGS84** default,
not IAU1976. No release-archive identity is inferred from `setup.py`.
Compile the unmodified SOFA 2023-10-11 `gd2gce.c` as an external shared library, using its
own include directory and `-lm`. Supply that library and an external PyMeeus checkout:

```sh
python statistics/observer_oracle_replay.py --sofa-library /path/to/sofa-gd2gce.so --pymeeus-root /path/to/PyMeeus
python statistics/observer_oracle_replay.py --horizons-dir /path/to/retained-horizons-responses
```

The replay reads the exact fixture inputs, never imports the production reduction, and
checks every retained output. SOFA's semi-major axis is 6378137 m, flattening
1/298.257223563. Its tolerance is 5e-9 m, above the measured 1-2e-9 m arithmetic/form gap.
The controlled PyMeeus comparison uses 1e-12 degrees per RA/Dec column, wrap-aware.
Its actual oracle distance is retained separately: multiply physical distance by
`sin(8.794 arcsec)/(6378.137/149597870.7)`. This matches the parallax sine without
silently identifying the rounded 8.794 arcsec coefficient with WGS84.
The ordinary branch is used only away from the celestial-pole counterexample covered
independently in `observer_test.cpp`.

`observer_reference.py` emits aligned C++ arrays from three JSON receipts using
`--sofa`, `--pymeeus`, `--horizons` and `--output`. SOFA rows hold longitude/latitude,
height and `sofa_xyz_m`; PyMeeus rows hold the physical/oracle distances and matched
RA/Dec; Horizons rows hold exact times, site, geocentric inputs and topocentric outputs.
The script derives tolerances only from the supplied frozen pre-implementation
`horizons_reduced` and `native_reduced` columns, never from the new observer query.

## Horizons request conditions

Use `https://ssd.jpl.nasa.gov/api/horizons.api`, observer ephemerides, `TIME_TYPE=TT`,
`TLIST_TYPE=JD`, `TLIST=2451545.0,2461351.5,2461351.875`, `ANG_FORMAT=DEG`,
`EXTRA_PREC=YES`, `CAL_FORMAT=BOTH`, `CSV_FORMAT=YES`, `APPARENT=AIRLESS`,
`REF_SYSTEM=ICRF`, `CAL_TYPE=GREGORIAN`, `RANGE_UNITS=AU`, `OBJ_DATA=NO` and `MAKE_EPHEM=YES`.
Targets are 10, 301, 199, 299, 499, 599, 699, 799, 899, 999 (Sun through Pluto).
For each target request geocentre `CENTER=500@399`, quantities 2,20, then the two
`CENTER=coord@399`, `COORD_TYPE=GEODETIC` sites, quantities 2,4,20,42,49:

| Label | East longitude | Geodetic latitude | Ellipsoidal height (km) |
| --- | ---: | ---: | ---: |
| north | -116.8625 | 33.3561111 | 1.706 |
| south | 151.2 | -33.85 | 0.05 |

Save responses as `sun-geocenter.txt`, `sun-north.txt`, `sun-south.txt`, and likewise
for the other targets. The replay checks airless output, source geocentric input,
topocentric RA/Dec/range/azimuth/altitude and Q49-derived UT1. TT-UTC is 64.184 s at
J2000 and 69.184 s at the 2026 epochs. These are the actual source inputs, not the
library's modelled UTC-to-UT1 values.

All 30 original responses explicitly print `API VERSION: 1.2`, `API SOURCE: NASA/JPL Horizons API`
and EOP file `eop.261106.p270202`. Their exact request dictionaries, response hashes and
target/centre source labels are retained in `SOURCE_PINS.json`. Sun, Moon, Mercury and
Venus use `DE441` for both; Mars uses `mar099`/`DE441`; Jupiter uses
`jup365_merged` for both; Saturn uses `sat441l`/`DE441`; Uranus uses
`ura184_merged`/`DE441`; Neptune uses `nep098_merged` for both; Pluto uses
`plu060_merged`/`DE441`. The service exposes those labels, not exact kernel revisions.
Fresh service results may differ as kernels/EOP are updated. Response prose is not
needed for offline numeric regeneration and is not redistributed by this dataset.

The two test layers distinguish source-geocentric translation from the full native
provider. Each target/column uses three times the frozen calibration's largest gap,
rounded upward to one significant digit. These finite-dataset regression bounds
include upstream ephemeris, diurnal aberration, EOP and observer light-time/range
definition differences; they are not API accuracy promises or convergence tolerances.
The civil UTC tests separately check resolver composition and model/provider admission.

Meeus Examples 11.a/40.a use IAU1976 and rounded rho/parallax inputs. Their printed
precision determines their tolerances; the production WGS84 path is tested separately.
