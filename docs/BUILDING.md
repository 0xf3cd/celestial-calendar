# Building CelestialCalendar

<a id="requirements"></a>
## Requirements

These are source-development instructions, not prerequisites for using installed packages. Installed packages
need only their own [Python](../README.md#python), [JavaScript](../README.md#javascript) or
[native runtime](../README.md#native-runtime) requirements. Building the core library from source needs:

- A C++23 compiler. CI uses clang++ 22 on Linux and Windows, Xcode 26's Apple clang on macOS, and g++ 14 on
  Linux; older compilers may work but are not checked.
- CMake ≥ 3.22 and make.
- Python 3 for build and test automation; its dependencies are in `Requirements.txt`.

Debian, Ubuntu and other distributions protect the system Python under PEP 668, so use a virtual environment as
shown in [Building from Source](#build). `--setup` installs nothing once the dependencies are present.
`Requirements.txt` does not include the [static-check tools](#lint); the notebooks and data-collection scripts in
`statistics/` use `Requirements-statistics.txt` instead.

<a id="build"></a>
## Building from Source

This section is for users and contributors who need to build the core library. Run every command from the root
of a full source checkout; outputs go to `build/`. [`AGENTS.md`](../AGENTS.md) is the source of truth for build,
test, code-style and contribution conventions, including direct test-binary execution and reconciliation against
the test macros rather than accepting ctest counts alone. Python wheels have their own recipes; see
[Python Wheels](#python-wheels).

### macOS / Linux

With a C++23 compiler, CMake, make and Python 3 installed, create and activate a virtual environment, then run the
single entry point:

```sh
python3 -m venv .venv
. .venv/bin/activate
export CXX=clang++
python project.py --all
```

`--all` runs setup, configure, build and test in that order. You can also run the steps separately or pick
individual tasks:

```sh
python project.py --setup
python project.py --cmake --build
python project.py --test
CELESTIAL_TEST_SEED=123 python project.py --test
python project.py --bench
python project.py --help
```

Randomized tests default to seed 42; set `CELESTIAL_TEST_SEED` to replay a failure or explore other seeds.
Benchmarks are optional and not part of `--all`. `python project.py --clean` removes build outputs.

### Windows

Besides Python 3 and CMake, Windows needs LLVM and make, because the build uses CMake's `Unix Makefiles`
generator. With Chocolatey installed, `choco install -y make llvm` supplies those two tools. From the source root
in PowerShell, set both the LLVM C and C++ compilers so the toolchains are not mixed:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
$env:CXX = "clang++"
$env:CC = "clang"
python project.py --all
```

The step-by-step commands are the same as above; set the random seed with `$env:CELESTIAL_TEST_SEED = "123"`.

<a id="python-wheels"></a>
### Python Wheels

There are two wheel recipes, for different purposes:

- To use a source build in your own Python environment, follow the
  [local wheel recipe](../bindings/python/README.md#build-a-local-wheel) in the Python package guide. It needs the
  full checkout, not a copy of `bindings/python/` alone, and builds one wheel for the host. That wheel is not a
  repaired, portable release wheel and does not extend the supported platforms.
- Release wheels come only from [`python-wheel.yml`](../.github/workflows/python-wheel.yml). It runs `cibuildwheel`
  with the `CIBW_*` settings and pinned manylinux images declared in that workflow, inspects each repaired wheel,
  and runs the installed-wheel tests on it. The release publishes those tested wheels and builds no others. The
  `cibuildwheel` settings live only in the workflow, not in `bindings/python/pyproject.toml`, so a local
  `cibuildwheel` run without them does not reproduce the release build.

<a id="api-docs"></a>
### API Documentation

Install Doxygen 1.15.0, run `python project.py --docs` from the source root, and open
`build/api-docs/html/index.html`. This task does not compile C++, does not need Graphviz, and is not part of
`--all`. [Core Tests](https://github.com/0xf3cd/celestial-calendar/actions/workflows/core_tests.yml) uploads the
same HTML as the `celestial-api-html` attachment; [`API.md`](API.md) describes it. The interface is English; the
lunar subtree keeps its existing English/Chinese comments. The HTML is a CI attachment, not a hosted site or
release artifact.

<a id="docs-site"></a>
### Development documentation site

With the pinned Python dependencies in `Requirements.txt` and Doxygen 1.15.0 installed, run
`python project.py --site` from a clean, committed source root. This opt-in task combines the existing
Markdown guides and a separate API generation in `build/docs-site/`; it leaves `build/api-docs/html/`
and `--all` unchanged. Core Tests uploads it as `celestial-docs-site`.

Every page identifies the exact source SHA, package version and unreleased development state. The site
describes that checkout, which may include APIs unavailable in the published packages. Its manifest records
source inputs, generator versions and every output member's digest. The build checks all HTML links,
fragments and assets at both the root and project subpath. It rejects dirty sources and linked output files.

Preview locally with `python -m http.server --bind 127.0.0.1 --directory build/docs-site 8000`, then open
`http://127.0.0.1:8000/`. No hosted URL is advertised until deployment and external checks are complete.
The separately guarded manual publishing path is described in [Maintenance](MAINTAINING.md#docs-publishing).

<a id="wasm"></a>
## WebAssembly and npm Packages

Using the published npm packages does not need the tools in this section. It covers source builds and artifact
maintenance, and needs Node ≥ 22, the Python build dependencies and an emsdk checkout. Point to emsdk with the
`EMSDK` environment variable or `build_wasm.py --emsdk`. From the root of a full source checkout:

```sh
npm ci --ignore-scripts --prefix bindings/javascript
python3 toolbox/build_wasm.py
node toolbox/wasm_check.mjs
python3 toolbox/build_npm.py
```

Ordinary `npm pack` in the `bindings/javascript` development directory refuses to pack; use the builder above.

The generated module is `build/wasm/celestial-jieqi.mjs` with `build/wasm/celestial-jieqi.wasm`. It carries every
function exported by `src/shared_lib/celestial.h`, including `celestial_chart_snapshot`;
`bindings/javascript/test/abi/manifest.json` is the checked list. The public JavaScript API of
`@0xf3cd/celestial` is the set of namespaces declared in `bindings/javascript/types/index.d.ts`, where
`chart.snapshot` wraps `celestial_chart_snapshot`. The public API does not expose the independent position,
equatorial, house, event or capability queries. What stays private is the WASM glue, which keeps heap pointers,
the count/fill protocol, sret layout and both error readers inside the package, and the ABI checks
(`toolbox/wasm_check.mjs` and `bindings/javascript/test/abi/`), which are not shipped.

`build_npm.py` uses the generated module and the version from `project.py` to pack
an npm primary package of exactly 12 files. `@0xf3cd/celestial/date` imports only pure validation functions, not
the root entry or the Emscripten glue. The same builder also packs `celestial-calendar`,
an npm alias package of exactly 7 files that forwards the ESM and type declarations of the root entry and `/date`.
The alias's only runtime dependency is `@0xf3cd/celestial` at exactly the same version; it carries no copy of the
WASM. The two names share export references and initialization state only when both resolve to the same
installation of the primary package; this is not a singleton guarantee across arbitrary mixed-version dependency
graphs.

The builder packs the primary and alias packages once each. `build/npm/npm-pack.json` and
`build/npm/npm-alias-pack.json` select the respective tarballs; do not select them by glob or rebuild them for each
consumer.

The alias's MIT `LICENSE` covers its own material;
retained-component notices remain in the primary dependency.

The separate `wasm.yml` CI workflow builds the module and both packages. The `celestial-wasm` artifact contains
exactly 10 top-level files: `celestial-jieqi.mjs`, `celestial-jieqi.wasm`, `LICENSE`, `THIRD_PARTY_NOTICES.txt`,
the npm primary package tarball exactly as packed, `npm-pack.json`, `npm-pack.sha256`, and the alias package
tarball, `npm-alias-pack.json`, `npm-alias-pack.sha256`. Each metadata file names its own tarball. The release
workflow publishes the primary package first and the alias second, without rebuilding or repacking. Historical
0.6.x archives keep the original single-package format.

The same workflow checks every signature and layout in `bindings/javascript/test/abi/manifest.json` against
`celestial.h`, and replays the golden data in `toolbox/bindings_golden.json`, which
`toolbox/make_bindings_golden.py` generates from the native library. It installs the same pair of packages into
applications outside the source directory on the minimum supported and the current Node version, compiles the
installed type declarations for both package names and `/date`, and runs Astro / Vite production-build smoke tests
in Chrome and Firefox. Windows/macOS current-Node consumers download the original pair from the same CI run and
check installation, API calls, alias identity and the separate date-only entry, without another build or pack.

To run the package tests locally after the four build commands above, select the two tarballs through their
metadata files and run, from the source root:

```sh
primary="build/npm/$(node -p 'require("./build/npm/npm-pack.json")[0].filename')"
alias_tgz="build/npm/$(node -p 'require("./build/npm/npm-alias-pack.json")[0].filename')"
node bindings/javascript/test/node/public_api_test.mjs build/npm/package/index.mjs
node bindings/javascript/test/node/fake_native_test.mjs build/npm/package/index.mjs
node bindings/javascript/test/node/date_test.mjs build/npm/package/date.mjs --exhaustive
node bindings/javascript/test/node/tarball_consumer_test.mjs "$primary" "$alias_tgz"
npm run test:types --prefix bindings/javascript
node bindings/javascript/test/browser/browser_test.mjs "$primary" "$alias_tgz"
```

`tarball_consumer_test.mjs` installs into temporary directories. The browser test builds its application under
`bindings/javascript/build/` and downloads the Chrome and Firefox builds pinned by `puppeteer-core` there. CI
repeats the four Node-only tests on the minimum supported Node version, omitting `--exhaustive` from
`date_test.mjs`.

<a id="lint"></a>
## Static Checks

C++ uses `clang-tidy` and Python uses Ruff, both run through `checks.py`; neither is in `Requirements.txt`.
Install them separately in the virtual environment used for the build:

```sh
python -m pip install ruff
python -m pip install clang-tidy==22.1.8
```

CI uses the clang-tidy 22.1.2 bundled with the runner, a patch release that pip does not provide; locally,
22.1.x or the distribution's `clang-tidy-22` works. The configuration is in `.clang-tidy` and enables
`WarningsAsErrors: '*'`. If the major version on `PATH` differs, set `CLANG_TIDY` to the executable you need.
It must match the repository's `run-clang-tidy.py`; otherwise the checks differ from CI's.

Run from the source root; these commands also work on Windows:

```sh
python checks.py --ruff
python checks.py --clang-tidy
python checks.py --signature-layout
python checks.py --provenance
```

<a id="ci-jobs"></a>
## CI Jobs and Local Commands

See the [CI jobs and local-command map](MAINTAINING.md#ci-jobs) in the maintenance guide.

<a id="artifacts"></a>
## CI Artifact Downloads

Contributor downloads and authenticated automation are in [CI artifact downloads](MAINTAINING.md#artifacts).
Ordinary users can use the [public release downloads](#releases), which need no token.

<a id="releases"></a>
## Release Downloads

Users open [Releases](https://github.com/0xf3cd/celestial-calendar/releases) directly, choose a version, and
download the native / WASM archives, Python wheels with SHA-256 sidecars, or source archives. Web downloads need
no token. A release may differ from the source version described here, so use the documentation that ships with
the selected release; building this checkout does not publish a version.

For CI artifacts, see [CI Artifact Downloads](#artifacts). Maintainers publishing a new release must follow
[`docs/RELEASING.md`](RELEASING.md), not the consumer install instructions; a successful download or build does
not complete a release.

<a id="todo"></a>
## Toolchain Feature Tracking

See [toolchain feature tracking](MAINTAINING.md#todo) for pending features and the compiler probes.
