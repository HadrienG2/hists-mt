#include "ROOT/RHist.hxx"
#include "ROOT/RHistConcurrentFill.hxx"
#include "ROOT/RHistBufferedFill.hxx"

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <random>
#include <thread>


// Typing this gets old quickly
namespace RExp = ROOT::Experimental;

// Benchmark tuning knobs
constexpr size_t NUM_BINS = 1000;  // Will tune this when testing atomic bins
constexpr size_t NUM_ITERS = 512 * 1024 * 1024;  // Should be a power of 2
constexpr std::pair<float, float> AXIS_RANGE = {0., 1.};

// For now, we'll be studying 1D hists with integer bins
using Hist1D = RExp::RHist<1, size_t>;


// Source of "random" data points for histograms
//
// Always produces the same sequence of random numbers, to guarantee that all
// benchmarks are subjecting their histograms to the same workload.
//
class RandomCoords {
public:
    // Generate a random point in the histogram's axis range
    //
    // We don't use std::uniform_real_distribution because it may call the
    // underlying generator multiple times, and this makes discard() impossible
    // to implement correctly. We don't care about the tiny bias that ensues.
    //
    RExp::Hist::RCoordArray<1> gen() {
        static const float a = (AXIS_RANGE.second - AXIS_RANGE.first)
                                   / (RNG::max() - RNG::min());
        static const float b = AXIS_RANGE.first;
        return { a * m_gen() + b };
    }

    // Skip N random rolls
    void discard(std::size_t num_rolls) { m_gen.discard(num_rolls); }

private:
    using RNG = std::mt19937;
    RNG m_gen;
};


// Basic microbenchmark harness
void bench(const std::string& name,
           std::function<Hist1D(Hist1D&&, RandomCoords&&)>&& work)
{
    using namespace std::chrono;
    std::cout << "* " << name;

    auto start = high_resolution_clock::now();
    Hist1D hist = work(Hist1D{{NUM_BINS,
                               AXIS_RANGE.first,
                               AXIS_RANGE.second}},
                       RandomCoords{});
    auto end = high_resolution_clock::now();

    if ( hist.GetEntries() != NUM_ITERS ) {
        throw std::runtime_error("Bad number of histogram entries");
    }
    // TODO: More correctness assersions would be nice:
    //       - Record output of first benchmark run
    //       - Check that number & contents of bins are identical for other runs
    //       - Can also dive into GetImpl, at a future compatibility cost.

    auto nanos_per_iter = duration_cast<duration<float, std::nano>>(end - start)
                          / NUM_ITERS;
    std::cout << " -> " << nanos_per_iter.count() << " ns/iter" << std::endl;
}


// Some benchmarks depend on a batch size parameter that must be known at
// compile time. We need to generate those using a template.
//
// BATCH_SIZE should be a power of 2 in order to evenly divide NUM_ITERS
//
template <size_t BATCH_SIZE>
void bench_batch()
{
    std::cout << "=== BATCH SIZE: " << BATCH_SIZE << " ===" << std::endl;

    // Manually insert data points in batches using FillN()
    //
    // Amortizes some of the indirection.
    //
    bench("Manually-batched FillN()", [&](Hist1D&& hist,
                                          RandomCoords&& rng) -> Hist1D {
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
    bench("ROOT-batched Fill()", [&](Hist1D&& hist,
                                     RandomCoords&& rng) -> Hist1D {
        RExp::RHistBufferedFill<Hist1D, BATCH_SIZE> buf_hist{hist};
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            buf_hist.Fill(rng.gen());
        }
        return hist;
    });

    // Sequential use of RHistConcurrentFiller
    //
    // Combines batching akin to the one of RHistBufferedFill with mutex
    // protection on the histogram of interest.
    //
    bench("Serial \"concurrent\" Fill()", [&](Hist1D&& hist,
                                              RandomCoords&& rng) -> Hist1D {
        RExp::RHistConcurrentFillManager<Hist1D, BATCH_SIZE> conc_hist{hist};
        auto conc_hist_filler = conc_hist.MakeFiller();
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            conc_hist_filler.Fill(rng.gen());
        }
        return hist;
    });

    // Parallel use of RHistConcurrentFiller
    bench("Parallel concurrent Fill()", [&](Hist1D&& hist,
                                            RandomCoords&& rng) -> Hist1D {
        // Shared concurrent histogram filler
        RExp::RHistConcurrentFillManager<Hist1D, BATCH_SIZE> conc_hist{hist};

        // Only check the host CPU's thread count once
        static const auto num_threads = std::thread::hardware_concurrency();
        static const auto local_iters = NUM_ITERS / num_threads;

        // Thread startup synchronization + storage for worker threads
        auto barrier = std::atomic{num_threads};
        auto threads = std::vector<std::thread>{};
        threads.reserve(num_threads-1);

        // Threads (including ourselves) will do this:
        auto work = [&]( size_t thread_id ) {
            // Setup thread-local RNG and histogram filler
            auto local_rng = rng;
            local_rng.discard(thread_id * local_iters);
            auto conc_hist_filler = conc_hist.MakeFiller();

            // Signal that we are ready + wait for other threads to be ready
            barrier.fetch_sub(1, std::memory_order_release);
            while (barrier.load(std::memory_order_acquire)) {}

            // Fill the histogram, then let it auto-flush
            for ( size_t i = 0; i < local_iters; ++i ) {
                conc_hist_filler.Fill(local_rng.gen());
            }
        };

        // Start all secondary threads
        for ( size_t thread_id = 1; thread_id < num_threads; ++thread_id ) {
            threads.emplace_back([&] { work(thread_id); });
        }

        // Do our share of the work
        work(0);

        // Wait for all secondary threads to finish
        for ( auto& thread: threads ) {
            thread.join();
        }

        // Output the final histogram
        return hist;
    });

    // TODO: Compare with other synchronization strategies
    //       - Fill thread-local histograms, merge at the end
    //       - Use std::atomic<BinData> as bin data type
    //       - Use a specialized variant of std::atomic that performes relaxed
    //         atomic operations instead of sequentially consistent ones.
    //
    //       Will probably want to factor out redundant parts from the MT test
    //       harness above when that time comes.
    //
    //       Not sure how compatible these other sync strategies are with
    //       complex binning schemes such as growable axes.

    std::cout << std::endl;
}


// Top-level benchmark logic
int main()
{
    std::cout << "=== NO BATCHING ===" << std::endl;

    // Unoptimized sequential Fill() pattern
    //
    // Pretty slow, as ROOT histograms have quite a few layers of indirection...
    //
    bench("Scalar Fill()", [&](Hist1D&& hist,
                               RandomCoords&& rng) -> Hist1D {
        for ( size_t i = 0; i < NUM_ITERS; ++i ) {
            hist.Fill(rng.gen());
        }
        return hist;
    });

    std::cout << std::endl;

    // So, I heard that C++ doesn't have constexpr for loops...
    bench_batch<1>();
    bench_batch<8>();
    bench_batch<128>();
    bench_batch<1024>();
    bench_batch<2048>();
    bench_batch<4096>();
    bench_batch<8192>();
    bench_batch<16384>();

    // TODO: Besides benchmarking & perf studies, in a different program...
    //       - Convert final histogram to ROOT 6 format
    //       - Write ROOT 6 histogram to file as a proof of concept.

    return 0;
}