# Parallel histogramming with ROOT 7

- Interesting places in the ROOT source code:
    * `hist/hist/v7` contains the ROOT 7 histogramming library
    * `tutorials/v7` contains some usage examples:
        - `concurrentfill.cxx` is a multi-threaded fill example based on the
          RHistConcurrentFillManager histogram wrapper.
        - `perf.cxx` is a microbenchmark for histogram fill code, demonstrating
          the RHistBufferedFill mechanism.
        - `perfcomp.cxx` is a more elaborate version of that microbenchmark that
          demonstrates the effect of the choice of axis binning and does a
          performance comparison with ROOT 6 histograms.
        - Sadly, these examples are written in ROOT style and don't even have
          a `main()` function, must be tweaked before they are usable.

- Test builds with `g++ -O3 -march=native -std=c++17 -pthread -lCore -lHist`

- What ROOT 7 `hist` provides...
    * Mostly headers, a little bit of impl extracted in `src`, and some tests
      and benchmarks. Let's look at those headers...
    * `RAxis.hxx` represents histogram axes, that's where binning is managed.
    * `RHist.hxx` is the `RHist` histogram template, which is parametrized by
      dimensionality, "precision" (= bin data type), and whatever statistics you
      may want to collect during the histogramming process (e.g. number of data
      points, uncertainties...)
        - Most of the work is offloaded to `RHistImplBase`, which we'll hear
          about soon enough...
    * `RHistBinIter.hxx` is the machinery used to iterate over histogram bins.
    * `RHistBufferedFill.hxx` provides the eponymous wrapper around `RHist` that
      internally buffers `Fill()` calls in order to call `FillN()` in batches.
      That is more efficient due to indirections in `RHist`...
    * `RHistConcurrentFill.hxx` provides the `RHistConcurrentFillManager`
      wrapper. This class provides mutex-protexted `FillN()` interfaces, and
      can spawn `RHistConcurrentFiller`s which are mostly `RHistBufferedFill`
      variants that commit their output as `FillN` transactions into the
      `RHistConcurrentFillManager`.
    * `RHistData.hxx` contains some statistical data that `RHist`/`RHistImpl`
      can be configured to record. And some dirty metaprogramming black magic.
    * `RHistImpl.hxx` is where most `RHist` methods eventually get dispatched.
      This indirection can be used for various purposes, from type erasure for
      plotting to hiding the fact that histograms are templated by axis
      configuration, which is necessary for performance.
    * `RHistUtils.hxx` contains the "coordinate array" template, which is
      essentially an extension of `std::array` that tries to be more ergonomic.
    * `RHistView.hxx` contains, as the name suggests, a way to extract a view
      into a subset of some histogram's data.

- Remarks so far:
    * `RHist` hides bin data behind multiple layers of abstraction. Extracting
      the data to fill a ROOT 6 histogram will not be so easy.
    * To use `RHistConcurrentFiller` in Marlin, we'd need to somehow define a
      master copy of the histogram and have "child" processors fetch their own
      accessor from it. That's not a bad design, but it can take some time to
      work around.
    * Would be nice to start with a consolidated microbenchmark that merges all
      the existing ones into one.
    * To get this kind of histogram to work with atomics, I'll need some custom
      float type whose members compile down to relaxed atomic ops.

- There is a bug in `RHistBufferedFill::Flush()`. It doesn't reset the cursor,
  so calling it multiple times in a row will write the same data multiple times.
    * fCursor = 0 from `RHistBufferedFillBase::Fill()` should probably go here
    * `RHistConcurrentFiller::Flush()` seems equally vulnerable
    * To be reported to the ROOT team...

- Another bug: `RHistConcurrentFiller::GetHist()` and friends map to a
  nonexistent method of `RHistConcurrentFillManager`.