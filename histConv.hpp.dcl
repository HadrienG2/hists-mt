// ROOT7 -> ROOT6 histogram converter (minimal declaration)
//
// You can use this simplified header to speed up your builds if you only call
// into_root_6_hist on RHist<DIMS, T> histograms without extra stats.

#pragma once

#include "TH1.h"
#include "TH2.h"
#include "TH3.h"

#include <type_traits>


// Forward declarations for ROOT types
namespace ROOT { namespace Experimental {
  template <int DIMS,
            class PRECISION,
            template <int D_, class P_> class... STAT>
  class RHist;
  //
  template <int D_, class P_>
  class RHistStatContent;
} }


// Evil machinery turning ROOT 7 histograms into ROOT 6 histograms
namespace detail
{
  // Typing this gets old quickly
  namespace RExp = ROOT::Experimental;

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


  // === TOP-LEVEL ENTRY POINT FOR INTO_ROOT6_HIST ===

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
    static_assert(always_false<Input>, "Unsupported histogram conversion");

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
    using type = TH1C;
  };
  template <>
  struct CheckRoot6Type<1, Short_t> : public std::true_type {
    using type = TH1S;
  };
  template <>
  struct CheckRoot6Type<1, Int_t> : public std::true_type {
    using type = TH1I;
  };
  template <>
  struct CheckRoot6Type<1, Float_t> : public std::true_type {
    using type = TH1F;
  };
  template <>
  struct CheckRoot6Type<1, Double_t> : public std::true_type {
    using type = TH1D;
  };

  template <>
  struct CheckRoot6Type<2, Char_t> : public std::true_type {
    using type = TH2C;
  };
  template <>
  struct CheckRoot6Type<2, Short_t> : public std::true_type {
    using type = TH2S;
  };
  template <>
  struct CheckRoot6Type<2, Int_t> : public std::true_type {
    using type = TH2I;
  };
  template <>
  struct CheckRoot6Type<2, Float_t> : public std::true_type {
    using type = TH2F;
  };
  template <>
  struct CheckRoot6Type<2, Double_t> : public std::true_type {
    using type = TH2D;
  };

  template <>
  struct CheckRoot6Type<3, Char_t> : public std::true_type {
    using type = TH3C;
  };
  template <>
  struct CheckRoot6Type<3, Short_t> : public std::true_type {
    using type = TH3S;
  };
  template <>
  struct CheckRoot6Type<3, Int_t> : public std::true_type {
    using type = TH3I;
  };
  template <>
  struct CheckRoot6Type<3, Float_t> : public std::true_type {
    using type = TH3F;
  };
  template <>
  struct CheckRoot6Type<3, Double_t> : public std::true_type {
    using type = TH3D;
  };

  // ...and finally we'll add a little bit of sugar on top
  template <int DIMENSIONS, class PRECISION>
  using CheckRoot6Type_t =
    typename CheckRoot6Type<DIMENSIONS, PRECISION>::type;
  //
  template <int DIMENSIONS, class PRECISION>
  static constexpr bool CheckRoot6Type_v =
    CheckRoot6Type<DIMENSIONS, PRECISION>::value;


  // === UNCHECKED HISTOGRAM CONVERTER ===

  // Convert a ROOT 7 histogram into a ROOT 6 one
  //
  // This template does not validate its input type arguments. You'll get
  // horrible C++ template error messages if you get them wrong. Use the checked
  // into_root6_hist API to avoid this.
  //
  template <class Output, class Input>
  Output convert_hist(const Input& src, const char* name);

  // Explicit instantiations are provided for all basic histogram types
  extern template TH1C convert_hist(const RExp::RHist<1, Char_t>&, const char*);
  extern template TH1S convert_hist(const RExp::RHist<1, Short_t>&, const char*);
  extern template TH1I convert_hist(const RExp::RHist<1, Int_t>&, const char*);
  extern template TH1F convert_hist(const RExp::RHist<1, Float_t>&, const char*);
  extern template TH1D convert_hist(const RExp::RHist<1, Double_t>&, const char*);
  //
  extern template TH2C convert_hist(const RExp::RHist<2, Char_t>&, const char*);
  extern template TH2S convert_hist(const RExp::RHist<2, Short_t>&, const char*);
  extern template TH2I convert_hist(const RExp::RHist<2, Int_t>&, const char*);
  extern template TH2F convert_hist(const RExp::RHist<2, Float_t>&, const char*);
  extern template TH2D convert_hist(const RExp::RHist<2, Double_t>&, const char*);
  //
  extern template TH3C convert_hist(const RExp::RHist<3, Char_t>&, const char*);
  extern template TH3S convert_hist(const RExp::RHist<3, Short_t>&, const char*);
  extern template TH3I convert_hist(const RExp::RHist<3, Int_t>&, const char*);
  extern template TH3F convert_hist(const RExp::RHist<3, Float_t>&, const char*);
  extern template TH3D convert_hist(const RExp::RHist<3, Double_t>&, const char*);


  // === CHECKED HISTOGRAM CONVERTER ===

  // This specialization of HistConverter uses SFINAE to assert that the input
  // ROOT7 histogram can be converted into a ROOT6 histogram, then calls the
  // unchecked convert_hist converter defined above.
  template <int DIMS,
            class PRECISION,
            template <int D_, class P_> class... STAT>
  struct HistConverter<RExp::RHist<DIMS, PRECISION, STAT...>,
                       std::enable_if_t<CheckRoot6Type_v<DIMS, PRECISION>
                                        && CheckStats_v<STAT...>>>
  {
  private:
    using Input = RExp::RHist<DIMS, PRECISION, STAT...>;
    using Output = CheckRoot6Type_t<DIMS, PRECISION>;

  public:
    static Output convert(const Input& src, const char* name) {
      return convert_hist<Output>(src, name);
    }
  };

  // TODO: Support THn someday, if someone asks for it
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
