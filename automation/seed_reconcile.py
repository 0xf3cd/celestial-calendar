# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import re

from typing import Final, List, Optional, Tuple

import yaml

from . import paths
from .utils import green_print, red_print, yellow_print


# The default test seed lives in four copies that cannot reference each other: two workflow
# env blocks, the Dockerfile ARG (a docker build layer does not see the Actions env context),
# and the C++ DEFAULT_SEED the tests actually read. Until this gate they were aligned by
# comments alone, and a drift fails *green*: every leg still runs, just with a seed nobody
# chose (#170). Pure parsing -- no build needed, any leg can run it.
#
# Structured files are parsed, not pattern-matched: review kept finding shapes where text
# *looks like* the config without *being* it (commented-out lines, inline and block
# comments, a job-level env shadowing the workflow-level one). Workflows go through the
# same YAML parser the ai-workflows gate uses (#144's argument: prose and config cannot be
# told apart by matching lines); random.hpp is read with C++ comments stripped. Only the
# Dockerfile keeps a regex -- its comment syntax is line-start `#` alone, so a `^` anchor
# is complete there.
WORKFLOW_COPIES: Final[Tuple[Tuple[str, str], ...]] = (
  ("core_tests.yml env", ".github/workflows/core_tests.yml"),
  ("build_and_test.yml env", ".github/workflows/build_and_test.yml"),
)

TEXT_COPIES: Final[Tuple[Tuple[str, str, re.Pattern], ...]] = (
  ("Dockerfile ARG",
   "Dockerfile",
   re.compile(r"^\s*ARG\s+CELESTIAL_TEST_SEED=(\d+)", re.MULTILINE)),
  ("random.hpp DEFAULT_SEED",
   "src/util/random.hpp",
   re.compile(r"^\s*inline\s+constexpr\s+uint64_t\s+DEFAULT_SEED\s*=\s*(\d+)", re.MULTILINE)),
)

# The docker legs pass the workflow seed into the image explicitly. Lose this line and the
# build does not fail -- it falls back to the Dockerfile default, so a later seed change in
# the workflows would silently stop reaching the docker legs (#170, issue comment).
BUILD_ARG_LINE: Final[str] = "--build-arg CELESTIAL_TEST_SEED=${{ env.CELESTIAL_TEST_SEED }}"


def _strip_continuation(line: str) -> str:
  """A line's payload: stripped, without a trailing shell/YAML continuation backslash."""
  return line.strip().rstrip("\\").strip()


def _strip_cpp_comments(text: str) -> str:
  """`random.hpp` with `/* ... */` blocks and `//` tails removed."""
  text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
  return re.sub(r"//[^\n]*", "", text)


def _read_workflow_seed(label: str, rel_path: str) -> Tuple[Optional[str], Optional[str]]:
  """The workflow-level `env.CELESTIAL_TEST_SEED`, from the parsed document.

  A job- or step-level `env` entry sits at a different path in the document, so it cannot
  shadow the copy this gate reconciles (#191 review).
  """
  path = paths.proj_root() / rel_path
  document = yaml.safe_load(path.read_text(encoding="utf-8"))
  env = document.get("env") or {}
  if "CELESTIAL_TEST_SEED" not in env:
    return None, f"{label}: no top-level env CELESTIAL_TEST_SEED in {rel_path}"
  return str(env["CELESTIAL_TEST_SEED"]), None


def _read_text_seed(label: str, rel_path: str, pattern: re.Pattern) -> Tuple[Optional[str], Optional[str]]:
  """Read one regex-held copy. Returns (seed, None) or (None, failure-message)."""
  path = paths.proj_root() / rel_path
  text = path.read_text(encoding="utf-8")
  if rel_path.endswith(".hpp"):
    text = _strip_cpp_comments(text)
  matches = pattern.findall(text)
  if len(matches) != 1:
    return None, f"{label}: expected exactly one seed in {rel_path}, found {len(matches)}"
  return matches[0], None


def _self_test() -> List[str]:
  """Prove every reader can still find the shape it was written for -- and rejects the
  comment-shaped decoys review injected (#191)."""
  failures = []

  workflow_example = 'env:\n  CELESTIAL_TEST_SEED: "42"\n'
  parsed = yaml.safe_load(workflow_example)
  if str((parsed.get("env") or {}).get("CELESTIAL_TEST_SEED")) != "42":
    failures.append("self-test: workflow reader lost the canonical env example")
  job_level = yaml.safe_load('jobs:\n  t:\n    env:\n      CELESTIAL_TEST_SEED: "42"\n')
  if (job_level.get("env") or {}).get("CELESTIAL_TEST_SEED") is not None:
    failures.append("self-test: workflow reader accepts a job-level env as the top-level copy")

  text_examples = {
    "Dockerfile ARG": "ARG CELESTIAL_TEST_SEED=42",
    "random.hpp DEFAULT_SEED": "inline constexpr uint64_t DEFAULT_SEED = 42;",
  }
  for label, _, pattern in TEXT_COPIES:
    matches = pattern.findall(text_examples[label])
    if matches != ["42"]:
      failures.append(f"self-test: pattern for {label} gave {matches} on its canonical example")
    if pattern.findall("# " + text_examples[label]):
      failures.append(f"self-test: pattern for {label} matches a commented-out line")
  block = "/*\ninline constexpr uint64_t DEFAULT_SEED = 42;\n*/"
  if TEXT_COPIES[1][2].findall(_strip_cpp_comments(block)):
    failures.append("self-test: DEFAULT_SEED pattern matches inside a block comment")

  # The build-arg matcher was falsified twice (substring, then prefix): pin its three states.
  canonical = "  --build-arg CELESTIAL_TEST_SEED=${{ env.CELESTIAL_TEST_SEED }} \\"
  for text, expected in [
    (canonical, True),  # the real line, with its continuation backslash
    ("# " + canonical, False),  # commented out
    (canonical.replace("}}", "}}_BROKEN"), False),  # suffixed payload
  ]:
    actual = any(_strip_continuation(line) == BUILD_ARG_LINE for line in text.splitlines())
    if actual != expected:
      failures.append(f"self-test: build-arg matcher gave {actual} on {text!r}")
  return failures


def check_seed_reconcile() -> int:
  """Hold the four CELESTIAL_TEST_SEED copies to each other.

  Two failure shapes, reported apart because the fixes differ: the values disagree (someone
  changed one copy), or the docker `--build-arg` line is gone (CI then silently falls back
  to the Dockerfile default -- the copies can agree while CI runs a different seed).
  """
  print("#" * 60)
  yellow_print("Checking the four CELESTIAL_TEST_SEED copies agree...")

  failures = _self_test()
  if failures:
    for f in failures:
      red_print(f"  - {f}")
    red_print("Seed-reconcile gate is broken (self-test failed); fix the parser, not the data")
    return 1

  seeds: List[Tuple[str, str]] = []
  for label, rel_path in WORKFLOW_COPIES:
    seed, failure = _read_workflow_seed(label, rel_path)
    if failure:
      failures.append(failure)
    else:
      seeds.append((label, seed))
  for label, rel_path, pattern in TEXT_COPIES:
    seed, failure = _read_text_seed(label, rel_path, pattern)
    if failure:
      failures.append(failure)
    else:
      seeds.append((label, seed))

  if seeds and len({seed for _, seed in seeds}) != 1:
    for label, seed in seeds:
      failures.append(f"value drift: {label} has {seed}")

  build_and_test = (paths.proj_root() / ".github" / "workflows" / "build_and_test.yml").read_text(encoding="utf-8")
  active_build_arg = any(_strip_continuation(line) == BUILD_ARG_LINE for line in build_and_test.splitlines())
  if not active_build_arg:
    failures.append(
      "build-arg line missing: build_and_test.yml no longer passes CELESTIAL_TEST_SEED into "
      "the docker build -- the docker legs silently fall back to the Dockerfile default"
    )

  print("#" * 60)
  if failures:
    red_print(f"Seed-reconcile gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"All {len(WORKFLOW_COPIES) + len(TEXT_COPIES)} seed copies agree "
              "(and the docker build-arg line is in place)")
  return 0
