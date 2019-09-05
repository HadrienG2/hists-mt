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
            "This ROOT7 -> ROOT6 histogram conversion is not supported");

        // Dummy conversion function to keep compiler errors bounded
        //
        // FIXME: Should return TH2 in the 2D case, TH3 in the 3D case, etc.
        //
        static TH1 convert(const Input& src, const char* name);
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
                      "Only ROOT7 histograms that record RHistStatContent "
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
                      "No known ROOT6 histogram type has the input histogram's "
                      "dimensionality and precision");
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

    // The logic for equidistant and irregular axes diverges enough to
    // warrant splitting it into two separate code paths for clarity.
    enum AxisKind : size_t { Equidistant = 0, Irregular, LENGTH };

    // Transfer histogram axis settings which exist in both equidistant and
    // irregular binning configurations
    void setup_axis_common(TAxis& dest, const RExp::RAxisBase& src) {
        // Propagate axis title
        dest.SetTitle(src.GetTitle().c_str());

        // Propagate axis growability
        // FIXME: No direct access fo fCanGrow in RAxisBase yet!
        dest.SetCanExtend((src.GetNOverflowBins() == 0));
    }

    // Transfer equidistant histogram axis settings
    void setup_axis(TAxis& dest, const RExp::RAxisEquidistant& src) {
        // Propagate basic axis properties
        setup_axis_common(dest, src);

        // If the axis is labeled, propagate labels
        //
        // FIXME: I cannot find a way to go from RAxisView to axis labels!
        //        Even dynamic_casting RAxisEquidistant* to RAxisLabels*
        //        fails because the type is not polymorphic (does not have a
        //        single virtual method).
        //
        /* const auto* lbl_axis_ptr =
            dynamic_cast<const RExp::RAxisLabels*>(&src);
        if (lbl_axis_ptr) {
            auto labels = lbl_axis_ptr->GetBinLabels();
            for (size_t bin = 0; bin < labels.size(); ++bin) {
                std::string label{labels[bin]};
                dest.SetBinLabel(bin, label.c_str());
            }
        } */
    }

    // Transfer irregular histogram axis settings
    void setup_axis(TAxis& dest, const RExp::RAxisIrregular& src) {
        // Propagate basic axis properties
        setup_axis_common(dest, src);

        // ...and for irregular axes, that's it!
    }

    // Transfer histogram settings other than axis settings.
    //
    // This function is dimension-independent, and delegates the dimension-
    // specific work of transfering histogram entries and weights to the
    // fill_hist_data callback. Later, this callback will be generalized too.
    //
    template <class Output, class Input>
    static void setup_hist(Output& dest,
                           const Input& src,
                           void(*fill_hist_data)(Output&, const Input&))
    {
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
        //        computed by TH2+.
        //
        std::array<Double_t, TH1::kNstat> stats;
        dest.GetStats(stats.data());
        dest.PutStats(stats.data());
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

    // Recursive implementation of build_hist, see its documentation for details
    //
    // This overload is called at the end of the recursion, once all constructor
    // parameters have been generated and the ROOT 6 constructor can be called.
    //
    template <class Output,
              size_t CURR_AXIS,
              class... BuildParams>
    Output build_hist_impl(std::tuple<BuildParams...>&& build_params) {
        // Build and return the requested ROOT 6 histogram
        return std::apply(Output::Output, std::forward(build_params));;
    }

    // Recursive implementation of build_hist, see its documentation for details
    //
    // This overload appends parameters for an equidistant axis to the list
    // of ROOT 6 histogram constructor parameters, recurses build_hist_impl to
    // build the histogram, and propagates the axis configuration that cannot
    // be set at construction time before returning.
    //
    template <class Output,
              size_t CURR_AXIS,
              class... BuildParams,
              class... RAxis>
    Output build_hist_impl(
        std::tuple<BuildParams...>&& build_params,
        const RExp::RAxisEquidistant& eq_axis,
        const RAxis&... other_axes
    ) {
        // Append constructor parameters for equidistant axes to build_params
        auto new_build_params =
            std::tuple_cat(std::forward(build_params),
                           std::make_tuple(eq_axis.GetNBinsNoOver(),
                                           eq_axis.GetMinimum(),
                                           eq_axis.GetMaximum()));

        // Recurse with other axes to finish constructing the histogram
        auto dest = build_hist_impl<Output,
                                    CURR_AXIS+1>(std::forward(build_params),
                                                 other_axes...);

        // Propagate axis configuration that can't be set by the constructor
        setup_axis(get_root6_axis(dest, CURR_AXIS), eq_axis);
        return dest;
    }

    // Recursive implementation of build_hist, see its documentation for details
    //
    // This overload appends parameters for an irregular axis to the list
    // of ROOT 6 histogram constructor parameters, recurses build_hist_impl to
    // build the histogram, and propagates the axis configuration that cannot
    // be set at construction time before returning.
    //
    template <class Output,
              size_t CURR_AXIS,
              class... BuildParams,
              class... RAxis>
    Output build_hist_impl(
        std::tuple<BuildParams...>&& build_params,
        const RExp::RAxisIrregular& irr_axis,
        const RAxis&... other_axes
    ) {
        // Append constructor parameters for equidistant axes to build_params
        auto new_build_params =
            std::tuple_cat(std::forward(build_params),
                           std::make_tuple(irr_axis.GetNBinsNoOver(),
                                           irr_axis.GetBinBorders().data()));

        // Recurse with other axes to finish constructing the histogram
        auto dest = build_hist_impl<Output,
                                    CURR_AXIS+1>(std::forward(build_params),
                                                 other_axes...);

        // Propagate axis configuration that can't be set by the constructor
        setup_axis(get_root6_axis(dest, CURR_AXIS), irr_axis);
        return dest;
    }

    // Build a ROOT 6 histogram configured like a ROOT 7 one
    //
    // The parameters are...
    // - The ROOT 7 histogram from which configuration is to be taken
    // - The "name" parameter of the ROOT 6 histogram constructor
    // - axes of the src histogram (FIXME: Should query them ourselves)
    template <class Output, class Input, class... RAxis>
    Output build_hist(const Input& src,
                      const char* name,
                      const RAxis&... axes) {
        // Compute ROOT 6 histogram title
        auto title = convert_hist_title(src.GetImpl()->GetTitle());

        // Start the histogram-building recursion by sending the initial
        // histogram parameters, namely name and C-style title.
        return build_hist_impl<Output, 0>(std::make_tuple(name,
                                                          title.c_str()),
                                          axes...);
    }

    // Generic implementation of a HistConverter's top-level convert() function.
    //
    // Offloads the work of creating the output histogram and of transferring
    // bin data to user-provided callbacks. Building is specific to a given axis
    // configuration, filling is not.
    //
    // Builder functions should be given in lexicographic order:
    // EqEq, then EqIrr, then IrrEq, then IrrIrr, etc.
    //
    template <class Output, class Input>
    Output convert_impl(
        const Input& src,
        const char* name,
        const std::array<Output(*)(const Input&, const char*),
                         (1 << Input::GetNDim())>& builders,
        void(*fill_hist_data)(Output&, const Input&)
    ) {
        // Document all the hacks
        static_assert(AxisKind::LENGTH == 2,
                      "The (1 << DIM) hack for constexpr computation of "
                      "integer powers of AxisKind::LENGTH only works because "
                      "AxisKind::LENGTH happens to be equal to 2.");

        // Make sure the histogram's impl-pointer is set
        const auto* impl_ptr = src.GetImpl();
        if (impl_ptr == nullptr) {
            throw std::runtime_error("Histogram has a null impl pointer");
        }

        // Examine axis kinds to determine which converter we should dispatch to
        size_t converter_index = 0;
        for (size_t axis = 0; axis < Input::GetNDim(); ++axis) {
            // Shift converter index to make room for new axis index
            converter_index *= AxisKind::LENGTH;

            // Query the current histogram axis
            const auto axis_view = impl_ptr->GetAxis(axis);

            // Handle equidistant axis case
            const auto* eq_view_ptr = axis_view.GetAsEquidistant();
            if (eq_view_ptr != nullptr) {
                converter_index += AxisKind::Equidistant;
                continue;
            }

            // Handle irregular axis case
            const auto* irr_view_ptr = axis_view.GetAsIrregular();
            if (irr_view_ptr != nullptr) {
                converter_index += AxisKind::Irregular;
                continue;
            }

            // As of ROOT 6.18.0, there is no other axis kind
            throw std::runtime_error("Unsupported histogram axis type");
        }

        // Build the output histogram
        auto dest = builders.at(converter_index)(src, name);

        // Transfer data from the input histogram
        setup_hist(dest, src, fill_hist_data);

        // Return the filled histogram
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
            return convert_impl<Output>(src,
                                        name,
                                        {build_hist_eq, build_hist_irr},
                                        fill_hist_data);
        }

    private:
        // Build a TH1 with equidistant binning
        static Output build_hist_eq(const Input& src, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *src.GetImpl();
            const auto& eq_axis = *impl.GetAxis(0).GetAsEquidistant();

            // Create the output histogram
            Output dest{name,
                        convert_hist_title(impl.GetTitle()).c_str(),
                        eq_axis.GetNBinsNoOver(),
                        eq_axis.GetMinimum(),
                        eq_axis.GetMaximum()};

            // Propagate basic axis properties
            setup_axis(*dest.GetXaxis(), eq_axis);

            // Return the histogram so it can be filled
            return dest;
        }

        // Build a TH1 with irregular binning
        static Output build_hist_irr(const Input& src, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *src.GetImpl();
            const auto& irr_axis = *impl.GetAxis(0).GetAsIrregular();

            // Create the output histogram
            Output dest{name,
                        convert_hist_title(impl.GetTitle()).c_str(),
                        irr_axis.GetNBinsNoOver(),
                        irr_axis.GetBinBorders().data()};

            // Propagate basic axis properties
            setup_axis(*dest.GetXaxis(), irr_axis);

            return dest;
        }

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