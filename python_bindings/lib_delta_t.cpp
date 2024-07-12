#include "delta_t.hpp"

extern "C" {

double delta_t_algo1(double year) {
  return calendar::delta_t::algo1::compute(year);
}

double delta_t_algo2(double year) {
  return calendar::delta_t::algo2::compute(year);
}

double delta_t_algo3(double year) {
  return calendar::delta_t::algo3::compute(year);
}

double delta_t_algo4(double year) {
  return calendar::delta_t::algo4::compute(year);
}

}
