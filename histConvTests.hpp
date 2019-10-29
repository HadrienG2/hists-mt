// Common ROOT7 -> ROOT6 histogram conversion test harness definitions
// You only need histConvTests.hpp.dcl if you don't instantiate test_conversion
#include "histConvTests.hpp.dcl"

#include <atomic>
#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <string>
#include <typeinfo>

#include "ROOT/RHist.hxx"

// Sufficient for testing classic RHist configurations with test_conversion.
// In order to test exotic statistics, you must also include histConv.hpp.
#include "histConv.hpp.dcl"


// What would a test suite be without assertions?
#define ASSERT_EQ(x, y, failure_message)  \
        if ((x) != (y)) { throw std::runtime_error((failure_message)); }
#define ASSERT_CLOSE(x, ref, tol, failure_message)  \
        if (std::abs((x) - (ref)) > (tol) * std::abs((ref)))  \
          { throw std::runtime_error((failure_message)); }
#define ASSERT_NOT_NULL(x, failure_message)  \
        if (nullptr == (x)) { throw std::runtime_error((failure_message)); }


// Run tests for a certain ROOT 7 histogram type and axis configuration
template <int DIMS,
          class PRECISION,
          template <int D_, class P_> class... STAT>
void test_conversion(std::array<RExp::RAxisConfig, DIMS>&& axis_configs) {
  // ROOT 6 is picky about unique names
  static std::atomic<size_t> ctr = 0;
  std::string name = "Hist" + std::to_string(ctr);
  ctr.fetch_add(1, std::memory_order_relaxed);

  // Generate a ROOT 7 histogram
  // TODO: Give it a random title -> extract title generator from utilities
  //                              +  pass an RNG to this function
  RExp::RHist<DIMS, PRECISION, STAT...> src(axis_configs);
  // TODO: Fill it with some test data

  // Convert it to ROOT 6 format and check the output
  try
  {
    // Perform the conversion
    auto dest = into_root6_hist(src, name.c_str());

    // Check general output histogram configuration
    const auto& src_impl = *src.GetImpl();
    ASSERT_EQ(name, dest.GetName(), "Incorrect output histogram name");
    ASSERT_EQ(src_impl.GetTitle(), dest.GetTitle(),
              "Incorrect output histogram title");
    ASSERT_EQ(DIMS, dest.GetDimension(),"Incorrect output histogram dimension");
    ASSERT_EQ(nullptr, dest.GetBuffer(),
              "Output histogram shouldn't be buffered");
    ASSERT_EQ(TH1::EStatOverflows::kConsider, dest.GetStatOverflows(),
              "Output histogram statistics should include overflow bins");
    ASSERT_EQ(TH1::EBinErrorOpt::kNormal, dest.GetBinErrorOption(),
              "Output histogram bin errors should use normal approximation");
    ASSERT_EQ(0, dest.GetNormFactor(),
              "Output histogram shouldn't have a normalization factor");

    // Check axis configuration
    if constexpr (DIMS >= 1) check_axis_config(*dest.GetXaxis(), axis_configs[0]);
    if constexpr (DIMS >= 2) check_axis_config(*dest.GetYaxis(), axis_configs[1]);
    if constexpr (DIMS >= 3) check_axis_config(*dest.GetZaxis(), axis_configs[2]);

    // TODO: Check output data and statistics (requires input data)
    /* Properties from TH1 to be tested are:

    // Used to check other properties only
    virtual Int_t    GetBin(Int_t binx, Int_t biny=0, Int_t binz=0) const;

    virtual Double_t GetBinContent(Int_t bin) const;
    virtual Double_t GetBinContent(Int_t bin, Int_t) const { return GetBinContent(bin); }
    virtual Double_t GetBinContent(Int_t bin, Int_t, Int_t) const { return GetBinContent(bin); }

    virtual Double_t GetBinError(Int_t bin) const;
    virtual Double_t GetBinError(Int_t binx, Int_t biny) const { return GetBinError(GetBin(binx, biny)); } // for 2D histograms only
    virtual Double_t GetBinError(Int_t binx, Int_t biny, Int_t binz) const { return GetBinError(GetBin(binx, biny, binz)); } // for 3D histograms only

    virtual Double_t GetEntries() const;
    virtual Double_t GetEffectiveEntries() const;

    virtual void     GetStats(Double_t *stats) const;
    virtual Double_t GetStdDev(Int_t axis=1) const;
    virtual Double_t GetStdDevError(Int_t axis=1) const;
    virtual Double_t GetSumOfWeights() const;
    virtual const TArrayD *GetSumw2() const {return &fSumw2;}
    virtual Int_t    GetSumw2N() const {return fSumw2.fN;}
    virtual Double_t GetSkewness(Int_t axis=1) const; */
  }
  catch (const std::runtime_error& e)
  {
    // Print exception text
    std::cout << "Histogram conversion error: " << e.what() << std::endl;

    // Use GCC ABI to print histogram type
    char * hist_type_name;
    int status;
    hist_type_name = abi::__cxa_demangle(typeid(src).name(), 0, 0, &status);
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