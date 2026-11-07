# Observer reference fixtures

`src/test/astro/observer_reference_data.hpp` retains numeric oracle output only.
SOFA (V20), PyMeeus (V15), JPL Horizons (V06) and the printed Meeus example (V32 in
`observer_test.cpp`) remain under their source terms, outside the project MIT grant.
Existing source notices are in `THIRD_PARTY_NOTICES.txt`. No SOFA/PyMeeus implementation,
binary or book page is redistributed here. PyMeeus output use remains conditional on its
source terms; the project licence does not grant rights in that output.

## Exact-input replay

The SOFA and PyMeeus cases were collected on 2026-11-07, seed 42, with 72 and 128 rows.
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

The two test layers distinguish source-geocentric translation from the full native
provider. Each target/column uses three times the frozen calibration's largest gap,
rounded upward to one significant digit. These finite-dataset regression bounds
include upstream ephemeris, diurnal aberration, EOP and observer light-time/range
definition differences; they are not API accuracy promises or convergence tolerances.
The civil UTC tests separately check resolver composition and model/provider admission.

Meeus Examples 11.a/40.a use IAU1976 and rounded rho/parallax inputs. Their printed
precision determines their tolerances; the production WGS84 path is tested separately.
