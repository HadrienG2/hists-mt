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
                throw std::runtime_error("Histogram with null impl pointer");
            }

            // Query the histogram's axis
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
        // Conversion function for histograms with equidistant binning
        static Output convert_eq(const Input& hist, const char* name) {
            // Get back the state that was validated by convert()
            const auto& impl = *hist.GetImpl();
            const auto& eq_view = *impl.GetAxis(0).GetAsEquidistant();

            // Create the output histogram
            //
            // TODO: Should sanitize the output of GetTitle() for ROOT6 compat
            //
            TH1C result{name,
                        impl.GetTitle().c_str(),
                        eq_view.GetNBinsNoOver(),
                        eq_view.GetMinimum(),
                        eq_view.GetMaximum()};

            // TODO: Propagate more configuration (e.g. axis titles, labels...)
            // TODO: Propagage histogram data

            return result;
        }

        // Conversion function for histograms with irregular binning
        static Output convert_irr(const Input& hist, const char* name) {
            const auto& impl = *hist.GetImpl();
            const auto& irr_view = *impl.GetAxis(0).GetAsIrregular();

            // Create the output histogram
            //
            // TODO: Should sanitize the output of GetTitle() for ROOT6 compat
            //
            TH1C result{name,
                        impl.GetTitle().c_str(),
                        irr_view.GetNBinsNoOver(),
                        irr_view.GetBinBorders().data()};

            // TODO: Propagate more configuration (e.g. axis titles...)
            // TODO: Propagage histogram data

            return result;
        }
    };


    // TODO: Support other kinds of histogram conversions.
    //
    //       At that time, I'll probably want to extract some general-purpose
    //       utilities from the TH1C converter, deduplicating those.
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