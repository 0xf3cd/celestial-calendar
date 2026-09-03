# Agent Instructions — CelestialCalendar

> **This file is the single source of truth (SSOT) for agent instructions.**
> `CLAUDE.md` is only an import pointer to this file. If anything elsewhere
> (README, PR comments, habit) conflicts with this file, this file wins — or fix
> the file, don't fork the convention. Don't create new per-tool instruction
> files — extend this one.

## Project Overview

**CelestialCalendar** — A C++23 astronomical-calculation library that converts among
Gregorian, Lunar, and Chinese Ganzhi calendars, and computes accurate Jieqi (节气)
moments, sun/moon positions, and sunrise/sunset. 天文计算与历法转换.
Repository: `https://github.com/0xf3cd/celestial-calendar` · License: MIT.

Write code **in the author's style**, so contributions stay indistinguishable from
hand-written code. For *style* questions, read a neighbouring header and imitate it
rather than inventing a convention.

## Human-in-the-loop (read first)

Stop and confirm with the author before proceeding whenever anything is unclear or
non-obvious — an ambiguous requirement, a design fork with no precedent in the code, an
algorithm/reference you're unsure of, an unexpected test failure, or any change whose
intent you can't verify from existing code. Prefer asking over guessing.

## Correctness is numerical, measured against references

This is precision astronomy, not vibes. Every algorithm traces to a named reference
(Meeus, VSOP87D, ELP2000-82B, SOFA, USNO ΔT — see README §References).

- **Name cross-formula physical constants** as `constexpr` (UPPER_CASE), shared between
  implementation and tests. Single-use coefficients of a cited reference polynomial stay
  **inline in the formula** with the `@ref` alongside (like `obliquity::mean`, the ΔT
  segments): naming each coefficient of a quoted equation adds indirection without audit
  value. Dense coefficient tables (e.g. `MEEUS_NUTATION_COEFFS`) keep raw literals with a
  source comment. `.clang-tidy` disables the magic-number checks for exactly these two cases.
- Never "simplify" a formula or drop a term for cleaner code — the constants ARE the algorithm.
- Correctness is proven by **golden datasets**: tests hold high-precision reference values
  and `ASSERT_NEAR` to a per-column tolerance. New numerics ⇒ add a reference dataset + tolerance.
- **Never loosen an existing tolerance on your own.** Surface the discrepancy instead of
  making the test green; only the author confirms case-by-case that it's an
  algorithm/reference issue, not a code bug.
- Cite the source of any new algorithm / coefficients in a comment.
- **Comments: sparse but load-bearing.** Key spots get 1-2 lines of *why* — segment
  boundaries, magic constants, bug fixes (with the issue number). No restating the code,
  no quoting the old expression, no measurement reports (those live in the PR/issue).
  Slim your added comments before wrapping up; that pass is part of the change.
- **ΔT provenance lives out-of-repo**: `delta_t.hpp` algo5 (the default) is trained in
  [AstroTime-Analysis](https://github.com/0xf3cd/AstroTime-Analysis) (`DeltaT/algo5.py`);
  algo1–algo4 are frozen exhibits. Refresh ritual: re-run that repo's downloaders +
  `algo5.py`, publish and retain a new algo5 grant/record commit, then repin the coefficients,
  V25 relation and provenance gates here — a new algoN is only for a methodology change.
  In-repo `statistics/` holds evaluation notebooks and golden-dataset crawlers — **no model
  training**.

### Golden data and the validation chain (worked example: sidereal time)

This is the layer recipe that the sidereal-time work validated in practice; treat it
as the **shape**, not a mandate that every numeric has all three layers. A new numeric
earns trust by **generating the data before fixing the tolerance**, drawing on whichever
authorities exist for that algorithm:

1. **Book-example goldens** — Meeus worked examples (12.a, 13.a/13.b, …). Tolerance
   follows the book's printed digits: 6 decimals → 5e-7, 4 → 1e-4, loosened where the
   book rounds an intermediate.
2. **pymeeus cross-dataset** — import the `pymeeus` clone (pure Python) directly, emit
   seeded random points (~60) as C++ initializer lines, substitute into the test file.
   Tolerance ~1e-6 after a sanity-check pass. When this repo and pymeeus disagree,
   **suspect this repo first** — this layer caught the bare-(12.3) 0h-grid error before
   any test existed. Not every algorithm has a pymeeus counterpart — use whichever
   independent implementation exists, or skip the layer and say so in the PR.
3. **USNO online API** — the external authority for sidereal time
   (`aa.usno.navy.mil/api/siderealtime`); other numerics use their own authority
   (an observatory dataset, a standards table). Tolerance ≈ 3× the measured model gap
   (IAU 1980/1982 here vs USNO's modern model, measured ≲0.07″).

USNO API gotchas: `coords=lat,lon` — latitude first, **east-positive** longitude; a
swapped pair is silently ignored (`last == gast`), so probe one point first. The API
quantizes coordinates to ~4 decimals — round before querying **and store the rounded
value**, or full-precision residuals carry ~0.18″ of API noise. `reps` / `intv_mag` /
`intv_unit` are all-or-nothing; the date window is the current year ±1; up to 9999
points per call; sleep ~0.15 s between calls.

## Tech Stack

- **C++23** — Core library (`src/`); CI builds it with clang++ 22 and g++ 14. Older compilers
  may work; nothing checks them.
- **CMake ≥ 3.22** — Build system.
- **Python 3** — Build/test/lint automation (`project.py`, `checks.py`, `automation/`,
  `toolbox/`). Core build and lint tasks are Python-orchestrated.
- **Node.js ≥ 22 / npm** — `bindings/javascript/` package tests and consumers; CI pins the
  runtime floor and the current build toolchain separately.
- **GoogleTest** — Fetched at configure time by CMake (`src/test/CMakeLists.txt`).
- **clang-tidy** — C++ static analysis; **ruff** — Python linting/formatting.

## Build, Test, and Lint

Preferred entry point is `project.py`. Set `CXX` (and `CC` on Windows) to a C++23-capable
compiler before running.

```sh
# macOS / Linux
export CXX=clang++
./project.py --all                          # full setup + configure + build + test
./project.py --test -k integration -v 1     # filtered, verbose tests
```

Windows PowerShell: `$env:CXX = "clang++"; $env:CC = "clang"`, then the same commands.
Individual steps: `--setup` / `--cmake` / `--build` / `--test` / `--bench` / `--clean`.

The WASM/npm exception has its own shared manual/CI path: `npm ci --ignore-scripts --prefix
bindings/javascript`, `python3 toolbox/build_wasm.py`, `node toolbox/wasm_check.mjs`, then
`python3 toolbox/build_npm.py`. Consumer tests take the exact tarball named by
`build/npm/npm-pack.json`; do not select it with a glob or rebuild it per consumer. The release
workflow publishes that exact tested tarball to npm; it never repacks from source.

Python wheels likewise use their independent package path: install the exact host pins from
`bindings/python/requirements-host.txt`; the before-build hook installs `requirements-build.txt`,
and `constraints-cibuildwheel.txt` selects the bootstrap pins. Then run `python -m cibuildwheel
--only <identifier> bindings/python --output-dir wheelhouse`. The wheel is `py3` but native per platform;
floor and current interpreters must install the same repaired wheel from an unrelated cwd. Do not
build an sdist. The release workflow publishes the same four CI-tested wheels to PyPI and GitHub
Release; it never builds another distribution.

Benchmarks are opt-in and `--all` leaves them out (targets are `EXCLUDE_FROM_ALL`). They
live in `src/bench/`, not `src/test/` — that directory turns every `.cpp` into a
GoogleTest target. Adding `src/bench/bench_*.cpp` is the whole job of adding a benchmark:
the glob is `CONFIGURE_DEPENDS`, the runner finds the binary by directory, and `--bench`
clears stale binaries first. Absolute nanoseconds are never comparable across machines,
and across runs only when a round is long enough to amortize its fixed cost; otherwise
read the paired ratios inside one run. `src/bench/harness.hpp` explains the
measurement-bias handling and why (#81).

Randomized tests draw from a shared, seeded engine (`util/random.hpp`, #69): default seed
42, override with the `CELESTIAL_TEST_SEED` env var. The PR/push gate pins the default;
`random_soak.yml` (weekly / dispatch) draws a fresh seed per run. A soak failure is never
a flake: replay the failing ctest entry with the same seed, then bake the find into a
directed regression test.

Direct CMake (`cmake -S src -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
--parallel`) works too — but run `ctest --test-dir build/test`, never bare
`ctest --test-dir build`: without the subdirectory it reports **zero** tests and still
exits 0, a green light that checked nothing (#72).

### Lint

- `clang-tidy` runs with **`WarningsAsErrors: '*'`** — zero warnings. `.clang-tidy` is the
  config; respect its deliberate disables (magic-numbers, identifier-length,
  identifier-naming) instead of "fixing" code to satisfy a disabled check.
- Python: `./checks.py --ruff` checks Ruff lint and formatting; `ruff format .` applies formatting.
  Configuration lives in `.ruff.toml`.
- Run `./checks.py --all` to check both C++ and Python.

## Code Style

When rules conflict, arbitrate by: const / compile-time / concepts are the default state,
not fastidiousness · the code should read next to the book — audit value outranks tidiness ·
the λ/β, UDL, phi touches are the project's signature, never bland them away. Transcription,
scaffolding, and CI can be delegated freely; API shape, type design, the numerical core,
and tolerance calls stay with the author.

### Naming

Nothing enforces this — `readability-identifier-naming` is deliberately off (#72). The
convention lives here:

`lower_case` — variables, functions, parameters, members, methods, namespaces, and any
`const inline auto` that is called rather than read.
`CamelCase` — classes, structs, **and enum types** (`Algo`, `AngleUnit`, `Jieqi`).
`UPPER_CASE` — `inline constexpr` constants and enumerators.

**Domain notation overrides all of it.** An identifier that stands for a symbol in the
source keeps the source's spelling: `cos_λ`, `argL`, `A1`, `θCoeffs`, `gen_eval_θ`, and
`Jieqi`'s 立春/冬至. Same discipline as the `@ref` rule — the code should read next to the book.

### Comment language

Narrative in English; domain terms keep their own spelling (`Jieqi::冬至`), glossed once in
parentheses where an English-only reader needs a handle — `the Chinese Jieqi (节气)`.
Translating an entity loses it. Outside the lunar subtree, keep new full-Chinese runs to
named or quoted material. The lunar subtree's Doxygen carries a Chinese line after or under
the English, on `@brief` / `@param` / `@return` / `@note` and member docs alike
(`@param year The Lunar year. 阴历年份。`) — match the neighbours when editing there; do not
spread the habit outward, do not strip it inside. Punctuation follows the language of the
run it sits in: full-width inside a Chinese phrase, half-width everywhere else.

### Header-only, and the code reads like the maths

Header-only is a **deliberate design choice** — the no-link, orthogonal, self-contained
feel is intentional. Keep it. That buys a discipline:

- All logic in `src/**/*.hpp` with `#pragma once`; every function `inline`, shared constants
  `inline constexpr`. `.cpp` exists only in `shared_lib/` (C-ABI exports), `test/` and
  `bench/` (entry points). New logic → a header. `inline` on namespace-scope constants is
  not cosmetic: plain `constexpr` has internal linkage, so an external-linkage inline
  entity that odr-uses it is IFNDR (#71).
- **Every header compiles alone**, and CI enforces it per-header with `-fsyntax-only`.
  Self-containment is a property of the *standard library*, not of the code — a header
  that leans on a transitive include passes on one implementation and fails on another
  (#71). **The gate therefore runs on more than one leg; a single green platform proves
  nothing.**
- **No `using namespace` and no namespace-scope `using` declarations in headers** — they
  leak into every including TU. Put `using` inside function bodies (as `sun.hpp` does with
  `using namespace astro::toolbox::literals;`) or fully-qualify. Allowed closed set:
  `using X = Y` type aliases, and using-declarations that import a named template from
  another namespace (see `datetime.hpp`). Test-only headers may carry function-body or
  file-local usings.
- **Nested `lower_case` namespaces** by domain (`astro::earth::nutation`, `lib::`,
  `calendar::`); close with a `} // namespace …` comment.
- **Trailing return types**: `inline auto f(const double jde) -> SphericalCoordinate`.
- **Unicode math identifiers are deliberate** (`λ`, `β`, `Δψ`, `Ω`, `Mp`). Name symbols as
  the papers do; do NOT ASCII-ise them.
- **Strong physical types** over bare doubles (`Angle<RAD>`, `Distance<AU>`,
  `SphericalCoordinate`) with designated initializers (`.λ = …`). Keep units in the type system.
- `const`-correct; modern C++ (`<ranges>`, `<span>`, `std::array`); 2-space indentation.
  Compiler flags: `-Wall -Wextra -Werror -Wpedantic -Wnull-dereference
  -Wunreachable-code`; the optimization level is `CMAKE_BUILD_TYPE`'s (Release = `-O3 -DNDEBUG`).
- **Multi-line call layout**: when an argument is itself a call, or the call grows long,
  put each argument on its own line and the closing `);` on its own line at the call's
  indentation. Trivial short calls stay on one line.
- Doxygen `/** @brief / @param / @return / @note / @ref */` on public functions; inline
  `//` comments explain the physics with order-of-magnitude arguments ("ΔT ≈ 69 s ≈ 0.29°
  of rotation"), not bare claims. `@ref` cites the exact formula/table number of the named
  reference.

### Modern C++ posture

Immutability by default — locals are `const`; value types are immutable, operations return
new values; no setters · composition over inheritance — no inheritance at all: structs are
plain data carriers; "polymorphism" is values + table lookup (`nutation::Model` →
`find_model` → `std::span`), never vtables · free functions over classes · declarative over
imperative — ranges pipelines describe *what*, not *how* · functional core — pure
functions, no shared mutable state, same inputs ⇒ same outputs; higher-order functions
where natural (`gen_eval_θ(jc)` returns the θ-evaluator closure) · compile-time over
runtime — `constexpr` wherever the language allows, but don't force it (`std::remainder`
is C++23 on paper and still rejected by the pinned toolchains, #82); `requires` clauses,
never SFINAE · **names as contracts** — parameter names make misuse visible at the call
site (the `jd_ut1` / `jde_tt` suffixes are the UT1/TT guard, #41) · every `NOLINT` /
`NOLINTNEXTLINE` carries a trailing comment saying why the warning does not apply.

### Small functions, honest errors, no premature abstraction

Functions stay short and single-purpose — compose thin layers that each do one thing
(`greenwich_mean` → `greenwich_apparent` → `local_apparent`). The doc comment is routinely
longer than the function body: the physics and the citation are the hard part. Throw with
context-rich messages (`std::vformat("Year {} is not supported …", …)`), never silently
clamp, swallow, or return a sentinel. Three similar lines beat a premature abstraction;
generalize when a second or third real caller demands it.

Which failure gets which mechanism (#97) — sort by *what happened to the answer*:

| The answer | Mechanism | Examples |
|---|---|---|
| **Cannot be produced correctly** — bad input, outside a declared window, no convergence | `throw` | `rise_set::detail::validate`; `algo2` outside [410, 2500]; `jieqi_jde` outside [1, 32766] (#154) |
| **Was produced, and it is "none"** | `std::optional` | no sunrise on a polar night; no leap month in a common year |
| **Is beside the point — the library's own bookkeeping is broken** | `assert` | internal invariants only |

A declared model window *is* the contract's domain, so leaving it is a bad argument like
any other. `optional` never carries an error — it carries a legitimate "none".
`std::expected` was evaluated for the core and declined (#97); the C-ABI boundary keeps
its own translation (`catch` → `valid = false`, see `celestial.h`).

Comments sit in one of three slots (#127): **contract** (time scale, unit, valid range,
sign convention) · **citation** (`@ref` to the formula or table number) · **numeric
argument** (why this tolerance, this bracket, this step). Textbook exposition is not a
slot. What gets cut is the exposition, never the contract: the twin `@note`s on
`nutation::longitude` and `obliquity` ("fix both or neither") are as load-bearing as the code.

### File header

Every project-authored source file opens with the neighbouring project banner, its copyright
line, and a language-appropriate `SPDX-License-Identifier: MIT`. New files use the current
year; never change an existing file's year — except a wholesale rewrite, which may set a
`<original>-<current>` range (user-directed, e.g. `util/random.hpp` 2024-2026).

## Tests

- GoogleTest `TEST(Suite, Case)` in `src/test/**`, mirroring `src/**`
  (`astro/earth.hpp` → `test/astro/earth_test.cpp`), namespace `…::test`.
- **Tests are auto-discovered:** `src/test/CMakeLists.txt` globs every `*.cpp` under
  `src/test/` into a separate executable.
- Data-driven: inline a column-aligned dataset of reference values (like `earth_test.cpp`);
  `ASSERT_NEAR` with a per-column tolerance; header-comment the columns. Alignment is
  computed **per column over the whole block**, never per-row — regenerating or editing
  rows must re-align the entire block.
- **Provenance is mandatory**: every golden dataset states where its values came from —
  book example number (tests mirror worked examples value-by-value, e.g. Meeus Example
  12.a), or the external source + collection date + generation seed — plus the rationale
  for the tolerance. A dataset must be regenerable and auditable by a reader.
- Dataset **input columns must reproduce the reference source's actual inputs** — including
  any quantization the source applied. Otherwise tolerances silently absorb the mismatch
  and the dataset loses discriminating power.

### Acceptance: run the binaries, and reconcile the count

**Do not accept on ctest's numbers.** ctest reports on what it was told about, and its
registration goes stale: the `GLOB` that discovers tests expands at configure time, so a
`--build --test` run after adding a source file silently omits it — the suite passes with
the new tests never compiled in.

Run the binaries directly, and track two things: did every binary exit 0, and does the
total reconcile against the `TEST` macros:

```sh
total=0; fail=0
for exe in build/test/*; do [ -x "$exe" ] && [ -f "$exe" ] || continue
  out=$("$exe" 2>&1) || { echo "FAIL $exe"; fail=1; }
  n=$(echo "$out" | grep -oE "^\[==========\] [0-9]+ test" | grep -oE "[0-9]+" | head -1)
  total=$((total + ${n:-0})); done
macros=$(grep -rhoE '^\s*TEST(_F)?\(' src/test --include=*.cpp | wc -l)
echo "ran=$total macros=$macros fail=$fail"
[ "$fail" -eq 0 ] && [ "$total" -eq "$macros" ]   # this line is the verdict
```

The two checks catch different failures and neither implies the other: a binary that fails
still prints its test count, so the totals can agree while `fail=1` — and a binary that is
missing or stale (cleared before every build, #155) trips the count while every survivor
exits 0.

## Project Layout

```
src/
  astro/        Astronomical algorithms (VSOP87D, ELP2000-82B, Sun, Moon, ΔT, Julian Day, ...)
  bench/        Benchmarks, built only by `--bench`
  calendar/     Calendar logic: datetime, lunar conversion algorithms, Jieqi
  shared_lib/   C++ shared-library wrapper over core algorithms
  test/         GoogleTest-based tests (auto-discovered by CMake)
  util/         Utility headers (hash, cache, random, YMD, ...)
automation/     Python modules used by project.py and checks.py
toolbox/        Helper scripts for artifacts, releases, build info
bindings/
  javascript/   ESM npm package source, declarations, ABI oracle and consumer tests
  python/       ctypes package, native-wheel CMake composition, ABI oracle and consumer tests
```

## Project-Specific Rules and Gotchas

1. **Single source of truth:** this `AGENTS.md` is the only agent-instruction file with
   content; `CLAUDE.md` just imports it. Don't create new per-tool instruction files —
   extend this one.
2. **C++23 features — "not yet", not "never":** basic C++20 ranges/views are in active
   use. Features the weakest CI toolchain doesn't support yet — **modules**,
   `std::generator`, C++23 ranges additions like `std::views::enumerate` / `pairwise` —
   wait for compiler support (README §11 tracks the wishlist). **Availability is settled by
   compiling a real use of the feature, never by reading a feature-test macro** (#131).
   `./checks.py --features LEG` holds each CI leg to the state in
   `automation/feature_probe.py`; an unlock fails CI and names the waiting sites it can
   see — the ones tagged `TODO` with the feature's name.
3. **Shared library target:** `src/shared_lib/CMakeLists.txt` builds
   `libcelestial_calendar` from `lib*.cpp`. Version is injected via the `BUILD_VERSION`
   environment variable (defaults to `0.0.0`).
4. **CI produces cross-platform artifacts:** the native release covers macOS, Windows, and
   two Linux architectures (x86_64 and arm64); Linux builds in Docker on native runners.
   Do not change compiler or Docker base images without checking matrix impact. The optional
   wasm target (#163) and Python wheels (#211) have independent `wasm.yml` and
   `python-wheel.yml` legs. The former uploads `celestial-wasm` (README §6 records its exact
   members); the latter uploads four exact wheels and sidecars;
   the release downloader pulls all three build legs' artifacts for the tagged commit. Cutting a
   release is the protected-tag, explicit-run ritual in `docs/RELEASING.md`; it freezes one candidate,
   publishes GitHub Release before npm/PyPI, and verifies registry bytes without rebuilding.
5. **Sensitive files:** Do not read or surface `.env`, `credentials.json`, or any file
   containing tokens/keys.
6. **`build/` is gitignored.** Generated artifacts and `compile_commands.json` live there;
   do not commit them.
7. **Use `project.py` for routine work.** It handles environment checks, version
   propagation, and test filtering consistently across platforms.
8. **Assertions:** the default build is `Release`, so `assert` is dead code in production.
   Test targets strip `NDEBUG` (#89), so asserts ARE live inside test binaries —
   `build_integrity_test.cpp` guards this. An input guard is never an `assert`; the
   mechanism table under "honest errors" says which failure gets what.
9. **CI toolchains are pinned — every one that can emit a diagnostic.** A self-updating
   tool turns "the code changed" and "the tool changed" into the same red X; a stale pin
   instead fails loudly with "no such version", which says what to do. Pinned: clang on
   the Linux legs, choco LLVM on Windows (a different source, not expected to agree below
   the major), the Xcode **major**, ruff, and emsdk (`wasm.yml`, cache-keyed).
   **clang-tidy is pinned by the runner image** since #73: the linters leg picks its
   runner for the clang-tidy that image ships, calls the binary by its major, and the
   vendored `run-clang-tidy.py` carries the matching `llvmorg-` tag — a wrong pairing can
   exit 0 having analysed with a smaller ruler, so read the version the binary reports,
   not the exit code. Exact versions live in the workflows, not here. Chocolatey's `make`
   is deliberately unpinned: it drives the build rather than diagnosing it, so a new
   version cannot turn `-Werror` red. Bump deliberately, never incidentally (#72, #73).

## Design ledger — settled decisions; check the trigger before re-proposing

Reviews kept re-litigating the same deliberate bets, so each entry records the decision
and the trigger that reopens it. "—" marks a structural line, not a provisional bet.
Premises and full arguments live in issue #127 and in notes kept outside this repo. If you are about to
propose one of these and the trigger has fired, say so and reopen it — that is what the
trigger is for.

| Decision (hold, unless the trigger fired) | Reopen when |
|---|---|
| **Bare Meeus (12.3) is valid only on the 0h UT grid; (12.4) is the complete any-time form, not an "extension" of it** — the (12.3) polynomial drops the daily 360.9856° term, which only cancels mod 360 at 0h; off the grid it drifts up to 180° (USNO-measured 158.1° at an arbitrary afternoon moment). The 12.3/12.4 numbering itself is a known erratum — settled by three independent sources | Any code path evaluating sidereal time off the 0h grid from the (12.3) polynomial |
| **No strong types for time scales** (`JdUt1` / `JdeTt`); the `jd_ut1` / `jde_tt` suffix convention carries it | Moon rise/set lands (#62), or a second #41-class mix-up reaches a test |
| **No caching or memoisation in the core layer**; `util/cache.hpp` wraps at the calendar layer | — (structural) |
| **Header-only is the identity, and its compile cost is accepted** | Someone produces a compile-time measurement |
| **The cache never evicts** — the mechanism has no eviction by design; each caller's key space is bounded by its declared window (`jieqi` [1, 32766], `algo2` by year range) | A caller with an unbounded key space appears |
| **`algo1` / `algo3` stay near-duplicates — and tests whose columns mean different things don't merge either** (#167) | A third algorithm of the same shape; for tests, a group whose rows differ only in data |
| **`moon_phase`'s year-boundary hole is `wontfix`** | A caller needs total coverage of the boundary |
| **No one-off namespace rename**; new code lands in the intended shape, old names stay | — (incidentally or not at all) |
| **No policy/context object for model selection**; the model stays a function parameter (`nutation::Model`) | A third real *ephemeris/nutation/EOP* backend appears (lunar `Algo` and frozen ΔT exhibits don't count) |
| **Coordinate frames stay untagged**; names disambiguate (D2) | Another mix-up survives naming and reaches a result |
| **Transcription runs on a single track**; equivalence is proved once during a migration, not maintained as a parallel implementation | A transcription lands that cannot be diffed against its predecessor in one run |
| **Error budgets are not part of the API contract**; a fitted residual is never dressed up as a 1σ | A caller needs a declared accuracy to decide something |
| **The tool-existence checks in `automation/` are not merged into one**; the `toolbox/` `sys.path.append` copies cannot be merged either (#166) | A third call site needs the same failure policy as an existing two |
| **The `using X = Y` aliases in the lunar headers stay** — load-bearing for self-containment (`common.hpp` / `converter.hpp` / `algo2.hpp`) | The style rule narrows what `using X = Y` may do |
| **External ephemerides are oracles, never dependencies** — ytliu0, Horizons, USNO appear only under `src/test/` and in `@ref` comments | — (a line, not a bet) |
| **`normalize_deg` / `normalize_rad` keep calling `std::remainder`, and stay `constexpr`** (#82) | Any target standard library marks `std::remainder` constexpr — `automation/feature_probe.py` watches it per leg |

**Decided and already done** (kept because the reasoning gets re-proposed): longitude sign
is west-positive in `sidereal`, east-positive elsewhere, disambiguated by name (D2) ·
`nutation::longitude` and `obliquity` stay verbatim twins with `@note`s pointing at each
other (#49).

**Decided and not yet built** (the decision is real, the artifact is not — do not cite
it as existing): the `cpp26-lab` experiment branch.

**Two conventions that look like drift and are not.** Time scales are spelled in the
parameter name inside C++ (`jd_ut1`, `jde_tt`) and in the *function* name at the C
boundary (`jde_to_ut1` takes a bare `jde`), because a C entry point only ever speaks one
scale; reopen if a C entry point ever needs to accept two. And every C export except the
`last_error` reader records failures through `wrap_export`; the boundary lives in
`celestial.h` and is mechanically reconciled with both bindings.

## Phase and PR workflow

- One issue per phase; branch `phaseN-<topic>` from `main`; PR body in the established
  four blocks — 内容 / 测试 / 验证 / 范围说明 (see #52, #54 for the shape).
- A closing PR body contains the sole unbackticked `Closes #N`. Before merge, query
  `closingIssuesReferences` through GraphQL and assert the exact expected issue list.
- Repo merge settings default the squash title and body to `PR_TITLE` and `PR_BODY`. A
  controlled GitHub merge API call may supply a different squash body; whichever body is
  supplied must contain only the same unbackticked `Closes #N` and pass protection scan as
  permanent main history. After merging, read back `git log -1 --format='%b' origin/main`
  to verify what actually landed.
- On the author's workstations `commit.gpgsign=true` is set machine-globally with a
  hardware key: there, a bare local `git commit` from an agent pops pinentry in the
  user's terminal and hangs the session — commit server-signed through the GitHub API
  instead. A fresh clone elsewhere (CI, a contributor machine) has no such config and
  may commit normally; if you are an agent unsure which world you are in, ask before
  the first commit.
- Every PR is preceded by local review rounds (adversarial correctness + style/design)
  before it is opened; CI and PR bots are later gates, not substitutes.
- Post-merge queue: delete the remote branch → update the phase tracking doc → record the
  phase's validation chain, decisions, environment notes and next-phase prep (that record is
  kept outside this repo) → open the next phase.

## AI do / don't

- DON'T round / drop astronomical constants or loosen tolerances to pass CI.
- DON'T ASCII-ise unicode identifiers, move logic out of headers, or add namespace-scope
  `using` to a header.
- DON'T add a dependency or build step outside the `project.py` / `checks.py` flow — the
  wasm/npm and Python wheel builds are the sanctioned exceptions (#163, #182, #211): their recipes live in
  `toolbox/build_wasm.py` and `toolbox/build_npm.py`, shared by the manual path and the
  `wasm.yml` CI leg, and in `bindings/python` / `python-wheel.yml`; the release flow consumes
  the CI-built artifacts and publishes those exact bytes, it does not rebuild them.
- DON'T quote paths, hostnames, or tooling from outside this repository in anything public
  (comments, reviews, commit messages, PR text) — a path appearing in a file here does not
  make it publishable, and that includes files you were handed rather than wrote.
- Match the neighbouring header's texture; internal consistency > external "best practice".

## Common Commands Reference

| Task | Command |
|------|---------|
| Full setup + build + test | `./project.py --all` |
| Configure / build / test | `./project.py --cmake` / `--build` / `--test` |
| Run tests, verbose / filtered | `./project.py --test -v 1 -k <keyword>` |
| Run benchmarks | `./project.py --bench` |
| Build the WASM module (needs emsdk) | `python3 toolbox/build_wasm.py` |
| Build the npm tarball (after WASM) | `python3 toolbox/build_npm.py` |
| Build one Python wheel | `python -m cibuildwheel --only <identifier> bindings/python --output-dir wheelhouse` |
| Clean | `./project.py --clean` |
| Python lint/format · C++ lint | `./checks.py --ruff` · `./checks.py --clang-tidy` |
| Show version | `./project.py --version` |
