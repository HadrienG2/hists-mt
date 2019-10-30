// Common ROOT7 -> ROOT6 histogram conversion test harness declarations
// Use histConvTests.hpp if you need to instantiate the test_conversion template

#pragma once

#include <array>
#include <exception>
#include <random>
#include <string>
#include <utility>

#include "ROOT/RAxis.hxx"
#include "ROOT/RHistUtils.hxx"


// === FORWARD DECLARATIONS ===

class TAxis;
class TH1;

namespace ROOT { namespace Experimental { namespace Detail {
  template <int DIMS>
  struct RHistImplPrecisionAgnosticBase;
} } }


// === TEST TUNING KNOBS ===

// Source of pseudorandom numbers for randomized testing
using RNG = std::mt19937_64;

// Parameter space being explored by the tests
// NOTE: Need max(data points) * max(weight) <= 127 to test char histograms
constexpr std::pair<int, int> NUM_BINS_RANGE{1, 10};
constexpr std::pair<size_t, size_t> NUM_DATA_POINTS_RANGE{0, 60};
constexpr std::pair<double, double> AXIS_LIMIT_RANGE{-10264.5, 1928.37};
constexpr std::pair<double, double> WEIGHT_RANGE{0.4, 2.1};

// Number of random test iterations, tune up for extra coverage at a speed cost
constexpr size_t NUM_TEST_RUNS = 10000;


// === COMMON DECLARATIONS ===

// Typing this gets old quickly
namespace RExp = ROOT::Experimental;

// RNG range. Should add +1 to avoid bias but that can lead to overflow...
constexpr auto RNG_CHOICE = RNG::max() - RNG::min();

// Generate a random boolean
inline bool gen_bool(RNG& rng) {
  return (rng() >= (RNG::min() + (RNG_CHOICE / 2)));
}

// Generate a random floating-point number
inline double gen_double(RNG& rng, double min, double max) {
  return min + (rng() - RNG::min()) * (max - min) / RNG_CHOICE;
}

// Generate a random ROOT 7 axis configuration
RExp::RAxisConfig gen_axis_config(RNG& rng);

// Generate a unique histogram name (ROOT 6 specific, used for e.g. ROOT I/O)
std::string gen_unique_hist_name();

// Generate a histogram title
std::string gen_hist_title(RNG& rng);

// Test data for filling up ROOT 7 histograms
template <int DIMS, typename Weight>
struct TestData {
  using CoordArray = RExp::Hist::RCoordArray<DIMS>;

  // Histogram coordinates to be filled
  std::vector<CoordArray> coords;

  // Weights associated with each data point, or empty vector if test data has
  // unity weight (i.e. should be filled using RHist::FillN(coords))
  std::vector<Weight> weights;

  // Whether "coords" contains out-of-range data for testing overflow bins
  bool exercizes_overflow;

  // Constructor that generates the test data
  TestData(RNG& rng, const std::array<RExp::RAxisConfig, DIMS>& axis_configs);

  // Print the test data as a (big) one-liner
  void print() const;
};

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
void test_conversion(RNG& rng,
                     std::array<RExp::RAxisConfig, DIMS>&& axis_configs);

// Check that a ROOT 6 axis is configured like a ROOT 7 one
// NOTE: Cannot use const TAxis& because some TAxis accessors are not const...
void check_axis_config(TAxis& axis, const RExp::RAxisConfig& config);

// Check that a ROOT 6 histogram is configured like a ROOT 7 one
// NOTE: Cannot use const TH1& because some TAxis accessors are not const...
template <int DIMS>
void check_hist_config(
  const RExp::Detail::RHistImplPrecisionAgnosticBase<DIMS>& src_impl,
  const std::array<RExp::RAxisConfig, DIMS>& axis_configs,
  const std::string& name,
  TH1& dest
);

// Check that a ROOT 6 histogram contains the same data as a ROOT 7 one
template <typename THn, typename Root7Hist>
void check_hist_data(const Root7Hist& src,
                     bool has_overflow_data,
                     const THn& dest);


// === TEST ASSERTIONS ===

// Check that two things are equal, else throw runtime error.
#define ASSERT_EQ(x, y, failure_message)  \
  if ((x) != (y)) { throw std::runtime_error((failure_message)); }

// Check that two floating-point numbers are relatively close (by given
// tolerance, else throw runtime error.
#define ASSERT_CLOSE(x, ref, tol, failure_message)  \
  if (std::abs((x) - (ref)) > (tol) * std::abs((ref)))  \
    { throw std::runtime_error((failure_message)); }

// Check that a pointer is not null, else throw runtime error
#define ASSERT_NOT_NULL(x, failure_message)  \
  if (nullptr == (x)) { throw std::runtime_error((failure_message)); }

// Check that a certain operation triggers an std::runtime_error exception
void assert_runtime_error(std::function<void()>&& operation,
                          std::string&& failure_message);
