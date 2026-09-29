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

sys.modules["planet_horizons_crawler"] = CRAWLER
PLUTO_MODULE_NAME = "pluto_horizons_crawler_test_subject"
PLUTO_MODULE_PATH = Path(__file__).resolve().parents[2] / "statistics" / "pluto_horizons_crawler.py"
PLUTO_SPEC = importlib.util.spec_from_file_location(PLUTO_MODULE_NAME, PLUTO_MODULE_PATH)
assert PLUTO_SPEC is not None and PLUTO_SPEC.loader is not None
PLUTO_CRAWLER = importlib.util.module_from_spec(PLUTO_SPEC)
sys.modules[PLUTO_MODULE_NAME] = PLUTO_CRAWLER
PLUTO_SPEC.loader.exec_module(PLUTO_CRAWLER)


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
Calendar mode   : Gregorian
Units conversion: 1 au= 149597870.700 km, c= 299792.458 km/s, 1 day= 86400.0 s
Rel. light bend : Sun, EARTH
Observer-centered IAU76/80 ecliptic-of-date longitude and latitude
with light-time, gravitational deflection of
light, and stellar aberrations.
Date__(TT)__HR:MN:SC.fff, Date_________JDTT, , , delta, deldot, S-O-T, ObsEcLon, ObsEcLat, phi, Illu%
$$SOE
2025-Jan-01, 2460676.500000000, , , 1.0, .01, 30, 10, 1, 60, 50
2025-Jan-02, 2460677.500000000, , , 1.1, .02, 31, 11, 2, 61, 51
$$EOE
"""
PLUTO_RESPONSE = VALID_RESPONSE.replace(
  "Target body name: Mercury (199) {source: DE441}",
  "Target body name: Pluto (999) {source: plu060_merged}",
)

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


def test_parse_horizons_response_pins_pluto_identity_and_quantity_31_semantics():
  pluto = CRAWLER.Target("PLUTO", "999", "Pluto", "plu060_merged", "DE441", "")
  rows = CRAWLER.parse_horizons_response(pluto, PLUTO_RESPONSE, REQUIRED_COLUMNS, EXPECTED_EPOCHS)

  assert tuple(row.jde for row in rows) == EXPECTED_EPOCHS
  with pytest.raises(RuntimeError, match="Pluto has an unexpected relativistic-light-bend identity"):
    CRAWLER.parse_horizons_response(
      pluto,
      PLUTO_RESPONSE.replace("Rel. light bend : Sun, EARTH", "Rel. light bend : Jupiter"),
      REQUIRED_COLUMNS,
      EXPECTED_EPOCHS,
    )
  with pytest.raises(RuntimeError, match="quantity-31 semantics changed for Pluto"):
    CRAWLER.parse_horizons_response(
      pluto,
      PLUTO_RESPONSE.replace("and stellar aberrations.", "without stellar aberration."),
      REQUIRED_COLUMNS,
      EXPECTED_EPOCHS,
    )


def test_parse_horizons_response_rejects_pluto_source_drift():
  pluto = CRAWLER.Target("PLUTO", "999", "Pluto", "plu060_merged", "DE441", "")
  with pytest.raises(RuntimeError, match="does not use plu060_merged"):
    CRAWLER.parse_horizons_response(
      pluto,
      PLUTO_RESPONSE.replace("plu060_merged", "DE441", 1),
      REQUIRED_COLUMNS,
      EXPECTED_EPOCHS,
    )


@pytest.mark.parametrize(
  ("old", "new", "message"),
  (
    ("Center-site name: GEOCENTRIC", "Center-site name: TOPOCENTRIC", "geocenter site"),
    ("Atmos refraction: NO (AIRLESS)", "Atmos refraction: YES", "applies atmospheric refraction"),
    ("Calendar mode   : Gregorian", "Calendar mode   : Mixed", "Gregorian calendar"),
    ("1 au= 149597870.700 km", "1 km= 1.000 km", "ranges in AU"),
    (
      "Rel. light bend : Sun, EARTH",
      "Rel. light bend : Jupiter",
      "Mercury has an unexpected relativistic-light-bend identity",
    ),
    (
      "and stellar aberrations.",
      "without stellar aberration.",
      "quantity-31 semantics changed for Mercury",
    ),
    ("$$SOE", "$$SOE\n$$SOE", "invalid data markers"),
    ("1.0, .01", "n.a., .01", "returned n.a."),
    ("2460677.500000000", "2460676.500000000", "not strictly increasing"),
    ("2460677.500000000", "2460677.500001000", "epochs differ"),
  ),
)
def test_parse_horizons_response_rejects_identity_drift(old, new, message):
  with pytest.raises(RuntimeError, match=message):
    parse_response(VALID_RESPONSE.replace(old, new, 1))


def test_horizons_request_pins_observer_conventions():
  params = CRAWLER.horizons_params(CRAWLER.TARGETS[0], EXPECTED_EPOCHS)

  assert params["APPARENT"] == "'AIRLESS'"
  assert params["REF_SYSTEM"] == "'ICRF'"
  assert params["CAL_TYPE"] == "'GREGORIAN'"
  assert params["RANGE_UNITS"] == "'AU'"
  assert params["TIME_TYPE"] == "'TT'"
  assert params["CENTER"] == "'500@399'"


def test_pluto_crawler_pins_sample_and_target_identity():
  epochs = PLUTO_CRAWLER.calibration_epochs()
  payload = ",".join(f"{epoch:.6f}" for epoch in epochs).encode()

  assert len(epochs) == 267
  assert epochs[0] == 2409543.5
  assert epochs[-1] == 2488068.5
  assert hashlib.sha256(payload).hexdigest() == "b098d9cc6a494f9fa61faa2410bd1f71a9cbb375fb9a08e9b6771835f9b4d3f0"
  assert set(PLUTO_CRAWLER.RETAINED_POSITION_EPOCHS) - set(epochs) == {2460697.020714}
  assert PLUTO_CRAWLER.SEED == 298
  assert PLUTO_CRAWLER.RANDOM_COUNT == 256
  assert PLUTO_CRAWLER.PLUTO.command == "999"
  assert PLUTO_CRAWLER.PLUTO.target_source == "plu060_merged"
  assert CRAWLER.horizons_params(PLUTO_CRAWLER.PLUTO, EXPECTED_EPOCHS)["COMMAND"] == "'999'"


@pytest.mark.parametrize(
  ("planet", "message"),
  (
    (
      CRAWLER.HorizonsRow(
        jde=2451545.0,
        date="2000-Jan-01",
        lon_deg=10.0,
        lat_deg=0.0,
        elongation_deg=20.0,
        illuminated_fraction=0.75,
        phase_angle_deg=60.0,
      ),
      "quantity 23 differs from quantity 31",
    ),
    (
      CRAWLER.HorizonsRow(
        jde=2451545.0,
        date="2000-Jan-01",
        lon_deg=10.0,
        lat_deg=0.0,
        elongation_deg=10.0,
        illuminated_fraction=0.5,
        phase_angle_deg=60.0,
      ),
      "quantity 10 differs from quantity 43",
    ),
  ),
)
def test_pluto_crawler_rejects_geometry_cross_check_drift(monkeypatch, planet, message):
  sun = CRAWLER.HorizonsRow(jde=2451545.0, date="2000-Jan-01", lon_deg=0.0, lat_deg=0.0)
  responses = iter(((planet,), (sun,)))
  monkeypatch.setattr(PLUTO_CRAWLER, "fetch_horizons", lambda *_args: next(responses))
  monkeypatch.setattr(PLUTO_CRAWLER.time, "sleep", lambda _seconds: None)

  with pytest.raises(RuntimeError, match=message):
    PLUTO_CRAWLER.retained_geometry()
