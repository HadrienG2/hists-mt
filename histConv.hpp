// ROOT7 -> ROOT6 histogram converter (full header)
//
// You only need to use this header if you want to instantiate into_root_6_hist
// for a ROOT7 histogram with a custom statistics configuration (that is, a
// RHist<DIMS, T, STAT...> histogram with a non-empty STAT... list).
//
// See histConv.hpp.dcl for the basic declarations, which may be all you need.

#include "histConv.hpp.dcl"

#include "ROOT/RAxis.hxx"
#include "ROOT/RHist.hxx"
#include "ROOT/RHistImpl.hxx"
#include "TAxis.h"

#include <cxxabi.h>
#include <exception>
#include <sstream>
#include <string>
#include <tuple>
#include <typeinfo>
#include <utility>


namespace detail
{
  // === BUILD A THx, FAILING AT RUNTIME IF NO CONSTRUCTOR EXISTS ===

  // TH1 and TH2 constructors enable building histograms with all supported
  // axes configurations. TH3, however, only provide constructors for
  // all-equidistant and all-irregular axis configurations.
  //
  // We do not know the axis configuration of a ROOT 7 histogram until runtime,
  // therefore we must fail at runtime when a ROOT 7 histogram with incompatible
  // axis configuration is requested.
  //
  // So, in the general case, used by TH1 and TH2, we just build a ROOT 6
  // histogram with the specified constructor parameters...
  //
  template <int DIMS>
  struct MakeRoot6Hist
  {
    template <typename Output, typename... BuildParams>
    static Output make(std::tuple<BuildParams...>&& build_params) {
      return std::make_from_tuple<Output>(std::move(build_params));
    }
  };

  // ...while in the TH3 case, we detect incompatible axis configurations and
  // fail at runtime upon encountering them.
  template <>
  struct MakeRoot6Hist<3>
  {
    // Generally speaking, we fail at runtime...
    //
    // FIXME: Instead of failing, build with wrong axis configuration and adapt
    //        to the actual configuration after construction
    template <typename Output, typename... BuildParams>
    static Output make(std::tuple<BuildParams...>&& th3_params) {
      std::ostringstream s;
      char * args_type_name;
      int status;
      args_type_name = abi::__cxa_demangle(typeid(th3_params).name(),
                                           0,
                                           0,
                                           &status);
      s << "Unsupported TH3 axis configuration"
        << " (no constructor from argument-tuple " << args_type_name
        << ')';
      free(args_type_name);
      throw std::runtime_error(s.str());
    }

    // ...except in the two cases where it actually works!
    template <typename Output>
    static Output make(std::tuple<const char*, const char*,
                                  Int_t, Double_t, Double_t,
                                  Int_t, Double_t, Double_t,
                                  Int_t, Double_t, Double_t>&& th3_params) {
      return std::make_from_tuple<Output>(std::move(th3_params));
    }
    template <typename Output>
    static Output make(std::tuple<const char*, const char*,
                                  Int_t, const Double_t*,
                                  Int_t, const Double_t*,
                                  Int_t, const Double_t*>&& th3_params) {
      return std::make_from_tuple<Output>(std::move(th3_params));
    }
  };


  // === MISC UTILITIES TO EASE THE ROOT7-ROOT6 IMPEDANCE MISMATCH ===

  // Convert a ROOT 7 histogram title into a ROOT 6 histogram title
  std::string convert_hist_title(const std::string& title);

  // Map from ROOT 7 axis index to ROOT 6 histogram axes
  TAxis& get_root6_axis(TH1& hist, size_t idx);
  TAxis& get_root6_axis(TH2& hist, size_t idx);
  TAxis& get_root6_axis(TH3& hist, size_t idx);

  // ROOT 7-like GetBinIndexFromLocalBins for ROOT 6
  inline Int_t get_bin_idx_from_local_root6(const TH1& hist,
                                            const std::array<Int_t, 1>& bins) {
    return hist.GetBin(bins[0]);
  }
  inline Int_t get_bin_idx_from_local_root6(const TH2& hist,
                                            const std::array<Int_t, 2>& bins) {
    return hist.GetBin(bins[0], bins[1]);
  }
  inline Int_t get_bin_idx_from_local_root6(const TH3& hist,
                                            const std::array<Int_t, 3>& bins) {
    return hist.GetBin(bins[0], bins[1], bins[2]);
  }

  // Transfer histogram axis settings which are common to all axis
  // configurations (currently equidistant, growable, irregular and labels)
  void setup_axis_base(TAxis& dest, const RExp::RAxisBase& src);


  // === MAIN CONVERSION FUNCTIONS ===

  // Shorthand for an excessively long name
  template <int DIMS>
  using RHistImplPABase = RExp::Detail::RHistImplPrecisionAgnosticBase<DIMS>;

  // Create a ROOT 6 histogram whose global and per-axis configuration matches
  // that of an input ROOT 7 histogram as closely as possible.
  template <class Output, int AXIS, int DIMS, class... BuildParams>
  Output convert_hist_loop(const RHistImplPABase<DIMS>& src_impl,
                           std::tuple<BuildParams...>&& build_params) {
    // This function is actually a kind of recursive loop for AXIS ranging
    // from 0 to the dimension of the histogram, inclusive.
    if constexpr (AXIS < DIMS) {
      // The first iterations query the input histogram axes one by one
      const auto& axis_view = src_impl.GetAxis(AXIS);

      // Is this an equidistant axis?
      const auto* eq_axis_ptr =
        dynamic_cast<const RExp::RAxisEquidistant*>(&axis_view);
      if (eq_axis_ptr != nullptr) {
        const auto& eq_axis = *eq_axis_ptr;

        // Append equidistant axis constructor parameters to the list of
        // ROOT 6 histogram constructor parameters
        auto new_build_params =
          std::tuple_cat(
            std::move(build_params),
            std::make_tuple((Int_t)(eq_axis.GetNBinsNoOver()),
                            (Double_t)(eq_axis.GetMinimum()),
                            (Double_t)(eq_axis.GetMaximum()))
          );

        // Process other axes and construct the histogram
        auto dest =
          convert_hist_loop<Output,
                            AXIS+1>(src_impl,
                                    std::move(new_build_params));

        // Propagate basic axis properties
        auto& dest_axis = get_root6_axis(dest, AXIS);
        setup_axis_base(dest_axis, eq_axis);

        // If the axis is labeled, propagate labels
        const auto* lbl_axis_ptr =
          dynamic_cast<const RExp::RAxisLabels*>(&eq_axis);
        if (lbl_axis_ptr) {
          dest_axis.SetNoAlphanumeric(false);
          auto labels = lbl_axis_ptr->GetBinLabels();
          for (size_t bin = 0; bin < labels.size(); ++bin) {
              std::string label{labels[bin]};
              dest_axis.SetBinLabel(bin, label.c_str());
          }
        } else {
          dest_axis.SetNoAlphanumeric(true);
        }

        // Send back the histogram to caller
        return dest;
      }

      // Is this an irregular axis?
      const auto* irr_axis_ptr =
        dynamic_cast<const RExp::RAxisIrregular*>(&axis_view);
      if (irr_axis_ptr != nullptr) {
        const auto& irr_axis = *irr_axis_ptr;

        // Append irregular axis constructor parameters to the list of
        // ROOT 6 histogram constructor parameters
        auto new_build_params =
          std::tuple_cat(
            std::move(build_params),
            std::make_tuple(
              (Int_t)(irr_axis.GetNBinsNoOver()),
              (const Double_t*)(irr_axis.GetBinBorders().data())
            )
          );

        // Process other axes and construct the histogram
        auto dest =
          convert_hist_loop<Output,
                            AXIS+1>(src_impl,
                                    std::move(new_build_params));

        // Propagate basic axis properties
        auto& dest_axis = get_root6_axis(dest, AXIS);
        setup_axis_base(dest_axis, irr_axis);

        // Only RAxisLabels can have labels as of ROOT 6.18
        dest_axis.SetNoAlphanumeric(true);

        // Send back the histogram to caller
        return dest;
      }

      // As of ROOT 6.18.0, there should be no other axis kind, so
      // reaching this point indicates a bug in the code.
      throw std::runtime_error("Unsupported histogram axis type");
    } else if constexpr (AXIS == DIMS) {
      // We've reached the bottom of the histogram construction recursion.
      // All histogram constructor parameters have been collected in the
      // build_params tuple, so we can now construct the ROOT 6 histogram.
      return MakeRoot6Hist<DIMS>::template make<Output>(
        std::move(build_params)
      );
    } else {
      // The loop shouldn't reach this point, there's a bug in the code
      static_assert(always_false<Output>,
                    "Invalid loop iteration in build_hist_loop");
    }
  }


  // Convert a ROOT 7 histogram into a ROOT 6 one
  template <class Output, class Input>
  Output convert_hist(const Input& src, const char* name) {
    // Make sure that the input histogram's impl-pointer is set
    const auto* impl_ptr = src.GetImpl();
    if (impl_ptr == nullptr) {
      throw std::runtime_error("Input histogram has a null impl pointer");
    }
    const auto& impl = *impl_ptr;

    // Compute the first ROOT 6 histogram constructor parameters
    //
    // Beware that "title" must remain a separate variable, otherwise
    // the title string will be deallocated before use...
    //
    auto title = convert_hist_title(impl.GetTitle());
    auto first_build_params = std::make_tuple(name, title.c_str());

    // Build the ROOT 6 histogram, copying src's axis configuration
    auto dest = convert_hist_loop<Output, 0>(impl,
                                             std::move(first_build_params));

    // Make sure that under- and overflow bins are included in the
    // statistics, to match the ROOT 7 behavior (as of ROOT v6.18.0).
    dest.SetStatOverflows(TH1::EStatOverflows::kConsider);

    // Use normal statistics for bin errors, since ROOT7 doesn't seem to support
    // Poisson bin error computation yet.
    dest.SetBinErrorOption(TH1::EBinErrorOpt::kNormal);

    // Set norm factor to zero (disable), since ROOT 7 doesn't seem to have this
    dest.SetNormFactor(0);

    // Now we're ready to transfer histogram data.
    // This is how we turn a ROOT 7 global bin index into its ROOT 6 equivalent.
    const auto& src_impl = *src.GetImpl();
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
      return get_bin_idx_from_local_root6(dest, local_bins);
    };

    // This is how we iterate over bins of the input ROOT 7 histogram, invoking
    // a callback with the index of the input bin and that of the matching bin
    // in the output ROOT 6 histogram.
    const auto& src_stat = src_impl.GetStat();
    auto for_each_src_bin = [&](auto&& bin_indices_callback) {
      for (int src_bin = 1; src_bin <= (int)src_stat.sizeNoOver(); ++src_bin) {
        bin_indices_callback(src_bin, to_dest_bin(src_bin));
      }
      for (int src_bin = -1; src_bin >= -(int)src_stat.sizeUnderOver(); --src_bin) {
        bin_indices_callback(src_bin, to_dest_bin(src_bin));
      }
    };

    // Propagate bin uncertainties, if present.
    //
    // This must be done before inserting any other data in the TH1,
    // otherwise Sumw2() will perform undesirable black magic...
    //
    // FIXME: if constexpr is C++17-only, will need to be backported to C++14
    //        for ROOT integration.
    //
    if constexpr (src_stat.HasBinUncertainty()) {
      dest.Sumw2();
      auto& sumw2 = *dest.GetSumw2();
      for_each_src_bin([&](int src_bin, Int_t dest_bin) {
        sumw2[dest_bin] = src_stat.GetSumOfSquaredWeights(src_bin);
      });
    }

    // Propagate basic histogram statistics
    dest.SetEntries(src.GetEntries());
    for_each_src_bin([&](int src_bin, Int_t dest_bin) {
      dest.AddBinContent(dest_bin, src_stat.GetBinContent(src_bin));
    });

    // Compute remaining statistics
    //
    // FIXME: If the input RHist computes all of...
    //        - fTsumw (total sum of weights)
    //        - fTsumw2 (total sum of square of weights)
    //        - fTsumwx (total sum of weight*x)
    //        - fTsumwx2 (total sum of weight*x*x)
    //
    //        ...then we should propagate those statistics to the TH1. The
    //        same applies for the higher-order statistics computed by TH2+.
    //
    //        But as of ROOT 6.18.0, we can never do this, because the
    //        RHistDataMomentUncert stats associated with fTsumwx and
    //        fTsumwx2 do not expose their contents publicly.
    //
    //        Therefore, we must always ask TH1 to do the computation
    //        for us. It's better to do so using a GetStats/PutStats
    //        pair, as ResetStats alters more than those stats...
    //
    //        The same problem occurs with the higher-order statistics
    //        computed by TH2+, but this approach is dimension-agnostic.
    //
    std::array<Double_t, TH1::kNstat> stats;
    dest.GetStats(stats.data());
    dest.PutStats(stats.data());

    // Return the ROOT 6 histogram to the caller
    return dest;
  }
}