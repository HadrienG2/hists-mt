#include "ROOT/RHist.hxx"
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
using BinData = size_t;
using Coords1D = RExp::Hist::RCoordArray<1>;
using Hist1D = RExp::RHist<1, BinData>;
using BufHist1D = RExp::RHistBufferedFill<Hist1D, BATCH_SIZE>;

// Just a simplistic microbenchmark harness
template <typename FUNC>
void time_it(const std::string& name, FUNC&& do_work) {
    using namespace std::chrono;

    auto start = high_resolution_clock::now();
    do_work();
    auto end = high_resolution_clock::now();
    auto time_span = duration_cast<duration<float, std::nano>>(end - start) / NUM_ITERS;
    std::cout << name << " -> " << time_span.count() << " ns/iter" << std::endl;
}

// ...and we're ready to go.
int main() {
    std::mt19937 gen;
    std::uniform_real_distribution<> dis(0., 1.);
    std::vector<Coords1D> batch;
    const RExp::RAxisConfig cfg = {NUM_BINS, 0., 1.};

    // Unoptimized sequential Fill() pattern
    //
    // Pretty slow, as ROOT histograms have quite a few layers of indirection...
    //
    time_it("Scalar Fill()", [&] {
        Hist1D hist{cfg};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            hist.Fill({dis(gen)});
        }
    });

    // Manually insert data points in batches using FillN()
    //
    // Amortizes some of the indirection.
    //
    batch.reserve(BATCH_SIZE);
    time_it("Manually-batched FillN()", [&] {
        Hist1D hist{cfg};
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
    time_it("ROOT-batched Fill()", [&] {
        Hist1D hist{cfg};
        BufHist1D buf_hist{hist};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            buf_hist.Fill({dis(gen)});
        }
        buf_hist.Flush();
    });

    // TODO: Manually batched RHistConcurrentFillManager
    // TODO: Auto-batched RHistConcurrentFillManager
    // TODO: Single-threaded RHistConcurrentFiller
    // TODO: Multi-threaded RHistConcurrentFillManager
    // TODO: Multi-threaded RHistConcurrentFiller
    // TODO: Parallel fill using seqcst atomics strategy
    // TODO: Parallel fill using relaxed atomics strategy
    // TODO: Parallel fill using thread-local strategy

    // TODO: And besides benchmarking & perf studies, in a different program...
    //       - Convert final histogram to ROOT 6 format
    //       - Write ROOT 6 histogram to file as a proof of concept.

    return 0;
}