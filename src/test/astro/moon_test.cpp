#include <gtest/gtest.h>
#include <chrono>
#include <unordered_map>
#include "util.hpp"
#include "astro.hpp"
#include "ymd.hpp"


namespace astro::moon::test {

using namespace astro::moon::geocentric_coord;


TEST(Moon, CoordAndPpi) {
  const std::unordered_map<double, std::tuple<double, double, double, double>> dataset {
    // JDE                  Longitude in deg     Latitude in deg      Radius in km          PPI in rad
    { 2455820.8787544146, {  36.726233329779895,   3.323172151719455,   405517.1384250576,  0.015729059017928735 } },
    {  2454305.403776242, {  228.53476952571248,  -4.898482383763094,    402649.731243161,   0.01584108039265102 } },       
    { 2453630.7615909837, {   343.0133818145305,  -2.682728118171436,  360802.15515138546,  0.017678586866276418 } },       
    { 2460787.5640272405, {   314.3452325680488,  -3.523168644615433,    380300.872532407,  0.016772086266495054 } },       
    {  2453920.978856268, {  201.02573730370074, -1.9331436884333921,  400130.03411087435,  0.015940843200019608 } },       
    { 2455033.0523539423, {   95.87461078862165,  2.1875042138644933,   359055.5318601611,  0.017764593262308823 } },       
    {  2455639.162253953, {  161.99120758906975,  -4.804833995445399,  357866.65220724087,  0.017823615795964403 } },       
    { 2460553.0908111953, {  118.43798960297818,   4.853252601045634,  394761.90397688467,   0.01615763189585266 } },       
    { 2456910.9583296073, {   7.041846959694951,  1.1282296348070076,   363236.5699360123,   0.01756009259597654 } },       
    { 2456354.0557590206, {  221.43774484937796, -0.6773561466785337,   372719.8423006021,   0.01711325936434138 } },       
    { 2456262.4702930865, {   94.63018828142246,  -3.297235121702206,    404855.041253622,  0.015754784340452296 } },       
    { 2459968.3639730234, {   331.9534232156953,  -4.615340256483026,   360452.5610748505,  0.017695734682067827 } },       
    { 2455248.6117273094, {    51.1773018633265,   4.434630488622709,   385800.7365308566,  0.016532966298430228 } },       
    {  2457316.173043621, {   294.6752617743573,   4.795290266491586,   380651.2106566481,  0.016756648376509708 } },       
    { 2452115.5234712195, {  180.85351543519127,   5.208934571611883,  366864.74260398035,  0.017386411418318606 } },       
    {  2452291.998539657, {  340.90970228066413,   -5.00121628636893,   405103.2172864411,  0.015745131780450883 } },       
    { 2461056.0768461325, {   258.9820037447788,  -5.062443918225046,    403934.312838539,  0.015790698798732045 } },       
    {  2455044.746044929, {  256.98302103663036,  -3.631360754522632,  402771.55140728096,   0.01583628878220182 } },       
    { 2454494.7564110793, {  208.28090482670223,  -4.574583707646692,   402089.7557796162,    0.0158631435261455 } },       
    {  2461422.982340823, {   47.59806660964453,   5.241808616859706,  372837.88691330294,  0.017107840587759347 } },
    {  2454701.751006527, {  50.679550456501175,   5.265450936332354,   371276.7709013708,  0.017179781383094415 } },
    {  2457469.060913669, {  160.02808987984963, -1.0854447468074448,   400923.8554201004,  0.015909277979770563 } },       
    { 2456276.6501769833, {   288.7483315552447,   4.177244639004016,  361668.50401068764,  0.017636234785769294 } },       
    {  2460023.685719419, {  338.18384006737267,  -4.308720062252872,  362923.69988531014,  0.017575232396904034 } },       
    {  2456669.529024311, {   66.18028413515557, -2.6342501055554854,   400939.4248340616,  0.015908660133237804 } },       
    {  2459725.925246734, {   19.48072787262379, -2.8845065474685128,  392494.79881497874,  0.016250968843356913 } },       
    { 2459521.6298222425, {   196.6579853192581,  3.7324788496706987,   365800.6317095171,  0.017436993458875703 } },       
    {   2457665.56476052, {  224.75420192148908,   4.613246023213594,  406024.83417547937,   0.01570938969444856 } },       
    {  2453906.926883445, {  22.961665485757518,  1.9574685218627927,   372785.9927473855,    0.0171102223396932 } },       
    { 2456700.6844779793, {   112.6591963702925,  -4.975589967800055,  406234.32422928634,   0.01570128788907858 } },       
    {  2452406.927536222, {  51.272942630415244, -2.3466150618166184,  396794.34981297236,  0.016074862703574735 } },       
    {  2457012.604115998, {  256.42692706563315,    4.35598309007935,    373203.466864144,   0.01709108058037593 } },       
    {  2456262.648551683, {    96.7518148364324,  -3.447633230189316,   404588.1500932373,   0.01576517802338375 } },       
    {  2453824.663742186, {   19.53130746718017,   1.388875659641162,   362286.3459306062,   0.01760615489934374 } },       
    {  2453291.036365227, {  179.62354727172087,   2.836703812342603,  382155.14496467466,  0.016690698084464335 } },       
    {  2453853.157831423, {  35.021336573462804,  2.6807452043216395,   367180.5645534421,  0.017371455389816458 } },       
    { 2455876.6858620266, {   52.14926977672405,  1.9801551957976204,  403832.57703632396,  0.015794677212282955 } },       
    {  2458183.763278761, {   219.1678653365976,  5.1931965938319955,   389093.9173576854,   0.01639302336007676 } },       
    { 2457570.5920485347, {  54.202136453979264,  -4.729897887573367,  366000.51250161475,    0.0174274697753436 } },
    {  2455625.981486084, {  351.24671272883677,   4.950033060679646,   406338.5842145311,  0.015697258858625933 } },
    { 2454601.7878137575, {   181.6026588443318,  -3.117497968644145,   396749.0435609729,   0.01607669851021385 } },       
    { 2459055.8928899015, {  184.50012336716898,   5.162489211924248,   368371.5120083447,  0.017315287740813208 } },       
    { 2451753.5675923247, {   82.66418840816155, -2.8049629190243004,    362791.597172753,  0.017581632696978357 } },       
    {   2453238.89733255, {  212.75459724845305, 0.13429379090921348,  378415.05002711655,   0.01685567746108276 } },       
    {  2452357.719895975, {   120.0259010414709,  3.1487457040151297,   370926.5688905136,  0.017196002887031794 } },       
    { 2459833.5887556397, {   357.2559293416962, -3.7577257055362008,  371702.47013963416,   0.01716010398304715 } },       
    {  2453367.752164485, {  114.45999491218349,  5.0014196028011275,  406418.72324364254,  0.015694163365444488 } },       
    {  2457443.366960138, {  179.05185843751482,  0.6726063973802492,  403051.65681776765,   0.01582528225099609 } },
  };

  for (const auto& [jde, expected] : dataset) {
    const auto& [λ, β, r] = apparent(jde);
    ASSERT_NEAR(λ.deg(),    std::get<0>(expected), 5e-7);
    ASSERT_NEAR(β.deg(),    std::get<1>(expected), 1e-11);
    ASSERT_NEAR(r.km(),     std::get<2>(expected), 1e-7);

    const auto ppi = equatorial_horizontal_parallax(r);
    ASSERT_NEAR(ppi.rad(),  std::get<3>(expected), 1e-14);
  }
}


TEST(Moon, Perturbation) {
  const std::unordered_map<double, std::tuple<double, double, double, double, double>> dataset {
    //JDE                   L expected            B expected          R expected           L perturbation       B perturbation
    { 2455157.8937397725, { -166691.76694696903,   961656.2178595652,  19612316.117669746,   832.4541805169106,  2363.0903304329554 } },
    {  2459024.303594928, { -4806951.8574562045,  3009081.9442531434,  -5839030.928844989,  3814.3547596030153,   -1998.21433929537 } },
    { 2454247.1712815687, {  2626369.4992469847,  -1994740.663008926,   19642498.37699763,  2203.4378066262357, -417.19114883240485 } },
    {  2458839.103782548, {  2571216.4321470065,   4877522.683768281, -12994778.491087278,   4164.101875812495,    784.524258564774 } },
    { 2455501.7090088516, {  -2803510.749636401,  -4245896.845846018, -17198227.500535592,  1131.8962744498722, -1521.4602271089614 } },
    {  2455256.449243678, {  2009366.2278057267, -4139760.6640153807,  -26113258.34509538,  1171.4691729516767, -1120.2670449184654 } },
    { 2457468.1410294734, {   4861625.052117809, -2071079.8835240002,     13464109.861983,   2853.457205200449, -1282.1050449138295 } },
    {   2452288.38119678, {  3737035.3705983423, -2622288.4151483527,  12186624.286768897,   5614.706534673221,  1902.1097937029206 } },
    { 2452080.7912898213, {  -5191543.926597807, -2103445.9129129797, -13569087.620115682,   5032.215759115408, -1809.5734262588974 } },
    {  2452644.532226886, {   5370606.041199202,  -4633521.881452868, -12497.779769977953,   5409.527013893643,   1768.677714711384 } },
    {  2458259.017873469, {  3029766.4278208106,  -41679.83629669532, -17166397.601741042,   3602.166791274663, -1830.6888529796986 } },
    { 2457418.6749046203, {  -2021059.175047659,   3170402.894250499,   19151678.44684718,  3014.7473571246733,  1014.0994246287117 } },
    {  2452346.264476558, {  1530540.1060659501,  -4816876.349202436,  21159319.346729293,  5532.8192031764265,  1125.2339756838182 } },
    { 2459541.9776189183, { -3913644.1598386546,   3691148.448942259,   19053779.20201317,  3558.7382895353453,   -2197.68272449847 } },
    { 2453410.2091475064, {  1768042.3044085996,   -4683563.98135823,  -25859137.51279563,   4285.369683076962,  1885.4953004027404 } },
    { 2452475.7448283276, {  4544770.7679507295,   953432.0983215941,  -7641696.469358849,    5355.90327376738,  1630.7386555527228 } },
    {  2455077.938787213, {  -2475755.787814365,  2391207.2543210653,  16539609.484173795,  1029.3709929361617,   1810.048358776741 } },
    { 2457640.7095056064, {  -4003911.775861074,   5283780.508430435,  16469475.420263646,    3328.42931637934,  2095.0353449525555 } },
    {  2461424.889284013, {  -4606221.920990586,   4787548.428431114,  -21940919.61483755,    752.912417389734,  -2200.669467718235 } },
    { 2460860.6486004125, {   2042855.019289483, -2983890.1443937714,    19076461.0633581,  1195.5300201894242,   547.5535106756491 } },
    { 2455350.4299401254, {   315800.5976898682,    3240258.21531578,  18897074.404015295,   707.4946308683161,  1933.4141721222506 } },
    { 2454329.4304830423, {  2103549.2666715994,  -2808992.681861885,  16840892.849777278,  2052.6990115431977, -223.87969848008044 } },
    { 2459268.0857312633, {  -5508979.001978031,  1611137.0965442187,    8838694.17883955,  3773.3071123954364, -2172.4524065460805 } },
    {  2457770.720007621, {   5971755.150505925,  2033021.3173364685,    5919327.78208087,   3372.299771273303, -311.34694823683793 } },
    { 2460609.7660753243, {   5994701.352241374,  3463098.5665131337,  14854981.764277285,  1909.4933394672337, -1736.8744781999194 } },
    {   2456337.79013492, {   5746799.829294766,   2889540.024031854,   4712040.242431984,   896.6483228144891, -419.76895868249284 } },
    { 2452976.9697959237, {   5874397.247726285, -3252040.2779016527,  14892185.737734796,   4998.467998604463,  130.47521781902475 } },
    {  2454988.902283522, {  4091923.9918090394,  -4469346.788118098,  14578953.459156282,   999.6696678932049,   1896.735546877169 } },
    {  2456955.412237598, {   -4857116.14931663,   2141524.521106418,  3026596.2576380456,  2215.2274947641586,  1472.0915525681883 } },
    {   2452415.19656138, {  -3333050.490795251,   5216289.562152856, -15634063.401547931,   5010.762649141517, -1014.9822039446442 } },
    { 2456389.9819942624, {    4928640.47413689,   4704676.772596339,  -2707062.095431847,   900.6479357316118,   916.6083537726427 } },
    {  2457892.835551289, {   -7689218.79801686,   -308054.763218238,   2317773.942702131,  3377.9460094646497,  1155.1509610777596 } },
    { 2458851.4771838924, { -1745187.7853275626,  -5284403.894843757,  19027322.876700174,   4228.880886415653,  -7.088469513493621 } },
    { 2454961.3233272787, {   3708508.716796902,  -4599800.980984988,  12088232.416508183,  1036.7981174853853,  1789.9708400510356 } },
    { 2460648.7740771538, {  -5219327.667087505,  -4886832.343919548,  1500773.2676442624,   1859.509087600623,   2382.809332305931 } },
    { 2452728.0325773926, {    5311262.87131708,   -5099360.05473127,  11225849.482212087,   5335.320908812326,   1356.164170140813 } },
    { 2460801.9257716807, {   6216479.848314902,  1670755.9512177613,  12225303.214631999,  1511.3028894933423, -1382.0784027525403 } },
    {  2453869.542432924, {  -5197925.322454052,  -4383730.085274373,  1640828.2608112136,   2953.576062735212,   1742.336451746418 } },
    { 2455826.5883640694, {  -7501804.298158116, -2434529.3419414405, -1615890.0120027564,  1173.1727402466556, -2522.6092639625913 } },
    {  2456585.023210534, {   5089112.633138301,   457696.8113389099,   2812810.866505511,  1181.7824267259139,  -1156.500816595624 } },
  };

  for (const auto& [jde, expected] : dataset) {
    const auto& [_, __, ___, per_l, per_b] = expected;
    const auto jc = julian_day::jde_to_jc(jde);

    const auto evaluated = evaluate(jc);
    const auto lon = perturbation::longitude(evaluated.ctx);
    const auto lat = perturbation::latitude(evaluated.ctx);

    ASSERT_NEAR(lon, per_l, 1e-9);
    ASSERT_NEAR(lat, per_b, 1e-8);
  }
}

TEST(Moon, DiffTest1) {
  // Compare our results with other data sources.
  // The distance/radius seems problematic, so we only test longitude and latitude here.
  // https://doncarona.tamu.edu/cgi-bin/moon?current=0&jd=
  const std::unordered_map<double, std::tuple<double, double>> dataset {{
    {  2454988.902283522, { 240.636, -4.429 } },
    { 2456389.9819942624, { 342.630,  4.682 } },
    {  2457770.720007621, { 176.765,  2.035 } },
    { 2455501.7090088516, { 150.642, -4.330 } },
    {  2456585.023210534, {  32.738,  0.370 } },
    {   2452288.38119678, { 297.123, -2.699 } },
    {        2460526.159, { 122.450,  4.597 } },
  }};

  for (const auto& [jde, expected] : dataset) {
    const auto& [lon, lat] = expected;
    const auto coord = apparent(jde);
    ASSERT_NEAR(coord.λ.deg(), lon, 0.1);
    ASSERT_NEAR(coord.β.deg(), lat, 0.1);
  }
}

TEST(Moon, DiffTest2) {
  using namespace std::chrono_literals;
  using hms = std::chrono::hh_mm_ss<std::chrono::nanoseconds>;

  // Compare our results with other data sources.
  // https://www.timeanddate.com/astronomy/moon/distance.html
  // San Jose, CA time is used, because I live in San Jose :)
  // Timezone is PDT.
  const std::unordered_map<calendar::Datetime, double> dataset {{
    { calendar::Datetime { util::to_ymd(2024,  1, 13), hms {  2h + 34min } }, 362267.0 },
    { calendar::Datetime { util::to_ymd(2024,  2, 10), hms { 10h + 52min } }, 358088.0 },
    { calendar::Datetime { util::to_ymd(2024,  3,  9), hms { 23h +  4min } }, 356895.0 },
    { calendar::Datetime { util::to_ymd(2024,  4,  7), hms { 10h + 50min } }, 358850.0 },
    { calendar::Datetime { util::to_ymd(2024,  5,  5), hms { 15h +  4min } }, 363163.0 },
    { calendar::Datetime { util::to_ymd(2024,  6,  2), hms {  0h + 16min } }, 368102.0 },
    { calendar::Datetime { util::to_ymd(2024,  6, 27), hms {  4h + 30min } }, 369286.0 },
    { calendar::Datetime { util::to_ymd(2024,  7, 23), hms { 22h + 41min } }, 364917.0 },
    { calendar::Datetime { util::to_ymd(2024,  8, 20), hms { 22h +  2min } }, 360196.0 },
    { calendar::Datetime { util::to_ymd(2024,  9, 18), hms {  6h + 23min } }, 357286.0 },
    { calendar::Datetime { util::to_ymd(2024, 10, 16), hms { 17h + 51min } }, 357175.0 },
    { calendar::Datetime { util::to_ymd(2024, 11, 14), hms {  3h + 14min } }, 360109.0 },
    { calendar::Datetime { util::to_ymd(2024, 12, 12), hms {  5h + 20min } }, 365361.0 },

    { calendar::Datetime { util::to_ymd(2025,  1, 20), hms { 20h + 54min } }, 404298.0 },
    { calendar::Datetime { util::to_ymd(2025,  2, 17), hms { 17h + 10min } }, 404882.0 },
    { calendar::Datetime { util::to_ymd(2025,  3, 17), hms {  9h + 36min } }, 405754.0 },
    { calendar::Datetime { util::to_ymd(2025,  4, 13), hms { 15h + 48min } }, 406295.0 },
    { calendar::Datetime { util::to_ymd(2025,  5, 10), hms { 17h + 47min } }, 406244.0 },
    { calendar::Datetime { util::to_ymd(2025,  6,  7), hms {  3h + 43min } }, 405554.0 },
    { calendar::Datetime { util::to_ymd(2025,  7,  4), hms { 19h + 29min } }, 404627.0 },
    { calendar::Datetime { util::to_ymd(2025,  8,  1), hms { 13h + 36min } }, 404161.0 },
    { calendar::Datetime { util::to_ymd(2025,  8, 29), hms {  8h + 33min } }, 404548.0 },
    { calendar::Datetime { util::to_ymd(2025,  9, 26), hms {  2h + 46min } }, 405548.0 },
    { calendar::Datetime { util::to_ymd(2025, 10, 23), hms { 16h + 30min } }, 406444.0 },
    { calendar::Datetime { util::to_ymd(2025, 11, 19), hms { 18h + 48min } }, 406691.0 },
    { calendar::Datetime { util::to_ymd(2025, 12, 16), hms { 22h +  9min } }, 406322.0 },
  }};

  for (const auto& [dt, expected] : dataset) {
    // Convert to UTC from PDT. 
    // Difference between UTC and UT1 is ignored.
    const double jde = astro::julian_day::ut1_to_jde(dt) + 7.0 / 24.0; 
    const auto coord = apparent(jde);
    const auto distance = coord.r.km();
    ASSERT_NEAR(distance, expected, 5.0);
  }
}

TEST(Moon, RandomApparentPosition) {
  // In this test, random apparent positions are generated.
  // Ensure the longitude and latitude are within the expected range.
  for (int i = 0; i < 100; ++i) {
    const auto jde = astro::julian_day::J2000 + util::random(-365250.0 * 5, 365250.0 * 5);
    const auto coord = apparent(jde);

    const auto lon = coord.λ.deg();
    const auto lat = coord.β.deg();

    ASSERT_TRUE(lon >= 0.0 and lon <= 360.0);
    ASSERT_TRUE(lat >= -90.0 and lat <= 90.0);
  }
}

} // namespace astro::moon::test
