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


// Run tests for a certain ROOT 7 histogram type and axis configuration
template <int DIMS,
          class PRECISION,
          template <int D_, class P_> class... STAT>
void test_conversion(RNG& rng,
                     std::array<RExp::RAxisConfig, DIMS>&& axis_configs) {
  // ROOT 6 is picky about unique names
  static std::atomic<size_t> ctr = 0;
  std::string name = "Hist" + std::to_string(ctr);
  ctr.fetch_add(1, std::memory_order_relaxed);

  // Generate a ROOT 7 histogram, testing both empty and semicolon-ridden titles
  std::string title = "";
  if (ctr > 0) title += "Hist;title;is;number " + std::to_string(ctr);
  using Source = RExp::RHist<DIMS, PRECISION, STAT...>;
  Source src(title, axis_configs);

  // FIXME: Factor out some of this for the sake of readability and fast builds

  // Determine which range the axes of that histogram span (from Fill's PoV)
  using CoordArray = typename Source::CoordArray_t;
  using Weight = typename Source::Weight_t;
  std::pair<CoordArray, CoordArray> coord_range;
  for (size_t axis_idx = 0; axis_idx < DIMS; ++axis_idx) {
    const auto& axis_config = axis_configs[axis_idx];
    // FIXME: Generate some out-of-range data for growable axes once ROOT 7 axis
    //        growth is actually implemented...
    switch (axis_config.GetKind())
    {
    case RExp::RAxisConfig::EKind::kEquidistant:
    case RExp::RAxisConfig::EKind::kGrow:
    case RExp::RAxisConfig::EKind::kIrregular: {
      const auto& bin_borders = axis_config.GetBinBorders();
      coord_range.first[axis_idx] = bin_borders[0];
      coord_range.second[axis_idx] = bin_borders[bin_borders.size()-1];
      break;
    }

    case RExp::RAxisConfig::EKind::kLabels: {
      const auto& bin_labels = axis_config.GetBinLabels();
      coord_range.first[axis_idx] = 0;
      coord_range.second[axis_idx] = bin_labels.size();
      break;
    }

    default:
      throw std::runtime_error("This switch is broken, please fix it");
    }
  }

  // Generate some test data to fill the ROOT 7 histogram with
  size_t num_data_points =
    static_cast<size_t>(gen_double(rng,
                                   NUM_DATA_POINTS_RANGE.first,
                                   NUM_DATA_POINTS_RANGE.second));
  bool variable_weight = gen_bool(rng);
  std::vector<CoordArray> coords;
  std::vector<Weight> weights;
  for (size_t data_idx = 0; data_idx < num_data_points; ++data_idx) {
    CoordArray coord;
    for (size_t axis_idx = 0; axis_idx < DIMS; ++axis_idx) {
      coord[axis_idx] = gen_double(rng,
                                   coord_range.first[axis_idx],
                                   coord_range.second[axis_idx]);
    }
    coords.push_back(coord);
    if (variable_weight) weights.push_back(gen_double(rng,
                                                      WEIGHT_RANGE.first,
                                                      WEIGHT_RANGE.second));
  }

  // Fill the ROOT 7 histogram with the test data
  if (variable_weight) {
    src.FillN(coords, weights);
  } else {
    src.FillN(coords);
  }

  // Convert the ROOT 7 histogram to ROOT 6 format and check the output
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

    // TODO: Check output data and statistics
    ASSERT_EQ(coords.size(), dest.GetEntries(), "Invalid entry count");
    std::array<Double_t, TH1::kNstat> stats;
    dest.GetStats(stats.data());
    // From the TH1 documentation, stats contains at least...
    // s[0]  = sumw       s[1]  = sumw2
    // s[2]  = sumwx      s[3]  = sumwx2
    // In TH2, stats also contains...
    // s[4]  = sumwy      s[5]  = sumwy2   s[6]  = sumwxy
    // In TH3, stats also contains...
    // s[7]  = sumwz      s[8]  = sumwz2   s[9]  = sumwxz   s[10]  = sumwyz
    double sumw = variable_weight ? std::accumulate(weights.cbegin(),
                                                    weights.cend(),
                                                    0.0)
                                  : coords.size();
    ASSERT_CLOSE(sumw, stats[0], 1e-6, "Sum of weights is incorrect");
    // TODO: Check other stats, using if constexpr for TH2 and TH3 ones

    // TODO: Check per-bin stats, namely
    // - Per-bin histogram stats (not sure what's the most convenient way...)
    // - Per-bin SumW2 (if recorded, can be queried via GetSumw2)
    /* Here's a reminder of some useful TH1 queries:

    // Used to check other properties only
    virtual Int_t    GetBin(Int_t binx, Int_t biny=0, Int_t binz=0) const;

    virtual Double_t GetBinContent(Int_t bin) const;
    virtual Double_t GetBinContent(Int_t bin, Int_t) const { return GetBinContent(bin); }
    virtual Double_t GetBinContent(Int_t bin, Int_t, Int_t) const { return GetBinContent(bin); }

    virtual Double_t GetBinError(Int_t bin) const;
    virtual Double_t GetBinError(Int_t binx, Int_t biny) const { return GetBinError(GetBin(binx, biny)); } // for 2D histograms only
    virtual Double_t GetBinError(Int_t binx, Int_t biny, Int_t binz) const { return GetBinError(GetBin(binx, biny, binz)); } // for 3D histograms only

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

    // Print coordinates and weights
    std::cout << "* Histogram was filled with the following "
              << coords.size() << " data points of "
              << (variable_weight ? "variable" : "constant") << " weight: { ";
    auto print_point = [&](size_t point) {
      std::cout << "[ ";
      for (size_t dim = 0; dim < DIMS-1; ++dim) {
        std::cout << coords[point][dim] << ", ";
      }
      std::cout << coords[point][DIMS-1] << " ]";
      if (!variable_weight) { return; }
      std::cout << " @ " << weights[point];
    };
    for (size_t point = 0; point < coords.size()-1; ++point) {
      print_point(point);
      std::cout << "; ";
    }
    print_point(coords.size()-1);
    std::cout << " }" << std::endl;


    // TODO: Decide if aborting semantics are wanted
    std::cout << std::endl;
    std::abort();
  }
}