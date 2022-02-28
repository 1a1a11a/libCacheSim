/** set the size parameter for cphy traces */

#include "../cachesim.h"

/** set size for each trace, generated using 
l = [638.8431, 19.5432, 1787.2432, 1662.5424, 1436.9431, 21.8565, 136.6533, 1013.4951, 1055.9987, 1459.7551, 75.7049, 7.6171, 189.5669, 705.3907, 62.4017, 86.6782, 44.6283, 69.2342, 36.0243, 1068.3415, 373.4571, 66.4570, 72.8716, 93.8472, 150.0806, 141.5784, 45.6178, 213.2740, 46.9392, 28.3889, 2614.3673, 44.5799, 8.4681, 205.2573, 52.0625, 7.7662, 46.5561, 27.4660, 5.7171, 6.5314, 9.3146, 57.6910, 119.9959, 17.7650, 2.6995, 1240.8514, 2.9347, 205.4541, 75.3536, 46.2845, 21.9158, 7.3060, 557.2461, 2577.9289, 38.3816, 13.8987, 14.5310, 37.0809, 10.3049, 32.7899, 28.9899, 36.8996, 58.0903, 396.7586, 54.5818, 2.7536, 5.7741, 20.6736, 31.4905, 32.1536, 6.8732, 6.9478, 6.7424, 4.4165, 18.7578, 8.2181, 17.6128, 34.4226, 61.2372, 4.5749, 19.1657, 2.4704, 19.7912, 20.1857, 22.2916, 82.1314, 32.9536, 7.5746, 21.8123, 16.2720, 15.7123, 21.7219, 31.2150, 52.1063, 8.2447, 16.2719, 50.8166, 49.9185, 18.8420, 50.2313, 50.1638, 7.6564, 7.6328, 46.1833, 17.2826, 4.3986, ]
for i in range(1, 107):
    print(r'  }' + ' else if (strstr(args->trace_path, "w{:0>2}.") != NULL)'.format(i) 
        + '{\n    ' + 'working_set_size_mb = {} * 1000.0;\n    '.format(l[i-1]) 
        + "    find_data = true;\n" 
        + 'INFO("use cphy w{:0>2} cache size'.format(i) + r'\n"' + ');'
    )
*/
static inline bool set_cphy_size(sim_arg_t *args) {
  // set size to 0.05%, 0.1%, 0.5%, 1%, 5%, 10%, 20% of working set size
  double working_set_size_mb;
  bool find_data = false;
  if (strstr(args->trace_path, "w01.") != NULL) {
    working_set_size_mb = 638.8431 * 1000.0;
    find_data = true;
    INFO("use cphy w01 cache size\n");
  } else if (strstr(args->trace_path, "w02.") != NULL) {
    working_set_size_mb = 19.5432 * 1000.0;
    find_data = true;
    INFO("use cphy w02 cache size\n");
  } else if (strstr(args->trace_path, "w03.") != NULL) {
    working_set_size_mb = 1787.2432 * 1000.0;
    find_data = true;
    INFO("use cphy w03 cache size\n");
  } else if (strstr(args->trace_path, "w04.") != NULL) {
    working_set_size_mb = 1662.5424 * 1000.0;
    find_data = true;
    INFO("use cphy w04 cache size\n");
  } else if (strstr(args->trace_path, "w05.") != NULL) {
    working_set_size_mb = 1436.9431 * 1000.0;
    find_data = true;
    INFO("use cphy w05 cache size\n");
  } else if (strstr(args->trace_path, "w06.") != NULL) {
    working_set_size_mb = 21.8565 * 1000.0;
    find_data = true;
    INFO("use cphy w06 cache size\n");
  } else if (strstr(args->trace_path, "w07.") != NULL) {
    working_set_size_mb = 136.6533 * 1000.0;
    find_data = true;
    INFO("use cphy w07 cache size\n");
  } else if (strstr(args->trace_path, "w08.") != NULL) {
    working_set_size_mb = 1013.4951 * 1000.0;
    find_data = true;
    INFO("use cphy w08 cache size\n");
  } else if (strstr(args->trace_path, "w09.") != NULL) {
    working_set_size_mb = 1055.9987 * 1000.0;
    find_data = true;
    INFO("use cphy w09 cache size\n");
  } else if (strstr(args->trace_path, "w10.") != NULL) {
    working_set_size_mb = 1459.7551 * 1000.0;
    find_data = true;
    INFO("use cphy w10 cache size\n");
  } else if (strstr(args->trace_path, "w11.") != NULL) {
    working_set_size_mb = 75.7049 * 1000.0;
    find_data = true;
    INFO("use cphy w11 cache size\n");
  } else if (strstr(args->trace_path, "w12.") != NULL) {
    working_set_size_mb = 7.6171 * 1000.0;
    find_data = true;
    INFO("use cphy w12 cache size\n");
  } else if (strstr(args->trace_path, "w13.") != NULL) {
    working_set_size_mb = 189.5669 * 1000.0;
    find_data = true;
    INFO("use cphy w13 cache size\n");
  } else if (strstr(args->trace_path, "w14.") != NULL) {
    working_set_size_mb = 705.3907 * 1000.0;
    find_data = true;
    INFO("use cphy w14 cache size\n");
  } else if (strstr(args->trace_path, "w15.") != NULL) {
    working_set_size_mb = 62.4017 * 1000.0;
    find_data = true;
    INFO("use cphy w15 cache size\n");
  } else if (strstr(args->trace_path, "w16.") != NULL) {
    working_set_size_mb = 86.6782 * 1000.0;
    find_data = true;
    INFO("use cphy w16 cache size\n");
  } else if (strstr(args->trace_path, "w17.") != NULL) {
    working_set_size_mb = 44.6283 * 1000.0;
    find_data = true;
    INFO("use cphy w17 cache size\n");
  } else if (strstr(args->trace_path, "w18.") != NULL) {
    working_set_size_mb = 69.2342 * 1000.0;
    find_data = true;
    INFO("use cphy w18 cache size\n");
  } else if (strstr(args->trace_path, "w19.") != NULL) {
    working_set_size_mb = 36.0243 * 1000.0;
    find_data = true;
    INFO("use cphy w19 cache size\n");
  } else if (strstr(args->trace_path, "w20.") != NULL) {
    working_set_size_mb = 1068.3415 * 1000.0;
    find_data = true;
    INFO("use cphy w20 cache size\n");
  } else if (strstr(args->trace_path, "w21.") != NULL) {
    working_set_size_mb = 373.4571 * 1000.0;
    find_data = true;
    INFO("use cphy w21 cache size\n");
  } else if (strstr(args->trace_path, "w22.") != NULL) {
    working_set_size_mb = 66.457 * 1000.0;
    find_data = true;
    INFO("use cphy w22 cache size\n");
  } else if (strstr(args->trace_path, "w23.") != NULL) {
    working_set_size_mb = 72.8716 * 1000.0;
    find_data = true;
    INFO("use cphy w23 cache size\n");
  } else if (strstr(args->trace_path, "w24.") != NULL) {
    working_set_size_mb = 93.8472 * 1000.0;
    find_data = true;
    INFO("use cphy w24 cache size\n");
  } else if (strstr(args->trace_path, "w25.") != NULL) {
    working_set_size_mb = 150.0806 * 1000.0;
    find_data = true;
    INFO("use cphy w25 cache size\n");
  } else if (strstr(args->trace_path, "w26.") != NULL) {
    working_set_size_mb = 141.5784 * 1000.0;
    find_data = true;
    INFO("use cphy w26 cache size\n");
  } else if (strstr(args->trace_path, "w27.") != NULL) {
    working_set_size_mb = 45.6178 * 1000.0;
    find_data = true;
    INFO("use cphy w27 cache size\n");
  } else if (strstr(args->trace_path, "w28.") != NULL) {
    working_set_size_mb = 213.274 * 1000.0;
    find_data = true;
    INFO("use cphy w28 cache size\n");
  } else if (strstr(args->trace_path, "w29.") != NULL) {
    working_set_size_mb = 46.9392 * 1000.0;
    find_data = true;
    INFO("use cphy w29 cache size\n");
  } else if (strstr(args->trace_path, "w30.") != NULL) {
    working_set_size_mb = 28.3889 * 1000.0;
    find_data = true;
    INFO("use cphy w30 cache size\n");
  } else if (strstr(args->trace_path, "w31.") != NULL) {
    working_set_size_mb = 2614.3673 * 1000.0;
    find_data = true;
    INFO("use cphy w31 cache size\n");
  } else if (strstr(args->trace_path, "w32.") != NULL) {
    working_set_size_mb = 44.5799 * 1000.0;
    find_data = true;
    INFO("use cphy w32 cache size\n");
  } else if (strstr(args->trace_path, "w33.") != NULL) {
    working_set_size_mb = 8.4681 * 1000.0;
    find_data = true;
    INFO("use cphy w33 cache size\n");
  } else if (strstr(args->trace_path, "w34.") != NULL) {
    working_set_size_mb = 205.2573 * 1000.0;
    find_data = true;
    INFO("use cphy w34 cache size\n");
  } else if (strstr(args->trace_path, "w35.") != NULL) {
    working_set_size_mb = 52.0625 * 1000.0;
    find_data = true;
    INFO("use cphy w35 cache size\n");
  } else if (strstr(args->trace_path, "w36.") != NULL) {
    working_set_size_mb = 7.7662 * 1000.0;
    find_data = true;
    INFO("use cphy w36 cache size\n");
  } else if (strstr(args->trace_path, "w37.") != NULL) {
    working_set_size_mb = 46.5561 * 1000.0;
    find_data = true;
    INFO("use cphy w37 cache size\n");
  } else if (strstr(args->trace_path, "w38.") != NULL) {
    working_set_size_mb = 27.466 * 1000.0;
    find_data = true;
    INFO("use cphy w38 cache size\n");
  } else if (strstr(args->trace_path, "w39.") != NULL) {
    working_set_size_mb = 5.7171 * 1000.0;
    find_data = true;
    INFO("use cphy w39 cache size\n");
  } else if (strstr(args->trace_path, "w40.") != NULL) {
    working_set_size_mb = 6.5314 * 1000.0;
    find_data = true;
    INFO("use cphy w40 cache size\n");
  } else if (strstr(args->trace_path, "w41.") != NULL) {
    working_set_size_mb = 9.3146 * 1000.0;
    find_data = true;
    INFO("use cphy w41 cache size\n");
  } else if (strstr(args->trace_path, "w42.") != NULL) {
    working_set_size_mb = 57.691 * 1000.0;
    find_data = true;
    INFO("use cphy w42 cache size\n");
  } else if (strstr(args->trace_path, "w43.") != NULL) {
    working_set_size_mb = 119.9959 * 1000.0;
    find_data = true;
    INFO("use cphy w43 cache size\n");
  } else if (strstr(args->trace_path, "w44.") != NULL) {
    working_set_size_mb = 17.765 * 1000.0;
    find_data = true;
    INFO("use cphy w44 cache size\n");
  } else if (strstr(args->trace_path, "w45.") != NULL) {
    working_set_size_mb = 2.6995 * 1000.0;
    find_data = true;
    INFO("use cphy w45 cache size\n");
  } else if (strstr(args->trace_path, "w46.") != NULL) {
    working_set_size_mb = 1240.8514 * 1000.0;
    find_data = true;
    INFO("use cphy w46 cache size\n");
  } else if (strstr(args->trace_path, "w47.") != NULL) {
    working_set_size_mb = 2.9347 * 1000.0;
    find_data = true;
    INFO("use cphy w47 cache size\n");
  } else if (strstr(args->trace_path, "w48.") != NULL) {
    working_set_size_mb = 205.4541 * 1000.0;
    find_data = true;
    INFO("use cphy w48 cache size\n");
  } else if (strstr(args->trace_path, "w49.") != NULL) {
    working_set_size_mb = 75.3536 * 1000.0;
    find_data = true;
    INFO("use cphy w49 cache size\n");
  } else if (strstr(args->trace_path, "w50.") != NULL) {
    working_set_size_mb = 46.2845 * 1000.0;
    find_data = true;
    INFO("use cphy w50 cache size\n");
  } else if (strstr(args->trace_path, "w51.") != NULL) {
    working_set_size_mb = 21.9158 * 1000.0;
    find_data = true;
    INFO("use cphy w51 cache size\n");
  } else if (strstr(args->trace_path, "w52.") != NULL) {
    working_set_size_mb = 7.306 * 1000.0;
    find_data = true;
    INFO("use cphy w52 cache size\n");
  } else if (strstr(args->trace_path, "w53.") != NULL) {
    working_set_size_mb = 557.2461 * 1000.0;
    find_data = true;
    INFO("use cphy w53 cache size\n");
  } else if (strstr(args->trace_path, "w54.") != NULL) {
    working_set_size_mb = 2577.9289 * 1000.0;
    find_data = true;
    INFO("use cphy w54 cache size\n");
  } else if (strstr(args->trace_path, "w55.") != NULL) {
    working_set_size_mb = 38.3816 * 1000.0;
    find_data = true;
    INFO("use cphy w55 cache size\n");
  } else if (strstr(args->trace_path, "w56.") != NULL) {
    working_set_size_mb = 13.8987 * 1000.0;
    find_data = true;
    INFO("use cphy w56 cache size\n");
  } else if (strstr(args->trace_path, "w57.") != NULL) {
    working_set_size_mb = 14.531 * 1000.0;
    find_data = true;
    INFO("use cphy w57 cache size\n");
  } else if (strstr(args->trace_path, "w58.") != NULL) {
    working_set_size_mb = 37.0809 * 1000.0;
    find_data = true;
    INFO("use cphy w58 cache size\n");
  } else if (strstr(args->trace_path, "w59.") != NULL) {
    working_set_size_mb = 10.3049 * 1000.0;
    find_data = true;
    INFO("use cphy w59 cache size\n");
  } else if (strstr(args->trace_path, "w60.") != NULL) {
    working_set_size_mb = 32.7899 * 1000.0;
    find_data = true;
    INFO("use cphy w60 cache size\n");
  } else if (strstr(args->trace_path, "w61.") != NULL) {
    working_set_size_mb = 28.9899 * 1000.0;
    find_data = true;
    INFO("use cphy w61 cache size\n");
  } else if (strstr(args->trace_path, "w62.") != NULL) {
    working_set_size_mb = 36.8996 * 1000.0;
    find_data = true;
    INFO("use cphy w62 cache size\n");
  } else if (strstr(args->trace_path, "w63.") != NULL) {
    working_set_size_mb = 58.0903 * 1000.0;
    find_data = true;
    INFO("use cphy w63 cache size\n");
  } else if (strstr(args->trace_path, "w64.") != NULL) {
    working_set_size_mb = 396.7586 * 1000.0;
    find_data = true;
    INFO("use cphy w64 cache size\n");
  } else if (strstr(args->trace_path, "w65.") != NULL) {
    working_set_size_mb = 54.5818 * 1000.0;
    find_data = true;
    INFO("use cphy w65 cache size\n");
  } else if (strstr(args->trace_path, "w66.") != NULL) {
    working_set_size_mb = 2.7536 * 1000.0;
    find_data = true;
    INFO("use cphy w66 cache size\n");
  } else if (strstr(args->trace_path, "w67.") != NULL) {
    working_set_size_mb = 5.7741 * 1000.0;
    find_data = true;
    INFO("use cphy w67 cache size\n");
  } else if (strstr(args->trace_path, "w68.") != NULL) {
    working_set_size_mb = 20.6736 * 1000.0;
    find_data = true;
    INFO("use cphy w68 cache size\n");
  } else if (strstr(args->trace_path, "w69.") != NULL) {
    working_set_size_mb = 31.4905 * 1000.0;
    find_data = true;
    INFO("use cphy w69 cache size\n");
  } else if (strstr(args->trace_path, "w70.") != NULL) {
    working_set_size_mb = 32.1536 * 1000.0;
    find_data = true;
    INFO("use cphy w70 cache size\n");
  } else if (strstr(args->trace_path, "w71.") != NULL) {
    working_set_size_mb = 6.8732 * 1000.0;
    find_data = true;
    INFO("use cphy w71 cache size\n");
  } else if (strstr(args->trace_path, "w72.") != NULL) {
    working_set_size_mb = 6.9478 * 1000.0;
    find_data = true;
    INFO("use cphy w72 cache size\n");
  } else if (strstr(args->trace_path, "w73.") != NULL) {
    working_set_size_mb = 6.7424 * 1000.0;
    find_data = true;
    INFO("use cphy w73 cache size\n");
  } else if (strstr(args->trace_path, "w74.") != NULL) {
    working_set_size_mb = 4.4165 * 1000.0;
    find_data = true;
    INFO("use cphy w74 cache size\n");
  } else if (strstr(args->trace_path, "w75.") != NULL) {
    working_set_size_mb = 18.7578 * 1000.0;
    find_data = true;
    INFO("use cphy w75 cache size\n");
  } else if (strstr(args->trace_path, "w76.") != NULL) {
    working_set_size_mb = 8.2181 * 1000.0;
    find_data = true;
    INFO("use cphy w76 cache size\n");
  } else if (strstr(args->trace_path, "w77.") != NULL) {
    working_set_size_mb = 17.6128 * 1000.0;
    find_data = true;
    INFO("use cphy w77 cache size\n");
  } else if (strstr(args->trace_path, "w78.") != NULL) {
    working_set_size_mb = 34.4226 * 1000.0;
    find_data = true;
    INFO("use cphy w78 cache size\n");
  } else if (strstr(args->trace_path, "w79.") != NULL) {
    working_set_size_mb = 61.2372 * 1000.0;
    find_data = true;
    INFO("use cphy w79 cache size\n");
  } else if (strstr(args->trace_path, "w80.") != NULL) {
    working_set_size_mb = 4.5749 * 1000.0;
    find_data = true;
    INFO("use cphy w80 cache size\n");
  } else if (strstr(args->trace_path, "w81.") != NULL) {
    working_set_size_mb = 19.1657 * 1000.0;
    find_data = true;
    INFO("use cphy w81 cache size\n");
  } else if (strstr(args->trace_path, "w82.") != NULL) {
    working_set_size_mb = 2.4704 * 1000.0;
    find_data = true;
    INFO("use cphy w82 cache size\n");
  } else if (strstr(args->trace_path, "w83.") != NULL) {
    working_set_size_mb = 19.7912 * 1000.0;
    find_data = true;
    INFO("use cphy w83 cache size\n");
  } else if (strstr(args->trace_path, "w84.") != NULL) {
    working_set_size_mb = 20.1857 * 1000.0;
    find_data = true;
    INFO("use cphy w84 cache size\n");
  } else if (strstr(args->trace_path, "w85.") != NULL) {
    working_set_size_mb = 22.2916 * 1000.0;
    find_data = true;
    INFO("use cphy w85 cache size\n");
  } else if (strstr(args->trace_path, "w86.") != NULL) {
    working_set_size_mb = 82.1314 * 1000.0;
    find_data = true;
    INFO("use cphy w86 cache size\n");
  } else if (strstr(args->trace_path, "w87.") != NULL) {
    working_set_size_mb = 32.9536 * 1000.0;
    find_data = true;
    INFO("use cphy w87 cache size\n");
  } else if (strstr(args->trace_path, "w88.") != NULL) {
    working_set_size_mb = 7.5746 * 1000.0;
    find_data = true;
    INFO("use cphy w88 cache size\n");
  } else if (strstr(args->trace_path, "w89.") != NULL) {
    working_set_size_mb = 21.8123 * 1000.0;
    find_data = true;
    INFO("use cphy w89 cache size\n");
  } else if (strstr(args->trace_path, "w90.") != NULL) {
    working_set_size_mb = 16.272 * 1000.0;
    find_data = true;
    INFO("use cphy w90 cache size\n");
  } else if (strstr(args->trace_path, "w91.") != NULL) {
    working_set_size_mb = 15.7123 * 1000.0;
    find_data = true;
    INFO("use cphy w91 cache size\n");
  } else if (strstr(args->trace_path, "w92.") != NULL) {
    working_set_size_mb = 21.7219 * 1000.0;
    find_data = true;
    INFO("use cphy w92 cache size\n");
  } else if (strstr(args->trace_path, "w93.") != NULL) {
    working_set_size_mb = 31.215 * 1000.0;
    find_data = true;
    INFO("use cphy w93 cache size\n");
  } else if (strstr(args->trace_path, "w94.") != NULL) {
    working_set_size_mb = 52.1063 * 1000.0;
    find_data = true;
    INFO("use cphy w94 cache size\n");
  } else if (strstr(args->trace_path, "w95.") != NULL) {
    working_set_size_mb = 8.2447 * 1000.0;
    find_data = true;
    INFO("use cphy w95 cache size\n");
  } else if (strstr(args->trace_path, "w96.") != NULL) {
    working_set_size_mb = 16.2719 * 1000.0;
    find_data = true;
    INFO("use cphy w96 cache size\n");
  } else if (strstr(args->trace_path, "w97.") != NULL) {
    working_set_size_mb = 50.8166 * 1000.0;
    find_data = true;
    INFO("use cphy w97 cache size\n");
  } else if (strstr(args->trace_path, "w98.") != NULL) {
    working_set_size_mb = 49.9185 * 1000.0;
    find_data = true;
    INFO("use cphy w98 cache size\n");
  } else if (strstr(args->trace_path, "w99.") != NULL) {
    working_set_size_mb = 18.842 * 1000.0;
    find_data = true;
    INFO("use cphy w99 cache size\n");
  } else if (strstr(args->trace_path, "w100.") != NULL) {
    working_set_size_mb = 50.2313 * 1000.0;
    find_data = true;
    INFO("use cphy w100 cache size\n");
  } else if (strstr(args->trace_path, "w101.") != NULL) {
    working_set_size_mb = 50.1638 * 1000.0;
    find_data = true;
    INFO("use cphy w101 cache size\n");
  } else if (strstr(args->trace_path, "w102.") != NULL) {
    working_set_size_mb = 7.6564 * 1000.0;
    find_data = true;
    INFO("use cphy w102 cache size\n");
  } else if (strstr(args->trace_path, "w103.") != NULL) {
    working_set_size_mb = 7.6328 * 1000.0;
    find_data = true;
    INFO("use cphy w103 cache size\n");
  } else if (strstr(args->trace_path, "w104.") != NULL) {
    working_set_size_mb = 46.1833 * 1000.0;
    find_data = true;
    INFO("use cphy w104 cache size\n");
  } else if (strstr(args->trace_path, "w105.") != NULL) {
    working_set_size_mb = 17.2826 * 1000.0;
    find_data = true;
    INFO("use cphy w105 cache size\n");
  } else if (strstr(args->trace_path, "w106.") != NULL) {
    working_set_size_mb = 4.3986 * 1000.0;
    find_data = true;
    INFO("use cphy w106 cache size\n");
  }

  double s[9] = {0.01, 0.05, 0.1, 0.5, 1, 5, 10, 20, 40};
  int n_skipped = 0;
  for (int i = 0; i < sizeof(s) / sizeof(double); i++) {
    if (working_set_size_mb / 100.0 * s[i] < 32) {
      n_skipped++;
      continue;
    }
    args->cache_sizes[i - n_skipped] = (uint64_t) (working_set_size_mb / 100.0 * s[i] * MiB);
  }
  args->n_cache_size = sizeof(s) / sizeof(double) - n_skipped;

  return find_data;
}
