#include "ROOT/RAxis.hxx"
#include "ROOT/RHist.hxx"
#include "TH1.h"

#include <exception>
#include <type_traits>


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


namespace detail
{
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
    template <typename Input>
    struct HistConverter
    {
    private:
        // Evil trick to prevent the static_assert below from firing spuriously
        template <typename T> struct always_false: std::false_type {};

    public:
        // Tell the user that they ended up on an unsupported conversion
        static_assert(always_false<Input>::value,
                      "This histogram conversion is not implemented");
    };


    // RHist<1, char, ...> to TH1C histogram converter
    //
    // FIXME: We actually have some requirements on the STATs, which we should
    //        properly assert via SFINAE to avoid template error horror.
    //
    template <template <int D_, class P_> class... STAT>
    struct HistConverter<RExp::RHist<1, char, STAT...>>
    {
    private:
        using Input = RExp::RHist<1, char, STAT...>;
        using Output = TH1C;

    public:
        // Top-level conversion function
        static Output convert(const Input& hist, const char* name) {
            // Make sure the histogram's impl-pointer is set
            const auto* impl_ptr = hist.GetImpl();
            if (impl_ptr == nullptr) {
                throw std::runtime_error("Histogram has a null impl pointer");
            }

            // Query the histogram's axis kind
            const auto axis_view = impl_ptr->GetAxis(0);

            // If equidistant, dispatch to the equidistant converter
            const auto* eq_view_ptr = axis_view.GetAsEquidistant();
            if (eq_view_ptr != nullptr) {
                return convert_eq(hist, name);
            }

            // If irregular, dispatch to the irregular converter
            const auto* irr_view_ptr = axis_view.GetAsIrregular();
            if (irr_view_ptr != nullptr) {
                return convert_irr(hist, name);
            }

            // As of ROOT 6.18.0, there is no other axis kind
            throw std::runtime_error("Unsupported histogram axis type");
        }

    private:
        // TODO: Deduplicate equidistant/irregular conversion

        // Conversion function for histograms with equidistant binning
        static Output convert_eq(const Input& hist, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *hist.GetImpl();
            const auto& eq_axis = *impl.GetAxis(0).GetAsEquidistant();

            // Create the output histogram
            TH1C result{name,
                        convert_hist_title(impl.GetTitle()).c_str(),
                        eq_axis.GetNBinsNoOver(),
                        eq_axis.GetMinimum(),
                        eq_axis.GetMaximum()};

            // Propagate basic axis properties
            auto& result_axis = *result.GetXaxis();
            setup_axis_common(result_axis, eq_axis);

            // If the axis is labeled, propagate labels
            //
            // FIXME: I cannot find a way to go from RAxisView to axis labels.
            //        Even dynamic_casting RAxisEquidistant* to RAxisLabels*
            //        fails because the type is not polymorphic (does not have a
            //        single virtual method). How can I get to those labels?
            //
            /* const auto* lbl_axis_ptr =
                dynamic_cast<const RExp::RAxisLabels*>(&eq_axis);
            if (lbl_axis_ptr) {
                auto labels = lbl_axis_ptr->GetBinLabels();
                for (size_t bin = 0; bin < labels.size(); ++bin) {
                    std::string label{labels[bin]};
                    result_axis.SetBinLabel(bin, label.c_str());
                }
            } */

            // TODO: Propagate histogram data
            /* const auto& stat = impl.GetStat(); */

            // TODO: Go through the TH1 documentation and try to configure it
            //       as close to a ROOT 7 histogram as possible generally
            //       speaking (already checked for TAxis, seems OK)

            return result;
        }

        // Conversion function for histograms with irregular binning
        static Output convert_irr(const Input& hist, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *hist.GetImpl();
            const auto& irr_axis = *impl.GetAxis(0).GetAsIrregular();

            // Create the output histogram
            TH1C result{name,
                        convert_hist_title(impl.GetTitle()).c_str(),
                        irr_axis.GetNBinsNoOver(),
                        irr_axis.GetBinBorders().data()};

            // Propagate basic axis properties
            setup_axis_common(*result.GetXaxis(), irr_axis);

            // TODO: Once data propagation is live for equidistant axes,
            //       make it work with irregular axes

            // TODO: Go through the TH1 documentation and try to configure it
            //       as close to a ROOT 7 histogram as possible generally
            //       speaking (already checked for TAxis, seems OK)

            return result;
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
    };


    // TODO: Support other kinds of histogram conversions.
    //
    //       At that time, I'll probably want to extract some general-purpose
    //       utilities from the TH1C converter, deduplicating those.
    //
    //       For 2D+ histogram, I'll also need to check if the bin data is
    //       ordered in the same way in ROOT 6 and ROOT 7
}


// High-level interface to the above conversion machinery
//
// "hist" is the histogram to be converted, and "name" plays the same role as in
// the ROOT 6 histogram constructors.
//
template <typename Root7Hist>
auto into_root6_hist(const Root7Hist& hist, const char* name) {
    return detail::HistConverter<Root7Hist>::convert(hist, name);
}


// Test for the conversion machinery
int main() {
    RExp::RHist<1, char> hist{{1000, 0., 1.}};
    auto res = into_root6_hist(hist, "TYolo");

    // TODO: Add more sophisticated tests

    return 0;
}