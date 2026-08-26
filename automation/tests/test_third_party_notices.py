# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

import hashlib

from dataclasses import replace
from pathlib import Path

import pytest

from automation.third_party_notices import (
  NOTICE_SOURCES,
  PREAMBLE,
  REPO_ROOT,
  ROOT_NOTICE,
  SEPARATOR,
  assemble_notices,
)


LLVM_LICENSE = REPO_ROOT / "third_party" / "llvm" / "llvmorg-22.1.2" / "LICENSE.TXT"
LLVM_LICENSE_SHA256 = "8d85c1057d742e597985c7d4e6320b015a9139385cff4cbae06ffc0ebe89afee"
UPSTREAM_RUN_CLANG_TIDY_SHA256 = "a651a6529eefbd12b7845afe6719773ba6578ecca222603d1262b4d2d48e1422"
LOCAL_RUN_CLANG_TIDY_BLOCK = (
  "#\n",
  "# Vendored from llvm-project at tag llvmorg-22.1.2; this comment is the only local edit. Its\n",
  "# major has to match the clang-tidy CI runs (AGENTS.md gotcha 9), so re-vendor rather than patch:\n",
  "# raw.githubusercontent.com/llvm/llvm-project/llvmorg-<tag>/clang-tools-extra/clang-tidy/tool/\n",
  "# Companion license: third_party/llvm/llvmorg-22.1.2/LICENSE.TXT\n",
)


def test_run_clang_tidy_matches_llvmorg_22_1_2_outside_the_local_pin():
  script = (REPO_ROOT / "run-clang-tidy.py").read_text(encoding="utf-8").splitlines(keepends=True)

  assert tuple(script[9:14]) == LOCAL_RUN_CLANG_TIDY_BLOCK
  upstream_bytes = "".join([*script[:9], *script[14:]]).encode()
  assert hashlib.sha256(upstream_bytes).hexdigest() == UPSTREAM_RUN_CLANG_TIDY_SHA256
  assert hashlib.sha256(LLVM_LICENSE.read_bytes()).hexdigest() == LLVM_LICENSE_SHA256


def test_canonical_notice_is_the_pinned_deterministic_assembly():
  assert ROOT_NOTICE.read_bytes() == assemble_notices()
  assert len(NOTICE_SOURCES) == 6


def materialize_inputs(destination: Path) -> None:
  for source in NOTICE_SOURCES:
    target = destination / source.path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes((REPO_ROOT / source.path).read_bytes())


@pytest.mark.parametrize("mutation", ["body", "delimiter", "applicability", "order", "omitted"])
def test_notice_assembly_mutations_change_the_canonical_bytes(tmp_path, mutation):
  materialize_inputs(tmp_path)
  sources = NOTICE_SOURCES
  separator = SEPARATOR
  if mutation == "body":
    target = tmp_path / sources[0].path
    target.write_bytes(target.read_bytes() + b"changed")
    with pytest.raises(RuntimeError, match="hash mismatch"):
      assemble_notices(repo_root=tmp_path)
    return
  if mutation == "delimiter":
    separator = b"-" * 78 + b"\n"
  elif mutation == "applicability":
    sources = (replace(sources[0], applicability="changed"), *sources[1:])
  elif mutation == "order":
    sources = tuple(reversed(sources))
  else:
    sources = sources[:-1]

  assert assemble_notices(tmp_path, sources, PREAMBLE, separator) != ROOT_NOTICE.read_bytes()


def test_notice_assembly_rejects_duplicate_inputs():
  with pytest.raises(RuntimeError, match="must be unique"):
    assemble_notices(sources=(*NOTICE_SOURCES, NOTICE_SOURCES[0]))
