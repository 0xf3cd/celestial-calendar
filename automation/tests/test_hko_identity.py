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
import json

from pathlib import Path
from shutil import copy2

import pytest

from automation.hko_identity import (
  HKO_ARTIFACT_SHA256,
  HKO_EXTRACTOR_SHA256,
  HKO_ROOT,
  REPO_ROOT,
  IdentityCounts,
  verify_hko_identity,
)


TARGET_FILES = (
  Path("src/calendar/lunar/algo1.hpp"),
  Path("src/calendar/lunar/algo3.hpp"),
)


def materialize_inputs(destination: Path) -> Path:
  hko_root = destination / HKO_ROOT.relative_to(REPO_ROOT)
  hko_root.mkdir(parents=True)
  for name in HKO_ARTIFACT_SHA256:
    copy2(HKO_ROOT / name, hko_root / name)
  for relative in TARGET_FILES:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)
  return hko_root


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def rehash(hko_root: Path, name: str) -> dict[str, str]:
  hashes = dict(HKO_ARTIFACT_SHA256)
  hashes[name] = hashlib.sha256((hko_root / name).read_bytes()).hexdigest()
  return hashes


def test_hko_identity_is_pinned():
  assert HKO_EXTRACTOR_SHA256 == "bfb8948025e4e5e78cea2f6f39061f7a181588bb67055d7cbfd01d47f855975f"
  assert HKO_ARTIFACT_SHA256 == {
    "extract.py": HKO_EXTRACTOR_SHA256,
    "hko-source-sha256.txt": "8f3980c799312490a516725a038162f6c16e6521515bfb196c4fcd6ab0baad1e",
    "hko-comparison.tsv": "aee2f24dc535fe2fd282e7d23e10acdd12dca9d9a8024584e50ac6a1deaa1d79",
    "hko-reconstructed-words.tsv": "ecca33a7bb3f77dfd5fb2dba75fc30391a9451d3e9fb00800488a0ac09064542",
    "summary.json": "5f43d03a904cbafcb95da58b62e0ce75cea9910c8e36591091b30d80867ab998",
  }
  assert verify_hko_identity() == IdentityCounts(200, 199, 199, 199, 1)


@pytest.mark.parametrize("mutation", ["extractor-byte", "extractor-pin", "response-hash"])
def test_hko_artifact_pins_fail(tmp_path, mutation):
  hko_root = materialize_inputs(tmp_path)
  hashes = dict(HKO_ARTIFACT_SHA256)
  if mutation == "extractor-byte":
    extractor = hko_root / "extract.py"
    extractor.write_bytes(extractor.read_bytes() + b"\n")
  elif mutation == "extractor-pin":
    hashes["extract.py"] = "0" * 64
  else:
    replace_once(
      hko_root / "hko-source-sha256.txt",
      "b4cc9e6df809b9e1c67c8ab4c46b5869d54eb32791daa9ef384838f3014f877f",
      "0" * 64,
    )

  with pytest.raises(RuntimeError, match="HKO artifact hash mismatch"):
    verify_hko_identity(repo_root=tmp_path, hko_root=hko_root, artifact_sha256=hashes)


@pytest.mark.parametrize("mutation", ["reconstructed-bit", "year-order", "boundary-role", "anomaly"])
def test_hko_record_structure_is_pinned(tmp_path, mutation):
  hko_root = materialize_inputs(tmp_path)
  if mutation == "reconstructed-bit":
    name = "hko-reconstructed-words.tsv"
    replace_once(hko_root / name, "1901\t0x620752", "1901\t0x620753")
    message = "comparison and reconstructed words differ"
  elif mutation == "year-order":
    name = "hko-reconstructed-words.tsv"
    replace_once(hko_root / name, "1901\t0x620752\n1902\t0x4c0ea5", "1902\t0x4c0ea5\n1901\t0x620752")
    message = "reconstructed year order differs"
  else:
    name = "summary.json"
    summary = json.loads((hko_root / name).read_text(encoding="utf-8"))
    if mutation == "boundary-role":
      summary["boundary_only_years"] = []
    else:
      summary["daily_row_anomalies"][0]["missing_dates"] = []
    (hko_root / name).write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    message = "HKO summary differs"

  with pytest.raises(RuntimeError, match=message):
    verify_hko_identity(repo_root=tmp_path, hko_root=hko_root, artifact_sha256=rehash(hko_root, name))


@pytest.mark.parametrize("table", ["algo1", "algo3"])
def test_hko_runtime_words_are_pinned(tmp_path, table):
  hko_root = materialize_inputs(tmp_path)
  header = tmp_path / f"src/calendar/lunar/{table}.hpp"
  replace_once(header, "0x620752, 0x4c0ea5", "0x620753, 0x4c0ea5")

  with pytest.raises(RuntimeError, match=f"{table} HKO words differ"):
    verify_hko_identity(repo_root=tmp_path, hko_root=hko_root)


def test_hko_artifact_inventory_is_exact(tmp_path):
  hko_root = materialize_inputs(tmp_path)
  (hko_root / "T1901e.txt").write_text("raw body must remain outside the tree\n", encoding="utf-8")

  with pytest.raises(RuntimeError, match="HKO artifact inventory differs"):
    verify_hko_identity(repo_root=tmp_path, hko_root=hko_root)
