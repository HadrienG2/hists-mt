// Common ROOT7 -> ROOT6 histogram conversion test harness definitions
// You only need histConvTests.hpp.dcl if you don't instantiate test_conversion
#include "histConvTests.hpp.dcl"

#include <atomic>
#include <cxxabi.h>
#include <iostream>
#include <typeinfo>

#include "ROOT/RHist.hxx"

// Sufficient for testing classic RHist configurations with test_conversion.
// In order to test exotic statistics, you must also include histConv.hpp.
#include "histConv.hpp.dcl"


template <int DIMS, typename Weight>
TestData<DIMS, Weight>::TestData(
  RNG& rng,
  const RHistImplPABase<DIMS>& target
)
  : exercizes_overflow{gen_bool(rng)}
{
  // Determine which coordinate range the test data should span
  std::pair<CoordArray, CoordArray> coord_range;
  for (size_t axis_idx = 0; axis_idx < DIMS; ++axis_idx) {
    const auto& axis = target.GetAxis(axis_idx);
    coord_range.first[axis_idx] = axis.GetMinimum();
    coord_range.second[axis_idx] = axis.GetMaximum();
    if (this->exercizes_overflow) {
      const auto bin_spacing = axis.GetBinTo(1) - axis.GetBinTo(0);
      coord_range.first[axis_idx] -= bin_spacing;
      coord_range.second[axis_idx] += bin_spacing;
    }
  }

  // Generate some test data to fill the ROOT 7 histogram with
  const size_t num_data_points =
    static_cast<size_t>(gen_double(rng,
                                   NUM_DATA_POINTS_RANGE.first,
                                   NUM_DATA_POINTS_RANGE.second));
  this->coords.reserve(num_data_points);
  const bool variable_weight = gen_bool(rng);
  if (variable_weight) this->weights.reserve(num_data_points);
  for (size_t data_idx = 0; data_idx < num_data_points; ++data_idx) {
    CoordArray coord;
    for (size_t axis_idx = 0; axis_idx < DIMS; ++axis_idx) {
      coord[axis_idx] = gen_double(rng,
                                   coord_range.first[axis_idx],
                                   coord_range.second[axis_idx]);
    }
    this->coords.push_back(coord);

    if (variable_weight) {
      this->weights.push_back(gen_double(rng,
                                         WEIGHT_RANGE.first,
                                         WEIGHT_RANGE.second));
    }
  }
}


template <int DIMS, typename Weight>
void TestData<DIMS, Weight>::print() const {
  // How many test data points? Weighted or unweighted?
  std::cout << this->coords.size() << " data points of "
            << (this->weights.empty() ? "unity" : "variable") << " weight";

  // Are we exercizing overflow bins with out of range data?
  if (this->exercizes_overflow) {
    std::cout << ", exercizing under- and overflow bins";
  }

  // This is how we print a single data point...
  auto print_point = [&](size_t point) {
    // First the coordinates
    std::cout << "[ ";
    for (size_t dim = 0; dim < DIMS-1; ++dim) {
      std::cout << this->coords[point][dim] << ", ";
    }
    std::cout << this->coords[point][DIMS-1] << " ]";

    // Then the weight, if not unity
    if (this->weights.empty()) { return; }
    std::cout << " @ " << double(this->weights[point]);
  };

  // ...now let's print all of them.
  std::cout << ": { ";
  for (size_t point = 0; point < this->coords.size()-1; ++point) {
    print_point(point);
    std::cout << "; ";
  }
  print_point(this->coords.size()-1);
  std::cout << " }";
}


template <int DIMS>
void check_hist_config(const RHistImplPABase<DIMS>& src_impl,
                       const std::string& name,
                       TH1& dest)
{
  // Check non-axis histogram configuration
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
  check_axis_config(src_impl.GetAxis(0), *dest.GetXaxis());
  if constexpr (DIMS >= 2)
    check_axis_config(src_impl.GetAxis(1), *dest.GetYaxis());
  if constexpr (DIMS == 3)
    check_axis_config(src_impl.GetAxis(2), *dest.GetZaxis());
}


template <typename THn, typename Root7Hist>
void check_hist_data(const Root7Hist& src,
                     bool has_overflow_data,
                     const THn& dest)
{
  // Mechanism to iterate over each source bin, be it overflow or non-overflow
  const auto& src_impl = *src.GetImpl();
  const auto& src_stat = src_impl.GetStat();
  auto for_each_src_bin = [&](auto&& bin_index_callback) {
    for (int src_bin = 1; src_bin <= (int)src_stat.sizeNoOver(); ++src_bin) {
      bin_index_callback(src_bin);
    }
    for (int src_bin = -1; src_bin >= -(int)src_stat.sizeUnderOver(); --src_bin) {
      bin_index_callback(src_bin);
    }
  };

  // Mechanism to turn a ROOT 7 global bin index into its ROOT 6 equivalent
  auto to_dest_bin = [&](const int src_bin) -> Int_t {
    // Convert to per-axis local bin coordinates
    auto local_bins = src_impl.GetLocalBins(src_bin);

    // Move to the ROOT 6 under/overflow bin indexing convention
    for (int dim = 0; dim < src.GetNDim(); ++dim) {
      if (local_bins[dim] == -1) {
        local_bins[dim] = 0;
      } else if (local_bins[dim] == -2) {
        local_bins[dim] = src_impl.GetAxis(dim).GetNBins() - 1;
      }
    }

    // Turn our local ROOT 6 coordinates into global ones
    if constexpr (src.GetNDim() == 1) {
      return dest.GetBin(local_bins[0]);
    } else if constexpr (src.GetNDim() == 2) {
      return dest.GetBin(local_bins[0], local_bins[1]);
    } else if constexpr (src.GetNDim() == 3) {
      return dest.GetBin(local_bins[0], local_bins[1], local_bins[2]);
    } else {
      throw std::runtime_error("Observed an unexpected histogram dimensionality");
    }
  };

  // With this, we can check that the bin contents and uncertainties were
  // transferred correctly from the ROOT 7 histogram to the ROOT 6 one.
  for_each_src_bin([&](int src_bin) {
    Int_t dest_bin = to_dest_bin(src_bin);
    ASSERT_CLOSE(src_stat.GetBinContent(src_bin),
                 dest.GetBinContent(dest_bin),
                 1e-6,
                 "Histogram bin content does not match");
    ASSERT_CLOSE(src_stat.GetBinUncertainty(src_bin),
                 dest.GetBinError(dest_bin),
                 1e-6,
                 "Histogram bin error does not match");
  });

  // Next we can check the global histogram stats
  ASSERT_EQ(src.GetEntries(), dest.GetEntries(), "Invalid entry count");
  std::array<Double_t, TH1::kNstat> stats;
  dest.GetStats(stats.data());
  auto sum_over_bins = [&](auto&& bin_contribution) -> double {
    double stat = 0.0;
    for_each_src_bin([&](int src_bin) {
      stat += bin_contribution(src_bin);
    });
    return stat;
  };
  // From the TH1 documentation, stats contains at least...
  // s[0]  = sumw       s[1]  = sumw2
  // s[2]  = sumwx      s[3]  = sumwx2
  //
  // NOTE: sumwx-style stats do not yield the same results in ROOT 6 and
  //       ROOT 7 when over- and underflow bins are filled up.
  //
  //       This does not seem fixable at the converter level, since ROOT 7
  //       always includes overflow bins in statistics, and ROOT 6 and ROOT 7
  //       disagree on what the coordinates of overflow bins are.
  //
  //       Given that those stats are somewhat meaningless in that situation
  //       anyway, I don't think achieving exact reproducibility should be a
  //       goal here, so I won't test for it.
  //
  const double sumw = sum_over_bins([&](int src_bin) -> double {
    return src_stat.GetBinContent(src_bin);
  });
  ASSERT_CLOSE(sumw, stats[0], 1e-6, "Sum of weights is incorrect");
  const double sumw2 = sum_over_bins([&](int src_bin) -> double {
    const double err = src_stat.GetBinUncertainty(src_bin);
    return err * err;
  });
  ASSERT_CLOSE(sumw2, stats[1], 1e-6, "Sum of squared error is incorrect");
  if (!has_overflow_data) {
    const double sumwx = sum_over_bins([&](int src_bin) -> double {
      const double x = src_impl.GetBinCenter(src_bin)[0];
      return src_stat.GetBinContent(src_bin) * x;
    });
    ASSERT_CLOSE(sumwx, stats[2], 1e-6, "Sum of weight * x is incorrect");
    const double sumwx2 = sum_over_bins([&](int src_bin) -> double {
      const double x = src_impl.GetBinCenter(src_bin)[0];
      return src_stat.GetBinContent(src_bin) * x * x;
    });
    ASSERT_CLOSE(sumwx2, stats[3], 1e-6, "Sum of weight * x^2 is incorrect");
  }
  // In TH2+, stats also contains...
  // s[4]  = sumwy      s[5]  = sumwy2   s[6]  = sumwxy
  if ((src.GetNDim() >= 2) && (!has_overflow_data)) {
    const double sumwy = sum_over_bins([&](int src_bin) -> double {
      const double y = src_impl.GetBinCenter(src_bin)[1];
      return src_stat.GetBinContent(src_bin) * y;
    });
    ASSERT_CLOSE(sumwy, stats[4], 1e-6, "Sum of weight * y is incorrect");
    const double sumwy2 = sum_over_bins([&](int src_bin) -> double {
      double y = src_impl.GetBinCenter(src_bin)[1];
      return src_stat.GetBinContent(src_bin) * y * y;
    });
    ASSERT_CLOSE(sumwy2, stats[5], 1e-6, "Sum of weight * y^2 is incorrect");
    const double sumwxy = sum_over_bins([&](int src_bin) -> double {
      const double x = src_impl.GetBinCenter(src_bin)[0];
      const double y = src_impl.GetBinCenter(src_bin)[1];
      return src_stat.GetBinContent(src_bin) * x * y;
    });
    ASSERT_CLOSE(sumwxy, stats[6], 1e-6, "Sum of weight * x * y is incorrect");
  }
  // In TH3, stats also contains...
  // s[7]  = sumwz      s[8]  = sumwz2   s[9]  = sumwxz   s[10]  = sumwyz
  if ((src.GetNDim() == 3) && (!has_overflow_data)) {
    const double sumwz = sum_over_bins([&](int src_bin) -> double {
      const double z = src_impl.GetBinCenter(src_bin)[2];
      return src_stat.GetBinContent(src_bin) * z;
    });
    ASSERT_CLOSE(sumwz, stats[7], 1e-6, "Sum of weight * z is incorrect");
    const double sumwz2 = sum_over_bins([&](int src_bin) -> double {
      double z = src_impl.GetBinCenter(src_bin)[2];
      return src_stat.GetBinContent(src_bin) * z * z;
    });
    ASSERT_CLOSE(sumwz2, stats[8], 1e-6, "Sum of weight * z^2 is incorrect");
    const double sumwxz = sum_over_bins([&](int src_bin) -> double {
      const double x = src_impl.GetBinCenter(src_bin)[0];
      const double z = src_impl.GetBinCenter(src_bin)[2];
      return src_stat.GetBinContent(src_bin) * x * z;
    });
    ASSERT_CLOSE(sumwxz, stats[9], 1e-6, "Sum of weight * x * z is incorrect");
    const double sumwyz = sum_over_bins([&](int src_bin) -> double {
      const double y = src_impl.GetBinCenter(src_bin)[1];
      const double z = src_impl.GetBinCenter(src_bin)[2];
      return src_stat.GetBinContent(src_bin) * y * z;
    });
    ASSERT_CLOSE(sumwyz, stats[10], 1e-6, "Sum of weight * y * z is incorrect");
  }
}


template <int DIMS,
          class PRECISION,
          template <int D_, class P_> class... STAT>
void test_conversion(RNG& rng,
                     std::array<RExp::RAxisConfig, DIMS>&& axis_configs)
{
  // Make a ROOT 7 histogram
  const std::string title = gen_hist_title(rng);
  using Source = RExp::RHist<DIMS, PRECISION, STAT...>;
  Source src(title, axis_configs);
  const auto& src_impl = *src.GetImpl();

  // Fill it with some test data
  const TestData<DIMS, typename Source::Weight_t> data(rng, src_impl);
  if (!data.weights.empty()) {
    src.FillN(data.coords, data.weights);
  } else {
    src.FillN(data.coords);
  }

  // Convert the ROOT 7 histogram to ROOT 6 format and check the output
  try
  {
    // Perform the conversion
    const std::string name = gen_unique_hist_name();
    auto dest = into_root6_hist(src, name.c_str());

    // Check the output histogram is configured like the input one
    check_hist_config<DIMS>(src_impl, name, dest);

    // Check that the output histogram contains the same data as the input one
    check_hist_data(src, data.exercizes_overflow, dest);
  }
  catch (const std::runtime_error& e)
  {
    // Print exception text
    std::cout << "\nHistogram conversion error: " << e.what() << std::endl;

    // Use GCC ABI to print histogram type
    char * hist_type_name;
    int status;
    hist_type_name = abi::__cxa_demangle(typeid(src).name(), 0, 0, &status);
    std::cout << "* Histogram type was " << hist_type_name << std::endl;
    free(hist_type_name);

    // Print input axis configuration
    std::cout << "* Axis configuration was..." << std::endl;
    for (int axis = 0; axis < DIMS; ++axis) {
      std::cout << "  - " << static_cast<char>('X' + axis) << ": ";
      print_axis_config(src_impl.GetAxis(axis));
      std::cout << std::endl;
    }

    // Print the test data that the histogram was filled with
    std::cout << "* Histogram was filled with ";
    data.print();
    std::cout << std::endl;

    // Time to die...
    std::cout << std::endl;
    throw std::runtime_error("Found a failing histogram conversion");
  }
}