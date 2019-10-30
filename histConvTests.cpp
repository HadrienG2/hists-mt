// Top-level ROOT7 -> ROOT6 histogram conversion test harness

#include "histConvTests.hpp"


int main() {
  // Conversion of null ROOT 7 histograms should fail with a clear exception
  assert_runtime_error([]() { into_root6_hist(RExp::RHist<1, char>(), "bad"); },
                       "Converting a null histograms should fail");

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
    // FIXME: Add a way to test compilation failures
    /* test_conversion<1, size_t>(rng, {gen_axis_config(rng)}); */

    // FIXME: Higher-dimensional histograms are broken for now, due to a binning
    //        convention mismatch.
    //
    //        It's not clear to me if the current ROOT 7 multi-dimensional
    //        histogram binning convention is correct, as it has some strange
    //        properties such as providing local bin coordinates in an order
    //        that is reversed wrt the order in which axis configurations are
    //        passed to the RHist constructor.
    //
    //        Therefore, I should have a chat with the ROOT team to understand
    //        what kind of binning convention they actually intend to have for
    //        multi-dimensional RHist before fixing this bug.
    //
    /* // Try it with a 2D histogram
    test_conversion<2, char>(rng, {gen_axis_config(rng), gen_axis_config(rng)}); */

    // Try it with a 3D histogram
    auto axis1 = gen_axis_config(rng);
    auto axis2 = gen_axis_config(rng);
    auto axis3 = gen_axis_config(rng);
    bool homogeneous_config = (axis1.GetKind() == axis2.GetKind())
                                && (axis2.GetKind() == axis3.GetKind());
    if (homogeneous_config) {
      // FIXME: 3D histograms with homogeneous axis configuration should work,
      //        but are currently disabled for the reason described above.
      /* test_conversion<3, char>(rng, {axis1, axis2, axis3}); */
    } else {
      // 3D histogram with inhomogeneous axis configuration are currently
      // unsupported because TH3 does not provide a suitable constructor
      RExp::RHist<3, char> bad{{axis1, axis2, axis3}};
      assert_runtime_error(
        [&] { into_root6_hist(bad, "nope"); },
        "Converting a TH3 with an inhomogeneous axis config should fail"
      );
    }
  }
  return 0;
}