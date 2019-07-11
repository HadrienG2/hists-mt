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
constexpr size_t NUM_BINS = 1000;  // Must tune this when testing atomic bins
constexpr size_t NUM_ITERS = 256 * 1024 * 1024;  // Should be a power of 2
constexpr std::pair<float, float> AXIS_RANGE = {0., 1.};

// For now, we'll be studying 1D hists with integer bins
using Hist1D = RExp::RHist<1, size_t>;


// Source of random data points for histograms
class RandomCoords {
public:
    // Generate a random point in the histogram's axis range
    RExp::Hist::RCoordArray<1> gen() { return {m_dis(m_gen)}; }

private:
    std::mt19937 m_gen;
    std::uniform_real_distribution<> m_dis{AXIS_RANGE.first, AXIS_RANGE.second};
};

// Basic microbenchmark harness
void bench(const std::string& name,
             std::function<Hist1D(Hist1D&&)>&& do_work)
{
    using namespace std::chrono;

    auto start = high_resolution_clock::now();
    Hist1D hist = do_work(Hist1D{{NUM_BINS,
                                  AXIS_RANGE.first,
                                  AXIS_RANGE.second}});
    auto end = high_resolution_clock::now();

    if ( hist.GetEntries() != NUM_ITERS ) {
        throw std::runtime_error("Bad number of histogram entries");
    }

    auto nanos_per_iter = duration_cast<duration<float, std::nano>>(end - start)
                          / NUM_ITERS;
    std::cout << "* " << name
              << " -> " << nanos_per_iter.count() << " ns/iter" << std::endl;
}


// Some benchmarks depend on a batch size parameter that must be known at
// compile time. We need to generate those using a template.
template <size_t BATCH_SIZE>
void bench_batch(RandomCoords& rng)
{
    std::cout << "=== BATCH SIZE: " << BATCH_SIZE << " ===" << std::endl;

    // Manually insert data points in batches using FillN()
    //
    // Amortizes some of the indirection.
    //
    bench("Manually-batched FillN()", [&](Hist1D&& hist) -> Hist1D {
        std::vector<RExp::Hist::RCoordArray<1>> batch;
        batch.reserve(BATCH_SIZE);
        for ( size_t i = 0; i < NUM_ITERS / BATCH_SIZE; ++i ) {
            batch.clear();
            for ( size_t j = 0; j < BATCH_SIZE; ++j ) {
                batch.push_back(rng.gen());
            }
            hist.FillN(batch);
        }
        return hist;
    });

    // Let ROOT7 do the batch insertion work for us
    //
    // Can be slightly slower than manual batching because RHistBufferedFill
    // buffers and records weights even when we don't need them.
    //
    bench("ROOT-batched Fill()", [&](Hist1D&& hist) -> Hist1D {
        RExp::RHistBufferedFill<Hist1D, BATCH_SIZE> buf_hist{hist};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            buf_hist.Fill(rng.gen());
        }
        buf_hist.Flush();
        return hist;
    });

    // Sequential use of RHistConcurrentFiller
    //
    // Combines batching akin to the one of RHistBufferedFill with mutex
    // protection on the histogram of interest.
    //
    bench("Serial Fill() from conc. filler", [&](Hist1D&& hist) -> Hist1D {
        RExp::RHistConcurrentFillManager<Hist1D, BATCH_SIZE> conc_hist{hist};
        auto conc_hist_filler = conc_hist.MakeFiller();
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            conc_hist_filler.Fill(rng.gen());
        }
        conc_hist_filler.Flush();
        return hist;
    });

    std::cout << std::endl;
}


// Top-level benchmark logic
int main()
{
    RandomCoords rng;
    std::cout << "=== NO BATCHING ===" << std::endl;

    // Unoptimized sequential Fill() pattern
    //
    // Pretty slow, as ROOT histograms have quite a few layers of indirection...
    //
    bench("Scalar Fill()", [&](Hist1D&& hist) -> Hist1D {
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            hist.Fill(rng.gen());
        }
        return hist;
    });

    std::cout << std::endl;

    // So, I heard that C++ doesn't have constexpr for loops, and in the
    // interest of keeping this code readable I don't want to hack around this
    // via recursive templated function calls...
    bench_batch<1>(rng);
    bench_batch<2>(rng);
    bench_batch<4>(rng);
    bench_batch<8>(rng);
    bench_batch<16>(rng);
    bench_batch<32>(rng);
    bench_batch<64>(rng);
    bench_batch<128>(rng);
    bench_batch<256>(rng);
    bench_batch<512>(rng);
    bench_batch<1024>(rng);
    bench_batch<2048>(rng);
    bench_batch<4096>(rng);
    bench_batch<8192>(rng);

    // TODO: Multi-threaded benchmarks
    // TODO: Other strategies (seqcst atomics, relaxed atomics, thread-local...)

    // TODO: And besides benchmarking & perf studies, in a different program...
    //       - Convert final histogram to ROOT 6 format
    //       - Write ROOT 6 histogram to file as a proof of concept.

    return 0;
}