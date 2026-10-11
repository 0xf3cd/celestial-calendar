# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

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
  SONIA_KEYS_MEEUS_LICENSE_SHA256,
  VSOP87D_PLANETARY_ATTRIBUTION_SHA256,
  assemble_notices,
)
from automation.source_digest import require_facts


LLVM_LICENSE = REPO_ROOT / "third_party" / "llvm" / "llvmorg-22.1.2" / "LICENSE.TXT"
LLVM_LICENSE_SHA256 = "8d85c1057d742e597985c7d4e6320b015a9139385cff4cbae06ffc0ebe89afee"
UPSTREAM_RUN_CLANG_TIDY_SHA256 = "a651a6529eefbd12b7845afe6719773ba6578ecca222603d1262b4d2d48e1422"


def verify_clang_tidy_source(text: str) -> None:
  start = text.index("\n#\n# Vendored") + 1
  end = text.index("# FIXME: Integrate with clang-tidy-diff.py", start)
  local = text[start:end]
  assert all(line.startswith("#") for line in local.splitlines())
  require_facts(
    local,
    ("llvmorg-22.1.2", "third_party/llvm/llvmorg-22.1.2/LICENSE.TXT", "source terms", "outside the project MIT grant"),
    "LLVM source attribution",
  )
  upstream_bytes = (text[:start] + text[end:]).encode()
  assert hashlib.sha256(upstream_bytes).hexdigest() == UPSTREAM_RUN_CLANG_TIDY_SHA256


def test_run_clang_tidy_matches_llvmorg_22_1_2_outside_the_local_pin():
  verify_clang_tidy_source((REPO_ROOT / "run-clang-tidy.py").read_text(encoding="utf-8"))
  assert hashlib.sha256(LLVM_LICENSE.read_bytes()).hexdigest() == LLVM_LICENSE_SHA256


def test_llvm_project_pin_prose_can_change(tmp_path):
  path = tmp_path / "run-clang-tidy.py"
  text = (REPO_ROOT / path.name).read_text(encoding="utf-8")
  path.write_text(text.replace("this comment is the only local edit", "this is a project-local pin"), encoding="utf-8")
  verify_clang_tidy_source(path.read_text(encoding="utf-8"))


@pytest.mark.parametrize(
  "old", ["Parallel clang-tidy runner", "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception"]
)
def test_llvm_upstream_source_and_licence_text_remain_immutable(tmp_path, old):
  path = tmp_path / "run-clang-tidy.py"
  text = (REPO_ROOT / path.name).read_text(encoding="utf-8")
  path.write_text(text.replace(old, "Changed upstream material"), encoding="utf-8")
  with pytest.raises(AssertionError):
    verify_clang_tidy_source(path.read_text(encoding="utf-8"))


def test_canonical_notice_is_the_deterministic_assembly():
  notice = ROOT_NOTICE.read_bytes()
  assert notice == assemble_notices()
  assert len(NOTICE_SOURCES) == 16
  by_title = {source.title: source for source in NOTICE_SOURCES}
  sofa = by_title["IAU SOFA issue 2023-10-11 — SOFA Software License"]
  erfa = by_title["ERFA v2.0.1 — LICENSE"]
  nasa = by_title["NASA/TP-2006-214141 — acknowledgment"]
  delta_t = by_title["Delta T algorithms 1, 3, and 5 — source attribution"]
  planetary = by_title["VSOP87D — retained planetary coefficients"]
  sonia_keys = by_title["Sonia Keys meeus v3 — MIT License"]
  for marking in sofa.marking:
    assert marking.encode() in notice
  assert "does not itself constitute software provided by or endorsed by SOFA" in sofa.marking[1]
  assert "user-replaceable DAT terms" in sofa.marking[-1]
  assert sofa.applicability == ("the lunar, nutation, and leap-second data derived from IAU SOFA issue 2023-10-11")
  assert erfa.title == "ERFA v2.0.1 — LICENSE"
  assert nasa.applicability == (
    "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp, the 398 non-HKO lunar-year "
    "table values in src/calendar/lunar/algo3.hpp preserving their NASA-backed historical generation relation, and the "
    "NASA-sourced historical Delta-T validation values in src/test"
  )
  assert delta_t.applicability == (
    "the Delta T algo1 coefficient table, algo3 expressions, and algo5 long-term branch in src/astro/delta_t.hpp"
  )
  assert delta_t.sha256 == DELTA_T_ATTRIBUTION_SHA256
  for marking in delta_t.marking:
    assert marking.encode() in notice
  assert [source.title for source in NOTICE_SOURCES[-6:]] == [
    "Hong Kong Observatory — retained lunar-year words",
    "VSOP87D — retained Earth coefficients",
    "VSOP87D — retained planetary coefficients",
    "Astronomical Algorithms — retained daily-variation series",
    "Sonia Keys meeus v3 — MIT License",
    "Microsoft — statically linked C/C++ runtime portions",
  ]
  assert planetary.sha256 == VSOP87D_PLANETARY_ATTRIBUTION_SHA256
  assert sonia_keys.applicability == (
    "the Chapter 37 Pluto evaluator and 43-term coefficient table in src/astro/pluto.hpp and their consumer binaries"
  )
  assert sonia_keys.upstream.endswith("bfbd9ac2c7f709c94f1dee190c633d24a723f628/LICENSE")
  assert sonia_keys.sha256 == SONIA_KEYS_MEEUS_LICENSE_SHA256
  assert sonia_keys.marking == (
    "Source-file copyright: Copyright 2013 Sonia Keys (`v3/pluto/pluto.go` at the pinned commit).",
  )
  for source in NOTICE_SOURCES[-6:-2] + NOTICE_SOURCES[-1:]:
    assert source.marking
    assert hashlib.sha256((REPO_ROOT / source.path).read_bytes()).hexdigest() in notice.decode()
  assert b"NASA/TP-2006-214141 (October 2006)" in notice
  assert b"not an upstream-byte identity claim" in notice
  assert b"generated lunar-year values remain project-authored material" in notice
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
    target = next(source for source in sources if source.title == "Hong Kong Observatory — retained lunar-year words")
    sources = tuple(replace(source, marking=()) if source == target else source for source in sources)
  elif mutation == "order":
    sources = tuple(reversed(sources))
  else:
    sources = sources[:-1]

  assert assemble_notices(tmp_path, sources, PREAMBLE, separator) != ROOT_NOTICE.read_bytes()


def test_notice_assembly_rejects_duplicate_inputs():
  with pytest.raises(RuntimeError, match="must be unique"):
    assemble_notices(sources=(*NOTICE_SOURCES, NOTICE_SOURCES[0]))
