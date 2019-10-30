#include "histConv.hpp"


namespace detail
{
  std::string convert_hist_title(const std::string& title) {
    // To prevent ROOT 6 from misinterpreting free-form histogram titles
    // from ROOT 7 as a mixture of a histogram title and axis titles, all
    // semicolons must be escaped with a preceding # character.
    std::string hist_title = title;
    size_t pos = 0;
    while(true) {
      pos = hist_title.find(';', pos);
      if (pos == std::string::npos) return hist_title;
      hist_title.insert(pos, 1, '#');
      pos += 2;
    }
  }


  TAxis& get_root6_axis(TH1& hist, size_t idx) {
    switch (idx) {
      case 0: return *hist.GetXaxis();
      default:
        throw std::runtime_error(std::to_string(idx)
                                 + " is not a valid axis index for TH1");
    }
  }


  TAxis& get_root6_axis(TH2& hist, size_t idx) {
    switch (idx) {
      case 0: return *hist.GetXaxis();
      case 1: return *hist.GetYaxis();
      default:
        throw std::runtime_error(std::to_string(idx)
                                 + " is not a valid axis index for TH2");
    }
  }


  TAxis& get_root6_axis(TH3& hist, size_t idx) {
    switch (idx) {
      case 0: return *hist.GetXaxis();
      case 1: return *hist.GetYaxis();
      case 2: return *hist.GetZaxis();
      default:
        throw std::runtime_error(std::to_string(idx)
                                 + " is not a valid axis index for TH3");
    }
  }


  Double_t get_bin_from_root6(const TAxis& axis, Int_t bin) {
    // FIXME: This matches the ROOT 7 behavior... but said behavior is wrong.
    //        std::numeric_limits<double>::lowest() should be used.
    if (axis.IsVariableBinSize() && (bin == 0)) {
      return std::numeric_limits<double>::min();
    } else {
      return axis.GetBinLowEdge(bin);
    }
  }


  std::array<Double_t, 1> get_bin_from_root6(const TH1& hist, Int_t bin) {
    std::array<Int_t, 3> bin_xyz;
    hist.GetBinXYZ(bin, bin_xyz[0], bin_xyz[1], bin_xyz[2]);
    return {get_bin_from_root6(*hist.GetXaxis(), bin_xyz[0])};
  }


  std::array<Double_t, 2> get_bin_from_root6(const TH2& hist, Int_t bin) {
    std::array<Int_t, 3> bin_xyz;
    hist.GetBinXYZ(bin, bin_xyz[0], bin_xyz[1], bin_xyz[2]);
    return {get_bin_from_root6(*hist.GetXaxis(), bin_xyz[0]),
            get_bin_from_root6(*hist.GetYaxis(), bin_xyz[1])};
  }


  std::array<Double_t, 3> get_bin_from_root6(const TH3& hist, Int_t bin) {
    std::array<Int_t, 3> bin_xyz;
    hist.GetBinXYZ(bin, bin_xyz[0], bin_xyz[1], bin_xyz[2]);
    return {get_bin_from_root6(*hist.GetXaxis(), bin_xyz[0]),
            get_bin_from_root6(*hist.GetYaxis(), bin_xyz[1]),
            get_bin_from_root6(*hist.GetZaxis(), bin_xyz[2])};
  }


  void setup_axis_base(TAxis& dest, const RExp::RAxisBase& src) {
    // Propagate axis title
    dest.SetTitle(src.GetTitle().c_str());

    // Propagate axis growability
    // FIXME: No direct access fo fCanGrow in RAxisBase yet...
    dest.SetCanExtend((src.GetNOverflowBins() == 0));

    // Propagate whether this is a labeled axis
    // FIXME: Can't support labeled axes yet because RAxisBase does not
    //        provide a way to check if an axis is labeled...
    dest.SetNoAlphanumeric(true);
  }


  template TH1C convert_hist(const RExp::RHist<1, char>&, const char*);
  template TH1S convert_hist(const RExp::RHist<1, Short_t>&, const char*);
  template TH1I convert_hist(const RExp::RHist<1, Int_t>&, const char*);
  template TH1F convert_hist(const RExp::RHist<1, Float_t>&, const char*);
  template TH1D convert_hist(const RExp::RHist<1, Double_t>&, const char*);
  //
  template TH2C convert_hist(const RExp::RHist<2, Char_t>&, const char*);
  template TH2S convert_hist(const RExp::RHist<2, Short_t>&, const char*);
  template TH2I convert_hist(const RExp::RHist<2, Int_t>&, const char*);
  template TH2F convert_hist(const RExp::RHist<2, Float_t>&, const char*);
  template TH2D convert_hist(const RExp::RHist<2, Double_t>&, const char*);
  //
  template TH3C convert_hist(const RExp::RHist<3, Char_t>&, const char*);
  template TH3S convert_hist(const RExp::RHist<3, Short_t>&, const char*);
  template TH3I convert_hist(const RExp::RHist<3, Int_t>&, const char*);
  template TH3F convert_hist(const RExp::RHist<3, Float_t>&, const char*);
  template TH3D convert_hist(const RExp::RHist<3, Double_t>&, const char*);
}
