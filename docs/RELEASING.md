<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions between
    Gregorian and Chinese Lunar calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# Releasing CelestialCalendar

The release workflow freezes one candidate for GitHub Release, PyPI, and npm. It accepts three explicit producer
run IDs and rejects a tag, commit, run, artifact, or registry version that does not match exactly. npm-primary
(`@0xf3cd/celestial`) and npm-alias (`celestial-calendar`) are separate from the PyPI homonym.

## One-Time Setup

Complete these settings before creating the release tag:

1. Add an active tag ruleset for `v*`. Test `github.ref_protected` with a disposable matching tag, then remove it.
2. Create `pypi` and `npm` environments. The maintainer is the required reviewer and may self-approve after reading
   the candidate manifest; disable self-review prevention and admin bypass. Restrict deployment tags to `v*.*.*`.
3. Register the PyPI pending publisher with owner `0xf3cd`, repository `celestial-calendar`, workflow
   `release.yml`, and environment `pypi`.
4. After npm v0.6.0 exists, register its Trusted Publisher for the same repository and workflow with environment
   `npm`.

The alias requires its own Trusted Publisher after the separately authorized bootstrap below. Package existence
does not prove publisher configuration or live OIDC readiness.

Environment approval is a deliberate manifest check, not independent review. Confirm the tag, commit, producer
run IDs, filenames, sizes, and hashes shown in the workflow summary before approving.

## Release

1. Confirm `project.py`, `docs/RELEASE_NOTES.md`, and the first `docs/CHANGELOG.md` entry all name the same version.
2. Create and push the protected `vMAJOR.MINOR.PATCH` tag from a commit already contained in `main`.
3. Dispatch `build_and_test.yml`, `wasm.yml`, and `python-wheel.yml` on that tag. Wait for all three to succeed and
   record their run IDs. Each must be a `workflow_dispatch` run at the tagged commit.
4. Finish the remaining steps within the producer artifacts' 30-day retention window.

Dispatch each producer once:

```sh
tag=vMAJOR.MINOR.PATCH
commit=$(git rev-parse "$tag^{commit}")
gh workflow run build_and_test.yml --ref "$tag"
gh workflow run wasm.yml --ref "$tag"
gh workflow run python-wheel.yml --ref "$tag"
```

After they finish, list the dispatched runs at that exact commit. Record the `databaseId` for one successful run of
each workflow, then inspect each selected ID before continuing:

```sh
tag=vMAJOR.MINOR.PATCH
commit=$(git rev-parse "$tag^{commit}")
for workflow in build_and_test.yml wasm.yml python-wheel.yml; do
  gh run list --workflow "$workflow" --event workflow_dispatch --commit "$commit" \
    --json databaseId,workflowName,event,headSha,status,conclusion,url
done
gh run view RUN_ID --json databaseId,workflowName,event,headSha,status,conclusion,url
```

The selected runs must report `workflow_dispatch`, the value of `$commit`, `completed`, and `success`. A displayed
run number is not a run ID; use `databaseId` in the release inputs.

### Historical npm v0.6.0 Bootstrap

npm cannot register a Trusted Publisher for a package that does not exist. Bootstrap v0.6.0 once, before running
`release.yml`:

1. Download `celestial-wasm` from the selected WASM run and use the single tarball named by `npm-pack.json`.
2. Verify that tarball with `npm-pack.sha256`.
3. Authenticate npm with a short-lived token without printing or storing it in the repository, then publish the
   exact tarball with `--access public --ignore-scripts`.
4. Revoke the token immediately and register the npm Trusted Publisher.
5. Use this same WASM run ID when dispatching `release.yml`.

From an empty `npm-bootstrap` directory, the byte-selection steps are:

```sh
gh run download <wasm-run-id> --name celestial-wasm --dir npm-bootstrap
tarball=$(python3 - <<'PY'
import json
from pathlib import Path

pack = json.loads(Path("npm-bootstrap/npm-pack.json").read_text(encoding="utf-8"))
if len(pack) != 1 or pack[0].get("name") != "@0xf3cd/celestial" or pack[0].get("version") != "0.6.0":
  raise SystemExit("unexpected npm package identity")
filename = pack[0].get("filename", "")
if Path(filename).name != filename or not (Path("npm-bootstrap") / filename).is_file():
  raise SystemExit("invalid npm tarball filename")
print(filename)
PY
)
(cd npm-bootstrap && sha256sum --check npm-pack.sha256)
npm publish "npm-bootstrap/$tarball" --access public --ignore-scripts
```

Authenticate the npm client with the short-lived token before the last command; never add the token to the command,
shell history, output, or repository.

Do not repack the module or select a tarball with a glob. npm v0.6.0 has no OIDC-generated provenance because of
this bootstrap. Its later byte-verified no-op proves package identity, not the npm OIDC publication path; the first
live OIDC publication is v0.6.1.

### Alias v0.7.0 Bootstrap

A genuinely nonexistent npm package cannot register a Trusted Publisher. Preparation records `bootstrap_required`
without failing; the npm job stops at that package, preserving any preceding npm-primary success.
For the alias's first version, after npm-primary 0.7.0 is available:

1. Obtain separate authorization for account setup and publication. Download the original release run's
   `celestial-release-candidate`, not a rebuilt producer output.
2. Select the alias tarball from `candidate/evidence/npm-alias-pack.json`, requiring exactly one
   `celestial-calendar@0.7.0` record and a plain filename. Verify its SHA-256 against
   `candidate/evidence/npm-alias-pack.sha256` in `candidate/npm`. Keep both frozen tarballs unchanged.
3. Publish that exact alias tarball with `--access public --ignore-scripts` using a separately authorized
   short-lived credential. Do not print or store that credential in the repository. Revoke it immediately.
4. Register the alias Trusted Publisher for owner `0xf3cd`, repository `celestial-calendar`, workflow `release.yml`,
   environment `npm`. Target only the npm publication job for retry in the original release run.

Fresh classification on that retry verifies primary and alias bytes and skips exact versions. This no-op proves
bytes, not live alias OIDC publication. Account readiness and the first live alias OIDC publication need separate
evidence; neither is implied by merging the alias code.

### Frozen Candidate

Dispatch the release workflow on the tag with the three recorded IDs:

```sh
gh workflow run release.yml --ref vMAJOR.MINOR.PATCH \
  -f native_run_id=<native-run-id> \
  -f wasm_run_id=<wasm-run-id> \
  -f python_run_id=<python-run-id>
```

Preparation validates the protected tag, main ancestry, producer runs, artifact API digests, archive contents,
and documentation. It then stages one candidate and classifies both npm identities:

- an absent version in an existing, correctly identified package requires OIDC publication;
- an absent version whose package also returns 404 records `bootstrap_required`;
- a byte-identical version is a verified no-op;
- any metadata, integrity, or byte mismatch stops before GitHub Release creation.

GitHub Release publishes first. Approve the `pypi` and `npm` jobs only after checking the candidate summary. The
final unprivileged job requires exactly four PyPI wheels and two npm tarballs, compares registry hashes and bytes,
and clean-installs PyPI plus npm-primary, npm-alias, and the npm pair from their respective registries.
The candidate has 30 files: 14 GitHub assets, four PyPI wheels, two npm tarballs and ten evidence files
(29 files recorded in the manifest, excluding the manifest itself). The WASM archive has ten members:
the raw module/glue, LICENSE/notices, and both tarball/metadata/SHA-256 triples.

The npm job checks out only the protected release commit and installs the hash-locked Requests closure to run
the versioned classifier. Immediately before each publication it rechecks that package against the same frozen
candidate, primary first and alias second. Exact bytes are a verified no-op; conflicts, invalid JSON, HTTP 429,
and transport failures stop rather than authorizing publication. Verification alone polls indexing and transient
transport states to a fixed ceiling and reports which of npm-primary, npm-alias, or PyPI is still pending.

After the workflow succeeds, confirm the immutable GitHub Release and its asset inventory, then install
`celestial-calendar==VERSION` from PyPI and both npm names at `VERSION` from unrelated temporary directories.

## Recovery

- If immutable GitHub Release creation fails, inspect the release first. Delete it only if it is still a draft,
  then rerun failed jobs against the same workflow artifact.
- An unambiguous PyPI failure before registry acceptance may use `gh run rerun RUN_ID --failed` after reviewing
  the evidence.
- For npm publication failure, inspect the evidence and retry only the npm job using
  `gh run rerun RUN_ID --job NPM_JOB_ID`. Fresh classification retains successful exact package bytes and continues
  with the remaining package. This also handles a command that failed after npm accepted its exact bytes.
- The unprivileged `verify_registries` job is idempotent. A transient verification failure after publication may use
  `gh run rerun RUN_ID --failed`; the publication jobs have already succeeded and are not rerun.
- If a PyPI publish command fails ambiguously but registry queries prove the exact candidate is present, leave that
  publication job red. Do not rerun it or use `skip-existing`; record the recovery and complete consumer validation
  manually.
- After any irreversible job succeeds, never use "Re-run all jobs". The original run is the identity of the frozen
  candidate.

For the terminal PyPI ambiguous-success case, verify and consume the same candidate manually from a clean checkout of
the release tag. `RUN_ID` is the release workflow run, not a producer run:

```sh
(
  set -euo pipefail
  tag=vMAJOR.MINOR.PATCH
  version=${tag#v}
  commit=$(git rev-parse "$tag^{commit}")
  test ! -e candidate
  gh run download RUN_ID --name celestial-release-candidate --dir candidate

  python3 -m venv registry-verify
  registry-verify/bin/python -m pip install --require-hashes --only-binary=:all: -r Requirements-producer.txt
  registry-verify/bin/python toolbox/registry_verifier.py verify \
    --candidate candidate --version "$version" --commit "$commit"

  python3 -m venv registry-python
  registry-python/bin/python -m pip --isolated install \
    --index-url https://pypi.org/simple --only-binary=:all: --no-cache-dir --no-deps \
    "celestial-calendar==$version"
  root=$(pwd)
  work=$(mktemp -d)
  (cd "$work" && "$root/registry-python/bin/python" "$root/bindings/python/test/run_all.py")
  node bindings/javascript/test/registry/registry_consumer_test.mjs "$version"
)
```

Rehearse environment self-approval and same-run failed-job artifact recovery after workflow changes and before the
final release tag. The instrument is `.github/workflows/release-rehearsal.yml`: push a disposable `vX.Y.Z-rehearsal`
tag — never a plain `vX.Y.Z`, which is release identity — (the tag rulesets cover it: create via the Admin bypass,
remove afterwards via the ruleset recovery path recorded in #215), dispatch the workflow on that tag, approve both
environment gates against the staged manifest, then rerun the deliberately failed job with
`gh run rerun RUN_ID --failed` and confirm it recovers the same-run artifact. The v0.6.0 npm no-op cannot rehearse a
live OIDC publish.
