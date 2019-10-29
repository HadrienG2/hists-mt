// ROOT7 -> ROOT6 histogram conversion test utilities
// Extracted from histConvTests.cpp to clean that file up and speed up builds

#include "ROOT/RAxis.hxx"
#include "TAxis.h"
#include "THashList.h"
#include "TObjString.h"

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
  static_assert(RNG_CHOICE > NUM_BINS_CHOICE, "Unexpectedly small RNG range");

  // Generate number of histogram bins
  int num_bins = NUM_BINS_RANGE.first + (rng() - rng.min()) % NUM_BINS_CHOICE;

  // Some random number generation helpers
  auto gen_axis_title = [](RNG& rng) -> std::string {
    return "Axis " + std::to_string(rng());
  };
  // FIXME: Re-enable labels once they're less broken in ROOT
  /* auto gen_axis_label = [](RNG& rng) -> std::string {
    return std::to_string(rng());
  }; */

  // Decide if histogram axis should have a title
  bool has_title = gen_bool(rng);

  // Generate an axis configuration
  switch (rng() % 2 /* FIXME: 3 with labels */) {
  // Equidistant axis (includes growable)
  case 0: {
    double min = gen_double(rng,
                            AXIS_LIMIT_RANGE.first,
                            AXIS_LIMIT_RANGE.second);
    double max = gen_double(rng, min, AXIS_LIMIT_RANGE.second);
    bool is_growable = false; /* FIXME: should call gen_bool(rng), but growable
                                        axes are broken at the ROOT level */

    // FIXME: Re-enable growable axes once ROOT fixes them.
    /* if (is_growable) {
      if (has_title) {
        return RExp::RAxisConfig(gen_axis_title(rng),
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
        return RExp::RAxisConfig(gen_axis_title(rng),
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
        gen_double(rng, AXIS_LIMIT_RANGE.first, AXIS_LIMIT_RANGE.second)
      );
    }
    std::sort(bin_borders.begin(), bin_borders.end());

    if (has_title) {
      return RExp::RAxisConfig(gen_axis_title(rng),
                               std::move(bin_borders));
    } else {
      return RExp::RAxisConfig(std::move(bin_borders));
    }
  }

  // Labeled axis (FIXME: Currently broken at the ROOT level)
  /* case 2: {
    std::vector<std::string> labels;
    for (int i = 0; i < num_bins; ++i) {
      labels.push_back(gen_axis_label(rng));
    }

    if (has_title) {
      return RExp::RAxisConfig(gen_axis_title(rng),
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


void check_axis_config(TAxis& axis, const RExp::RAxisConfig& config) {
  // Checks which are common to all axis configurations
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
}
