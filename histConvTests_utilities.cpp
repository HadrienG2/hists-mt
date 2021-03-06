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
  // FIXME: Cannot test labeled axes yet as the RHist constructor will blow up
  //        when we try to use them.
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

    // FIXME: Growable axes cannot be used in ROOT 7 yet as the axis growth
    //        function is not even implemented yet... re-enable this once ROOT 7
    //        has reasonably feature-complete working growable axes.
    /* bool is_growable = gen_bool(rng);
    if (is_growable) {
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

  // Labeled axis (FIXME: Re-enable once they work at the ROOT 7 level)
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


std::string gen_unique_hist_name() {
  static std::atomic<size_t> ctr = 0;

  size_t last_ctr = ctr.fetch_add(1, std::memory_order_relaxed);
  std::string name = "Histogram" + std::to_string(last_ctr);
  if (last_ctr == std::numeric_limits<size_t>::max()) {
    throw std::runtime_error("Unique ROOT 6 histogram name pool was exhausted");
  }

  return name;
}


std::string gen_hist_title(RNG& rng) {
  switch (rng() % 3) {
  case 0:
    return "";
  case 1:
    return "Hist title without semicolons";
  case 2:
    return "Hist;title;with;semicolons";
  default:
    throw std::runtime_error("This switch statement is wrong, please fix it");
  }
}


void print_axis_config(const RExp::RAxisBase& axis) {
  // Things which we display for all axis types
  std::cout << (axis.CanGrow() ? "G" : "Non-g") << "rowable, "
            << axis.GetNBinsNoOver() << " bins exc. overflow";

  // Recipe for partial printout of std::vector-like data
  auto print_vector = [](const auto& vector, auto&& elem_printer) {
    std::cout << "{ ";
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
    std::cout << " }";
  };

  // Specifics of equidistant (and growable) axes
  const auto eq_axis = dynamic_cast<const RExp::RAxisEquidistant*>(&axis);
  if (eq_axis != nullptr) {
    std::cout << ", equidistant from " << axis.GetMinimum() << " to "
              << axis.GetMaximum();
  }

  // Specifics of irregular axes
  const auto irr_axis = dynamic_cast<const RExp::RAxisIrregular*>(&axis);
  if (irr_axis != nullptr) {
    std::cout << ", irregular with borders ";
    print_vector(irr_axis->GetBinBorders(),
                 []( double border ) { std::cout << border; });
  }

  // Specifics of labeled axes
  const auto lab_axis = dynamic_cast<const RExp::RAxisLabels*>(&axis);
  if (lab_axis != nullptr) {
    std::cout << ", with bin labels ";
    print_vector(lab_axis->GetBinLabels(),
                 []( const auto label ) {
                   std::cout << '"' << label << '"';
                 });
  }

  // Warn about unsupported axis types
  if ((eq_axis == nullptr) && (irr_axis == nullptr)) {
    throw std::runtime_error("Unsupported axis kind, please fix this code.");
  }
}


void assert_runtime_error(std::function<void()>&& operation,
                          std::string&& failure_message)
{
  bool failed = false;
  try {
    operation();
  } catch(const std::runtime_error&) {
    failed = true;
  }
  if (!failed) {
    throw std::runtime_error(failure_message);
  }
}


void check_axis_config(const RExp::RAxisBase& src, TAxis& dest) {
  // Checks which are common to all axis configurations
  ASSERT_EQ(src.GetTitle(), dest.GetTitle(),
            "Incorrect output axis title");
  ASSERT_EQ(src.GetNBinsNoOver(), dest.GetNbins(),
            "Incorrect number of bins");
  ASSERT_EQ(src.CanGrow(), dest.CanExtend(),
            "Axis growability does not match");

  // Checks which are specific to RAxisEquidistant and RAxisGrow
  const auto src_eq = dynamic_cast<const RExp::RAxisEquidistant*>(&src);
  if (src_eq != nullptr) {
    ASSERT_CLOSE(src.GetMinimum(), dest.GetXmin(), 1e-6,
                 "Axis minimum does not match");
    ASSERT_CLOSE(src.GetMaximum(), dest.GetXmax(), 1e-6,
                 "Axis maximum does not match");
  }

  // Checks which are specific to irregular axes
  const auto src_irr = dynamic_cast<const RExp::RAxisIrregular*>(&src);
  const bool is_irregular = (src_irr != nullptr);
  ASSERT_EQ(is_irregular,
            dest.IsVariableBinSize(),
            "Irregular ROOT 7 axes (only) should have variable bins");
  if (is_irregular) {
    const auto& bins = *dest.GetXbins();
    const auto& expected_bins = src_irr->GetBinBorders();
    ASSERT_EQ(size_t(bins.fN), expected_bins.size(),
              "Wrong number of bin borders");
    for (int i = 0; i < bins.fN; ++i) {
      ASSERT_EQ(bins[i], expected_bins[i], "Wrong bin border values");
    }
  }

  // Checks which are specific to labeled axes
  const auto src_lab = dynamic_cast<const RExp::RAxisLabels*>(&src);
  bool is_labeled = (src_lab != nullptr);
  ASSERT_EQ(is_labeled,
            dest.CanBeAlphanumeric() || dest.IsAlphanumeric(),
            "Labeled ROOT 7 axes (only) should be alphanumeric");
  if (is_labeled) {
    auto labels_ptr = dest.GetLabels();
    ASSERT_NOT_NULL(labels_ptr, "Labeled axes should have labels");

    auto labels_iter_ptr = labels_ptr->MakeIterator();
    const auto expected_labels = src_lab->GetBinLabels();
    auto expected_labels_iter = expected_labels.cbegin();
    TObject* label_ptr;
    size_t num_labels = 0;

    while ((label_ptr = labels_iter_ptr->Next())
           && (expected_labels_iter != expected_labels.cend())) {
      num_labels += 1;
      const auto& label = dynamic_cast<const TObjString&>(*label_ptr);
      std::string expected_label_str(*expected_labels_iter);
      ASSERT_EQ(expected_label_str.c_str(), label.GetString(),
                "Some axis labels do not match");
      ++expected_labels_iter;
    }

    ASSERT_EQ(expected_labels.size(), num_labels,
              "Number of axis labels does not match");
    delete labels_iter_ptr;
  }
}
