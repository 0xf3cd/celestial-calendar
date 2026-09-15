# Celestial Calendar

Astronomical calculations and Gregorian / Chinese Lunar date conversion, with Jieqi (节气, the 24 solar terms),
Sun/Moon positions, and sunrise/sunset. The core is a C++23 header-only library, also available through Python,
JavaScript / TypeScript, and a C ABI.

[中文文档](README.md) · [Python](#python) · [JavaScript / TypeScript](#javascript) ·
[C++](#cpp) · [C / FFI](#c-abi)

This guide describes the **0.7.0 source and APIs**, not a claim that 0.7.0 has been published. The install commands
below select versions currently available on PyPI or npm, which may lag this guide. Check the documentation shipped
with your installed version. To use APIs not yet published, build from the full checkout using the
[local Python wheel recipe (Chinese)](bindings/python/README.md#build-a-local-wheel) or the [WASM/npm recipe](README.md#wasm).

For Ganzhi (干支) calculations, use [bazi](https://github.com/0xf3cd/bazi); this library has no Ganzhi API.

<a id="python"></a>
## 1. Python

Python 3.11 or newer is supported. Install in your application's virtual environment:

```sh
python -m pip install celestial-calendar
```

Each wheel contains its platform's native library, so using it requires no compiler or separate CelestialCalendar
installation. Importing neither searches system libraries nor downloads a fallback. See the
[package platform table (Chinese)](bindings/python/README.md) for available wheel targets.

Each Python block below is a standalone program for `example.py` in your application directory. Run it with
`python example.py` in the environment where the package is installed.

### Today's Lunar Date

Choose today's civil date at a fixed UTC+8 offset first, then convert that date label with `ALGO3`.
This does not involve UT1.

```python
from datetime import datetime, timedelta, timezone

import celestial_calendar as celestial

east_eight = timezone(timedelta(hours=8))
today = datetime.now(east_eight).date()
gregorian = celestial.GregorianDate.from_date(today)
lunar = celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO3, gregorian)
print(today, lunar)
```

`GregorianDate.from_date()` accepts a standard-library `date`, not a `datetime`. Calling `.date()` after selecting
the offset is deliberate: the library does not silently discard a time or timezone. The lunar algorithms use
different date bases and year windows; see [Choosing Inputs](#choosing-inputs).

### Next Jieqi and Remaining Days

The reference instant is explicitly supplied as **2026-12-31 00:00 UT1**, not taken from the current clock.
Query every Jieqi in both 2026 and 2027, sort by actual UT1 moment, and select the first strictly later event.
The enum starts at Lichun (立春), not the first event in a Gregorian year.

```python
import celestial_calendar as celestial

cutoff_ut1 = celestial.CivilDateTime(2026, 12, 31, 0)
cutoff_jd = celestial.ut1_to_jd(cutoff_ut1)
events = [celestial.jieqi_moment(year, jieqi) for year in (2026, 2027) for jieqi in celestial.Jieqi]
ordered = sorted(events, key=lambda event: celestial.ut1_to_jd(event.moment_ut1))
upcoming = next(event for event in ordered if celestial.ut1_to_jd(event.moment_ut1) > cutoff_jd)
remaining_days = celestial.ut1_to_jd(upcoming.moment_ut1) - cutoff_jd
print(celestial.jieqi_name(upcoming.jieqi))
print("UT1:", upcoming.moment_ut1)
print("Remaining UT1 days:", remaining_days)
```

The difference between two UT1 Julian Days retains fractional days; subtracting calendar dates would lose the
time of day. This is a fixed contemporary query across one year boundary, not a search algorithm for the entire
API domain. Jieqi queries accept years in `[401, 32766]`, but an in-range calculation can still fail if it cannot
produce a unique moment. A returned UT1 year can differ from the query year.

`moment_ut1` is not UTC or a UTC+8 wall clock. Establish the time-scale conversion before displaying either.

### Gregorian / Lunar Round Trip

```python
from datetime import date

import celestial_calendar as celestial

day = date(2024, 2, 10)
gregorian = celestial.GregorianDate.from_date(day)
algorithm = celestial.LunarAlgorithm.ALGO3
lunar = celestial.gregorian_to_lunar(algorithm, gregorian)
restored = celestial.lunar_to_gregorian(algorithm, lunar).to_date()
print(lunar)
print(restored, restored == day)
```

Both bridges preserve a date label, not an instant. `LunarDate` uses traditional month numbers and a separate
`is_leap` flag. `GregorianDate.to_date()` is limited to the standard library's years `[1, 9999]`; project Gregorian
values have the wider calculation domain `[1, 32767]`.

The flat Python API uses enums, frozen dataclasses, scalars, and tuples rather than exposing ctypes. Wrong types
raise `TypeError`, Python value guards raise `ValueError`, and native failures raise `CelestialError`. Its
`operation` names the public call; `recorded` says whether the message came from the native error channel.
A legitimate absence remains `None` or `()`. The [Python API docstrings](bindings/python/src/celestial_calendar/__init__.py)
provide the per-function contracts in English.

<a id="javascript"></a>
## 2. JavaScript / TypeScript

Node 22 or newer is supported. Browser consumers are tested on Chrome and Firefox. Install in your application
directory:

```sh
npm install @0xf3cd/celestial
```

Each JavaScript block below is a standalone ES module for `example.mjs` in that directory; run it with
`node example.mjs`. TypeScript declarations ship with the package. For browser deployment, keep the emitted `.wasm`
URL available and serve the asset with `Content-Type: application/wasm`.

Call and await `init()` before using calculation methods, which then run synchronously. Concurrent and repeated
calls share one initialization promise, resolving to `undefined`. A failed load preserves the loader's original
rejection, and a later explicit call may retry.

### Today's Lunar Date

Use `/date` to choose the fixed UTC+8 civil date. Destructure only the year, month, and day: the full civil record
contains `fraction`, which a Gregorian date input must not carry.

```js
import * as celestial from "@0xf3cd/celestial";
import { dateToCivilAtOffset } from "@0xf3cd/celestial/date";

await celestial.init();
const { year, month, day } = dateToCivilAtOffset(new Date(), 480);
const gregorian = { year, month, day };
const lunar = celestial.lunar.fromGregorian("algo3", gregorian);
console.log(gregorian, lunar);
```

### Next Jieqi and Remaining Days

As in the Python example, the supplied cutoff is **2026-12-31 00:00 UT1**. No `Date` timestamp is treated as UT1.

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const cutoffUt1 = { year: 2026, month: 12, day: 31, fraction: 0 };
const cutoffJd = celestial.time.ut1ToJd(cutoffUt1);
const upcoming = [2026, 2027]
  .flatMap((year) => Object.values(celestial.Jieqi).map((jieqi) => {
    const event = celestial.jieqi.moment(year, jieqi);
    return { ...event, jdUt1: celestial.time.ut1ToJd(event.momentUt1) };
  }))
  .filter((event) => event.jdUt1 > cutoffJd)
  .sort((a, b) => a.jdUt1 - b.jdUt1)[0];
if (upcoming === undefined) throw new Error("No later Jieqi in the queried years.");
console.log(celestial.jieqi.name(upcoming.jieqi));
console.log("UT1:", upcoming.momentUt1);
console.log("Remaining UT1 days:", upcoming.jdUt1 - cutoffJd);
```

This enumerates all terms in both years and orders them by moment, not enum value. Remaining days are fractional
UT1 days. `momentUt1` is UT1; the example scope, query-year domain and failure conditions are the same as
in the Python example.

### Fixed-Offset Date Round Trip

The pure `/date` subpath does not import the root entry or load WASM, and needs no `init()`. Offsets are minutes
east of UTC; `480` selects fixed UTC+8.

```js
import { civilAtOffsetToDate, dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const date = new Date("2024-02-09T16:00:00.123Z");
const eastEight = dateToCivilAtOffset(date, 480);
const restored = civilAtOffsetToDate(eastEight, 480);
console.log(eastEight);
console.log(restored.toISOString(), restored.getTime() === date.getTime());
```

The same-offset round trip preserves the `Date` millisecond timestamp. The subpath also exports `dateToCivilUtc()`
and `civilUtcToDate()`. It does not convert UT1 / TT, account for leap seconds, or implement IANA zones or DST.
Do not pass `momentUt1` to `civilUtcToDate()` as though it were UTC.

Root APIs read explicit fields, not `Date` methods or timestamps. `GregorianDate`, `CivilDateTime`, and `LunarDate`
are disjoint record kinds; unrelated extra fields are allowed, but the other kinds' tags are rejected.
`CivilDateTime` uses `fraction` in `[0, 1)`. Results also supply derived `hour`, `minute`, and fractional `second`;
subsequent calls still read `fraction`, not those clock fields.

Before initialization completes, namespace methods throw `CelestialError` with `recorded === false`, before
argument validation. After initialization, wrong shapes or types throw `TypeError`, JavaScript value guards throw
`RangeError`, and native failures throw `CelestialError`. A legitimate absence remains `null` or `[]`.
See the English [type declarations](bindings/javascript/types/index.d.ts) and
[/date declarations](bindings/javascript/types/date.d.ts) for individual contracts.

The 0.7.0 source also provides the npm alias `celestial-calendar`, forwarding the root and `/date` entries to an
exact same-version `@0xf3cd/celestial` dependency without another WASM copy. The names share exports and initialized
state only when they resolve to the same primary installation, not across arbitrary mixed-version dependency
graphs. Check availability before choosing the alias; see its [package guide (Chinese)](bindings/javascript-alias/README.md).

<a id="cpp"></a>
## 3. C++

Use a source checkout or source archive. The library is header-only: compile its headers with your application,
without building or linking a separate CelestialCalendar library. Native release ZIPs do not contain the C++
header tree.

This complete `quickstart.cpp` queries the UT1 moment of winter solstice, Dongzhi (冬至):

```cpp
#include <iostream>

#include "jieqi.hpp"

int main() {
  using namespace calendar::jieqi;
  const Jieqi jq = Jieqi::冬至;
  const auto moment = jieqi_ut1_moment(2026, jq);
  std::cout << name_of(jq) << " 2026 (UT1): " << moment.year() << '-' << moment.month() << '-'
            << moment.day() << ", day fraction " << moment.fraction() << '\n';
}
```

Run from the source root with a C++23-capable clang++ installation:

```sh
# Headers include each other by basename; all three directories are needed.
clang++ -std=c++23 -I src/astro -I src/calendar -I src/util \
  quickstart.cpp -o quickstart
./quickstart
```

The Jieqi API is in [`src/calendar/jieqi.hpp`](src/calendar/jieqi.hpp), including `jieqi_ut1_moment`, `jieqi_jde`,
and `JieqiGenerator`. Other self-contained headers are organized under `src/astro/`, `src/calendar/`, and `src/util/`.
See [core features and algorithms](README.md#features) for the full scope.

<a id="c-abi"></a>
## 4. C and FFI

The C ABI in [`celestial.h`](src/shared_lib/celestial.h) supports C, ctypes, and other FFIs. This is a complete
`quickstart.c` for the same query:

```c
#include <stdio.h>

#include "celestial.h"

int main(void) {
  char name[16]; /* index 21 = 冬至 in the to_index order (0 = 立春) */
  if (!get_jieqi_name(21, name, sizeof name)) {
    fprintf(stderr, "%s\n", last_error());
    return 1;
  }
  const JieqiMomentQuery m = query_jieqi_moment(2026, 21);
  if (!m.valid) {
    fprintf(stderr, "%s\n", last_error());
    return 1;
  }
  printf("%s (UT1): %d-%02u-%02u, day fraction %.6f\n", name, m.y, m.m, m.d, m.frac);
  return 0;
}
```

Open a chosen version on [Releases](https://github.com/0xf3cd/celestial-calendar/releases) and download the native
ZIP for your platform. Public downloads need no GitHub token. Check the canonical
[native runtime table](README.md#native-runtime): supported floors are the compatibility promise, while measured
values record the artifact's requirements and do not lower that promise. Linux requires the system
`libstdc++.so.6` and `libgcc_s.so.1`; no Windows OS version floor is declared.

- Linux x86_64: `linux_amd64.zip`, with `lib/libcelestial_calendar.so` and its versioned files.
- Linux arm64: `linux_arm64.zip`, with the same library layout.
- macOS arm64: `macos_arm64.zip`, with `lib/libcelestial_calendar.dylib` and its versioned files.
- Windows x86_64: `windows_x86_64.zip`, with link-time `lib/celestial_calendar.lib` and runtime `bin/celestial_calendar.dll`.

Every ZIP includes `include/celestial.h`. Extract the entire ZIP into `native/` in the application directory,
preserving all library filenames: the runtime loader also uses the versioned name. Unix uploads dereference
installed symlinks, so the downloaded versioned libraries are ordinary files.

On Linux, run from the directory containing `quickstart.c` and `native/`, recording an absolute runtime search path:

```sh
prefix="$PWD/native"
cc -std=c11 quickstart.c -I "$prefix/include" -L "$prefix/lib" \
  -lcelestial_calendar -Wl,-rpath,"$prefix/lib" -o quickstart_c
./quickstart_c
```

On macOS, link the `.dylib` from `lib/` and make that directory available to the runtime loader. On Windows, use
an MSVC-compatible toolchain, link `lib/celestial_calendar.lib`, and put `bin/celestial_calendar.dll` beside the
executable. Consumers do not define `CELESTIAL_BUILDING_DLL`.

If you built the shared library yourself, the corresponding Linux command from the source root is:

```sh
cc -std=c11 quickstart.c -I src/shared_lib -L build/shared_lib \
  -lcelestial_calendar -Wl,-rpath,"$PWD/build/shared_lib" -o quickstart_c
./quickstart_c
```

Check `valid` before reading a returned result structure. Follow each scalar/count contract in `celestial.h`;
a zero count can be a legitimate empty result. Every export except `last_error()` clears or records the calling
thread's error message. Read or copy the library-owned string before another recording call on the same thread.
Jieqi moments are UT1, not UTC or a UTC+8 wall date.

<a id="choosing-inputs"></a>
## 5. Choosing Inputs

Gregorian / lunar conversions operate on calendar-date labels, not instants. Choose an algorithm by its source
and compatibility role, not an assumed accuracy ranking:

- Algo1 preserves Hong Kong Observatory labels for lunar years 1901–2099.
- Algo2 computes from VSOP87D / truncated ELP2000-82B for lunar years 410–2500. It renders TT through the library's
  UTC model, then applies a fixed eight-hour eastward offset. Before 1972 the model uses UT1 as a proxy; from 1972
  it uses the leap-second table, holding ΔAT at 37 seconds after the final entry.
- Algo3 is a baked hybrid for lunar years 1600–2199, using Algo1 / HKO for 1901–2099 and Algo2-generated dates elsewhere.

These are lunar-year bounds, not Gregorian January-to-December bounds. Query them with
`supported_lunar_year_range()` in Python or `lunar.supportedYearRange()` in JavaScript. Gregorian coverage runs
from the first day of the first supported lunar year through the last day of the last supported lunar year.
The Algo2 ceiling is an enforced civil-date domain, not a physical limit of the astronomical method.

Keep time scales separate. JD UT1 and JDE TT are not interchangeable; Jieqi results are UT1, whereas
`apparent_solar_time()` / `sun.apparentSolarTime()` take civil UTC. DUT1 is not modelled, and the pure `/date`
bridge supplies no UTC-to-UT1 or Date-to-JDE conversion. Geographic longitude in these binding APIs is
east-positive. Solar and lunar angular results are degrees; solar distance is AU, lunar distance is kilometres.
The equation of time is degrees of hour angle, so multiply by 240 for seconds of time.

For ΔT (TT minus UT1, in seconds), default / Algo5 is the current model: Algo2 before 2005, an IERS Bulletin A
fit through about 2026.41, then anchored long-term extrapolation. Algo1–Algo4 are frozen comparison models;
Algo2 remains the live pre-2005 branch. A model's hard year domain is not an accuracy guarantee, and fitted
residuals or test tolerances are not statistical error bounds. The [canonical algorithm description](README.md#features)
and [reference list](README.md#references) give the sources; English per-model contracts remain in the API
docstrings and declarations linked above.

## 6. Building and Checking

These are source-development instructions, not prerequisites for using installed packages. Work from a full
checkout. The core build needs a C++23 compiler, CMake ≥ 3.22, make, and Python 3. CI uses clang++ 22 on Linux and
Windows, Xcode 26's Apple clang on macOS, and g++ 14 on Linux; older compilers are not checked.

On macOS / Linux, from the source root:

```sh
python3 -m venv .venv
. .venv/bin/activate
export CXX=clang++
python project.py --all
```

The virtual environment also avoids PEP 668 restrictions on distro-managed Python. `--all` performs setup,
configure, build, and test; it does not run benchmarks. Use `--setup`, `--cmake`, `--build`, `--test`, or `--bench`
separately as needed, and `--clean` to remove build outputs. Randomized tests default to seed 42; set
`CELESTIAL_TEST_SEED` to replay another seed.

Windows also needs LLVM and make because the build uses CMake's `Unix Makefiles` generator. With Chocolatey
installed, `choco install -y make llvm` supplies those two tools. From the source root in PowerShell:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
$env:CXX = "clang++"
$env:CC = "clang"
python project.py --all
```

The separate [local wheel recipe (Chinese)](bindings/python/README.md#build-a-local-wheel) uses the full checkout,
hash-locked Python build requirements, system CMake, and an installed C/C++23 toolchain. It builds only a wheel,
not an sdist. That host-local wheel uses the host's native libraries; it is not an official repaired portable
wheel and does not extend the official platform list.

For JavaScript source builds, use the [WASM/npm recipe](README.md#wasm). It requires Node ≥ 22, Python build
dependencies, and an emsdk checkout selected through `EMSDK` or `--emsdk`. The builder packs primary and alias
once each. `build/npm/npm-pack.json` and `build/npm/npm-alias-pack.json` select the respective tarballs; do not
select them by glob or rebuild them for each consumer.

[Static analysis](README.md#lint) uses Ruff and clang-tidy through `checks.py`, with clang-tidy warnings treated
as errors. [AGENTS.md](AGENTS.md) is the source of truth for build, test, and contribution conventions, including
direct test-binary execution and reconciliation against the test macros rather than accepting ctest counts alone.

## 7. Downloads and Reference Data

Users can download native / WASM archives, Python wheels with SHA-256 sidecars, and source archives from the
public [Releases page](https://github.com/0xf3cd/celestial-calendar/releases) without a token. Use the documentation
for the selected release; building this checkout does not publish a version.

[CI artifact downloads](README.md#artifacts) are a separate contributor/operator path. The optional
`toolbox/artifact_downloader.py` and `toolbox/release_downloader.py` helpers require the `Requirements.txt`
dependencies and GitHub authentication through `GITHUB_TOKEN`. The former selects builds of HEAD by default;
the latter downloads the latest release. Their `--help` output documents the options. Maintainers publishing a
release must follow [docs/RELEASING.md](docs/RELEASING.md), not the consumer install instructions.

For an application that needs a static event table without a linked library, the [Jieqi JSON exporter](README.md#jieqi-table)
provides entries sorted by actual moment. Its default 1950–2051 window includes a successor year for 1950–2050.
Despite the `iso_utc` field name, the table's time scale is UT1; read its `timescale_note` before interpreting the
timestamp fields.

Further reading:

- [Native runtime requirements](README.md#native-runtime)
- [Algorithm sources and reference links](README.md#references)
- [License scope and third-party exceptions](README.md#13-license)
- [Release downloads](README.md#releases) and [remaining work](README.md#todo)
