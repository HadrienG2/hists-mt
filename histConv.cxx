#include "ROOT/RAxis.hxx"
#include "ROOT/RHist.hxx"
#include "TH1.h"

#include <exception>
#include <type_traits>


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


namespace detail
{
    // Evil trick to prevent base case static_asserts from always firing
    template <typename T> struct always_false: std::false_type {};


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
        // Tell the user that they ended up on an unsupported conversion
        static_assert(always_false<Input>::value,
                      "This histogram conversion is not implemented");

        // Dummy conversion function to keep compiler errors bounded
        static TH1 convert(const Input& src, const char* name);
    };


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
    struct CheckStats {
        static constexpr bool result = stats_ok<STAT...>;
        static_assert(result,
                      "Only RHists that record RHistStatContent statistics may "
                      "be converted into ROOT 6 histograms");
    };

    // ...and finally we can clean up
    template <template <int D_, class P_> class... STAT>
    using CheckStats_t = std::enable_if_t<CheckStats<STAT...>::result>;


    // RHist<1, char, ...> to TH1C histogram converter
    template <template <int D_, class P_> class... STAT>
    struct HistConverter<RExp::RHist<1, char, STAT...>, CheckStats_t<STAT...>>
    {
    private:
        using Input = RExp::RHist<1, char, STAT...>;
        using Output = TH1C;

    public:
        // Top-level conversion function
        static Output convert(const Input& src, const char* name) {
            // Make sure the histogram's impl-pointer is set
            const auto* impl_ptr = src.GetImpl();
            if (impl_ptr == nullptr) {
                throw std::runtime_error("Histogram has a null impl pointer");
            }

            // Query the histogram's axis kind
            const auto axis_view = impl_ptr->GetAxis(0);

            // If equidistant, dispatch to the equidistant converter
            const auto* eq_view_ptr = axis_view.GetAsEquidistant();
            if (eq_view_ptr != nullptr) {
                return convert_eq(src, name);
            }

            // If irregular, dispatch to the irregular converter
            const auto* irr_view_ptr = axis_view.GetAsIrregular();
            if (irr_view_ptr != nullptr) {
                return convert_irr(src, name);
            }

            // As of ROOT 6.18.0, there is no other axis kind
            throw std::runtime_error("Unsupported histogram axis type");
        }

    private:

        // Conversion function for histograms with equidistant binning
        static Output convert_eq(const Input& src, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *src.GetImpl();
            const auto& eq_axis = *impl.GetAxis(0).GetAsEquidistant();

            // Create the output histogram
            TH1C dest{name,
                      convert_hist_title(impl.GetTitle()).c_str(),
                      eq_axis.GetNBinsNoOver(),
                      eq_axis.GetMinimum(),
                      eq_axis.GetMaximum()};

            // Propagate basic axis properties
            auto& dest_axis = *dest.GetXaxis();
            setup_axis_common(dest_axis, eq_axis);

            // If the axis is labeled, propagate labels
            //
            // FIXME: I cannot find a way to go from RAxisView to axis labels!
            //        Even dynamic_casting RAxisEquidistant* to RAxisLabels*
            //        fails because the type is not polymorphic (does not have a
            //        single virtual method).
            //
            /* const auto* lbl_axis_ptr =
                dynamic_cast<const RExp::RAxisLabels*>(&eq_axis);
            if (lbl_axis_ptr) {
                auto labels = lbl_axis_ptr->GetBinLabels();
                for (size_t bin = 0; bin < labels.size(); ++bin) {
                    std::string label{labels[bin]};
                    dest_axis.SetBinLabel(bin, label.c_str());
                }
            } */

            // Propagate histogram configuration and contents
            setup_hist_common(dest, src);

            return dest;
        }

        // Conversion function for histograms with irregular binning
        static Output convert_irr(const Input& src, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *src.GetImpl();
            const auto& irr_axis = *impl.GetAxis(0).GetAsIrregular();

            // Create the output histogram
            TH1C dest{name,
                      convert_hist_title(impl.GetTitle()).c_str(),
                      irr_axis.GetNBinsNoOver(),
                      irr_axis.GetBinBorders().data()};

            // Propagate basic axis properties.
            // For irregular axes, there is nothing else to propagate.
            setup_axis_common(*dest.GetXaxis(), irr_axis);

            // Propagate histogram configuration and contents
            setup_hist_common(dest, src);

            return dest;
        }

        // Convert a ROOT 7 histogram title into a ROOT 6 histogram title
        //
        // To prevent ROOT 6 from misinterpreting free-form histogram titles
        // from ROOT 7 as a mixture of a histogram title and axis titles, all
        // semicolons must be escaped with a preceding # character.
        //
        static std::string convert_hist_title(const std::string& title) {
            std::string hist_title = title;
            size_t pos = 0;
            while(true) {
                pos = hist_title.find(';', pos);
                if (pos == std::string::npos) return hist_title;
                hist_title.insert(pos, 1, '#');
                pos += 2;
            }
        }

        // Transfer histogram axis settings which exist in both equidistant and
        // irregular binning configurations
        static void setup_axis_common(TAxis& dest, const RExp::RAxisBase& src)
        {
            // Propagate axis title
            dest.SetTitle(src.GetTitle().c_str());

            // Propagate axis growability
            // FIXME: No direct access fo fCanGrow in RAxisBase yet!
            dest.SetCanExtend((src.GetNOverflowBins() == 0));
        }

        // Transfer histogram-wide configuration and contents
        static void setup_hist_common(Output& dest, const Input& src) {
            // Make sure that under- and overflow bins are included in the
            // statistics, to match the ROOT 7 behavior (as of ROOT v6.18.0).
            dest.SetStatOverflows(TH1::kConsider);

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

            // FIXME: If the input RHist computes all of...
            //        - fTsumw (total sum of weights)
            //        - fTsumw2 (total sum of square of weights)
            //        - fTsumwx (total sum of weight*x)
            //        - fTsumwx2 (total sum of weight*x*x)
            //
            //        ...then we should propagate those statistics to the TH1.
            //
            //        But as of ROOT 6.18.0, we can never do this, because the
            //        RHistDataMomentUncert stats associated with fTsumwx and
            //        fTsumwx2 do not expose their contents publicly.
            //
            //        Therefore, we must always ask TH1 to do the computation
            //        for us. It's better to do so using a GetStats/PutStats
            //        pair, as ResetStats changes more than just the stats...
            //
            std::array<Double_t, TH1::kNstat> stats;
            dest.GetStats(stats.data());
            dest.PutStats(stats.data());
        }
    };


    // TODO: Support other kinds of histogram conversions.
    //
    //       At that time, I'll probably want to extract some general-purpose
    //       utilities from the TH1C converter, deduplicating those.
    //
    //       For 2D+ histogram, I'll also need to check if the bin data is
    //       ordered in the same way in ROOT 6 and ROOT 7. I should ideally have
    //       some kind of static assertion that checks that it remains the case
    //       as ROOT 7 development marches on.
    //
    //       Alternatively, I could always work in global bin coordinates, but
    //       that would greatly reduce my ability to factor out stuff between
    //       the 1D case and 2D+ cases.
    //
    //       I also expect THn to be one super messy edge case. It's probably a
    //       good idea to keep it a TODO initially, and wait for people to come
    //       knocking on my door asking for it.
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
    RExp::RHist<1, char, RExp::RHistStatContent, RExp::RHistDataMomentUncert>
        s3{{1000, 0., 1.}};
    auto d3 = into_root6_hist(s3, "Yolo3");

    // Compilation errors due to missing stats. Unlike ROOT, we fail fast.
    /* RExp::RHist<1, char, RExp::RHistDataMomentUncert> s4{{1000, 0., 1.}};
    auto d4 = into_root6_hist(s4, "Yolo4"); */
    /* RExp::RHist<1, char, RExp::RHistStatUncertainty, RExp::RHistDataMomentUncert> s4{{1000, 0., 1.}};
    auto d4 = into_root6_hist(s4, "Yolo4"); */

    // FIXME: Doesn't work yet
    /* RExp::RHist<1, float> s5{{1000, 0., 1.}};
    auto d5 = into_root6_hist(s5, "Yolo5"); */

    // TODO: Add more sophisticated tests

    return 0;
}