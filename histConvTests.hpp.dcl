// Common ROOT7 -> ROOT6 histogram conversion test harness declarations
// Use histConvTests.hpp if you need to instantiate the test_conversion template

#pragma once

#include <array>
#include <exception>
#include <random>
#include <utility>

#include "ROOT/RAxis.hxx"


// === TEST TUNING KNOBS ===

// Source of pseudorandom numbers for randomized testing
using RNG = std::mt19937_64;

// Parameter space being explored by the tests
constexpr std::pair<int, int> NUM_BINS_RANGE{1, 1000};
constexpr std::pair<double, double> AXIS_LIMIT_RANGE{-10264.5, 1928.37};

// Number of random test iterations, tune up for extra coverage at a speed cost
constexpr size_t NUM_TEST_RUNS = 1000;


// === COMMON DECLARATIONS ===

// Typing this gets old quickly
namespace RExp = ROOT::Experimental;

// Generate a random ROOT 7 axis configuration
RExp::RAxisConfig gen_axis_config(RNG& rng);

// Print out axis configurations from a histogram
// (split from test_conversion to reduce template code bloat)
void print_axis_config(const RExp::RAxisConfig& axis_config);

// Calls test_conversion with some exotic stats configurations
// (extracted from main() to test both histConv.hpp.dcl and full histConv.hpp)
void test_conversion_exotic_stats(RNG& rng);

// Run tests for a certain ROOT 7 histogram type and axis configuration
template <int DIMS,
          class PRECISION,
          template <int D_, class P_> class... STAT>
void test_conversion(std::array<RExp::RAxisConfig, DIMS>&& axis_configs);
