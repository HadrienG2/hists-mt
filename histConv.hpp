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

  // ROOT 7-like GetBinFrom for ROOT 6
  Double_t get_bin_from_root6(const TAxis& axis, Int_t bin);
  std::array<Double_t, 1> get_bin_from_root6(const TH1& hist, Int_t bin);
  std::array<Double_t, 2> get_bin_from_root6(const TH2& hist, Int_t bin);
  std::array<Double_t, 3> get_bin_from_root6(const TH3& hist, Int_t bin);

  // Transfer histogram axis settings which are common to all axis
  // configurations (currently equidistant, growable, irregular and labels)
  void setup_axis_base(TAxis& dest, const RExp::RAxisBase& src);


  // === MAIN CONVERSION FUNCTIONS ===

  // Shorthand for an excessively long name
  template <int DIMS>
  using RHistImplBase = RExp::Detail::RHistImplPrecisionAgnosticBase<DIMS>;

  // Create a ROOT 6 histogram whose global and per-axis configuration matches
  // that of an input ROOT 7 histogram as closely as possible.
  template <class Output, int AXIS, int DIMS, class... BuildParams>
  Output convert_hist_loop(const RHistImplBase<DIMS>& src_impl,
                           std::tuple<BuildParams...>&& build_params) {
    // This function is actually a kind of recursive loop for AXIS ranging
    // from 0 to the dimension of the histogram, inclusive.
    if constexpr (AXIS < DIMS) {
      // The first iterations query the input histogram axes one by one
      const auto axis_view = src_impl.GetAxis(AXIS);

      // Is this an equidistant axis?
      const auto* eq_axis_ptr = axis_view.GetAsEquidistant();
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
        setup_axis_base(get_root6_axis(dest, AXIS), eq_axis);

        // If the axis is labeled, propagate labels
        //
        // FIXME: There does not seem to be a way to go from RAxisView to axis
        //        labels at the moment. Even dynamic_cast will fail because
        //        RAxisXyz do not contain a single virtual method, and therefore
        //        do not have the required infrastructure for dcasting.
        //
        /* const auto* lbl_axis_ptr =
          dynamic_cast<const RExp::RAxisLabels*>(&eq_axis);
        if (lbl_axis_ptr) {
          auto labels = lbl_axis_ptr->GetBinLabels();
          for (size_t bin = 0; bin < labels.size(); ++bin) {
              std::string label{labels[bin]};
              dest.SetBinLabel(bin, label.c_str());
          }
        } */

        // Send back the histogram to caller
        return dest;
      }

      // Is this an irregular axis?
      const auto* irr_axis_ptr = axis_view.GetAsIrregular();
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
        // There aren't any other properties for irregular axes.
        setup_axis_base(get_root6_axis(dest, AXIS), irr_axis);

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


  // Check that the input and output histogram use the same binning
  // conventions (starting index, N-d array serialization order) since we
  // currently rely on that assumption for fast histogram bin data transfer.
  template <class THx, int DIMS>
  void check_binning(const THx& dest, const RHistImplBase<DIMS>& src_impl)
  {
    // Check that bins from ROOT 7 are "close enough" to those from ROOT 7
    auto bins_similar = [](auto src_bins, auto dest_bins) -> bool {
      static constexpr double TOLERANCE = 1e-6;
      if (src_bins.size() != dest_bins.size()) return false;
      for (size_t i = 0; i < src_bins.size(); ++i) {
        double diff = std::abs(src_bins[i] - dest_bins[i]);
        if (diff >= TOLERANCE * std::abs(src_bins[i])) { return false; }
      }
      return true;
    };

    // Display a bunch of bins into a stringstream
    auto print_bins = [](std::ostringstream& s, auto local_bin_indices) {
      s << "{ ";
      for (size_t i = 0; i < local_bin_indices.size()-1; ++i) {
        s << local_bin_indices[i] << ", ";
      }
      s << local_bin_indices[local_bin_indices.size()-1] << " }";
    };

    // If any of these test fails, ROOT 7 binning went out of sync with
    // ROOT 6 binning, and thusly broke our simple data transfer strategy :(
    if (!bins_similar(src_impl.GetBinFrom(0), get_bin_from_root6(dest, 0))) {
      std::ostringstream s;
      s << "Binning origin doesn't match"
        << " (source histogram's first bin is at ";
      print_bins(s, src_impl.GetBinFrom(0));
      s << ", target histogram's first bin is at ";
      print_bins(s, get_bin_from_root6(dest, 0));
      s << ')';
      throw std::runtime_error(s.str());
    }
    if ((src_impl.GetNBins() >=2)
        && (!bins_similar(src_impl.GetBinFrom(1), get_bin_from_root6(dest, 1)))) {
      std::ostringstream s;
      s << "Binning order doesn't match"
        << " (source histogram's second bin is at ";
      print_bins(s, src_impl.GetBinFrom(1));
      s << ", target histogram's second bin is at ";
      print_bins(s, get_bin_from_root6(dest, 1));
      s << ')';
      throw std::runtime_error(s.str());
    }
    if (src_impl.GetNBins() != dest.GetNcells()) {
      std::ostringstream s;
      s << "Bin count doesn't match"
        << " (source histogram has " << src_impl.GetNBins() << " bins"
        << ", target histogram has " << dest.GetNcells() << " bins)";
      throw std::runtime_error(s.str());
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

    // Now we're ready to transfer histogram data. First of all, let's
    // assert that ROOT 6 and ROOT 7 use the same binning convention. This
    // seems true as of ROOT 6.18.0, but may change in the future...
    check_binning(dest, *src.GetImpl());

    // Propagate bin uncertainties, if present.
    //
    // This must be done before inserting any other data in the TH1,
    // otherwise Sumw2() will perform undesirable black magic...
    //
    const auto& stat = src.GetImpl()->GetStat();
    if (stat.HasBinUncertainty()) {
      dest.Sumw2();
      auto& sumw2 = *dest.GetSumw2();
      for (size_t bin = 0; bin < stat.size(); ++bin) {
        sumw2[bin] = stat.GetBinUncertainty(bin);
      }
    }

    // Propagate basic histogram statistics
    dest.SetEntries(stat.GetEntries());
    for (size_t bin = 0; bin < stat.size(); ++bin) {
      dest.AddBinContent(bin, stat.GetBinContent(bin));
    }

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