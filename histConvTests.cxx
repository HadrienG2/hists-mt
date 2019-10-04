#include "histConv.hxx"


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;


// Make sure that the converter can be instantiated for all supported histogram types
template auto into_root6_hist(const RExp::RHist<1, Char_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Short_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Int_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Float_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<1, Double_t>& src, const char* name);
//
template auto into_root6_hist(const RExp::RHist<2, Char_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Short_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Int_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Float_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<2, Double_t>& src, const char* name);
//
template auto into_root6_hist(const RExp::RHist<3, Char_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Short_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Int_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Float_t>& src, const char* name);
template auto into_root6_hist(const RExp::RHist<3, Double_t>& src, const char* name);


// Test for the conversion machinery
int main() {
    // FIXME: Test with irregular axes, with more axes configurations...
    const RExp::RAxisConfig eq_axis(1000, 0., 1.);

    // Works (Minimal stats that we need)
    RExp::RHist<1, char, RExp::RHistStatContent> s1(eq_axis);
    auto d1 = into_root6_hist(s1, "Yolo1");

    // Also works (ROOT 7 implicitly adds an RHistStatContent here)
    RExp::RHist<1, char> s2(eq_axis);
    auto d2 = into_root6_hist(s2, "Yolo2");

    // Also works (More stats than actually needed)
    RExp::RHist<1,
                char,
                RExp::RHistStatContent,
                RExp::RHistDataMomentUncert> s3(eq_axis);
    auto d3 = into_root6_hist(s3, "Yolo3");

    // Compilation errors: insufficient stats. Unlike ROOT, we fail fast.
    /* RExp::RHist<1,
                   char,
                   RExp::RHistDataMomentUncert> s4(eq_axis);
    auto d4 = into_root6_hist(s4, "Yolo4"); */
    /* RExp::RHist<1,
                   char,
                   RExp::RHistStatUncertainty,
                   RExp::RHistDataMomentUncert> s4(eq_axis);
    auto d4 = into_root6_hist(s4, "Yolo4"); */

    // Data types other than char work just as well, if supported by ROOT 6
    RExp::RHist<1, short> s5(eq_axis);
    auto d5 = into_root6_hist(s5, "Yolo5");
    RExp::RHist<1, int> s6(eq_axis);
    auto d6 = into_root6_hist(s6, "Yolo6");
    RExp::RHist<1, float> s7(eq_axis);
    auto d7 = into_root6_hist(s7, "Yolo7");
    RExp::RHist<1, double> s8(eq_axis);
    auto d8 = into_root6_hist(s8, "Yolo8");

    // Compilation error: data type not supported by ROOT 6. We fail fast.
    /* RExp::RHist<1, size_t> s9(eq_axis);
    auto d9 = into_root6_hist(s9, "Yolo9"); */

    // Try it with a 2D histogram
    RExp::RHist<2, char> s10(eq_axis, eq_axis);
    auto d10 = into_root6_hist(s10, "Yolo10");

    // Try it with a 3D histogram
    RExp::RHist<3, char> s11(eq_axis, eq_axis,  eq_axis);
    auto d11 = into_root6_hist(s11, "Yolo11");

    // TODO: Add more sophisticated tests

    return 0;
}