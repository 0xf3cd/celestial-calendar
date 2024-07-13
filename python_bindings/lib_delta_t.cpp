/**
 * ChineseCalendar: A C++ library that deals with conversions between calendar systems.
 * Copyright (C) 2024 Ningqi Wang (0xf3cd) <https://github.com/0xf3cd>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "delta_t.hpp"

extern "C" {

double delta_t_algo1(double year) {
  return astro::delta_t::algo1::compute(year);
}

double delta_t_algo2(double year) {
  return astro::delta_t::algo2::compute(year);
}

double delta_t_algo3(double year) {
  return astro::delta_t::algo3::compute(year);
}

double delta_t_algo4(double year) {
  return astro::delta_t::algo4::compute(year);
}

}
