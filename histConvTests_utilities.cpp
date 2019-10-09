// ROOT7 -> ROOT6 histogram conversion test utilities
// Extracted from histConvTests.cpp to clean that file up and speed up builds

#include "ROOT/RAxis.hxx"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "histConvTests.hpp.dcl"


RExp::RAxisConfig gen_axis_config(RNG& rng) {
  // Cross-check that RNG is alright and set up some useful consts
  constexpr int NUM_BINS_CHOICE =
    NUM_BINS_RANGE.second - NUM_BINS_RANGE.first;
  constexpr auto RNG_CHOICE = RNG::max() - RNG::min();
  static_assert(RNG_CHOICE > NUM_BINS_CHOICE, "Unexpectedly small RNG range");

  // Generate number of histogram bins
  int num_bins = NUM_BINS_RANGE.first + (rng() - rng.min()) % NUM_BINS_CHOICE;

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
  auto gen_float = [&rng](double min, double max) -> double {
    return min + (rng() - rng.min()) * (max - min) / (rng.max() - rng.min());
  };

  // Decide if histogram should have a title
  bool has_title = gen_bool();

  // Generate an axis configuration
  switch (rng() % 2 /* FIXME: 3 with labels */) {
  // Equidistant axis (includes growable)
  case 0: {
    double min = gen_float(AXIS_LIMIT_RANGE.first,
                              AXIS_LIMIT_RANGE.second);
    double max = gen_float(min, AXIS_LIMIT_RANGE.second);
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
    std::vector<double> bin_borders;
    for (int i = 0; i < num_bins+1; ++i) {
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
    for (int i = 0; i < num_bins; ++i) {
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
                 []( double border ) { std::cout << border; });
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