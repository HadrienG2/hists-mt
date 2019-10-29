// Top-level ROOT7 -> ROOT6 histogram conversion test harness

#include "histConvTests.hpp"


int main() {
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

    // FIXME: Higher-dimensional histograms are broken for now. I need to have
    //        a chat with the ROOT team to figure out if they _really_ think
    //        that providing local coordinates in an order that's reversed wrt
    //        the one specified at axis creation is a good idea in order to
    //        determine how to best fix that.
    /* // Try it with a 2D histogram
    test_conversion<2, char>(rng, {gen_axis_config(rng), gen_axis_config(rng)});

    // Try it with a 3D histogram, using a homogeneous axis config
    auto axis1 = gen_axis_config(rng);
    auto axis2 = gen_axis_config(rng);
    while (axis2.GetKind() != axis1.GetKind()) axis2 = gen_axis_config(rng);
    auto axis3 = gen_axis_config(rng);
    while (axis3.GetKind() != axis1.GetKind()) axis3 = gen_axis_config(rng);
    test_conversion<3, char>(rng, {axis1, axis2, axis3}); */

    // This will fail at run-time after a few iterations: TH3 currently does not
    // support inhomogeneous axis configs.
    // FIXME: Add a way to test runtime failures
    /* test_conversion<3, char>(rng,
                                {gen_axis_config(rng),
                                 gen_axis_config(rng),
                                 gen_axis_config(rng)}); */
  }
  return 0;
}