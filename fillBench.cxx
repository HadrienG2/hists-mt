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
using Hist1D = RExp::RHist<1, BinData>;
using BufHist1D = RExp::RHistBufferedFill<Hist1D, BATCH_SIZE>;
using Coords1D = RExp::Hist::RCoordArray<1>;

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
    std::vector<Coords1D> buffer;
    buffer.reserve(BATCH_SIZE);
    time_it("Manually-batched FillN()", [&] {
        Hist1D hist{cfg};
        for ( size_t i = 0; i < NUM_ITERS / BATCH_SIZE; ++i ) {
            for ( size_t j = 0; j < BATCH_SIZE; ++j ) {
                buffer.push_back({dis(gen)});
            }
            hist.FillN(buffer);
            buffer.clear();
        }
    });

    // Let ROOT7 do the batch insertion work for us
    //
    // Should be slightly slower than manual batching because RHistBufferedFill
    // buffers and records weights even when we don't need them.
    //
    time_it("ROOT-batched Fill()", [&] {
        Hist1D hist{cfg};
        BufHist1D buf_hist{hist};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            buf_hist.Fill({dis(gen)});
        }
    });

    return 0;
}