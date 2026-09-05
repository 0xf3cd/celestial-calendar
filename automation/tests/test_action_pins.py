# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

from automation.action_pins import check_action_pins


SHA = "0123456789abcdef0123456789abcdef01234567"


def write_workflow(directory, uses, suffix=""):
  workflow = directory / "check.yml"
  workflow.write_text(
    f"jobs:\n  check:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: {uses}\n{suffix}",
    encoding="utf-8",
  )
  return workflow


def test_repository_action_pins_satisfy_policy():
  assert check_action_pins() == 0


def test_action_pin_policy_accepts_first_party_major_third_party_sha_and_local(tmp_path):
  write_workflow(
    tmp_path,
    "actions/checkout@v7",
    suffix=f"      - uses: owner/action@{SHA} # v2\n      - uses: ./.github/actions/local\n",
  )

  assert check_action_pins(tmp_path) == 0


def test_action_pin_policy_ignores_uses_text_inside_prompt(tmp_path):
  write_workflow(
    tmp_path,
    "actions/checkout@v7",
    suffix="      - run: true\n        env:\n          PROMPT: |\n            uses: owner/action@main\n",
  )

  assert check_action_pins(tmp_path) == 0


def test_action_pin_policy_rejects_empty_workflow_directory(tmp_path, capsys):
  assert check_action_pins(tmp_path) == 1
  assert "no workflow files found" in capsys.readouterr().out


def test_third_party_action_requires_full_sha_and_reports_location(tmp_path, capsys):
  write_workflow(tmp_path, "owner/action@v2")

  assert check_action_pins(tmp_path) == 1
  output = capsys.readouterr().out
  assert "check.yml:5" in output
  assert "owner/action@v2" in output
  assert "40-character commit SHA" in output


def test_third_party_sha_requires_provenance_comment(tmp_path, capsys):
  write_workflow(tmp_path, f"owner/action@{SHA}")

  assert check_action_pins(tmp_path) == 1
  assert "needs a trailing release/tag provenance comment" in capsys.readouterr().out


def test_hash_inside_another_scalar_is_not_provenance(tmp_path, capsys):
  workflow = tmp_path / "check.yml"
  workflow.write_text(
    f'jobs:\n  check: {{uses: owner/workflow@{SHA}, note: "not a # provenance comment"}}\n',
    encoding="utf-8",
  )

  assert check_action_pins(tmp_path) == 1
  assert "needs a trailing release/tag provenance comment" in capsys.readouterr().out


def test_flow_style_action_accepts_trailing_provenance_comment(tmp_path):
  workflow = tmp_path / "check.yml"
  workflow.write_text(
    f"jobs:\n  check: {{uses: owner/workflow@{SHA}, name: reusable}} # v2\n",
    encoding="utf-8",
  )

  assert check_action_pins(tmp_path) == 0


def test_multiline_flow_style_action_rejects_comment_below_uses_line(tmp_path, capsys):
  workflow = tmp_path / "check.yml"
  workflow.write_text(
    f"jobs:\n  check: {{uses: owner/workflow@{SHA},\n          name: reusable}} # v2\n",
    encoding="utf-8",
  )

  assert check_action_pins(tmp_path) == 1
  assert "rewrite it as plain `uses: <target>@<SHA> # v2`" in capsys.readouterr().out


def test_block_scalar_action_rejects_with_plain_uses_remedy(tmp_path, capsys):
  workflow = tmp_path / "check.yml"
  workflow.write_text(
    f"jobs:\n  check:\n    uses: >-\n      owner/workflow@{SHA}\n",
    encoding="utf-8",
  )

  assert check_action_pins(tmp_path) == 1
  assert "rewrite it as plain `uses: <target>@<SHA> # v2`" in capsys.readouterr().out


def test_nested_flow_style_action_accepts_trailing_provenance_comment(tmp_path):
  workflow = tmp_path / "check.yml"
  workflow.write_text(
    f"jobs:\n  check:\n    steps:\n      - [{{uses: owner/action@{SHA}}}] # v2\n",
    encoding="utf-8",
  )

  assert check_action_pins(tmp_path) == 0


def test_first_party_action_rejects_floating_branch(tmp_path, capsys):
  write_workflow(tmp_path, "actions/checkout@main")

  assert check_action_pins(tmp_path) == 1
  assert "major tag or commit SHA" in capsys.readouterr().out
