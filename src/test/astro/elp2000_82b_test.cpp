#include <gtest/gtest.h>
#include "julian_day.hpp"
#include "elp2000_82b.hpp"

namespace astro::elp2000_82b::test {

using namespace astro::elp2000_82b::coeff;
using namespace astro::elp2000_82b;

TEST(Elp2000, Evaluate) {
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
    const auto& [Σl, Σb, Σr, per_l, per_b] = expected;
    const auto jc = julian_day::jde_to_jc(jde);

    const auto evaluated = evaluate(jc);
    ASSERT_NEAR(evaluated.Σl, Σl, 4e-5);
    ASSERT_NEAR(evaluated.Σb, Σb, 5e-6);
    ASSERT_NEAR(evaluated.Σr, Σr, 2e-4);

    ASSERT_NEAR(evaluated.perturbation_l, per_l, 1e-9);
    ASSERT_NEAR(evaluated.perturbation_b, per_b, 1e-8);
  }
}


} // namespace astro::elp2000_82b::test