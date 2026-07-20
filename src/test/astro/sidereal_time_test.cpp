/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "random.hpp"
#include "toolbox.hpp"
#include "julian_day.hpp"
#include "earth.hpp"
#include "sidereal_time.hpp"


namespace astro::sidereal::test {

using namespace astro::sidereal;
using namespace astro::toolbox;
using namespace astro::toolbox::literals;
using astro::toolbox::AngleUnit::DEG;

/** @brief The absolute angular difference |a − b|, accounting for the 0°/360° wrap, in [0°, 180°]. */
inline auto ang_diff(const double a, const double b) -> double {
  return std::fabs(std::fmod(a - b + 540.0, 360.0) - 180.0);
}

TEST(SiderealTime, GreenwichMeanMeeus12a) {
  // Meeus Example 12.a: 1987 April 10, 0h UT, JD = 2446895.5 → GMST = 197.693195°.
  // This is a 0h UT case, where the completed (12.4) form coincides with the bare (12.3) polynomial.
  ASSERT_NEAR(greenwich_mean(2446895.5).deg(), 197.693195, 5e-7);
}

TEST(SiderealTime, GreenwichMeanJ2000) {
  // At J2000.0 (2000-01-01 12h UT) GMST = 280.46061837° (= 18h41m50.548s); the polynomial's
  // constant 100.46061837° is the same angle offset by 180°, since J2000.0 falls at noon UT.
  ASSERT_NEAR(greenwich_mean(astro::julian_day::J2000).deg(), 280.46061837, 1e-9);
}

TEST(SiderealTime, GreenwichMeanArbitraryInstant) {
  // PyMeeus doctest value: 1987 April 10, 19h21m UT → mean sidereal time 0.357605204 d = 128.73787344°.
  // An arbitrary (non-0h) instant: the bare (12.3) polynomial would be off by ~360°×0.80625 ≈ 290°.
  constexpr double jd_ut1 = 2446895.5 + (19.0 + 21.0 / 60.0) / 24.0;
  ASSERT_NEAR(greenwich_mean(jd_ut1).deg(), 0.357605204 * 360.0, 5e-7);
}

TEST(SiderealTime, GreenwichMeanDailyRate) {
  // One UT1 day advances GMST by the mean sidereal rate (toolbox::SIDEREAL_RATE_DEG_PER_DAY);
  // the T²/T³ terms wobble the per-day advance by ≲ 5e-8° around it.
  for (auto i = 0; i < 100; ++i) {
    const double jd_ut1 = util::random(2378495.5, 2488069.5); // ~1800–2100
    const double advance = greenwich_mean(jd_ut1 + 1.0).deg() - greenwich_mean(jd_ut1).deg();
    ASSERT_NEAR(ang_diff(advance, 0.0), SIDEREAL_RATE_DEG_PER_DAY - 360.0, 1e-6);
  }
}

TEST(SiderealTime, GreenwichApparentMeeus12a) {
  // Same instant as Example 12.a (1987 April 10, 0h). The book takes Δψ = −3".788 and
  // ε = 23°26'36".85 from Example 22.a (evaluated at 0h TD):
  //   GAST = 197.693195° + (−3.788/3600)°·cos(23.443569°) ≈ 197.6922296°.
  constexpr double jd_ut1 = 2446895.5;
  constexpr double jde_tt = 2446895.5; // The example evaluates nutation at 0h TD of the same date.

  constexpr double ε = 23.0 + 26.0 / 60.0 + 36.85 / 3600.0;
  const double expected = normalize_deg(197.693195 + (-3.788 / 3600.0) * std::cos(deg_to_rad(ε)));
  ASSERT_NEAR(greenwich_apparent(jd_ut1, jde_tt).deg(), expected, 2e-6);

  // Cross-check with PyMeeus's doctest value (its nutation table = Meeus 22.A, Δψ = −3".788 there):
  // apparent_sidereal_time = 0.54914508 d = 197.6922288° (input rounding dominates the tolerance).
  using astro::earth::nutation::Model;
  ASSERT_NEAR(greenwich_apparent(jd_ut1, jde_tt, Model::MEEUS).deg(), 0.54914508 * 360.0, 2e-6);
}

TEST(SiderealTime, GreenwichApparentComposition) {
  // GAST must equal GMST + Δψ·cos ε with the nutation terms taken at jde_tt (TT scale),
  // and the correction (equation of the equinoxes) must stay small: |Δψ·cos ε| < ~17".4.
  for (auto i = 0; i < 100; ++i) {
    const double jd_ut1 = util::random(2378495.5, 2488069.5); // ~1800–2100
    const double jde_tt = jd_ut1 + util::random(30.0, 100.0) / 86400.0; // realistic ΔT

    const auto Δψ = astro::earth::nutation::longitude(jde_tt);
    const auto ε  = astro::earth::obliquity::true_obliquity(jde_tt);
    const double expected = normalize_deg(greenwich_mean(jd_ut1).deg() + Δψ.deg() * std::cos(ε.rad()));

    const auto gast = greenwich_apparent(jd_ut1, jde_tt);
    ASSERT_NEAR(gast.deg(), expected, 1e-12);
    ASSERT_LT(ang_diff(gast.deg(), greenwich_mean(jd_ut1).deg()), 18.0 / 3600.0);
  }
}

TEST(SiderealTime, LocalApparentMeeus13b) {
  // Meeus Example 13.b (Washington USNO, 1987 April 10, 19h21m UT): with α = 23h09m16.641s
  // (apparent RA of Venus) and H = +64°.352133, the local apparent sidereal time is
  // θ = H + α (mod 360°). Meeus's longitude is west-positive: lon = +77°03'56".
  constexpr double jd_ut1 = 2446895.5 + (19.0 + 21.0 / 60.0) / 24.0;
  constexpr double jde_tt = jd_ut1; // ΔT ≈ 55 s in 1987; immaterial at the example's precision.
  constexpr double lon = 77.0 + 3.0 / 60.0 + 56.0 / 3600.0;

  constexpr double α_hours = 23.0 + 9.0 / 60.0 + 16.641 / 3600.0;
  const double expected = normalize_deg(64.352133 + α_hours * 15.0); // std::remainder is not constexpr until C++26.

  ASSERT_NEAR(local_apparent(jd_ut1, jde_tt, Angle<DEG> { lon }).deg(), expected, 2e-4);
}

TEST(SiderealTime, LocalApparentLongitude) {
  // θ_local = θ_Greenwich − longitude, with longitude measured positive west (Meeus).
  for (auto i = 0; i < 100; ++i) {
    const double jd_ut1 = util::random(2378495.5, 2488069.5);
    const double jde_tt = jd_ut1 + util::random(30.0, 100.0) / 86400.0;
    const double lon = util::random(-180.0, 180.0); // east-positive sites are negative here

    const double gast = greenwich_apparent(jd_ut1, jde_tt).deg();
    const double last = local_apparent(jd_ut1, jde_tt, Angle<DEG> { lon }).deg();

    ASSERT_NEAR(ang_diff(last, gast - lon), 0.0, 1e-12);
  }
}

TEST(SiderealTime, GreenwichMeanPymeeus) {
  // The following data was collected from running PyMeeus's `mean_sidereal_time` (see README
  // §8. References), over random jd_ut1 ∈ ~[1800, 2100] including fractional-day instants
  // (seed 42). PyMeeus uses the (12.2) 0h polynomial + the 1.00273790935 ratio; agreement with
  // our (12.4) form is ≲ 3e-7° on this dataset.
  const std::vector<std::tuple<double, double>> dataset {
    { 2448560.052014224, 237.07962100837617 },
    { 2381236.0284927683, 110.86686023100371 },
    { 2408631.5625309777, 305.45568624249495 },
    { 2402953.5934219193, 120.10127812945018 },
    { 2459193.5968208076, 114.13543831873227 },
    { 2452644.169634878, 344.92283058638947 },
    { 2476255.183951691, 222.21113703057364 },
    { 2388021.7356465356, 213.7563368378096 },
    { 2424727.161470194, 185.6581809309286 },
    { 2381760.500522707, 77.74209181799588 },
    { 2402452.53745113, 326.08731020819505 },
    { 2433869.300338638, 246.5758004327127 },
    { 2381403.1523421397, 320.17780910702726 },
    { 2400282.936736339, 131.3688183188355 },
    { 2449705.9373852536, 245.25208799735788 },
    { 2438206.9177956167, 64.22149605120717 },
    { 2402650.0607194873, 349.15220248933167 },
    { 2443063.6980450186, 92.18391138089514 },
    { 2467188.032860016, 230.80443448491786 },
    { 2379207.595092964, 115.52297657987198 },
    { 2466792.338700328, 310.8920369102628 },
    { 2454993.42606644, 232.7766104738009 },
    { 2415778.1100969426, 346.5550565546765 },
    { 2395532.0107123763, 155.2626171272041 },
    { 2483381.165173986, 39.15590442550161 },
    { 2415377.510686171, 95.9174328908052 },
    { 2388658.033042536, 227.9837261686917 },
    { 2389093.10027515, 321.0103233050094 },
    { 2471358.8476981567, 315.0988686334411 },
    { 2444648.176160996, 26.04233805958163 },
    { 2466935.773415767, 248.76564577576397 },
    { 2458455.1307951882, 298.49911899793665 },
    { 2437252.1568930573, 289.23901330559386 },
    { 2485123.6867222753, 144.4250965083102 },
    { 2419973.025848228, 130.9330991882332 },
    { 2438984.8001311324, 68.5799511229426 },
    { 2469376.6866808576, 103.4208329827829 },
    { 2446269.18334556, 186.37023760687003 },
    { 2472916.1718946532, 166.78211875400126 },
    { 2441758.2839643643, 96.4369322713874 },
    { 2455698.254381414, 145.68217500019284 },
    { 2383516.6610146854, 66.47409731080214 },
    { 2403467.2256562426, 133.96587909671 },
    { 2410204.8967237375, 176.51774461761562 },
    { 2387238.6260794294, 122.44232537139733 },
    { 2404003.3285821234, 339.4276182361858 },
    { 2389562.6306261416, 254.72801709086463 },
    { 2408954.1795871854, 125.58256478871564 },
    { 2448149.9872958274, 169.60178920166328 },
    { 2418471.621178468, 305.39645540210245 },
    { 2419057.7092908653, 194.79311158829216 },
    { 2401452.023389755, 234.8711945361934 },
    { 2407749.3278732095, 71.40664509573156 },
    { 2481128.489794009, 95.67551706405541 },
    { 2449503.3293030104, 186.64235800813213 },
    { 2445240.4208149547, 337.86214433610974 },
    { 2397247.846237658, 347.2604299708843 },
    { 2458388.8397586117, 128.3863655846731 },
    { 2396400.1648514695, 346.446503318044 },
    { 2420073.9505751524, 203.31098140430962 },
  };

  for (const auto& [jd_ut1, expected] : dataset) {
    ASSERT_NEAR(ang_diff(greenwich_mean(jd_ut1).deg(), expected), 0.0, 1e-6);
  }
}

TEST(SiderealTime, GreenwichApparentPymeeus) {
  // The following data was collected from PyMeeus's `apparent_sidereal_time`, with ε and Δψ
  // from PyMeeus's `true_obliquity` / `nutation_longitude` (Meeus Table 22.A) evaluated at
  // jde_tt; jd_ut1 random in ~[1800, 2100], jde_tt = jd_ut1 + ΔT with ΔT ∈ [30, 100] s
  // (seed 42). Our Model::MEEUS uses the same 22.A table, hence the tight tolerance.
  const std::vector<std::tuple<double, double, double>> dataset {
    { 2486921.5316226543, 2486921.532488395, 60.626669081473125 },
    { 2439522.711224363, 2439522.71212625, 206.76161033564242 },
    { 2470850.156302878, 2470850.157278804, 284.8060419180626 },
    { 2403593.2134373947, 2403593.213810624, 253.74402633160088 },
    { 2413060.952288026, 2413060.952852168, 131.58447313022415 },
    { 2401613.7341031283, 2401613.735214282, 290.1215906112437 },
    { 2474522.6063031163, 2474522.6069052857, 106.54758429711947 },
    { 2450314.536311021, 2450314.5369787784, 340.7303251534932 },
    { 2478706.1375982305, 2478706.138317208, 101.29607861387483 },
    { 2407519.479363858, 2407519.479910894, 259.39753652652064 },
    { 2440006.851932793, 2440006.8524928847, 14.609837105960763 },
    { 2442550.925292754, 2442550.9263673793, 28.583637967228125 },
    { 2422259.4109502537, 2422259.411475166, 3.142545231329787 },
    { 2487799.685694095, 2487799.686454128, 261.64576448231117 },
    { 2388456.807929531, 2388456.8083149265, 308.60904403585 },
    { 2390510.193809043, 2390510.194664613, 311.4326928420828 },
    { 2465286.8042707075, 2465286.8049599575, 74.56763644349837 },
    { 2385456.4848738946, 2385456.485530299, 115.04406818925108 },
    { 2487644.5041184286, 2487644.5048943316, 43.32349265082103 },
    { 2484900.4421486347, 2484900.443193248, 196.3362578935932 },
    { 2379753.5214983625, 2379753.522429503, 267.1221007246069 },
    { 2453193.231975718, 2453193.2327579856, 188.54498295403272 },
    { 2407732.60336386, 2407732.6042303797, 154.10328317823704 },
    { 2390718.7178695947, 2390718.718569057, 345.62572841459104 },
    { 2428211.8213973166, 2428211.8225173065, 257.88234980062276 },
    { 2474466.2100890004, 2474466.2106496166, 268.32362319676525 },
    { 2433346.7227513734, 2433346.7232433367, 243.56669550691922 },
    { 2478495.7828683695, 2478495.783920873, 126.25830652483232 },
    { 2411197.2895781924, 2411197.290443082, 216.0907055977978 },
    { 2445222.8019481264, 2445222.802419177, 97.69981499901986 },
    { 2462046.8584074345, 2462046.859191654, 140.62037831997947 },
    { 2463812.717769465, 2463812.718546372, 30.502830455399067 },
    { 2378558.1649463233, 2378558.165556172, 40.55913575720798 },
    { 2380629.6445701853, 2380629.6456701495, 94.97791774347135 },
    { 2474780.5710405977, 2474780.572061623, 348.11687306138316 },
    { 2412191.0527768712, 2412191.0531710237, 30.34294101401779 },
    { 2474702.5238231835, 2474702.52493761, 254.19075697650985 },
    { 2387880.891356886, 2387880.8920978503, 130.99367300671693 },
    { 2386079.392498655, 2386079.3934621057, 335.75820471813404 },
    { 2462411.0417568837, 2462411.0422081267, 205.58283924763822 },
    { 2430574.0912977904, 2430574.0920904553, 163.40546405564433 },
    { 2407538.815059478, 2407538.8161135325, 39.30643943896154 },
    { 2424860.416659572, 2424860.41717839, 48.86453423003922 },
    { 2437588.3296319204, 2437588.330570522, 322.76987016830407 },
    { 2400536.4266198627, 2400536.427219633, 197.5816538619671 },
    { 2487537.995606761, 2487537.996480505, 115.27960674118484 },
    { 2426499.878594848, 2426499.8793614022, 31.09607215730198 },
    { 2391754.4137580693, 2391754.414287338, 176.97705754612036 },
    { 2415540.8873867453, 2415540.8882106068, 32.564794895330806 },
    { 2403710.0917095374, 2403710.0922351764, 325.12265404078465 },
    { 2386274.4964063535, 2386274.497264886, 205.4692331002345 },
    { 2403581.5670193234, 2403581.5681001036, 9.554375423034847 },
    { 2472689.189347404, 2472689.189752034, 309.33566943590006 },
    { 2404574.6198063483, 2404574.6206955663, 287.3554416982365 },
    { 2401970.2839308083, 2401970.2843852276, 119.49130809475248 },
    { 2481003.5373973865, 2481003.5382072595, 349.6536303048488 },
    { 2430287.9550370886, 2430287.956019998, 192.32263315760306 },
    { 2466976.176033282, 2466976.176534772, 73.52864921114777 },
    { 2389116.597038309, 2389116.597734763, 163.00571915607256 },
    { 2424908.704038785, 2424908.7047643834, 199.91572913617026 },
  };

  using astro::earth::nutation::Model;
  for (const auto& [jd_ut1, jde_tt, expected] : dataset) {
    ASSERT_NEAR(ang_diff(greenwich_apparent(jd_ut1, jde_tt, Model::MEEUS).deg(), expected), 0.0, 1e-6);
  }
}

TEST(SiderealTime, GreenwichUsno) {
  // Data collected from the USNO Sidereal Time API (aa.usno.navy.mil/api/siderealtime) on
  // 2026-07-19: 40 random UT1 instants in 2025–2027 (the API's allowed window), at lon = 0.
  // jde_tt uses a fixed ΔT = 69.184 s (TT − UT1 for that window; DUT1 < 1 s is immaterial here).
  // USNO uses the modern IAU 2000/2006 models; on this dataset our GMST (IAU 1982 polynomial)
  // and GAST (IAU 1980 nutation) were observed to agree with USNO to ≲ 0.07". The tolerances
  // (0.2" / 0.25") leave ~3× margin while staying ~1000× tighter than the ~0.29° error a
  // UT1/TT mixup would produce.
  constexpr double TOL_GMST = 5.56e-5; // 0.2" in degrees
  constexpr double TOL_GAST = 6.94e-5; // 0.25" in degrees

  const std::vector<std::tuple<double, double, double, double>> dataset {
    { 2461428.1534375, 2461428.154238241, 357.00228625, 357.00534999999996 },
    { 2461438.902615741, 2461438.903416482, 277.30135208333337, 277.3044208333333 },
    { 2461079.6915856483, 2461079.6923863892, 207.27511166666665, 207.2768795833333 },
    { 2461622.921875, 2461622.922675741, 105.61278458333332, 105.61635541666666 },
    { 2461261.3744675927, 2461261.3752683336, 272.18786666666665, 272.19026875 },
    { 2460914.3613310186, 2460914.3621317595, 285.4261145833333, 285.42714249999995 },
    { 2461439.1521296296, 2461439.1529303705, 7.372285, 7.37536 },
    { 2461409.4263541666, 2461409.4271549075, 76.79398583333332, 76.79671083333334 },
    { 2460748.985625, 2460748.986425741, 347.1698183333333, 347.1701175 },
    { 2460858.66462963, 2460858.6654303707, 339.71630749999997, 339.7170433333333 },
    { 2460866.4265046297, 2460866.4273053706, 261.64177916666665, 261.64261125 },
    { 2461175.196574074, 2461175.197374815, 123.20518583333333, 123.20663124999999 },
    { 2461671.218391204, 2461671.2191919447, 259.9619520833333, 259.96525375 },
    { 2460775.7176967594, 2460775.7184975003, 277.06404791666665, 277.0641870833333 },
    { 2460973.0421875, 2460973.0429882407, 228.37307958333335, 228.37374125 },
    { 2461558.550138889, 2461558.55093963, 268.3399520833333, 268.34274208333335 },
    { 2461312.031111111, 2461312.031911852, 198.50912083333333, 198.51122958333332 },
    { 2460922.3513310184, 2460922.3521317593, 289.70143708333336, 289.70248541666666 },
    { 2460952.1751967594, 2460952.1759975003, 255.68891875, 255.68976791666668 },
    { 2460861.441574074, 2460861.442374815, 262.1533954166666, 262.15410375 },
    { 2461515.094502315, 2461515.095303056, 61.47885166666667, 61.481521666666666 },
    { 2461451.157361111, 2461451.158161852, 21.088542916666665, 21.09155125 },
    { 2461660.4346875, 2461660.435488241, 327.19968958333334, 327.20317916666664 },
    { 2461002.814178241, 2461002.814978982, 175.6344308333333, 175.63528624999998 },
    { 2461227.171678241, 2461227.172478982, 165.47181041666667, 165.47395166666666 },
    { 2461264.4290046296, 2461264.4298053705, 294.83189625, 294.8344020833333 },
    { 2460691.1211458333, 2460691.121946574, 338.92334666666665, 338.92365041666665 },
    { 2461440.0376273147, 2461440.0384280556, 327.02423999999996, 327.02733666666666 },
    { 2461531.4332407406, 2461531.4340414815, 199.52891916666667, 199.5315175 },
    { 2460777.7849768517, 2460777.7857775926, 303.32249041666665, 303.3225583333333 },
    { 2461306.8025115742, 2461306.803312315, 111.05973208333333, 111.06200625000001 },
    { 2461650.167847222, 2461650.168647963, 221.01770541666664, 221.02123041666664 },
    { 2461547.6035763887, 2461547.6043771296, 276.78800125, 276.7907633333333 },
    { 2460800.389699074, 2460800.390499815, 183.30277541666666, 183.30294874999998 },
    { 2461653.772337963, 2461653.773138704, 82.18712875, 82.19056125 },
    { 2461518.8428703705, 2461518.8436711114, 334.5859208333333, 334.5885616666667 },
    { 2461737.7639699075, 2461737.7647706484, 161.96076, 161.963985 },
    { 2461188.5507870372, 2461188.551587778, 263.8843975, 263.8859954166666 },
    { 2461613.776666667, 2461613.7774674078, 44.32383416666667, 44.327245000000005 },
    { 2460851.6308101853, 2460851.6316109262, 320.6084416666667, 320.60908291666664 },
  };

  for (const auto& [jd_ut1, jde_tt, expected_gmst, expected_gast] : dataset) {
    ASSERT_NEAR(ang_diff(greenwich_mean(jd_ut1).deg(), expected_gmst), 0.0, TOL_GMST);
    ASSERT_NEAR(ang_diff(greenwich_apparent(jd_ut1, jde_tt).deg(), expected_gast), 0.0, TOL_GAST);
  }
}

TEST(SiderealTime, LocalApparentUsno) {
  // Same source and date as above: 20 random instants at random longitudes. USNO's longitude
  // is east-positive (LAST = GAST + lon_east); the dataset stores west-positive values
  // (lon_west = −lon_east), matching `local_apparent`'s convention. USNO evaluated LAST at
  // longitudes quantized to 4 decimal places, so this column stores those quantized values —
  // with full-precision longitudes the residuals reach 0.22" (89% of tolerance); quantized,
  // they stay ≤ 0.07", leaving ~4× margin against the IAU-model gap.
  constexpr double TOL_LAST = 6.94e-5; // 0.25" in degrees

  const std::vector<std::tuple<double, double, double, double>> dataset {
    { 2461123.9418287035, 2461123.9426294444, -74.0403, 55.0196375 },
    { 2461409.5752314813, 2461409.5760322222, -137.9183, 268.45758875 },
    { 2460921.334583333, 2460921.335384074, -29.7034, 312.3745458333333 },
    { 2460861.5418171296, 2460861.5426178705, 48.7255, 249.61490833333335 },
    { 2460834.1820486113, 2460834.1828493522, -60.0960, 201.9522795833333 },
    { 2461311.9063657406, 2461311.9071664815, -43.4381, 196.91804375 },
    { 2460793.662488426, 2460793.663289167, -107.9593, 22.835579583333335 },
    { 2460857.679351852, 2460857.6801525927, -152.8154, 136.86132375 },
    { 2461373.2350462964, 2461373.2358470373, 90.6598, 241.59378666666672 },
    { 2461013.8825347223, 2461013.8833354632, 166.0591, 45.09409958333334 },
    { 2461138.316574074, 2461138.317374815, 14.2908, 115.76520708333334 },
    { 2461138.5236574076, 2461138.5244581485, 110.4733, 94.33682166666667 },
    { 2461188.569409722, 2461188.570210463, -98.3976, 9.0061175 },
    { 2461361.8996643517, 2461361.9004650926, -173.2139, 13.557140833333335 },
    { 2461557.615335648, 2461557.616136389, -169.3442, 100.23639166666668 },
    { 2460957.538252315, 2460957.539053056, -34.7657, 66.44140166666668 },
    { 2461400.8136689817, 2461400.8144697226, -38.2352, 245.9761595833333 },
    { 2461450.3994907406, 2461450.4002914815, 111.5716, 355.93961375 },
    { 2461023.501689815, 2461023.502490556, -153.2071, 236.73741625 },
    { 2461734.4402430556, 2461734.4410437965, -85.3336, 127.47985083333333 },
  };

  for (const auto& [jd_ut1, jde_tt, lon_west, expected_last] : dataset) {
    ASSERT_NEAR(ang_diff(local_apparent(jd_ut1, jde_tt, Angle<DEG> { lon_west }).deg(), expected_last), 0.0, TOL_LAST);
  }
}

} // namespace astro::sidereal::test
