#include "ROOT/RHist.hxx"
#include "TH1.h"

// TODO: Add missing includes


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


// ROOT 7 -> ROOT 6 histogram converter
//
// Must be specialized for every supported ROOT 7 histogram type. The base case
// will fail to indicate that a certain histogram conversion is unsupported.
//
// Will provide a "convert()" static function that performs the desired
// conversion. It expects the following parameters:
//
// - A name, playing the same role as ROOT 6's "name" constructor parameter.
// - The ROOT 7 histogram that must be converted into a ROOT 6 one.
//
template <typename Input>
struct HistConverter
{
private:
    // Evil trick to prevent the static_assert below from firing all the time
    template <typename T> struct always_false: std::false_type {};

public:
    // Tell the user that they ended up on an unsupported conversion
    static_assert(always_false<Input>::value,
                  "This histogram conversion is not implemented");
};

// Conversion from RHist<1, char> to TH1C
template <template <int D_, class P_> class... STAT>
struct HistConverter<RExp::RHist<1, char, STAT...>>
{
private:
    using Input = RExp::RHist<1, char, STAT...>;
    using Output = TH1C;

public:
    // Top-level conversion function
    static Output convert(const char* name, const Input& hist)
    {
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
            return convert_eq(name, hist);
        }

        // If irregular, dispatch to the irregular converter
        const auto* irr_view_ptr = axis_view.GetAsIrregular();
        if (irr_view_ptr != nullptr) {
            return convert_irr(name, hist);
        }

        // As of ROOT 6.18.0, there is no other axis kind
        throw std::runtime_error("Unsupported histogram axis type");
    }

private:
    // Conversion function for histograms with equidistant binning
    static Output convert_eq(const char* name, const Input& hist) {
        const auto& impl = *hist.GetImpl();
        const auto& eq_view = *impl.GetAxis(0).GetAsEquidistant();

        // Create the output histogram
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
    static Output convert_irr(const char* name, const Input& hist) {
        const auto& impl = *hist.GetImpl();
        const auto& irr_view = *impl.GetAxis(0).GetAsIrregular();

        // Create the output histogram
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
//       Will probably want to extract some general-purpose utilities from the
//       TH1C converter at that time, thusly deduplicating code.

// High-level interface to the above conversion machinery
template <typename Root7Hist>
decltype(auto) into_root6_hist(const char* name, const Root7Hist& hist) {
    return HistConverter<Root7Hist>::convert(name, hist);
}


// Let's test this
int main() {
    // TODO: Share some configuration with fillBench?

    // Tuning knobs
    constexpr size_t NUM_BINS = 1000;
    constexpr std::pair<float, float> AXIS_RANGE = {0., 1.};
    constexpr const char* NAME = "Yolo";

    // Type of histogram under study
    using Hist1D = RExp::RHist<1, char>;

    // Let's try it!
    Hist1D hist = Hist1D{{NUM_BINS, AXIS_RANGE.first, AXIS_RANGE.second}};
    decltype(auto) res = into_root6_hist(NAME, hist);

    // TODO: Add more sophisticated tests

    return 0;
}