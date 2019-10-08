#pragma once

#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <random>
#include <string>
#include <typeinfo>
#include <utility>

#include "ROOT/RAxis.hxx"
#include "ROOT/RHist.hxx"

// Sufficient for testing classic RHist configurations with test_conversion.
// In order to test exotic statistics, you must also include histConv.hpp.
#include "histConv.hpp.dcl"


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


// === TEST RUNNER ===

// Run tests for a certain ROOT 7 histogram type and axis configuration
template <int DIMS,
          class PRECISION,
          template <int D_, class P_> class... STAT>
void test_conversion(std::array<RExp::RAxisConfig, DIMS>&& axis_configs) {
  // ROOT 6 is picky about unique names
  static size_t ctr = 0;
  std::string name = "Hist" + std::to_string(ctr);
  ctr += 1;

  // Generate a ROOT 7 histogram
  RExp::RHist<DIMS, PRECISION, STAT...> source(axis_configs);
  // TODO: Fill it with some test data

  // Convert it to ROOT 6 format and check the output
  try
  {
    auto dest = into_root6_hist(std::move(source), name.c_str());
    // TODO: Check histogram configuration
    // TODO: Check output data and statistics
  }
  catch (const std::runtime_error& e)
  {
    // Print exception text
    std::cout << "Histogram conversion error: " << e.what() << std::endl;

    // Use GCC ABI to print histogram type
    char * hist_type_name;
    int status;
    hist_type_name = abi::__cxa_demangle(typeid(source).name(), 0, 0, &status);
    std::cout << "* Histogram type was " << hist_type_name << std::endl;
    free(hist_type_name);

    // Print input axis configuration
    std::cout << "* Axis configuration was..." << std::endl;
    char axis_name = 'X';
    for (const auto& axis_config : axis_configs) {
      std::cout << "  - " << axis_name++ << ": ";
      print_axis_config(axis_config);
      std::cout << std::endl;
    }

    // TODO: Decide if aborting semantics are wanted
    std::cout << std::endl;
    std::abort();
  }
}