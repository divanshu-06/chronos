// quick benchmark for the matching engine
// build: g++ -std=c++17 -O2 -Iinclude src/bench.cpp src/MatchingEngine.cpp -o bin/bench
#include "MatchingEngine.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;
using ns = std::chrono::nanoseconds;

static double pct(std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    return v[static_cast<size_t>(p / 100.0 * (v.size() - 1))];
}

static void report(const char* name, std::vector<double>& lat, double wall_s, size_t n) {
    std::sort(lat.begin(), lat.end());
    double sum = 0;
    for (double x : lat) sum += x;
    double mean = sum / lat.size();
    printf("\n%-28s  (n=%zu)\n", name, n);
    printf("  mean        : %8.1f ns  (%.3f us)\n", mean, mean / 1000.0);
    printf("  p50         : %8.1f ns  (%.3f us)\n", pct(lat, 50), pct(lat, 50) / 1000.0);
    printf("  p99         : %8.1f ns  (%.3f us)\n", pct(lat, 99), pct(lat, 99) / 1000.0);
    printf("  p99.9       : %8.1f ns  (%.3f us)\n", pct(lat, 99.9), pct(lat, 99.9) / 1000.0);
    printf("  max         : %8.1f ns  (%.3f us)\n", lat.back(), lat.back() / 1000.0);
    printf("  throughput  : %8.0f orders/sec\n", n / wall_s);
}

// timing the clock itself so we can subtract it later
static double clock_overhead_ns() {
    const int N = 2'000'000;
    std::vector<double> s;
    s.reserve(N);
    for (int i = 0; i < N; ++i) {
        auto a = Clock::now();
        auto b = Clock::now();
        s.push_back(std::chrono::duration_cast<ns>(b - a).count());
    }
    std::sort(s.begin(), s.end());
    return s[s.size() / 2];
}

int main() {
    std::mt19937_64 rng(42);
    printf("=== Chronos matching engine microbenchmark ===\n");
    double ovh = clock_overhead_ns();
    printf("clock read-pair overhead (median): %.1f ns\n", ovh);

    // case A: orders that don't cross, so they just get added to the book
    {
        MatchingEngine eng;
        const int LEVELS = 1000;
        const int N = 1'000'000;
        std::vector<double> lat;
        lat.reserve(N);
        int64_t oid = 1;
        // fill the book first so the map isn't empty
        for (int i = 0; i < LEVELS; ++i) {
            Order o; o.order_id = oid++; o.user_id = 1; o.symbol = "AAPL";
            o.side = Side::Sell; o.type = OrderType::Limit;
            o.limit_price = 200.0 + i * 0.01; o.quantity = 100;
            eng.restOrder(o);
        }
        std::uniform_real_distribution<double> price(100.0, 199.0); // stays below the asks
        auto t0 = Clock::now();
        for (int i = 0; i < N; ++i) {
            Order o; o.order_id = oid++; o.user_id = 2; o.symbol = "AAPL";
            o.side = Side::Buy; o.type = OrderType::Limit;
            o.limit_price = price(rng); o.quantity = 100;
            auto s = Clock::now();
            auto fills = eng.submitOrder(o);
            auto e = Clock::now();
            asm volatile("" :: "g"(fills.data()) : "memory"); // stop -O2 deleting the call
            lat.push_back(std::chrono::duration_cast<ns>(e - s).count() - ovh);
        }
        double wall = std::chrono::duration<double>(Clock::now() - t0).count();
        report("A) resting limit (no cross)", lat, wall, N);
    }

    // case B: every order matches exactly one resting order
    {
        const int N = 1'000'000;
        std::vector<double> lat;
        lat.reserve(N);
        MatchingEngine eng;
        int64_t oid = 1;
        // one sell per buy we're about to send
        for (int i = 0; i < N; ++i) {
            Order o; o.order_id = oid++; o.user_id = 1; o.symbol = "AAPL";
            o.side = Side::Sell; o.type = OrderType::Limit;
            o.limit_price = 150.0; o.quantity = 100;
            eng.restOrder(o);
        }
        auto t0 = Clock::now();
        for (int i = 0; i < N; ++i) {
            Order o; o.order_id = oid++; o.user_id = 2; o.symbol = "AAPL";
            o.side = Side::Buy; o.type = OrderType::Limit;
            o.limit_price = 150.0; o.quantity = 100;
            auto s = Clock::now();
            auto fills = eng.submitOrder(o);
            auto e = Clock::now();
            asm volatile("" :: "g"(fills.data()) : "memory");
            lat.push_back(std::chrono::duration_cast<ns>(e - s).count() - ovh);
        }
        double wall = std::chrono::duration<double>(Clock::now() - t0).count();
        report("B) single fill (1 match)", lat, wall, N);
    }

    // case C: big market order that eats through 10 levels at once
    {
        const int N = 200'000;
        const int LEVELS = 10;
        std::vector<double> lat;
        lat.reserve(N);
        MatchingEngine eng;
        int64_t oid = 1;
        auto reseed = [&](MatchingEngine& e) {
            for (int l = 0; l < LEVELS; ++l) {
                Order o; o.order_id = oid++; o.user_id = 1; o.symbol = "AAPL";
                o.side = Side::Sell; o.type = OrderType::Limit;
                o.limit_price = 150.0 + l; o.quantity = 100;
                e.restOrder(o);
            }
        };
        auto t0 = Clock::now();
        for (int i = 0; i < N; ++i) {
            reseed(eng); // refill so there's always something to sweep
            Order o; o.order_id = oid++; o.user_id = 2; o.symbol = "AAPL";
            o.side = Side::Buy; o.type = OrderType::Market;
            o.limit_price = 0.0; o.quantity = LEVELS * 100;
            auto s = Clock::now();
            auto fills = eng.submitOrder(o);
            auto e = Clock::now();
            asm volatile("" :: "g"(fills.data()) : "memory");
            lat.push_back(std::chrono::duration_cast<ns>(e - s).count() - ovh);
        }
        double wall = std::chrono::duration<double>(Clock::now() - t0).count();
        report("C) market sweep (10 levels)", lat, wall, N);
        printf("  (throughput here is off, the reseed is inside the loop)\n");
    }

    printf("\nclock overhead (~%.0f ns) already subtracted above.\n", ovh);
    printf("this is submitOrder() only, no JSON/IPC/DB.\n");
    return 0;
}
