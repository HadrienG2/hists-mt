// Common ROOT7 -> ROOT6 histogram conversion test harness definitions
// You only need histConvTests.hpp.dcl if you don't instantiate test_conversion
#include "histConvTests.hpp.dcl"

#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <string>
#include <typeinfo>

#include "ROOT/RHist.hxx"
#include "THashList.h"
#include "TObjString.h"

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
  //
  // TODO: Use an atomic and fetch_add here if we decide to make the test
  //       multi-threaded... but that will also require work on the ROOT 6 side.
  //
  static size_t ctr = 0;
  std::string name = "Hist" + std::to_string(ctr);
  ctr += 1;

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

    // FIXME: Deduplicate some checks out of this templated function so that we
    //        don't generate one copy of the code per type of ROOT 7 histogram.

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
    // FIXME: Non-const TAxis& because xxxAlphanumeric methods aren't const
    auto check_axis = [](TAxis& axis, const RExp::RAxisConfig& config) {
      ASSERT_EQ(config.GetTitle(), axis.GetTitle(),
                "Incorrect output axis title");
      ASSERT_EQ(config.GetNBinsNoOver(), axis.GetNbins(),
                "Incorrect number of bins");

      // FIXME: No direct axis to RAxisBase::fCanGrow at this point in time...
      ASSERT_EQ(config.GetNOverflowBins() == 0, axis.CanExtend(),
                "Axis growability does not match");

      // Checks which are specific to RAxisEquidistant and RAxisGrow
      const bool is_equidistant = 
        config.GetKind() == RExp::RAxisConfig::EKind::kEquidistant;
      const bool is_grow =
        config.GetKind() == RExp::RAxisConfig::EKind::kGrow;
      if (is_equidistant || is_grow) {
        ASSERT_CLOSE(config.GetBinBorders()[0], axis.GetXmin(), 1e-6,
                     "Axis minimum does not match");
        ASSERT_CLOSE(config.GetBinBorders()[1], axis.GetXmax(), 1e-6,
                     "Axis maximum does not match");
      }

      // Checks which are specific to irregular axes
      const bool is_irregular =
        config.GetKind() == RExp::RAxisConfig::EKind::kIrregular;
      ASSERT_EQ(is_irregular,
                axis.IsVariableBinSize(),
                "Irregular ROOT 7 axes (only) should have variable bins");
      if (is_irregular) {
        const auto& bins = *axis.GetXbins();
        const auto& expected_bins = config.GetBinBorders();
        ASSERT_EQ(bins.fN, expected_bins.size(), "Wrong number of bin borders");
        for (size_t i = 0; i < bins.fN; ++i) {
          ASSERT_EQ(bins[i], expected_bins[i], "Wrong bin border values");
        }
      }

      // Checks which are specific to labeled axes
      // FIXME: Untested code path because ROOT 7 labeled axes don't work yet
      bool is_labeled = config.GetKind() == RExp::RAxisConfig::EKind::kLabels;
      ASSERT_EQ(is_labeled,
                axis.CanBeAlphanumeric() || axis.IsAlphanumeric(),
                "Labeled ROOT 7 axes (only) should be alphanumeric");
      if (is_labeled) {
        auto labels_ptr = axis.GetLabels();
        ASSERT_NOT_NULL(labels_ptr, "Labeled axes should have labels");

        auto labels_iter_ptr = labels_ptr->MakeIterator();
        const auto expected_labels = config.GetBinLabels();
        auto expected_labels_iter = expected_labels.cbegin();
        TObject* label_ptr;
        size_t num_labels = 0;

        while ((label_ptr = labels_iter_ptr->Next())
               && (expected_labels_iter != expected_labels.cend())) {
          num_labels += 1;
          const auto& label = dynamic_cast<const TObjString&>(*label_ptr);
          ASSERT_EQ(expected_labels_iter->c_str(), label.GetString(),
                    "Some axis labels do not match");
          ++expected_labels_iter;
        }

        ASSERT_EQ(expected_labels.size(), num_labels,
                  "Number of axis labels does not match");
        delete labels_iter_ptr;
      }
    };
    if constexpr (DIMS >= 1) check_axis(*dest.GetXaxis(), axis_configs[0]);
    if constexpr (DIMS >= 2) check_axis(*dest.GetYaxis(), axis_configs[1]);
    if constexpr (DIMS >= 3) check_axis(*dest.GetZaxis(), axis_configs[2]);

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