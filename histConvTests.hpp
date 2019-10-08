#pragma once

#include <random>

#include "ROOT/RAxis.hxx"


// Test tuning knobs
using RNG = std::mt19937_64;
constexpr std::pair<int, int> NUM_BINS_RANGE{1, 1000};
constexpr std::pair<double, double> AXIS_LIMIT_RANGE{-10264.5, 1928.37};
constexpr size_t NUM_TEST_RUNS = 1000;

// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


// Test runner for all supported ROOT 7 histogram types
int main();

// Test runner for a specific ROOT 7 histogram type
template <int DIMS,
          class PRECISION,
          template <int D_, class P_> class... STAT>
void test_conversion(std::array<RExp::RAxisConfig, DIMS>&& axis_configs);

// Generate a random ROOT 7 axis configuration
RExp::RAxisConfig gen_axis_config(RNG& rng);

// Print out axis configurations from a histogram
// (split from test_conversion to reduce template code bloat)
void print_axis_config(const RExp::RAxisConfig& axis_config);