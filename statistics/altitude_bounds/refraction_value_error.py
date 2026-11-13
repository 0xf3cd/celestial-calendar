#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Complete-support Meeus (16.3)/(16.4) value errors and monotonicity bounds."""

import re
from fractions import Fraction as F

from .apparent_correction_error import positive_quotient
from .floating_operation_bounds import FloatingBound as B
from .poisson_majorants import pi_interval, stored
from .rational_intervals import PI, Interval as I, sincos
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded
from .rounded_light_time_reach import outward_binary64
from .source_reading import stored_constant


def weather_factor_error() -> B:
  """Bound P/1010 * 283/(273+T) for T in [-90, 60] C and P in [300, 1100] hPa."""
  denominator = B(F(273)) + B(F(90))
  temperature_factor = positive_quotient(B(F(283)), denominator, F(183))
  return (B(F(1100)) / 1010) * temperature_factor


def source_parameters(model: str, source: str) -> tuple:
  """Read stored coefficients only when the complete native expression matches the bounded operation order."""
  if model not in ("bennett", "saemundsson"):
    raise ValueError("Unknown refraction model")
  source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
  source = re.sub(r"\s+", "", source)
  parameter = "apparent_alt" if model == "bennett" else "true_alt"
  number = r"([0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)"
  pattern = (
    r"\[\[nodiscard\]\]inlineauto"
    + model
    + r"\(consttoolbox::AngleDeg&"
    + parameter
    + r"\)->toolbox::AngleDeg\{usingtoolbox::AngleDeg;constdoubleh_deg="
    + parameter
    + r"\.deg\(\);"
    + r"constdoubledenominator=h_deg\+"
    + number
    + r";constdoubleinner=h_deg\+\("
    + number
    + r"/denominator\);constdoubler_arcmin="
    + number
    + r"/std::tan\(AngleDeg\{inner\}\.rad\(\)\);returnAngleDeg::from_arcmin\(r_arcmin\);\}"
  )
  matches = re.findall(pattern, source)
  if len(matches) != 1:
    raise ValueError("Unsupported refraction source expression: " + model)
  offset, numerator, amplitude = matches[0]
  return stored(amplitude), stored(numerator), stored(offset), F(0) if model == "bennett" else F(-3, 2)


def refraction_map_interval(model: str, value: F, temperature: F, pressure: F, source: str) -> I:
  """Enclose the stored-coefficient map at a rational input by outward Taylor sine/cosine."""
  amplitude, numerator, offset, left = source_parameters(model, source)
  if not left <= value <= 90 or not -90 <= temperature <= 60 or not 300 <= pressure <= 1100:
    raise ValueError("The exact refraction map input leaves its support")
  phase = (I.enclose(value) + I.enclose(numerator) / (I.enclose(value) + offset)) * PI / 180
  sine, cosine = sincos(phase)
  correction = I.enclose(amplitude) * cosine / sine / 60 * F(pressure, 1010) * F(283, 273 + temperature)
  return I.enclose(value) - correction if model == "bennett" else I.enclose(value) + correction


def refraction_value_error(model: str, source: str) -> dict:
  """Bound the native formula and weather correction under the conditional finite-tangent error premise."""
  amplitude, numerator, offset, left = source_parameters(model, source)
  right = F(90)
  denominator_lower = left + offset
  inner_rate_lower = 1 - numerator / denominator_lower**2
  if denominator_lower <= 0 or inner_rate_lower <= 0:
    raise ValueError("The source phase is not certified increasing on its complete support")
  phase_lower = left + numerator / denominator_lower
  phase_upper = right + numerator / (right + offset)
  pi_lower, pi_upper = pi_interval()
  theta_lower, theta_upper = phase_lower * pi_lower / 180, phase_upper * pi_upper / 180
  if not 0 < theta_lower < 1 or theta_upper >= pi_lower - theta_lower:
    raise ValueError("The complete phase does not have the stated positive sine separation")
  sine_lower = I.enclose(theta_lower - theta_lower**3 / 6).lo
  denominator = B(right) + B(offset)
  inner = B(right) + positive_quotient(B(numerator), denominator, denominator_lower)
  angle = inner.degrees_to_radians()
  stored_sine_lower = sine_lower - angle.error
  if stored_sine_lower <= 0:
    raise ValueError("Radian argument rounding consumes the sine separation")

  # |tan_fl/tan-1| <= eps*(1+1/|tan|)+eta/|tan|, and |tan| >= |sin|.
  relative = LIBM_ALLOWANCE * (1 + 1 / stored_sine_lower) + SUBNORMAL_ALLOWANCE / stored_sine_lower
  if relative >= 1:
    raise ValueError("The reciprocal tangent has no positive computed denominator")
  quotient_magnitude = amplitude / (stored_sine_lower * (1 - relative))
  argument_error = amplitude * angle.error / stored_sine_lower**2
  tangent_error = amplitude / stored_sine_lower * relative / (1 - relative)
  magnitude, quotient_rounding = rounded(quotient_magnitude, F(0))
  native_arcminutes = B(magnitude, argument_error + tangent_error + quotient_rounding)
  native_degrees = native_arcminutes / 60
  weather = weather_factor_error()
  correction = native_degrees * weather
  mapped = B(right) + correction
  weather_upper = F(1100, 1010) * F(283, 183)
  derivative_upper = weather_upper * amplitude / 60 * pi_upper / 180 / sine_lower**2
  lower_slope = 1 if model == "bennett" else 1 - derivative_upper
  if lower_slope <= 0:
    raise ValueError("The apparent map is not certified strictly increasing")
  return {
    "model": model,
    "input_interval_degrees": (left, right),
    "source_constants": (amplitude, numerator, offset),
    "phase_interval_degrees": (phase_lower, phase_upper),
    "phase_derivative_lower": inner_rate_lower,
    "sine_source_lower": sine_lower,
    "radian_argument": angle,
    "stored_sine_lower": stored_sine_lower,
    "tangent_relative_error_upper": relative,
    "native_degrees": native_degrees,
    "weather_factor": weather,
    "corrected_degrees": correction,
    "map": mapped,
    "map_derivative_lower": I.enclose(lower_slope).lo,
  }


def derive(source: str, toolbox: str) -> dict:
  """Bind the operation/units premises to source, then serialize both inverse certificates outward."""
  environment = {}
  for name, expected in (("NATIVE_TEMPERATURE_C", 10), ("NATIVE_PRESSURE_HPA", 1010), ("NATIVE_TEMPERATURE_K", 283)):
    environment[name] = stored_constant(source, name, environment)
    if environment[name] != expected:
      raise ValueError("Unsupported refraction weather constant: " + name)
  compact = tuple(
    re.sub(r"\s+", "", re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.DOTALL)) for text in (source, toolbox)
  )
  required = (
    (0, "enumclassModel:uint8_t{BENNETT,SAEMUNDSSON};"),
    (
      0,
      "constdoublefactor=(params.pressure_hpa/NATIVE_PRESSURE_HPA)*(NATIVE_TEMPERATURE_K/(273.0+params.temperature_c));returnr_native*factor;",
    ),
    (
      0,
      "constboolsupported_weather=params.temperature_c>=-90.0andparams.temperature_c<=60.0andparams.pressure_hpa>=300.0andparams.pressure_hpa<=1100.0;",
    ),
    (1, "inlineconstexprdoubleDEG_PER_RAD=180.0/std::numbers::pi;"),
    (1, "inlineconstexpruint32_tMIN_PER_DEG=60;"),
    (1, "autodeg_to_rad(constdoubledeg)->double{returndeg/DEG_PER_RAD;}"),
    (1, "autoarcmin_to_deg(constdoublearcmin)->double{returnarcmin/MIN_PER_DEG;}"),
    (
      1,
      "autofrom_arcmin(constdoublevalue)->Angle<Unit>{returnAngle<Unit>{Angle<AngleUnit::DEG>{arcmin_to_deg(value)}};}",
    ),
    (1, "constexprexplicitAngle(constdoublevalue):_value{value}{}"),
    (1, "autooperator*(constdoubleother)const->Angle<Unit>{returnAngle<Unit>{_value*other};}"),
    (
      1,
      "autoas()const->double{ifconstexpr(Unit==As){return_value;}ifconstexpr(As==AngleUnit::DEG){returnrad_to_deg(_value);}else{returndeg_to_rad(_value);}}",
    ),
    (1, "autodeg()const->double{returnas<AngleUnit::DEG>();}"),
    (1, "autorad()const->double{returnas<AngleUnit::RAD>();}"),
    (1, "usingAngleDeg=Angle<AngleUnit::DEG>;"),
  )
  for index, expression in required:
    if compact[index].count(expression) != 1:
      raise ValueError("Unsupported refraction operation or unit conversion: " + expression)
  proofs = {model: refraction_value_error(model, source) for model in ("bennett", "saemundsson")}
  certificates = []
  for model, proof in proofs.items():
    residual = proof["map"].error + UNIT_ROUNDOFF * (proof["map"].magnitude + 90) + SUBNORMAL_ALLOWANCE
    certificates.append(
      {
        "model": model,
        "map_error": outward_binary64(proof["map"].error, True),
        "residual_error": outward_binary64(residual, True),
        "slope_lower": outward_binary64(proof["map_derivative_lower"], False),
      }
    )
  return {"proofs": proofs, "certificates": certificates}
