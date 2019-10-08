#include <algorithm>
#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <typeinfo>

#include "histConv.hpp"
#include "histConvTests.hpp"


// Assert that instantiations are available for all basic histogram types
//
// TODO: Turn those into non-extern template instantiations if we decide to drop
//       some explicit instantiations from the histConv backend.
//
extern template auto into_root6_hist(const RExp::RHist<1, Char_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<1, Short_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<1, Int_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<1, Float_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<1, Double_t>& src,
                                     const char* name);
//
extern template auto into_root6_hist(const RExp::RHist<2, Char_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<2, Short_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<2, Int_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<2, Float_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<2, Double_t>& src,
                                     const char* name);
//
extern template auto into_root6_hist(const RExp::RHist<3, Char_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<3, Short_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<3, Int_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<3, Float_t>& src,
                                     const char* name);
extern template auto into_root6_hist(const RExp::RHist<3, Double_t>& src,
                                     const char* name);


int main() {
  // For the most part, we'll use reproducible but pseudo-random test data...
  RNG rng;

  // Anyway, let's start the test
  for (size_t i = 0; i < NUM_TEST_RUNS; ++i) {
    // Works (Minimal stats that we need)
    test_conversion<1, char, RExp::RHistStatContent>({gen_axis_config(rng)});

    // Also works (ROOT 7 implicitly adds an RHistStatContent here)
    test_conversion<1, char>({gen_axis_config(rng)});

    // Also works (More stats than actually needed)
    test_conversion<1,
                    char,
                    RExp::RHistStatContent,
                    RExp::RHistDataMomentUncert>({gen_axis_config(rng)});

    // Fails at compile time: insufficient stats. We fail fast, ROOT doesn't.
    /* test_conversion<1,
                       char,
                       RExp::RHistDataMomentUncert>({gen_axis_config(rng)}); */
    /* test_conversion<1,
                       char,
                       RExp::RHistStatUncertainty,
                       RExp::RHistDataMomentUncert>({gen_axis_config(rng)}); */

    // Data types other than char work just as well, if supported by ROOT 6
    test_conversion<1, short>({gen_axis_config(rng)});
    test_conversion<1, int>({gen_axis_config(rng)});
    test_conversion<1, float>({gen_axis_config(rng)});
    test_conversion<1, double>({gen_axis_config(rng)});

    // Fails at compile time: data type not supported by ROOT 6. We fail fast.
    /* test_conversion<1, size_t>({gen_axis_config(rng)}); */

    // FIXME: Higher-dimensional histograms are broken for now. I need to have
    //        a chat with the ROOT team to figure out if they _really_ think
    //        that providing local coordinates in an order that's reversed wrt
    //        the one specified at axis creation is a good idea in order to
    //        determine how to best fix that.
    /* // Try it with a 2D histogram
    test_conversion<2, char>({gen_axis_config(rng), gen_axis_config(rng)});

    // Try it with a 3D histogram
    auto axis1 = gen_axis_config(rng);
    auto axis2 = gen_axis_config(rng);
    while (axis2.GetKind() != axis1.GetKind()) axis2 = gen_axis_config(rng);
    auto axis3 = gen_axis_config(rng);
    while (axis3.GetKind() != axis1.GetKind()) axis3 = gen_axis_config(rng);
    test_conversion<3, char>({axis1, axis2, axis3}); */

    // Will fail at run time: TH3 only supports homogeneous axis configs
    /* test_conversion<3, char>({gen_axis_config(rng),
                                 gen_axis_config(rng),
                                 gen_axis_config(rng)}); */
  }

  return 0;
}


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


RExp::RAxisConfig gen_axis_config(RNG& rng) {
  // Cross-check that RNG is alright and set up some useful consts
  constexpr Int_t NUM_BINS_CHOICE =
    NUM_BINS_RANGE.second - NUM_BINS_RANGE.first;
  constexpr auto RNG_CHOICE = RNG::max() - RNG::min();
  static_assert(RNG_CHOICE > NUM_BINS_CHOICE, "Unexpectedly small RNG range");

  // Generate number of histogram bins
  Int_t num_bins = NUM_BINS_RANGE.first + (rng() - rng.min()) % NUM_BINS_CHOICE;

  // Some random number generation helpers
  auto gen_axis_title = [&rng]() -> std::string {
    return "Axis " + std::to_string(rng());
  };
  // FIXME: Re-enable labels once they're less broken in ROOT
  /* auto gen_axis_label = [&rng]() -> std::string {
    return std::to_string(rng());
  }; */
  auto gen_bool = [&rng]() -> bool {
    return (rng() > rng.min() + RNG_CHOICE / 2);
  };
  auto gen_float = [&rng](Double_t min, Double_t max) -> Double_t {
    return min + (rng() - rng.min()) * (max - min) / (rng.max() - rng.min());
  };

  // Decide if histogram should have a title
  bool has_title = gen_bool();

  // Generate an axis configuration
  switch (rng() % 2 /* FIXME: 3 with labels */) {
  // Equidistant axis (includes growable)
  case 0: {
    Double_t min = gen_float(AXIS_LIMIT_RANGE.first,
                              AXIS_LIMIT_RANGE.second);
    Double_t max = gen_float(min, AXIS_LIMIT_RANGE.second);
    bool is_growable = false; /* FIXME: should call gen_bool(), but growable
                                        axes are broken at the ROOT level */

    // FIXME: Re-enable growable axes once ROOT fixes them.
    /* if (is_growable) {
      if (has_title) {
        return RExp::RAxisConfig(gen_axis_title(),
                                 RExp::RAxisConfig::Grow,
                                 num_bins,
                                 min,
                                 max);
      } else {
        return RExp::RAxisConfig(RExp::RAxisConfig::Grow,
                                 num_bins,
                                 min,
                                 max);
      }
    } else { */
      if (has_title) {
        return RExp::RAxisConfig(gen_axis_title(),
                                 num_bins,
                                 min,
                                 max);
      } else {
        return RExp::RAxisConfig(num_bins,
                                 min,
                                 max);
      }
    /* } */
  }

  // Irregular axis
  case 1: {
    std::vector<Double_t> bin_borders;
    for (Int_t i = 0; i < num_bins+1; ++i) {
      bin_borders.push_back(
        gen_float(AXIS_LIMIT_RANGE.first, AXIS_LIMIT_RANGE.second)
      );
    }
    std::sort(bin_borders.begin(), bin_borders.end());

    if (has_title) {
      return RExp::RAxisConfig(gen_axis_title(),
                               std::move(bin_borders));
    } else {
      return RExp::RAxisConfig(std::move(bin_borders));
    }
  }

  // Labeled axis (FIXME: Currently broken at the ROOT level)
  /* case 2: {
    std::vector<std::string> labels;
    for (Int_t i = 0; i < num_bins; ++i) {
      labels.push_back(gen_axis_label());
    }

    if (has_title) {
      return RExp::RAxisConfig(gen_axis_title(),
                               std::move(labels));
    } else {
      return RExp::RAxisConfig(std::move(labels));
    }
  } */

  default:
    throw std::runtime_error("There's a bug in this switch, please fix it.");
  }
}


void print_axis_config(const RExp::RAxisConfig& axis_config) {
  // Common subset of all axis configuration displays
  auto print_header = [&axis_config]() {
    std::cout << " (" << axis_config.GetNBins() << " bins inc. overflow";
  };

  // Common subset of equidistant/growable axes display
  auto print_equidistant = [&axis_config, &print_header]() {
    print_header();
    std::cout << " from " << axis_config.GetBinBorders()[0]
              << " to " << axis_config.GetBinBorders()[1]
              << ')';
  };

  // Recipe for partial printout of std::vector-like data
  auto print_vector = [](const auto& vector, auto&& elem_printer) {
    std::cout << " { ";
    for (size_t i = 0; i < std::min(3ul, vector.size()-1); ++i) {
      elem_printer(vector[i]);
      std::cout << ", ";
    }
    if (vector.size() > 6) {
      std::cout << "..., ";
    }
    for (size_t i = std::max(3ul, vector.size()-4); i < vector.size()-1; ++i) {
      elem_printer(vector[i]);
      std::cout << ", ";
    }
    elem_printer(vector[vector.size()-1]);
    std::cout << " })";
  };

  // And now we get into the specifics of individual axis types.
  switch (axis_config.GetKind())
  {
  case RExp::RAxisConfig::kEquidistant:
    std::cout << "Equidistant";
    print_equidistant();
    break;

  case RExp::RAxisConfig::kGrow:
    std::cout << "Growable";
    print_equidistant();
    break;

  case RExp::RAxisConfig::kIrregular:
    std::cout << "Irregular";
    print_header();
    std::cout << " with borders";
    print_vector(axis_config.GetBinBorders(),
                 []( Double_t border ) { std::cout << border; });
    break;

  case RExp::RAxisConfig::kLabels:
    std::cout << "Labeled";
    print_header();
    std::cout << " with labels";
    print_vector(axis_config.GetBinLabels(),
                 []( const std::string& label ) {
                   std::cout << '"' << label << '"';
                 });
    break;

  default:
    throw std::runtime_error("Unsupported axis kind, please fix it.");
  }
}