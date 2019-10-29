// ROOT7 -> ROOT6 histogram conversion tests for custom stats configuration
// Extracted from histConvTests.cpp to test both histConv.hpp & histConv.hpp.dcl

#include "ROOT/RHistData.hxx"

// Full histConv header needed because we use custom stats configurations
#include "histConv.hpp"
#include "histConvTests.hpp"


void test_conversion_exotic_stats(RNG& rng) {
  // We need at least RHistStatContent statistics
  test_conversion<1, char, RExp::RHistStatContent>(rng, {gen_axis_config(rng)});

  // More stats won't hurt
  test_conversion<1,
                  char,
                  RExp::RHistStatContent,
                  RExp::RHistDataMomentUncert>(rng, {gen_axis_config(rng)});

  // Insufficient stats will be reported at compile time with a clear error
  // message (unfortunately followed by ROOT blowing up, for now...)
  // FIXME: Add a way to test compilation failures
  /* test_conversion<1,
                  char,
                  RExp::RHistDataMomentUncert>(rng, {gen_axis_config(rng)});
  test_conversion<1,
                  char,
                  RExp::RHistStatUncertainty,
                  RExp::RHistDataMomentUncert>(rng, {gen_axis_config(rng)}); */
}