#include "ROOT/RAxis.hxx"
#include "ROOT/RHist.hxx"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"

#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


// Evil machinery turning ROOT 7 histograms into ROOT 6 histograms
namespace detail
{
    // === TOP-LEVEL ENTRY POINT FOR INTO_ROOT6_HIST ===

    // Trick for static_asserts that only fail when a template is instantiated
    //
    // When you write a struct template that must always be specialized, you may
    // want to print a compiler error when the non-specialized struct is
    // instantiated, by adding a failing static_assert to it.
    //
    // However, you must then prevent the compiler from firing the static_assert
    // even when the non-specialized version of the template struct is never
    // instantiated, by making its evaluation "depend on" the template
    // parameters of the struct. This variable template seems to do the job.
    //
    template <typename T> constexpr bool always_false = false;

    // ROOT 7 -> ROOT 6 histogram converter
    //
    // Must be specialized for every supported ROOT 7 histogram type. Falling
    // back on the base case means that a histogram conversion is not supported,
    // and will be reported as such.
    //
    // Every specialization will provide a "convert()" static function that
    // performs the conversion. That function takes the following parameters:
    //
    // - The ROOT 7 histogram that must be converted into a ROOT 6 one.
    // - A name, playing the same role as ROOT 6's "name" constructor parameter.
    //
    template <typename Input, typename Enable = void>
    struct HistConverter
    {
        // Tell the user that we haven't implemented this conversion (yet?)
        static_assert(
            always_false<Input>,
            "This ROOT 7 -> ROOT 6 histogram conversion is not supported");

        // Dummy conversion function to keep compiler errors bounded
        static auto convert(const Input& src, const char* name);
    };


    // === CHECK THAT REQUIRED RHIST STATS ARE PRESENT ===

    // For a ROOT 7 histogram to be convertible to the ROOT 6 format, it must
    // collect the RHistStatContent statistic. Let's check for this.
    template <template <int D_, class P_> class... STAT>
    constexpr bool stats_ok;

    // If the user declares an RHist with an empty stats list, ROOT silently
    // adds RHistStatContent. So we must special-case this empty list.
    template <>
    constexpr bool stats_ok<> = true;

    // If there is only one stat in the list, then the assertion will succeed or
    // fail depending on if this stat is RHistStatContent.
    template <template <int D_, class P_> class SINGLE_STAT>
    constexpr bool stats_ok<SINGLE_STAT> = false;
    template <>
    constexpr bool stats_ok<RExp::RHistStatContent> = true;

    // If there are 2+ stats in the list, then we iterate through recursion.
    // This case won't catch the 1-stat scenario due to above specializations.
    template <template <int D_, class P_> class STAT_HEAD,
              template <int D_, class P_> class... STAT_TAIL>
    constexpr bool stats_ok<STAT_HEAD, STAT_TAIL...> =
        stats_ok<STAT_HEAD> || stats_ok<STAT_TAIL...>;

    // We'll also want a nice compiler error message in the failing case
    template <template <int D_, class P_> class... STAT>
    struct CheckStats : public std::bool_constant<stats_ok<STAT...>> {
        static_assert(stats_ok<STAT...>,
                      "Only ROOT 7 histograms that record RHistStatContent "
                      "statistics may be converted into ROOT 6 histograms");
    };

    // ...and finally we can clean up
    template <template <int D_, class P_> class... STAT>
    static constexpr bool CheckStats_v = CheckStats<STAT...>::value;


    // === LOOK UP THE ROOT 6 EQUIVALENT OF OUR RHIST (IF ANY) ===

    // We also need a machinery that, given a ROOT 7 histogram type, can give us
    // the corresponding ROOT 6 histogram type, if any.
    //
    // This must be done via specialization, so let's define the failing case...
    //
    template <int DIMENSIONS, class PRECISION>
    struct CheckRoot6Type : public std::false_type
    {
        // Tell the user we don't know of an equivalent to this histogram type
        static_assert(always_false<RExp::RHist<DIMENSIONS, PRECISION>>,
                      "No known ROOT 6 histogram type matches the input "
                      "histogram's dimensionality and precision");
    };

    // ...then we can define the successful cases...
    template <>
    struct CheckRoot6Type<1, Char_t> : public std::true_type {
        using Result = TH1C;
    };
    template <>
    struct CheckRoot6Type<1, Short_t> : public std::true_type {
        using Result = TH1S;
    };
    template <>
    struct CheckRoot6Type<1, Int_t> : public std::true_type {
        using Result = TH1I;
    };
    template <>
    struct CheckRoot6Type<1, Float_t> : public std::true_type {
        using Result = TH1F;
    };
    template <>
    struct CheckRoot6Type<1, Double_t> : public std::true_type {
        using Result = TH1D;
    };

    template <>
    struct CheckRoot6Type<2, Char_t> : public std::true_type {
        using Result = TH2C;
    };
    template <>
    struct CheckRoot6Type<2, Short_t> : public std::true_type {
        using Result = TH2S;
    };
    template <>
    struct CheckRoot6Type<2, Int_t> : public std::true_type {
        using Result = TH2I;
    };
    template <>
    struct CheckRoot6Type<2, Float_t> : public std::true_type {
        using Result = TH2F;
    };
    template <>
    struct CheckRoot6Type<2, Double_t> : public std::true_type {
        using Result = TH2D;
    };

    template <>
    struct CheckRoot6Type<3, Char_t> : public std::true_type {
        using Result = TH3C;
    };
    template <>
    struct CheckRoot6Type<3, Short_t> : public std::true_type {
        using Result = TH3S;
    };
    template <>
    struct CheckRoot6Type<3, Int_t> : public std::true_type {
        using Result = TH3I;
    };
    template <>
    struct CheckRoot6Type<3, Float_t> : public std::true_type {
        using Result = TH3F;
    };
    template <>
    struct CheckRoot6Type<3, Double_t> : public std::true_type {
        using Result = TH3D;
    };

    // ...and finally we'll add CheckStats_v-like sugar on top for consistency
    template <int DIMENSIONS, class PRECISION>
    static constexpr bool CheckRoot6Type_v =
        CheckRoot6Type<DIMENSIONS, PRECISION>::value;


    // === DIMENSION-GENERIC RHIST -> THx CONVERSION BUILDING BLOCKS ===

    // Convert a ROOT 7 histogram title into a ROOT 6 histogram title
    //
    // To prevent ROOT 6 from misinterpreting free-form histogram titles
    // from ROOT 7 as a mixture of a histogram title and axis titles, all
    // semicolons must be escaped with a preceding # character.
    //
    std::string convert_hist_title(const std::string& title) {
        std::string hist_title = title;
        size_t pos = 0;
        while(true) {
            pos = hist_title.find(';', pos);
            if (pos == std::string::npos) return hist_title;
            hist_title.insert(pos, 1, '#');
            pos += 2;
        }
    }

    // Map from ROOT 7 axis index to ROOT 6 histogram axes
    TAxis& get_root6_axis(TH1& hist, size_t idx) {
        switch (idx) {
            case 0: return *hist.GetXaxis();
            default: throw std::runtime_error("Invalid axis index");
        }
    }
    TAxis& get_root6_axis(TH2& hist, size_t idx) {
        switch (idx) {
            case 0: return *hist.GetXaxis();
            case 1: return *hist.GetYaxis();
            default: throw std::runtime_error("Invalid axis index");
        }
    }
    TAxis& get_root6_axis(TH3& hist, size_t idx) {
        switch (idx) {
            case 0: return *hist.GetXaxis();
            case 1: return *hist.GetYaxis();
            case 2: return *hist.GetZaxis();
            default: throw std::runtime_error("Invalid axis index");
        }
    }

    // Transfer histogram axis settings which exist in both equidistant and
    // irregular binning configurations
    void setup_axis_base(TAxis& dest, const RExp::RAxisBase& src) {
        // Propagate axis title
        dest.SetTitle(src.GetTitle().c_str());

        // Propagate axis growability
        // FIXME: No direct access fo fCanGrow in RAxisBase yet!
        dest.SetCanExtend((src.GetNOverflowBins() == 0));
    }

    // Shorthand for a ridiculously long name
    template <int DIMS>
    using RHistImplBase = RExp::Detail::RHistImplPrecisionAgnosticBase<DIMS>;

    // Histogram construction and configuration recursion for convert_hist
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
                    std::tuple_cat(std::move(build_params),
                                   std::make_tuple(eq_axis.GetNBinsNoOver(),
                                                   eq_axis.GetMinimum(),
                                                   eq_axis.GetMaximum()));

                // Process other axes and construct the histogram
                auto dest =
                    convert_hist_loop<Output,
                                      AXIS+1>(src_impl,
                                              std::move(new_build_params));

                // Propagate basic axis properties
                setup_axis_base(get_root6_axis(dest, AXIS), eq_axis);

                // If the axis is labeled, propagate labels
                //
                // FIXME: I cannot find a way to go from RAxisView to labels!
                //        Even dynamic_casting RAxisEquidistant* to RAxisLabels*
                //        fails because the type is not polymorphic (does not
                //        have a single virtual method).
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
                        std::make_tuple(irr_axis.GetNBinsNoOver(),
                                        irr_axis.GetBinBorders().data())
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
            // All histogram constructor parameters have been collected, we can
            // now construct the ROOT 6 histogram.
            return std::make_from_tuple<Output>(std::move(build_params));
        } else {
            // The loop shouldn't reach this point, there's a bug in the code
            static_assert(always_false<Output>,
                          "Invalid loop iteration in build_hist_loop");
        }


    }

    // Convert a ROOT 7 histogram into a ROOT 6 one
    //
    // Offloads the work of filling up histogram data, which hasn't been made
    // dimension-agnostic yet, to a dimension-specific worker.
    template <class Output, class Input>
    Output convert_hist(const Input& src,
                        const char* name,
                        void(*fill_hist_data)(Output&, const Input&)) {
        // Make sure that the input histogram's impl-pointer is set
        const auto* impl_ptr = src.GetImpl();
        if (impl_ptr == nullptr) {
            throw std::runtime_error("Histogram has a null impl pointer");
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
        auto dest =
            convert_hist_loop<Output, 0>(impl, std::move(first_build_params));

        // Make sure that under- and overflow bins are included in the
        // statistics, to match the ROOT 7 behavior (as of ROOT v6.18.0).
        dest.SetStatOverflows(TH1::kConsider);

        // Transfer basic histogram data
        fill_hist_data(dest, src);

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


    // === HISTCONVERTER SPECIALIZATIONS FOR SUPPORTED HISTOGRAM TYPES ===

    // One-dimensional histogram converter
    template <class PRECISION, template <int D_, class P_> class... STAT>
    struct HistConverter<RExp::RHist<1, PRECISION, STAT...>,
                         std::enable_if_t<CheckRoot6Type_v<1, PRECISION>
                                          && CheckStats_v<STAT...>>>
    {
    private:
        using Input = RExp::RHist<1, PRECISION, STAT...>;
        using Output = typename CheckRoot6Type<1, PRECISION>::Result;

    public:
        // Top-level conversion function
        static Output convert(const Input& src, const char* name) {
            return convert_hist<Output>(src, name, fill_hist_data);
        }

    private:
        // Transfer TH1 per-bin statistics
        //
        // TODO: Generalize to 2D+ by adding a check that bin layout is the
        //       same for ROOT 6 and ROOT 7.
        //
        static void fill_hist_data(Output& dest, const Input& src) {
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
        }
    };


    // TODO: Support other kinds of histogram conversions.
    //
    //       I am currently the process of extracting dimension-independent
    //       and dimension-generic utilities from the TH1 HistConverter.
    //       Ultimately, the dimension-specific part should become very small,
    //       basically just calling GetXaxis and friends, and then writing
    //       HistConverters for TH2+ will be trivial.
    //
    //       For 2D+ histogram, I'll also need to check if the bin data is
    //       ordered in the same way in ROOT 6 and ROOT 7. I should ideally have
    //       some kind of assertion that checks that it remains the case as
    //       ROOT 7 development marches on.
    //
    //       Alternatively, I could always work in local bin coordinates, but
    //       that would greatly reduce my ability to factor out stuff between
    //       the 1D case and 2D+ cases.
    //
    //       I also expect THn to be one super messy edge case that no one uses,
    //       so it's probably a good idea to keep it a TODO initially and wait
    //       for people to come knocking on my door asking for it.
}


// High-level interface to the above conversion machinery
//
// "src" is the histogram to be converted, and "name" plays the same role as in
// the ROOT 6 histogram constructors.
//
template <typename Root7Hist>
auto into_root6_hist(const Root7Hist& src, const char* name) {
    return detail::HistConverter<Root7Hist>::convert(src, name);
}


// Test for the conversion machinery
int main() {
    // Works (Minimal stats that we need)
    RExp::RHist<1, char, RExp::RHistStatContent> s1{{1000, 0., 1.}};
    auto d1 = into_root6_hist(s1, "Yolo1");

    // Also works (ROOT 7 implicitly adds an RHistStatContent here)
    RExp::RHist<1, char> s2{{1000, 0., 1.}};
    auto d2 = into_root6_hist(s2, "Yolo2");

    // Also works (More stats than actually needed)
    RExp::RHist<1,
                char,
                RExp::RHistStatContent,
                RExp::RHistDataMomentUncert> s3{{1000, 0., 1.}};
    auto d3 = into_root6_hist(s3, "Yolo3");

    // Compilation errors: insufficient stats. Unlike ROOT, we fail fast.
    /* RExp::RHist<1,
                   char,
                   RExp::RHistDataMomentUncert> s4{{1000, 0., 1.}};
    auto d4 = into_root6_hist(s4, "Yolo4"); */
    /* RExp::RHist<1,
                   char,
                   RExp::RHistStatUncertainty,
                   RExp::RHistDataMomentUncert> s4{{1000, 0., 1.}};
    auto d4 = into_root6_hist(s4, "Yolo4"); */

    // Data types other than char work just as well, if supported by ROOT 6
    RExp::RHist<1, short> s5{{1000, 0., 1.}};
    auto d5 = into_root6_hist(s5, "Yolo5");
    RExp::RHist<1, int> s6{{1000, 0., 1.}};
    auto d6 = into_root6_hist(s6, "Yolo6");
    RExp::RHist<1, float> s7{{1000, 0., 1.}};
    auto d7 = into_root6_hist(s7, "Yolo7");
    RExp::RHist<1, double> s8{{1000, 0., 1.}};
    auto d8 = into_root6_hist(s8, "Yolo8");

    // Compilation error: data type not supported by ROOT 6. We fail fast.
    /* RExp::RHist<1, size_t> s9{{1000, 0., 1.}};
    auto d9 = into_root6_hist(s9, "Yolo9"); */

    // TODO: Add more sophisticated tests

    return 0;
}