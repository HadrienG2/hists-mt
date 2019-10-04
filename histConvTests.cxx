#include <random>

#include "histConv.hxx"


// Test tuning knobs
using RNG = std::mt19937_64;
constexpr std::pair<Int_t, Int_t> NUM_BINS_RANGE{1, 1000};
constexpr std::pair<Double_t, Double_t> AXIS_LIMIT_RANGE{-12345.6, 1928.37};
constexpr size_t NUM_TEST_RUNS = 1000;

// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


// Make sure that the converter can be instantiated for all supported histogram types
template auto into_root6_hist(const RExp::RHist<1, Char_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Short_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Int_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Float_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Double_t>& src, const char* name);
//
template auto into_root6_hist(const RExp::RHist<2, Char_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Short_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Int_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Float_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Double_t>& src, const char* name);
//
template auto into_root6_hist(const RExp::RHist<3, Char_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Short_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Int_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Float_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Double_t>& src, const char* name);


// Generate a random ROOT 7 axis configuration
RExp::RAxisConfig gen_axis_config(RNG& rng) {
    // Cross-check that RNG is alright and set up some useful consts
    constexpr Int_t NUM_BINS_CHOICE =
      NUM_BINS_RANGE.second - NUM_BINS_RANGE.first;
    constexpr auto RNG_CHOICE = RNG::max() - RNG::min();
    static_assert(RNG_CHOICE > NUM_BINS_CHOICE, "Unexpectedly small RNG range");

    // Generate number of histogram bins
    Int_t num_bins =
        NUM_BINS_RANGE.first + (rng() - rng.min()) % NUM_BINS_CHOICE;

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
      bool is_growable = gen_bool();

      if (is_growable) {
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
      } else {
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
      }
    }

    // Irregular axis
    case 1: {
      // This method will yield ~logarithmic bin spacing
      // (if that doesn't work, bias more towards regular binning)
      std::vector<Double_t> bin_borders(
        1,
        gen_float(AXIS_LIMIT_RANGE.first, AXIS_LIMIT_RANGE.second)
      );
      for (Int_t i = 1; i < num_bins; ++i) {
        bin_borders.push_back(
          gen_float(bin_borders[i-1], AXIS_LIMIT_RANGE.second)
        );
      }

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


// Test for the conversion machinery
int main() {
  // For the most part, we'll use randomized testing...
  RNG rng;

  // ...but ROOT 6 is picky about unique names
  size_t ctr = 0;
  auto convert_to_root6 = [&ctr](auto source) -> auto {
    ctr += 1;
    std::string name = "Hist" + std::to_string(ctr);
    return into_root6_hist(source, name.c_str());
  };

  // Anyway, let's start the test
  for (size_t i = 0; i < NUM_TEST_RUNS; ++i) {
    // Works (Minimal stats that we need)
    RExp::RHist<1, char, RExp::RHistStatContent> s1(gen_axis_config(rng));
    auto d1 = convert_to_root6(s1);

    // Also works (ROOT 7 implicitly adds an RHistStatContent here)
    RExp::RHist<1, char> s2(gen_axis_config(rng));
    auto d2 = convert_to_root6(s2);

    // Also works (More stats than actually needed)
    RExp::RHist<1,
                char,
                RExp::RHistStatContent,
                RExp::RHistDataMomentUncert> s3(gen_axis_config(rng));
    auto d3 = convert_to_root6(s3);

    // Compilation errors: insufficient stats. Unlike ROOT, we fail fast.
    /* RExp::RHist<1,
                   char,
                   RExp::RHistDataMomentUncert> s4(gen_axis_config(rng));
    auto d4 = convert_to_root6(s4); */
    /* RExp::RHist<1,
                   char,
                   RExp::RHistStatUncertainty,
                   RExp::RHistDataMomentUncert> s4(gen_axis_config(rng));
    auto d4 = convert_to_root6(s4); */

    // Data types other than char work just as well, if supported by ROOT 6
    RExp::RHist<1, short> s5(gen_axis_config(rng));
    auto d5 = convert_to_root6(s5);
    RExp::RHist<1, int> s6(gen_axis_config(rng));
    auto d6 = convert_to_root6(s6);
    RExp::RHist<1, float> s7(gen_axis_config(rng));
    auto d7 = convert_to_root6(s7);
    RExp::RHist<1, double> s8(gen_axis_config(rng));
    auto d8 = convert_to_root6(s8);

    // Compilation error: data type not supported by ROOT 6. We fail fast.
    /* RExp::RHist<1, size_t> s9(gen_axis_config(rng));
    auto d9 = convert_to_root6(s9); */

    // Try it with a 2D histogram
    RExp::RHist<2, char> s10(gen_axis_config(rng), gen_axis_config(rng));
    auto d10 = convert_to_root6(s10);

    // Try it with a 3D histogram
    auto axis1 = gen_axis_config(rng);
    auto axis2 = gen_axis_config(rng);
    while (axis2.GetKind() != axis1.GetKind()) axis2 = gen_axis_config(rng);
    auto axis3 = gen_axis_config(rng);
    while (axis3.GetKind() != axis1.GetKind()) axis3 = gen_axis_config(rng);
    RExp::RHist<3, char> s11(axis1, axis2,  axis3);
    auto d11 = convert_to_root6(s11);

    // TODO: Also test the failing case with diverging axis kinds
  }

  return 0;
}