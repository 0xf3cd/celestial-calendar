<!-- Copyright (C) 2026 Ningqi Wang (0xf3cd). SPDX-License-Identifier: MIT -->

# Consumer evaluation protocol

Compare the same task, model, installed artifact and public documentation in fresh
with-skill and without-skill sessions. Only skill availability changes. Save each
prompt, skill digest, tool trace, generated program, direct execution result and
human disposition. Report ties and regressions as well as improvements.
A passing local structure check does not establish that generated programs work or that the skill improves them.

## Prepare the inputs

Use exact Python wheel and npm tarball files, not a latest-version selection or
filename glob. Record each digest, version and source/producer provenance. Probe
the installed public interface; a version label alone cannot establish availability.
Keep old-interface controls separate from current-source positive cases.
The evaluator's shell sandbox can deny reads under the user's home directory, so
base a Python environment on an interpreter installed elsewhere and check that it
starts under an equivalent read restriction.

Create consumer working directories outside the contributor checkout. Supply the
same frozen public guides and runnable examples to both arms. Each consumer
directory gets a `docs/index.json` that maps each public URL to a local path and
records its source commit and SHA-256, and a `runtime.json` that records the
provided interpreter/package environment. Do not rewrite the skill to point to
those copies. Missing mapped material invalidates the
case; do not fetch moving development documentation during the run.

Keep graders outside both the consumer cwd and installed skill directory. Inspect
actual user-level instructions, auto-memory, visible skills and tool traces in
both arms. Private contributor context, a grader read, or insufficient evidence
to exclude them makes the comparison invalid or unknown. A fresh cwd alone does
not prove isolation. Stop rather than moving credentials to another profile.

## Run and inspect

Start with one paired case to check the native runner's actual child cwd, installed
environment access and complete trace capture. Use the same tool permissions in
both arms. Permit local reads and program execution, not package installation,
network access, nested agents or runtime replacement. Keep reports local; for
Claude Code's native evaluator explicitly use `--no-publish` and verify that the
requested debug file contains the verbose trace.

Pin both agent and judge models and begin with one run per arm. Record actual
concurrency, timing and available token metrics; missing metrics stay unknown.
Distinguish below-threshold scoring from a failed or partial run using its result
records, not its process exit code alone. Set explicit cost and concurrency bounds.

Reconstruct the first program execution from complete write/edit/shell inputs and
the corresponding tool results. Save those bytes, arguments, environment, exit
and output separately from later self-repair and parent replay. If reconstruction
is impossible, mark first execution unknown. Never grade a hand-repaired program
as the model's original success.

Deterministic replay/result checks decide execution correctness. The case's LLM
grader adds a separate trace judgment; its score alone is not an execution oracle.
Use a second input to reject hardcoded answers. For a UTC/Jieqi case, require
fractional-second output and measure the wrong-scale controls with the exact
evaluated artifact. Use held-out cutoffs where the discrepancy exceeds the
comparison allowance; a gap measured with an older artifact does not establish
the current check's discriminating power. An artifact-derived expectation
establishes consistency with that artifact, not independent astronomical
accuracy. Existing numerical goldens and tolerances are unchanged.

## Observer and query comparisons

The observer-coordinate, daily rise/set, observing-window and selected-position/
station cases exercise documentation discovery and multi-step use of the newer
interfaces. Each declares two runs per arm. Freeze their prompts before revising
the skill, and retain those tasks when evaluating a revision instead of tuning
every task to the revised wording. Rerun existing positive workflows to check
whether a change helps one task at the expense of another.

The `unsupported-js-events` case is an old-interface control: use a frozen
package verified to lack the interval-station interface, separately from the
current-source positive station case. Run `missing-interface` against the
older Python and JavaScript packages independently. Record each case's artifact
and guide versions; never infer an unavailable operation from a package version
alone or score a current package against an old negative expectation.

These case definitions are not execution results. Report first-output
correctness, subsequent repair, ties and regressions from the actual traces.

## 2026-11-20 candidate comparison

Claude Code 2.1.325 ran Claude Sonnet 5.5 as both agent and judge, with concurrency
two and two runs per arm. Seven positive cases and three old-interface controls
produced 40 sessions after a separate one-pair preflight. Both arms received the
same frozen public guides from `28e44a01be4ddc063571a75dcfc0d82c6c8e228c`.
SKILL.md remained unchanged, with SHA-256
`245cb92fc6f87c9928901207d9d05aaf2ff608bc08a1938cd5251efef8767db1`.

The positive artifacts were the exact 1.2.0 candidate wheel from producer run
`44471202192` (SHA-256
`7c44ed8f98e039ac56c7cb980c1bfe150005309bcd975482694b6c19241398e4`)
and npm tarball from run `44471200855` (SHA-256
`f14211b8d7077b0c75adc54de5b74d3eb077cb0f90b97abff48b23b495c45923`).
The controls used the frozen registry 1.0.0 packages, whose exact source commits
were not established.

### Context validity

The original formal audit failed and remains recorded as failed. Inspection of
the saved traces identified five false positives: two sessions printed an
`AGENTS.md` link from supplied public guides, one searched documentation for the
literal `npm install`, and two queried installed metadata with `pip list` or
`pip --version`. None read contributor instructions or installed a package.
The other context checks passed for these sessions.

One baseline `unsupported-js-events` session did encounter a sandbox-denied
`cd /tmp`. That arm and its pair are excluded. Post-hoc context inspection
therefore supports all fourteen positive pairs and five of the six
old-interface control pairs. It does not turn the original audit into a pass.

### First output and repair

Direct tool results and artifact-consistency checks give the following
first-program results. Each entry is the number of passing runs out of two.

| Positive case | With skill | Without skill |
|---|---:|---:|
| Gregorian/lunar conversion | 2 | 2 |
| UTC-input next Jieqi | 2 | 2 |
| Chart snapshot | 0 | 2 |
| Observer coordinates | 2 | 2 |
| Daily rise/set | 2 | 1 |
| Observing windows | 2 | 2 |
| Selected positions and stations | 2 | 2 |

Eleven positive pairs tie, one favours the skill, and two favour the baseline.
One with-skill chart program failed on the Moon's enum name; the other printed
an empty node-distance object. Both agents subsequently repaired those programs.
One baseline daily program incorrectly assumed the lunar result had `h0_deg`;
that agent also repaired its program.

Unchanged first-program bytes were replayed on held-out inputs: three additional
dates each for lunar conversion and UTC/Jieqi, and one additional input each for
the observer, daily, window and selected-position tasks. Chart has no held-out
input. All first programs that passed the original input also passed these
checks. The three repaired programs passed separate replay checks using only
their agents' recorded edits. One repaired chart's native output was truncated
by its display pipeline; its complete output was checked by replay. Parent
replay is separate from the original model execution.

These checks compare each platform's output with that platform's exact
artifact, not with an independent astronomical reference. Agent durations from
the native traces totalled 271.720 s with the skill and 246.012 s without it
across the positive sessions.
The LLM grader passed both initially incorrect chart outputs and rejected three
window runs whose programs returned the requested values and disclosed sampled
search limits. Its grades are retained separately from execution results.

In the five eligible old-interface control pairs, both arms established that
the requested interface was absent without installing packages or implementing
a replacement. This checks that boundary, not every explanatory claim: for
example, one with-skill Python response consulted only legacy guides and did
not establish the current documented conversion route.

The mixed first-output results do not establish a benefit over the guides alone.
The skill remains unchanged; no revised-skill comparison, new trigger test,
C/C++ model task or competing-skill test was run. The earlier results below
remain a separate sample.

## Historical results

On 2026-11-02, Claude Code 2.1.316 ran every case with Claude Sonnet 5.5 as agent
and judge, one run per arm. The positive cases used a development wheel built by
this repository's CI (SHA-256
`e8c6e69a90444706dddcaf251d6446e71b7b5c728003da19ba4f7f5093178ab8`) and a locally
built development npm tarball (SHA-256
`b3714c9545cba5422ad2f17602212242c51f242288693a0e4bbd2503c91e3fb6`); both come
from runtime sources identical to the frozen documentation's source commit,
`67a7c4c`. missing-interface ran once against the registry 1.0.0 Python wheel and
once against the registry 1.0.0 npm package, so there were six pairs.

Each of the twelve sessions started in a fresh directory with no user instructions
and an empty auto-memory, and its shell could not read the grader files. No trace
showed a grader read, network access or package installation. In gregorian-lunar
and utc-jieqi, each arm's first program execution succeeded, and the unchanged
program matched the installed artifact's public results, including on held-out
inputs. In chart-snapshot, both first programs ran, but only the one written
without the skill printed every requested value: the with-skill program printed an
empty node-distance object instead of a null for each of the four nodes. The agent
fixed it after seeing the output, and the fixed program passes the same check. In
the three limitation runs (missing-interface against each old package, and
unsupported-js-events), both arms reported the missing operation without
substituting another. Agent time totalled 105 s with the skill and 97 s without
it.

Five pairs tie, and in chart-snapshot the arm without the skill did better: on
these tasks, Claude Sonnet 5.5 did at least as well without the skill. An earlier
run of the same cases, whose SKILL.md had one more sentence, tied all six pairs,
chart-snapshot included. One run per arm is a small sample.

The trigger queries ran separately, once each with the skill installed. All ten
positive queries loaded the skill and none of the ten near misses did. The queries
were written with the skill, and the session had no competing library skill, so
this checks the description's wording; triggering among many installed skills was
not measured.

The LLM grader gave every arm full marks, including the with-skill chart-snapshot
arm. It also passed both arms of an earlier diagnostic in which the evaluator's
sandbox rejected every shell command before the program ran, so its score is not
reported as a result.
