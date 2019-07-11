#include "ROOT/RHist.hxx"
#include "ROOT/RHistConcurrentFill.hxx"
#include "ROOT/RHistBufferedFill.hxx"

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <random>


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;

// Benchmark tuning knobs
constexpr size_t NUM_BINS = 1000;
constexpr size_t BATCH_SIZE = 1000;  // Should be a divisor of NUM_ITERS
constexpr size_t NUM_ITERS = 300 * 1000 * 1000;

// For now, we'll be studying 1D hists with integer bins
using Hist1D = RExp::RHist<1, size_t>;

// Basic microbenchmark harness
void time_it(const std::string& name,
             std::function<size_t(Hist1D&&)>&& do_work)
{
    using namespace std::chrono;

    auto start = high_resolution_clock::now();
    size_t num_entries = do_work(Hist1D{{NUM_BINS, 0., 1.}});
    auto end = high_resolution_clock::now();

    if ( num_entries != NUM_ITERS ) {
        throw std::runtime_error("Bad number of histogram entries");
    }

    auto nanos_per_iter = duration_cast<duration<float, std::nano>>(end - start)
                          / NUM_ITERS;
    std::cout << "* " << name
              << " -> " << nanos_per_iter.count() << " ns/iter" << std::endl;
}

// Top-level benchmark
int main()
{
    std::mt19937 gen;
    std::uniform_real_distribution<> dis(0., 1.);

    // Unoptimized sequential Fill() pattern
    //
    // Pretty slow, as ROOT histograms have quite a few layers of indirection...
    //
    time_it("Scalar Fill()", [&](Hist1D&& hist) -> size_t {
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            hist.Fill({dis(gen)});
        }
        return hist.GetEntries();
    });

    // Manually insert data points in batches using FillN()
    //
    // Amortizes some of the indirection.
    //
    time_it("Manually-batched FillN()", [&](Hist1D&& hist) -> size_t {
        std::vector<RExp::Hist::RCoordArray<1>> batch;
        batch.reserve(BATCH_SIZE);
        for ( size_t i = 0; i < NUM_ITERS / BATCH_SIZE; ++i ) {
            batch.clear();
            for ( size_t j = 0; j < BATCH_SIZE; ++j ) {
                batch.push_back({dis(gen)});
            }
            hist.FillN(batch);
        }
        return hist.GetEntries();
    });

    // Let ROOT7 do the batch insertion work for us
    //
    // Can be slightly slower than manual batching because RHistBufferedFill
    // buffers and records weights even when we don't need them.
    //
    time_it("ROOT-batched Fill()", [&](Hist1D&& hist) -> size_t {
        RExp::RHistBufferedFill<Hist1D, BATCH_SIZE> buf_hist{hist};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            buf_hist.Fill({dis(gen)});
        }
        buf_hist.Flush();
        return hist.GetEntries();
    });

    // Sequential use of RHistConcurrentFiller
    //
    // Combines batching akin to the one of RHistBufferedFill with mutex
    // protection on the histogram of interest.
    //
    time_it("Serial Fill() from conc filler", [&](Hist1D&& hist) -> size_t {
        RExp::RHistConcurrentFillManager<Hist1D, BATCH_SIZE> conc_hist{hist};
        auto conc_hist_filler = conc_hist.MakeFiller();
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            conc_hist_filler.Fill({dis(gen)});
        }
        conc_hist_filler.Flush();
        return hist.GetEntries();
    });

    // TODO: Test with various batch sizes
    // TODO: Multi-threaded benchmarks
    // TODO: Other strategies (seqcst atomics, relaxed atomics, thread-local...)

    // TODO: And besides benchmarking & perf studies, in a different program...
    //       - Convert final histogram to ROOT 6 format
    //       - Write ROOT 6 histogram to file as a proof of concept.

    return 0;
}