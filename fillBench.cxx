#include "ROOT/RHist.hxx"
#include "ROOT/RHistConcurrentFill.hxx"
#include "ROOT/RHistBufferedFill.hxx"

#include <chrono>
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

// Just a simplistic microbenchmark harness
template <typename FUNC>
void time_it(const std::string& name, FUNC&& do_work) {
    using namespace std::chrono;

    Hist1D hist{{NUM_BINS, 0., 1.}};

    auto start = high_resolution_clock::now();
    do_work(hist);
    auto end = high_resolution_clock::now();

    auto time_span = duration_cast<duration<float, std::nano>>(end - start)
                         / NUM_ITERS;
    std::cout << "* " << name
              << " -> " << time_span.count() << " ns/iter" << std::endl;
}

// ...and we're ready to go.
int main() {
    std::mt19937 gen;
    std::uniform_real_distribution<> dis(0., 1.);

    // Unoptimized sequential Fill() pattern
    //
    // Pretty slow, as ROOT histograms have quite a few layers of indirection...
    //
    time_it("Scalar Fill()", [&](Hist1D& hist) {
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            hist.Fill({dis(gen)});
        }
    });

    // Manually insert data points in batches using FillN()
    //
    // Amortizes some of the indirection.
    //
    time_it("Manually-batched FillN()", [&](Hist1D& hist) {
        std::vector<RExp::Hist::RCoordArray<1>> batch;
        batch.reserve(BATCH_SIZE);
        for ( size_t i = 0; i < NUM_ITERS / BATCH_SIZE; ++i ) {
            batch.clear();
            for ( size_t j = 0; j < BATCH_SIZE; ++j ) {
                batch.push_back({dis(gen)});
            }
            hist.FillN(batch);
        }
    });

    // Let ROOT7 do the batch insertion work for us
    //
    // Can be slightly slower than manual batching because RHistBufferedFill
    // buffers and records weights even when we don't need them.
    //
    time_it("ROOT-batched Fill()", [&](Hist1D& hist) {
        RExp::RHistBufferedFill<Hist1D, BATCH_SIZE> buf_hist{hist};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            buf_hist.Fill({dis(gen)});
        }
        buf_hist.Flush();
    });

    // Sequential use of RHistConcurrentFiller
    //
    // Combines batching akin to the one of RHistBufferedFill with mutex
    // protection on the histogram of interest.
    //
    time_it("Serial Fill() from conc filler", [&](Hist1D& hist) {
        RExp::RHistConcurrentFillManager<Hist1D, BATCH_SIZE> conc_hist{hist};
        auto conc_hist_filler = conc_hist.MakeFiller();
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            conc_hist_filler.Fill({dis(gen)});
        }
        conc_hist_filler.Flush();
    });

    // TODO: Test with various batch sizes
    // TODO: Multi-threaded benchmarks
    // TODO: Other strategies (seqcst atomics, relaxed atomics, thread-local...)

    // TODO: And besides benchmarking & perf studies, in a different program...
    //       - Convert final histogram to ROOT 6 format
    //       - Write ROOT 6 histogram to file as a proof of concept.

    return 0;
}