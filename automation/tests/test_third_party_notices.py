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
  DELTA_T_ATTRIBUTION_SHA256,
  NOTICE_SOURCES,
  PREAMBLE,
  REPO_ROOT,
  ROOT_NOTICE,
  SEPARATOR,
  assemble_notices,
)


LLVM_LICENSE = REPO_ROOT / "third_party" / "llvm" / "llvmorg-22.1.2" / "LICENSE.TXT"
LLVM_LICENSE_SHA256 = "8d85c1057d742e597985c7d4e6320b015a9139385cff4cbae06ffc0ebe89afee"
CANONICAL_NOTICE_SHA256 = "4e142b6b1c0821d08eaaef232f224c396d9510bd3b08f479d49dbc15c1f83665"
UPSTREAM_RUN_CLANG_TIDY_SHA256 = "a651a6529eefbd12b7845afe6719773ba6578ecca222603d1262b4d2d48e1422"
LOCAL_RUN_CLANG_TIDY_BLOCK = (
  "#\n",
  "# Vendored from llvm-project at tag llvmorg-22.1.2; this comment is the only local edit. Its\n",
  "# major has to match the clang-tidy CI runs (AGENTS.md gotcha 9), so re-vendor rather than patch:\n",
  "# raw.githubusercontent.com/llvm/llvm-project/llvmorg-<tag>/clang-tools-extra/clang-tidy/tool/\n",
  "# Companion license: third_party/llvm/llvmorg-22.1.2/LICENSE.TXT (upstream LICENSE.TXT at the same tag)\n",
)


def test_run_clang_tidy_matches_llvmorg_22_1_2_outside_the_local_pin():
  script = (REPO_ROOT / "run-clang-tidy.py").read_text(encoding="utf-8").splitlines(keepends=True)

  assert tuple(script[9:14]) == LOCAL_RUN_CLANG_TIDY_BLOCK
  upstream_bytes = "".join([*script[:9], *script[14:]]).encode()
  assert hashlib.sha256(upstream_bytes).hexdigest() == UPSTREAM_RUN_CLANG_TIDY_SHA256
  assert hashlib.sha256(LLVM_LICENSE.read_bytes()).hexdigest() == LLVM_LICENSE_SHA256


def test_canonical_notice_is_the_pinned_deterministic_assembly():
  notice = ROOT_NOTICE.read_bytes()
  assert notice == assemble_notices()
  assert hashlib.sha256(notice).hexdigest() == CANONICAL_NOTICE_SHA256
  assert len(NOTICE_SOURCES) == 10
  for marking in NOTICE_SOURCES[-4].marking:
    assert marking.encode() in notice
  assert "does not itself constitute software provided by or endorsed by SOFA" in NOTICE_SOURCES[-4].marking[1]
  assert "user-replaceable DAT terms" in NOTICE_SOURCES[-4].marking[-1]
  assert NOTICE_SOURCES[-3].title == "ERFA v2.0.1 — LICENSE"
  assert NOTICE_SOURCES[-2].title == "NASA/TP-2006-214141 — acknowledgment"
  assert NOTICE_SOURCES[-2].applicability == (
    "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp, the 398 non-HKO lunar-year "
    "table values in src/calendar/lunar/algo3.hpp retained from its NASA-backed original generation, and the "
    "NASA-sourced historical Delta-T validation values in src/test"
  )
  assert NOTICE_SOURCES[-1].title == "Delta T algorithms 1, 3, and 5 — source attribution"
  assert NOTICE_SOURCES[-1].applicability == (
    "the Delta T algo1 coefficient table, algo3 expressions, and algo5 long-term branch in src/astro/delta_t.hpp"
  )
  assert NOTICE_SOURCES[-1].sha256 == DELTA_T_ATTRIBUTION_SHA256
  for marking in NOTICE_SOURCES[-1].marking:
    assert marking.encode() in notice
  assert b"NASA/TP-2006-214141 (October 2006)" in notice
  assert b"not an upstream-byte identity claim" in notice
  assert b"covers this repository's acknowledgment file, not the linked NASA page" in notice
  assert b"It records attribution only" in notice
  assert b"Xu Jianwei" in notice
  assert b"Fred Espenak" in notice
  assert b"M. Zawilski" in notice
  assert b"does not describe the corrected expression as MIT, CC BY, OGL, permission-granted, or" in notice
  assert b"Redistributions in binary form must reproduce the above copyright" in notice


def test_delta_t_source_markings_are_immutable_and_exact():
  delta_t = (REPO_ROOT / "src" / "astro" / "delta_t.hpp").read_text(encoding="utf-8")
  readme = (REPO_ROOT / "README.md").read_text(encoding="utf-8")
  xu_url = "https://web.archive.org/web/20080919020456id_/http://www.fjptsz.com/xxjs/xjw/rj/115.htm"
  eclipsewise_url = "https://www.eclipsewise.com/help/deltatpoly2014.html"
  addendum_url = "https://doi.org/10.1098/rspa.2020.0776"
  hmnao_url = "https://web.archive.org/web/20230103030546id_/https://astro.ukho.gov.uk/nao/lvm/"
  record_url = (
    "https://github.com/0xf3cd/AstroTime-Analysis/blob/"
    "ed1cdc2fd6c5122b391a82289aa2cc060340552d/DeltaT/algo5/record.json"
  )

  assert delta_t.count(xu_url) == 2
  assert "Xu Jianwei, 寿星万年历2008版(V1.3.2)" in delta_t
  assert "www.cnblogs.com/qintangtao" not in delta_t
  assert delta_t.count(eclipsewise_url) == 2
  assert "Fred Espenak, Thousand Year Canon of Solar Eclipses 1501 to 2500 (2014)" in delta_t
  assert "quadratic trend to Marc van der Sluys" in delta_t
  assert delta_t.count(addendum_url) == 2
  assert delta_t.count(hmnao_url) == 2
  assert delta_t.count(record_url) == 2
  assert "AstroTime-Analysis/blob/main/DeltaT/algo5.py" not in delta_t
  assert delta_t.count("https://astro.ukho.gov.uk/nao/lvm/") == 2
  for source in (xu_url, eclipsewise_url, addendum_url):
    assert source in readme
  assert "10.1098/rspa.2016.0404" not in readme
  assert "www.cnblogs.com/qintangtao" not in readme


def materialize_inputs(destination: Path) -> None:
  for source in NOTICE_SOURCES:
    target = destination / source.path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes((REPO_ROOT / source.path).read_bytes())


@pytest.mark.parametrize("mutation", ["body", "delimiter", "applicability", "marking", "order", "omitted"])
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
  elif mutation == "marking":
    sources = (*sources[:-4], replace(sources[-4], marking=()), *sources[-3:])
  elif mutation == "order":
    sources = tuple(reversed(sources))
  else:
    sources = sources[:-1]

  assert assemble_notices(tmp_path, sources, PREAMBLE, separator) != ROOT_NOTICE.read_bytes()


def test_notice_assembly_rejects_duplicate_inputs():
  with pytest.raises(RuntimeError, match="must be unique"):
    assemble_notices(sources=(*NOTICE_SOURCES, NOTICE_SOURCES[0]))
