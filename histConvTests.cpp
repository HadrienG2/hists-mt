// Top-level ROOT7 -> ROOT6 histogram conversion test harness

#include <iostream>

#include "histConvTests.hpp"


int main() {
  // Conversion of null ROOT 7 histograms should fail with a clear exception
  assert_runtime_error([]() { into_root6_hist(RExp::RHist<1, char>(), "bad"); },
                       "Converting a null histogram should fail");

  // For the most part, we'll use reproducible but pseudo-random test data...
  RNG rng;
  for (size_t i = 0; i < NUM_TEST_RUNS; ++i) {
    // Conversion from ROOT7's default histogram configuration works
    test_conversion<1, char>(rng, {gen_axis_config(rng)});

    // Exotic statistics configurations work as well
    test_conversion_exotic_stats(rng);

    // Data types other than char work just as well, if supported by ROOT6
    test_conversion<1, short>(rng, {gen_axis_config(rng)});
    test_conversion<1, int>(rng, {gen_axis_config(rng)});
    test_conversion<1, float>(rng, {gen_axis_config(rng)});
    test_conversion<1, double>(rng, {gen_axis_config(rng)});

    // This data type is not supported by ROOT6. The problem will be reported at
    // compile time with a clear error message.
    /* test_conversion<1, size_t>(rng, {gen_axis_config(rng)}); */

    // Try it with a 2D histogram
    test_conversion<2, char>(rng, {gen_axis_config(rng), gen_axis_config(rng)});

    // Try it with a 3D histogram
    test_conversion<3, char>(rng,
                             {gen_axis_config(rng),
                              gen_axis_config(rng),
                              gen_axis_config(rng)});
  }

  // ...and we're good.
  std::cout << "All tests passed successfully!" << std::endl;
  return 0;
}