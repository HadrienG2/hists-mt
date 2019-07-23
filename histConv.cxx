#include "ROOT/RHist.hxx"
#include "TH1.h"


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
    static TH1C convert(const char* name, const RExp::RHist<1, char>& hist)
    {
        // TODO: Stop placeholding and do the actual conversion
        return TH1C{name, "hello", 123, 2.4, 4.2};
    }
};

// TODO: Support other kinds of histogram conversions

// High-level interface to the above conversion machinery
template <typename Root7Hist>
decltype(auto) into_root6_hist(const char* name, const Root7Hist& hist) {
    return HistConverter<Root7Hist>::convert(name, hist);
}


// Let's test this
int main() {
    // Tuning knobs
    constexpr size_t NUM_BINS = 1000;
    constexpr std::pair<float, float> AXIS_RANGE = {0., 1.};
    constexpr const char* NAME = "Yolo";

    // Type of histogram under study
    using Hist1D = RExp::RHist<1, char>;

    // Let's try it!
    Hist1D hist = Hist1D{{NUM_BINS, AXIS_RANGE.first, AXIS_RANGE.second}};
    decltype(auto) res = into_root6_hist(NAME, hist);

    return 0;
}