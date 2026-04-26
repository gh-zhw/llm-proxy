# llm-proxy

A lightweight, high-performance AI API gateway / reverse proxy for OpenAI-compatible endpoints.  
It does **not** run inference — it intercepts, validates, caches, and forwards requests, adding observability and resilience.

## Features (current)

- ✅ OpenAI-compatible `/v1/chat/completions` (non-streaming)
- ✅ Thread‑safe LRU cache with TTL
- ✅ YAML configuration + CLI overrides (`--port`, `--log-level`, `--config`)
- ✅ Graceful shutdown (SIGINT/SIGTERM)
- ✅ Structured logging (spdlog, console colour)
- ✅ Retry with exponential backoff & timeout control
- ✅ Cache hit‑rate reporter thread

## Build & Run

### Dependencies
- C++20 compiler (GCC 11+ / Clang 14+)
- CMake 3.22+
- Python 3.8+ (for test scripts)

### Build
```bash
git clone https://github.com/gh-zhw/llm-proxy.git
cd llm-proxy
cmake -B build
cmake --build build --parallel $(nproc)
```

### Start the proxy
```bash
./build/bin/llm-proxy --config config/proxy.yaml
```

### Start mock backend (simulates LLM, 200ms delay)
```bash
python scripts/mock_backend.py --port 11434 --delay 200
```

### Testing
```bash
# Scenario A: cache miss – every request different prompt
python scripts/benchmark.py --proxy http://localhost:8080 --concurrency 50 --total 5000 --mode cache_miss

# Scenario B: cache hit – all requests identical (>95% hit after first)
python scripts/benchmark.py --proxy http://localhost:8080 --concurrency 100 --total 10000 --mode cache_hit

# Scenario C: mixed – 70% hits, 30% misses
python scripts/benchmark.py --proxy http://localhost:8080 --concurrency 50 --total 5000 --mode mixed --hit-ratio 0.7
```

### Configuration
Edit config/proxy.yaml to adjust:

- Server port / listen address

- Backend URL, timeout, max retries

- Cache size (entries), TTL

- Log level

Command-line overrides:

```bash
./llm-proxy --config custom.yaml --port 9090 --log-level debug
```
