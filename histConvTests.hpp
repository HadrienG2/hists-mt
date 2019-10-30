// Common ROOT7 -> ROOT6 histogram conversion test harness definitions
// You only need histConvTests.hpp.dcl if you don't instantiate test_conversion
#include "histConvTests.hpp.dcl"

#include <atomic>
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
  // Generate a ROOT 7 histogram, testing both empty and semicolon-ridden titles
  const std::string title = gen_hist_title(rng);
  using Source = RExp::RHist<DIMS, PRECISION, STAT...>;
  Source src(title, axis_configs);

  // FIXME: Factor out some of this for the sake of readability and fast builds

  // Determine which coordinate range the test data should span
  using CoordArray = typename Source::CoordArray_t;
  using Weight = typename Source::Weight_t;
  std::pair<CoordArray, CoordArray> coord_range;
  const bool exercise_overflow_bins = gen_bool(rng);
  for (size_t axis_idx = 0; axis_idx < DIMS; ++axis_idx) {
    const auto& axis_config = axis_configs[axis_idx];
    switch (axis_config.GetKind())
    {
    case RExp::RAxisConfig::EKind::kEquidistant:
    case RExp::RAxisConfig::EKind::kGrow:
    case RExp::RAxisConfig::EKind::kIrregular: {
      const auto& bin_borders = axis_config.GetBinBorders();
      coord_range.first[axis_idx] = bin_borders[0];
      coord_range.second[axis_idx] = bin_borders[bin_borders.size()-1];
      if (exercise_overflow_bins) {
        const double bin_border_delta = bin_borders[1] - bin_borders[0];
        coord_range.first[axis_idx] -= bin_border_delta;
        coord_range.second[axis_idx] += bin_border_delta;
      }
      break;
    }

    case RExp::RAxisConfig::EKind::kLabels: {
      const auto& bin_labels = axis_config.GetBinLabels();
      coord_range.first[axis_idx] = 0;
      coord_range.second[axis_idx] = bin_labels.size();
      if (exercise_overflow_bins) {
        coord_range.first[axis_idx] -= 1;
        coord_range.second[axis_idx] += 1;
      }
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
  const bool variable_weight = gen_bool(rng);
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
    const std::string name = gen_unique_hist_name();
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
    check_axis_config(*dest.GetXaxis(), axis_configs[0]);
    if constexpr (DIMS >= 2) check_axis_config(*dest.GetYaxis(), axis_configs[1]);
    if constexpr (DIMS == 3) check_axis_config(*dest.GetZaxis(), axis_configs[2]);

    // Check global histogram stats
    ASSERT_EQ(coords.size(), dest.GetEntries(), "Invalid entry count");
    std::array<Double_t, TH1::kNstat> stats;
    dest.GetStats(stats.data());
    auto sum_over_bins = [&src](auto&& bin_contribution) -> double {
      double stat = 0.0;
      for (const auto& bin : src) {
        stat += bin_contribution(bin);
      }
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
    const double sumw = sum_over_bins([](const auto& bin) -> double {
      return bin.GetStat().GetContent();
    });
    ASSERT_CLOSE(sumw, stats[0], 1e-6, "Sum of weights is incorrect");
    const double sumw2 = sum_over_bins([](const auto& bin) -> double {
      const double err = bin.GetStat().GetUncertainty();
      return err * err;
    });
    ASSERT_CLOSE(sumw2, stats[1], 1e-6, "Sum of squared error is incorrect");
    if (!exercise_overflow_bins) {
      const double sumwx = sum_over_bins([](const auto& bin) -> double {
        return bin.GetStat().GetContent() * bin.GetCenter()[0];
      });
      ASSERT_CLOSE(sumwx, stats[2], 1e-6, "Sum of weight * x is incorrect");
      const double sumwx2 = sum_over_bins([](const auto& bin) -> double {
        const double x = bin.GetCenter()[0];
        return bin.GetStat().GetContent() * x * x;
      });
      ASSERT_CLOSE(sumwx2, stats[3], 1e-6, "Sum of weight * x^2 is incorrect");
    }
    // In TH2+, stats also contains...
    // s[4]  = sumwy      s[5]  = sumwy2   s[6]  = sumwxy
    if ((DIMS >= 2) && (!exercise_overflow_bins)) {
      const double sumwy = sum_over_bins([](const auto& bin) -> double {
        return bin.GetStat().GetContent() * bin.GetCenter()[1];
      });
      ASSERT_CLOSE(sumwy, stats[4], 1e-6, "Sum of weight * y is incorrect");
      const double sumwy2 = sum_over_bins([](const auto& bin) -> double {
        double y = bin.GetCenter()[1];
        return bin.GetStat().GetContent() * y * y;
      });
      ASSERT_CLOSE(sumwy2, stats[5], 1e-6, "Sum of weight * y^2 is incorrect");
      const double sumwxy = sum_over_bins([](const auto& bin) -> double {
        const double x = bin.GetCenter()[0];
        const double y = bin.GetCenter()[1];
        return bin.GetStat().GetContent() * x * y;
      });
      ASSERT_CLOSE(sumwxy, stats[6], 1e-6, "Sum of weight * x * y is incorrect");
    }
    // In TH3, stats also contains...
    // s[7]  = sumwz      s[8]  = sumwz2   s[9]  = sumwxz   s[10]  = sumwyz
    if ((DIMS == 3) && (!exercise_overflow_bins)) {
      const double sumwz = sum_over_bins([](const auto& bin) -> double {
        return bin.GetStat().GetContent() * bin.GetCenter()[2];
      });
      ASSERT_CLOSE(sumwz, stats[7], 1e-6, "Sum of weight * z is incorrect");
      const double sumwz2 = sum_over_bins([](const auto& bin) -> double {
        double z = bin.GetCenter()[2];
        return bin.GetStat().GetContent() * z * z;
      });
      ASSERT_CLOSE(sumwz2, stats[8], 1e-6, "Sum of weight * z^2 is incorrect");
      const double sumwxz = sum_over_bins([](const auto& bin) -> double {
        const double x = bin.GetCenter()[0];
        const double z = bin.GetCenter()[2];
        return bin.GetStat().GetContent() * x * z;
      });
      ASSERT_CLOSE(sumwxz, stats[9], 1e-6, "Sum of weight * x * z is incorrect");
      const double sumwyz = sum_over_bins([](const auto& bin) -> double {
        const double y = bin.GetCenter()[1];
        const double z = bin.GetCenter()[2];
        return bin.GetStat().GetContent() * y * z;
      });
      ASSERT_CLOSE(sumwyz, stats[10], 1e-6, "Sum of weight * y * z is incorrect");
    }

    // FIXME: Like the current converter code, this assumes that ROOT 6 and
    //        ROOT 7 histograms follow the same binning conventions. However,
    //        there are already two known cases of deviation:
    //
    //        - ROOT 6 histograms always have overflow bins, whereas growable
    //          ROOT 7 histograms don't have them. IIUC, this means that we need
    //          to check that the over- and underflow bins of growable ROOT 6
    //          histograms are zeroed... and to use local bin coordinates.
    //        - ROOT 7 multi-dimensional histograms seem to enumerate local bin
    //          coordinates in a different order, but the logic is strange
    //          enough that it could be a bug (e.g. local coordinates are given
    //          in reverse order wrt histogram constructor axis configurations).
    //
    //        Need to think about how to best handle this without sacrificing
    //        performance in the common case.
    //
    //        Having a way to get local bin coordinates while iterating a ROOT 7
    //        histogram could be helpful here...
    //
    size_t root6_bin = 0;
    for (const auto& bin: src) {
      ASSERT_CLOSE(bin.GetStat().GetContent(),
                   dest.GetBinContent(root6_bin),
                   1e-6,
                   "Histogram bin content does not match");
      ASSERT_CLOSE(bin.GetStat().GetUncertainty(),
                   dest.GetBinError(root6_bin),
                   1e-6,
                   "Histogram bin error does not match");
      ++root6_bin;
    }
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
    char axis_name = 'X';
    for (const auto& axis_config : axis_configs) {
      std::cout << "  - " << axis_name++ << ": ";
      print_axis_config(axis_config);
      std::cout << std::endl;
    }

    // Print coordinates and weights
    std::cout << "* Histogram was filled with the following "
              << coords.size() << " data points of "
              << (variable_weight ? "variable" : "unity") << " weight";
    if (exercise_overflow_bins) {
      std::cout << ", some of which exercized under- and overflow bins";
    }
    std::cout << ": { ";
    auto print_point = [&](size_t point) {
      std::cout << "[ ";
      for (size_t dim = 0; dim < DIMS-1; ++dim) {
        std::cout << coords[point][dim] << ", ";
      }
      std::cout << coords[point][DIMS-1] << " ]";
      if (!variable_weight) { return; }
      std::cout << " @ " << double(weights[point]);
    };
    for (size_t point = 0; point < coords.size()-1; ++point) {
      print_point(point);
      std::cout << "; ";
    }
    print_point(coords.size()-1);
    std::cout << " }" << std::endl;

    // Time to die...
    std::cout << std::endl;
    throw std::runtime_error("Found a failing histogram conversion");
  }
}