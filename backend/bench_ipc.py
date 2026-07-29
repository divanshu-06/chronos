# times a full order roundtrip: python -> json -> pipe -> C++ -> back
# run from backend/ with the engine binary already built
import time
import statistics
from app.engine_bridge import _get_engine, _send_raw, _lock

N = 20_000
WARMUP = 1_000

def make_payload(i: int) -> dict:
    # limit buy way below the asks so it never crosses, just rests
    return {
        "order_id": 1_000_000 + i,
        "user_id": 2,
        "symbol": "AAPL",
        "side": "Buy",
        "type": "Limit",
        "limit_price": 100.0,
        "quantity": 100,
    }

def main():
    _get_engine()  # start the subprocess once
    with _lock:  # warmup, don't measure these
        for i in range(WARMUP):
            _send_raw("submit", make_payload(i))
    samples = []
    with _lock:
        for i in range(N):
            p = make_payload(WARMUP + i)
            t0 = time.perf_counter_ns()
            _send_raw("submit", p)
            t1 = time.perf_counter_ns()
            samples.append((t1 - t0) / 1000.0)  # ns -> us
    samples.sort()
    def pct(p):
        return samples[int(p / 100 * (len(samples) - 1))]
    print(f"IPC roundtrip (JSON + pipe + C++ submit), n={N}")
    print(f"  mean : {statistics.mean(samples):8.2f} us")
    print(f"  p50  : {pct(50):8.2f} us")
    print(f"  p99  : {pct(99):8.2f} us")
    print(f"  p999 : {pct(99.9):8.2f} us")
    print(f"  max  : {samples[-1]:8.2f} us")

if __name__ == "__main__":
    main()
