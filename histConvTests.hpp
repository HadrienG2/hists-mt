// Common ROOT7 -> ROOT6 histogram conversion test harness definitions
// You only need histConvTests.hpp.dcl if you don't instantiate test_conversion
#include "histConvTests.hpp.dcl"

#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <string>
#include <typeinfo>

#include "ROOT/RHist.hxx"

// Sufficient for testing classic RHist configurations with test_conversion.
// In order to test exotic statistics, you must also include histConv.hpp.
#include "histConv.hpp.dcl"


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