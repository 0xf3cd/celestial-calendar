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
import subprocess
import sys

from pathlib import Path
from shutil import copy2

import pytest

from automation.astrotime_delta_t_provenance import (
  ALGO4_FUNCTION_SHA256,
  ALGO5_FUNCTION_SHA256,
  ASTROTIME_GENERATION_COMMIT,
  ASTROTIME_ALGO5_DATA_COMMIT,
  ASTROTIME_ALGO5_GENERATION_COMMIT,
  ASTROTIME_ALGO5_GRANT_NORMALIZED_SHA256,
  ASTROTIME_ALGO5_GRANT_SHA256,
  ASTROTIME_ALGO5_RECORD_COMMIT,
  ASTROTIME_ALGO5_RECORD_SHA256,
  ASTROTIME_ALGO5_RECORD_URL,
  ASTROTIME_ALGO5_ROOT,
  ASTROTIME_ALGO5_ROOT_RELATIVE,
  ASTROTIME_GRANT_NORMALIZED_SHA256,
  ASTROTIME_GRANT_SHA256,
  ASTROTIME_RECORD_COMMIT,
  ASTROTIME_RECORD_SHA256,
  ASTROTIME_RECORD_URL,
  ASTROTIME_ROOT,
  DEFAULT_FUNCTION_SHA256,
  REPO_ROOT,
  _cpp_block,
  verify_astrotime_delta_t_provenance,
)
from automation.source_digest import canonical_cpp


def materialize_inputs(destination: Path) -> Path:
  astrotime_root = destination / ASTROTIME_ROOT.relative_to(REPO_ROOT)
  astrotime_root.mkdir(parents=True)
  for source in ASTROTIME_ROOT.iterdir():
    copy2(source, astrotime_root / source.name)
  algo5_root = destination / ASTROTIME_ALGO5_ROOT_RELATIVE
  algo5_root.mkdir(parents=True)
  for source in ASTROTIME_ALGO5_ROOT.iterdir():
    copy2(source, algo5_root / source.name)
  delta_t = destination / "src" / "astro" / "delta_t.hpp"
  delta_t.parent.mkdir(parents=True)
  copy2(REPO_ROOT / "src" / "astro" / "delta_t.hpp", delta_t)
  return astrotime_root


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def replace_once_in_algo4(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  start = text.index("namespace algo4 {")
  end = text.index("} // namespace algo4", start)
  block = text[start:end]
  assert block.count(old) == 1
  path.write_text(text[:start] + block.replace(old, new) + text[end:], encoding="utf-8")


def replace_once_in_algo5(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  start = text.index("namespace algo5 {")
  end = text.index("} // namespace algo5", start)
  block = text[start:end]
  assert block.count(old) == 1
  path.write_text(text[:start] + block.replace(old, new) + text[end:], encoding="utf-8")


def mutate_record(path: Path, mutation) -> str:
  payload = json.loads(path.read_text(encoding="utf-8"))
  mutation(payload)
  path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
  return hashlib.sha256(path.read_bytes()).hexdigest()


def algo4_function_digest(path: Path) -> str:
  delta_t = path.read_text(encoding="utf-8")
  algo4_start = delta_t.index("namespace algo4 {")
  algo4_end = delta_t.index("} // namespace algo4", algo4_start)
  function_source = _cpp_block(
    delta_t[algo4_start:algo4_end],
    "[[nodiscard]] constexpr auto compute(const double year) -> double",
  )
  return hashlib.sha256(canonical_cpp(function_source).encode()).hexdigest()


def algo5_function_digest(path: Path) -> str:
  delta_t = path.read_text(encoding="utf-8")
  algo5_start = delta_t.index("namespace algo5 {")
  algo5_end = delta_t.index("} // namespace algo5", algo5_start)
  function_source = _cpp_block(
    delta_t[algo5_start:algo5_end],
    "[[nodiscard]] constexpr auto compute(const double year) noexcept -> double",
  )
  return hashlib.sha256(canonical_cpp(function_source).encode()).hexdigest()


def default_function_digest(path: Path) -> str:
  delta_t = path.read_text(encoding="utf-8")
  algo5_end = delta_t.index("} // namespace algo5")
  function_source = _cpp_block(
    delta_t[algo5_end:],
    "[[nodiscard]] constexpr auto compute(const double year) noexcept -> double",
  )
  return hashlib.sha256(canonical_cpp(function_source).encode()).hexdigest()


def test_astrotime_record_is_pinned():
  assert ASTROTIME_RECORD_COMMIT == "55115f4bf59cbdc47970b7f2d69a9715a467a3e9"
  assert ASTROTIME_GENERATION_COMMIT == "298fa51777ec533951c4c1f83f8e5704b586754f"
  assert ASTROTIME_RECORD_URL == (
    "https://github.com/0xf3cd/AstroTime-Analysis/blob/"
    "55115f4bf59cbdc47970b7f2d69a9715a467a3e9/DeltaT/algo4/record.json"
  )
  assert ASTROTIME_GRANT_SHA256 == "075b3e670fbe4b32156d9f41386f444d65a0a7bbdc1e87dca2c0318ffd7c586f"
  assert ASTROTIME_RECORD_SHA256 == "ac3f00a8fe69af51c0e0fea8f945e5cf48cb81bfa0bac3a0471950e6558fd022"
  assert ASTROTIME_GRANT_NORMALIZED_SHA256 == "f00038723177475993d52bf4ec5bdd45f123cfad5201a7a9a478ba3fb3df4132"
  assert ALGO4_FUNCTION_SHA256 == "0b6065a66073f45c3cd510be87f31a22e65625665f569a91e7802c29fc17539e"
  assert ASTROTIME_ALGO5_RECORD_COMMIT == "ed1cdc2fd6c5122b391a82289aa2cc060340552d"
  assert ASTROTIME_ALGO5_GENERATION_COMMIT == "a1e95811b47c084f44b4b4bb7444560dd5b863bc"
  assert ASTROTIME_ALGO5_DATA_COMMIT == "ddf3be1972e405ed02233837394cd3377226b65c"
  assert ASTROTIME_ALGO5_RECORD_URL == (
    "https://github.com/0xf3cd/AstroTime-Analysis/blob/"
    "ed1cdc2fd6c5122b391a82289aa2cc060340552d/DeltaT/algo5/record.json"
  )
  assert ASTROTIME_ALGO5_GRANT_SHA256 == "f87240e7cbc4c0a21457ca0cf026dd848e619e1257737dab9307e3d1e73443f7"
  assert ASTROTIME_ALGO5_RECORD_SHA256 == "77fa6748fc19954073ee3c013731ac0200f4730efcb483f9f08fe8c921666455"
  assert ASTROTIME_ALGO5_GRANT_NORMALIZED_SHA256 == ("63fbad6abcbf10a91f0561e675e1ae040401d54739d89c6d804598dd3b0cf447")
  assert ALGO5_FUNCTION_SHA256 == "44ef6c75688b7036892e8cd6ba528822ae97664ec67732db5f208fe467960d4b"
  assert DEFAULT_FUNCTION_SHA256 == "883c5e0652ec79f72d15bf125af88c7eceedb510d95f6936478e0c4fd37d1be8"
  assert verify_astrotime_delta_t_provenance() is None


def test_direct_verifier_runs_from_an_unrelated_directory(tmp_path):
  result = subprocess.run(
    [sys.executable, REPO_ROOT / "automation" / "astrotime_delta_t_provenance.py"],
    cwd=tmp_path,
    check=True,
    capture_output=True,
    text=True,
  )
  assert result.stdout == ""
  assert result.stderr == ""


def test_direct_verifier_returns_nonzero_on_failure(tmp_path):
  shadow_root = tmp_path / "repo"
  astrotime_root = materialize_inputs(shadow_root)
  automation_root = shadow_root / "automation"
  automation_root.mkdir()
  for relative in ("astrotime_delta_t_provenance.py", "source_digest.py"):
    copy2(REPO_ROOT / "automation" / relative, automation_root / relative)
  grant = astrotime_root / "GRANT.md"
  grant.write_bytes(grant.read_bytes() + b"\n")

  result = subprocess.run(
    [sys.executable, automation_root / "astrotime_delta_t_provenance.py"],
    cwd=tmp_path,
    check=False,
    capture_output=True,
    text=True,
  )
  assert result.returncode != 0
  assert result.stdout == ""
  assert "AstroTime grant hash mismatch" in result.stderr


@pytest.mark.parametrize("relative", ["GRANT.md", "record.json"])
def test_retained_bytes_are_pinned(tmp_path, relative):
  astrotime_root = materialize_inputs(tmp_path)
  path = astrotime_root / relative
  path.write_bytes(path.read_bytes() + b"\n")

  with pytest.raises(RuntimeError, match="hash mismatch"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, astrotime_root=astrotime_root)


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    (
      lambda payload: payload["historical_generation"].update({"commit": "main"}),
      "historical generation identity differs",
    ),
    (
      lambda payload: payload["historical_generation"]["notebook"].update({"git_blob_sha1": "unknown"}),
      "notebook identity differs",
    ),
    (
      lambda payload: payload["historical_generation"]["inputs"]["iers_bulletin_a"].update({"raw_file_count": 1019}),
      "historical inputs differ",
    ),
    (
      lambda payload: payload["historical_generation"]["inputs"]["usno_predictions"].update({"sha256": "unknown"}),
      "historical inputs differ",
    ),
    (
      lambda payload: payload["historical_generation"]["environment"]["unrecovered"].remove("timezone"),
      "environment boundary differs",
    ),
    (
      lambda payload: payload["outputs"]["observed_segment"]["coefficients"].__setitem__(0, "-1539.6"),
      "generated outputs differ",
    ),
    (
      lambda payload: payload["outputs"]["prediction_segment"].update({"consumer_basis": "u = year - 2021"}),
      "generated outputs differ",
    ),
    (
      lambda payload: payload["reproducibility"].update({"bit_for_bit_regeneration_environment": "recovered"}),
      "reproducibility boundary differs",
    ),
    (
      lambda payload: payload["consumer_contract"].update({"pre_2005": "Delegate to astro::delta_t::algo3::compute"}),
      "consumer contract record differs",
    ),
    (
      lambda payload: payload["consumer_contract"].update({"non_finite_or_year_at_least_2035": "Return NaN"}),
      "consumer contract record differs",
    ),
    (
      lambda payload: payload["consumer_contract"].update({"default_model": True}),
      "consumer contract record differs",
    ),
    (lambda payload: payload.update({"extra_section": {}}), "record schema inventory differs"),
    (
      lambda payload: payload["historical_generation"].update({"replay_command": "unknown"}),
      "historical generation inventory differs",
    ),
  ],
  ids=[
    "generation-commit",
    "notebook-blob",
    "iers-count",
    "usno-hash",
    "environment-gap",
    "coefficient",
    "basis",
    "replay-claim",
    "consumer-delegation",
    "consumer-exception",
    "consumer-default-model",
    "top-level-key",
    "historical-generation-key",
  ],
)
def test_semantic_record_fields_are_pinned(tmp_path, mutation, message):
  astrotime_root = materialize_inputs(tmp_path)
  record = astrotime_root / "record.json"
  digest = mutate_record(record, mutation)

  with pytest.raises(RuntimeError, match=message):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      astrotime_root=astrotime_root,
      record_sha256=digest,
    )


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    (
      "[`0xf3cd/celestial-calendar`](https://github.com/0xf3cd/celestial-calendar) as project-authored",
      "[`example/other-project`](https://example.invalid/other-project) as unrelated",
      "as project-authored material",
    ),
    (
      "no separate attribution, notice, or other condition",
      "attribution is required",
      "no separate attribution",
    ),
    (
      "does not apply to IERS Bulletin A",
      "applies to IERS Bulletin A",
      "does not apply to IERS Bulletin A",
    ),
    (
      "To the extent that I hold copyright or other licensable rights in the material listed below, I grant",
      "I grant",
      "grant scope differs",
    ),
    (
      "[`0xf3cd/celestial-calendar`](https://github.com/0xf3cd/celestial-calendar)",
      "[`example/other-project`](https://example.invalid/other-project)",
      "grant scope differs",
    ),
    (
      "to their respective source terms.",
      "to their respective source terms.\n\nRedistributors must add attribution.",
      "grant scope differs",
    ),
  ],
  ids=["target", "conditions", "input-exclusion", "rights-qualifier", "grantee", "added-condition"],
)
def test_grant_scope_is_pinned_after_rehash(tmp_path, old, new, message):
  astrotime_root = materialize_inputs(tmp_path)
  grant = astrotime_root / "GRANT.md"
  replace_once(grant, old, new)
  digest = hashlib.sha256(grant.read_bytes()).hexdigest()

  with pytest.raises(RuntimeError, match=message):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      astrotime_root=astrotime_root,
      grant_sha256=digest,
    )


@pytest.mark.parametrize(
  ("old", "new"),
  [
    ("-1539.5103964825782", "-1539.5103964825781"),
    ("year >= 2005 and year < 2024", "year >= 2005 and year < 2025"),
    (
      "  if (year < 2005) {\n    return algo2::compute(year);\n  }\n\n  if (year >= 2005 and year < 2024) {",
      "  if (year < 2005) {\n    return algo3::compute(year);\n  }\n\n  if (year >= 2005 and year < 2024) {",
    ),
    (
      "[[nodiscard]] constexpr auto compute(const double year) -> double {\n"
      "  if (not std::isfinite(year) or year >= 2035) {",
      "[[nodiscard]] constexpr auto compute(const double year) -> double {\n"
      "  if (year == 2006) { return 0; }\n"
      "  if (not std::isfinite(year) or year >= 2035) {",
    ),
    (
      "+ (116.17205714035308 * u) \n         - (1.1279910329686536 * std::pow(u, 2))",
      "- (1.1279910329686536 * std::pow(u, 2))\n         + (116.17205714035308 * u)",
    ),
  ],
  ids=["coefficient", "boundary", "delegation", "early-return", "expression-order"],
)
def test_algo4_function_is_pinned(tmp_path, old, new):
  astrotime_root = materialize_inputs(tmp_path)
  replace_once(tmp_path / "src" / "astro" / "delta_t.hpp", old, new)

  with pytest.raises(RuntimeError, match="algo4 function differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, astrotime_root=astrotime_root)


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    ("year >= 2005 and year < 2024", "year >= 2005 and year < 2025", "consumer contract differs"),
    ("return algo2::compute(year);", "return algo3::compute(year);", "consumer contract differs"),
    ("    throw std::out_of_range {", "    throw std::runtime_error {", "consumer contract differs"),
    (
      "+ (116.17205714035308 * u) \n         - (1.1279910329686536 * std::pow(u, 2))",
      "- (1.1279910329686536 * std::pow(u, 2))\n         + (116.17205714035308 * u)",
      "observed consumer expression differs",
    ),
    (
      "- (1.3053623848472002 * u) \n         + (0.14136771053009262 * std::pow(u, 2))",
      "+ (0.14136771053009262 * std::pow(u, 2))\n         - (1.3053623848472002 * u)",
      "prediction consumer expression differs",
    ),
    ("const double u = year - 1990;", "const double u = year - 19900;", "consumer basis differs"),
    ("const double u = year - 2020;", "const double u = year - 2021;", "consumer basis differs"),
    (
      "    return -1539.5103964825782",
      "    // -1539.5103964825782\n    return -1539.5103964825782",
      "consumer coefficient differs",
    ),
  ],
  ids=[
    "boundary",
    "delegation",
    "exception",
    "observed-expression-order",
    "prediction-expression-order",
    "observed-basis",
    "prediction-basis",
    "coefficient-count",
  ],
)
def test_algo4_semantic_bindings_survive_repin(tmp_path, old, new, message):
  astrotime_root = materialize_inputs(tmp_path)
  delta_t = tmp_path / "src" / "astro" / "delta_t.hpp"
  replace_once_in_algo4(delta_t, old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      astrotime_root=astrotime_root,
      algo4_function_sha256=algo4_function_digest(delta_t),
    )


def test_source_marking_must_be_immutable_and_scoped(tmp_path):
  astrotime_root = materialize_inputs(tmp_path)
  source = tmp_path / "src" / "astro" / "delta_t.hpp"
  text = source.read_text(encoding="utf-8")
  immutable = (
    "https://github.com/0xf3cd/AstroTime-Analysis/blob/"
    "55115f4bf59cbdc47970b7f2d69a9715a467a3e9/DeltaT/algo4/record.json"
  )
  floating = "https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/algo4/record.json"
  assert text.count(immutable) == 2
  text = text.replace(immutable, floating)
  text += f"\n// {immutable}\n// {immutable}\n"
  source.write_text(text, encoding="utf-8")

  with pytest.raises(RuntimeError, match="immutable source marking differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, astrotime_root=astrotime_root)


def test_source_marking_rejects_any_floating_astrotime_link(tmp_path):
  astrotime_root = materialize_inputs(tmp_path)
  source = tmp_path / "src" / "astro" / "delta_t.hpp"
  replace_once(
    source,
    "// Algo4 is a polynomial model fitting the ΔT values.",
    "// Algo4 is a polynomial model fitting the ΔT values.\n"
    "// https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/algo4/record.json",
  )

  with pytest.raises(RuntimeError, match="source marking floats"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, astrotime_root=astrotime_root)


def test_retained_record_inventory_is_closed(tmp_path):
  astrotime_root = materialize_inputs(tmp_path)
  (astrotime_root / "untracked.txt").write_text("untracked\n", encoding="utf-8")

  with pytest.raises(RuntimeError, match="record inventory differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, astrotime_root=astrotime_root)


def test_retained_record_directory_is_the_record_commit(tmp_path):
  astrotime_root = materialize_inputs(tmp_path)
  wrong_root = astrotime_root.with_name("0" * 40)
  astrotime_root.rename(wrong_root)

  with pytest.raises(RuntimeError, match="record commit path differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, astrotime_root=wrong_root)


@pytest.mark.parametrize("relative", ["GRANT.md", "record.json"])
def test_algo5_retained_bytes_are_pinned(tmp_path, relative):
  materialize_inputs(tmp_path)
  algo5_root = tmp_path / ASTROTIME_ALGO5_ROOT_RELATIVE
  path = algo5_root / relative
  path.write_bytes(path.read_bytes() + b"\n")

  with pytest.raises(RuntimeError, match="AstroTime algo5 .* hash mismatch"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path)


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    (
      lambda payload: payload["scope"].update({"consumer_symbol": "astro::delta_t::algo4::compute"}),
      "grant scope record differs",
    ),
    (
      lambda payload: payload["historical_generation"].update({"commit": "main"}),
      "historical generation identity differs",
    ),
    (
      lambda payload: payload["historical_generation"].update({"parent_data_commit": "main"}),
      "historical generation identity differs",
    ),
    (
      lambda payload: payload["historical_generation"]["script"].update({"sha256": "unknown"}),
      "script identity differs",
    ),
    (
      lambda payload: payload["historical_generation"]["inputs"]["iers_bulletin_a"].update({"raw_file_count": 1123}),
      "historical inputs differ",
    ),
    (
      lambda payload: payload["historical_generation"]["transformations"].pop(),
      "historical transformations differ",
    ),
    (
      lambda payload: payload["historical_generation"]["observed_input"].update({"deduplicated_rows": 7847}),
      "observed input differs",
    ),
    (
      lambda payload: payload["historical_generation"]["environment"].update(
        {"recorded": {"environment": "recovered"}}
      ),
      "environment boundary differs",
    ),
    (
      lambda payload: payload["third_party_inputs"]["corrected_lod_expression"].update({"lod": "1.78 * t"}),
      "third-party input differs",
    ),
    (
      lambda payload: payload["outputs"]["observed_segment"]["coefficients"].__setitem__(0, "-9963.5"),
      "generated outputs differ",
    ),
    (
      lambda payload: payload["outputs"].update({"last_observation_year": "2026.5"}),
      "generated outputs differ",
    ),
    (
      lambda payload: payload["outputs"].update({"fitted_integration_constant": "-150.6"}),
      "generated outputs differ",
    ),
    (
      lambda payload: payload["validation_relations"]["v25_2015_2026"]["source_medians"].__setitem__(0, "67.0"),
      "V25 relation differs",
    ),
    (
      lambda payload: payload["consumer_contract"].update({"default_model": False}),
      "consumer contract record differs",
    ),
    (
      lambda payload: payload["reproducibility"].update({"bit_for_bit_regeneration_environment": "recovered"}),
      "reproducibility boundary differs",
    ),
    (lambda payload: payload.update({"extra_section": {}}), "record schema inventory differs"),
  ],
  ids=[
    "scope",
    "generation-commit",
    "parent-data-commit",
    "script-hash",
    "input-count",
    "transformations",
    "observation-count",
    "environment",
    "upstream-lod",
    "coefficient",
    "boundary",
    "integration-constant",
    "v25",
    "default-model",
    "replay-claim",
    "top-level-key",
  ],
)
def test_algo5_semantic_record_fields_are_pinned(tmp_path, mutation, message):
  materialize_inputs(tmp_path)
  algo5_root = tmp_path / ASTROTIME_ALGO5_ROOT_RELATIVE
  record = algo5_root / "record.json"
  digest = mutate_record(record, mutation)

  with pytest.raises(RuntimeError, match=message):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      algo5_record_sha256=digest,
    )


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    (
      "[`0xf3cd/celestial-calendar`](https://github.com/0xf3cd/celestial-calendar) as project-authored",
      "[`example/other-project`](https://example.invalid/other-project) as unrelated",
      "as project-authored material",
    ),
    (
      "no separate attribution, notice, or other condition",
      "attribution is required",
      "no separate attribution",
    ),
    (
      "does not apply to IERS Bulletin A",
      "applies to IERS Bulletin A",
      "does not apply to IERS Bulletin A",
    ),
    ("`31.4115`", "31.4115", "grant exclusion differs"),
    (
      "It does not\nlicense AstroTime-Analysis as a whole.",
      "It licenses\nAstroTime-Analysis as a whole.",
      "does not license AstroTime-Analysis as a whole",
    ),
    (
      "Those materials remain subject to their respective source\nterms.",
      "Those materials remain subject to their respective source\nterms. Redistributors must add attribution.",
      "grant scope differs",
    ),
  ],
  ids=["target", "conditions", "bulletin-exclusion", "lod-token", "whole-repository", "added-condition"],
)
def test_algo5_grant_scope_is_pinned_after_rehash(tmp_path, old, new, message):
  materialize_inputs(tmp_path)
  algo5_root = tmp_path / ASTROTIME_ALGO5_ROOT_RELATIVE
  grant = algo5_root / "GRANT.md"
  replace_once(grant, old, new)
  digest = hashlib.sha256(grant.read_bytes()).hexdigest()

  with pytest.raises(RuntimeError, match=message):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      algo5_grant_sha256=digest,
    )


@pytest.mark.parametrize(
  ("old", "new"),
  [
    ("-9963.526300002133", "-9963.526300002134"),
    ("year <= LAST_OBSERVATION_YEAR", "year < LAST_OBSERVATION_YEAR"),
    ("return algo2::compute(year);", "return algo3::compute(year);"),
    ("-150.64706473230285", "-150.64706473230284"),
    ("year - 1825.0", "year - 1826.0"),
    (
      "+ (1251.8488008787037 * u)\n         - (84.95374332215921 * std::pow(u, 2))",
      "- (84.95374332215921 * std::pow(u, 2))\n         + (1251.8488008787037 * u)",
    ),
  ],
  ids=["coefficient", "boundary", "delegation", "integration-constant", "lod-epoch", "expression-order"],
)
def test_algo5_function_is_pinned(tmp_path, old, new):
  materialize_inputs(tmp_path)
  delta_t = tmp_path / "src" / "astro" / "delta_t.hpp"
  replace_once_in_algo5(delta_t, old, new)

  with pytest.raises(RuntimeError, match="algo5 function differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path)


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    ("year <= LAST_OBSERVATION_YEAR", "year < LAST_OBSERVATION_YEAR", "consumer contract differs"),
    (
      "+ (1251.8488008787037 * u)\n         - (84.95374332215921 * std::pow(u, 2))",
      "- (84.95374332215921 * std::pow(u, 2))\n         + (1251.8488008787037 * u)",
      "observed expression differs",
    ),
  ],
  ids=["boundary", "expression-order"],
)
def test_algo5_semantic_bindings_survive_repin(tmp_path, old, new, message):
  materialize_inputs(tmp_path)
  delta_t = tmp_path / "src" / "astro" / "delta_t.hpp"
  replace_once_in_algo5(delta_t, old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      algo5_function_sha256=algo5_function_digest(delta_t),
    )


def test_algo5_default_dispatch_is_pinned(tmp_path):
  materialize_inputs(tmp_path)
  delta_t = tmp_path / "src" / "astro" / "delta_t.hpp"
  replace_once(delta_t, "return algo5::compute(year);", "return algo4::compute(year);")

  with pytest.raises(RuntimeError, match="default Delta T function differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path)


def test_algo5_default_dispatch_survives_repin(tmp_path):
  materialize_inputs(tmp_path)
  delta_t = tmp_path / "src" / "astro" / "delta_t.hpp"
  replace_once(delta_t, "return algo5::compute(year);", "return algo4::compute(year);")

  with pytest.raises(RuntimeError, match="default dispatch differs"):
    verify_astrotime_delta_t_provenance(
      repo_root=tmp_path,
      default_function_sha256=default_function_digest(delta_t),
    )


def test_algo5_source_markings_must_be_immutable(tmp_path):
  materialize_inputs(tmp_path)
  delta_t = tmp_path / "src" / "astro" / "delta_t.hpp"
  text = delta_t.read_text(encoding="utf-8")
  assert text.count(ASTROTIME_ALGO5_RECORD_URL) == 2
  text = text.replace(
    ASTROTIME_ALGO5_RECORD_URL,
    "https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/algo5/record.json",
  )
  delta_t.write_text(text, encoding="utf-8")

  with pytest.raises(RuntimeError, match="algo5 source marking differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path)


def test_algo5_retained_record_inventory_is_closed(tmp_path):
  materialize_inputs(tmp_path)
  algo5_root = tmp_path / ASTROTIME_ALGO5_ROOT_RELATIVE
  (algo5_root / "untracked.txt").write_text("untracked\n", encoding="utf-8")

  with pytest.raises(RuntimeError, match="algo5 retained record inventory differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path)


def test_algo5_retained_directory_is_the_record_commit(tmp_path):
  materialize_inputs(tmp_path)
  algo5_root = tmp_path / ASTROTIME_ALGO5_ROOT_RELATIVE
  wrong_root = algo5_root.with_name("0" * 40)
  algo5_root.rename(wrong_root)

  with pytest.raises(RuntimeError, match="algo5 record commit path differs"):
    verify_astrotime_delta_t_provenance(repo_root=tmp_path, algo5_root=wrong_root)
