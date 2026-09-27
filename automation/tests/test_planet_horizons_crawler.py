# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import importlib.util
import sys

from pathlib import Path

import pytest


MODULE_NAME = "planet_horizons_crawler_test_subject"
MODULE_PATH = Path(__file__).resolve().parents[2] / "statistics" / "planet_horizons_crawler.py"
SPEC = importlib.util.spec_from_file_location(MODULE_NAME, MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
CRAWLER = importlib.util.module_from_spec(SPEC)
sys.modules[MODULE_NAME] = CRAWLER
SPEC.loader.exec_module(CRAWLER)


class Response:
  text = "mock response"

  @staticmethod
  def raise_for_status() -> None:
    pass


VALID_RESPONSE = """API VERSION: 1.2
Target body name: Mercury (199) {source: DE441}
Center body name: Earth (399) {source: DE441}
Center-site name: GEOCENTRIC
Atmos refraction: NO (AIRLESS)
Date__(TT)__HR:MN:SC.fff, Date_________JDTT, , , delta, deldot, S-O-T, ObsEcLon, ObsEcLat, phi, Illu%
$$SOE
2025-Jan-01, 2460676.500000000, , , 1.0, .01, 30, 10, 1, 60, 50
2025-Jan-02, 2460677.500000000, , , 1.1, .02, 31, 11, 2, 61, 51
$$EOE
"""

REQUIRED_COLUMNS = ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat", "phi", "Illu%")
EXPECTED_EPOCHS = (2460676.5, 2460677.5)


def parse_response(text=VALID_RESPONSE):
  return CRAWLER.parse_horizons_response(
    CRAWLER.TARGETS[0],
    text,
    REQUIRED_COLUMNS,
    EXPECTED_EPOCHS,
  )


def test_fetch_horizons_quantizes_requested_and_expected_epochs(monkeypatch):
  captured = {}

  def get(url, *, params, timeout):
    captured["url"] = url
    captured["params"] = params
    captured["timeout"] = timeout
    return Response()

  def parse(target, text, required_columns, expected_epochs):
    captured["target"] = target
    captured["text"] = text
    captured["required_columns"] = required_columns
    captured["expected_epochs"] = expected_epochs
    return ()

  monkeypatch.setattr(CRAWLER.requests, "get", get)
  monkeypatch.setattr(CRAWLER, "parse_horizons_response", parse)

  target = CRAWLER.TARGETS[0]
  epochs = (2460676.500800741, 2460677.123456789)
  assert CRAWLER.fetch_horizons(target, epochs, "31") == ()
  assert captured == {
    "url": CRAWLER.HORIZONS_URL,
    "params": CRAWLER.horizons_params(target, (2460676.500801, 2460677.123457), "31"),
    "timeout": 60,
    "target": target,
    "text": "mock response",
    "required_columns": ("ObsEcLon", "ObsEcLat"),
    "expected_epochs": (2460676.500801, 2460677.123457),
  }


def test_parse_horizons_response_accepts_pinned_shape():
  rows = parse_response()

  assert tuple(row.jde for row in rows) == EXPECTED_EPOCHS
  assert rows[0].lon_deg == 10.0
  assert rows[0].illuminated_fraction == 0.5


@pytest.mark.parametrize(
  ("old", "new", "message"),
  (
    ("Center-site name: GEOCENTRIC", "Center-site name: TOPOCENTRIC", "geocenter site"),
    ("Atmos refraction: NO (AIRLESS)", "Atmos refraction: YES", "applies atmospheric refraction"),
    ("$$SOE", "$$SOE\n$$SOE", "invalid data markers"),
    ("1.0, .01", "n.a., .01", "returned n.a."),
    ("2460677.500000000", "2460676.500000000", "not strictly increasing"),
    ("2460677.500000000", "2460677.500001000", "epochs differ"),
  ),
)
def test_parse_horizons_response_rejects_identity_drift(old, new, message):
  with pytest.raises(RuntimeError, match=message):
    parse_response(VALID_RESPONSE.replace(old, new, 1))
