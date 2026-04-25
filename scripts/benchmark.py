#!/usr/bin/env python3
"""
Benchmark llm-proxy with concurrent requests.
Supports cache hit/miss/mixed scenarios, collects latency percentiles, QPS, error rate.
"""

import asyncio
import aiohttp
import argparse
import time
import json
import random
import sys
from statistics import quantiles
from collections import defaultdict

# Default request template (OpenAI-compatible)
REQUEST_TEMPLATE = {
    "model": "gpt-3.5-turbo",
    "messages": [{"role": "user", "content": "Hello, this is a test message."}],
    "temperature": 0.7,
    "max_tokens": 100,
    "stream": False
}

class BenchmarkRunner:
    def __init__(self, proxy_url, concurrency, total_requests, mode,
                 prompt_length=20, mixed_hit_ratio=0.7):
        self.proxy_url = proxy_url.rstrip('/')
        self.concurrency = concurrency
        self.total = total_requests
        self.mode = mode  # 'cache_hit', 'cache_miss', 'mixed'
        self.prompt_length = prompt_length
        self.mixed_hit_ratio = mixed_hit_ratio  # for mixed mode

        # Storage for results
        self.results = []  # list of (latency_ms, status, cache_hit)
        self.errors = 0

    def generate_payload(self, request_id):
        """Generate request body based on mode."""
        payload = REQUEST_TEMPLATE.copy()
        content = "This is a fixed prompt for cache hit testing."
        if self.mode == 'cache_miss':
            # Unique content for each request
            content = f"Request {request_id}: " + "x" * self.prompt_length
        elif self.mode == 'mixed':
            # Randomly choose hit or miss
            if random.random() < self.mixed_hit_ratio:
                content = "Fixed cacheable content"
            else:
                content = f"Unique request {request_id}"
        payload["messages"][0]["content"] = content
        return payload

    async def fetch(self, session, request_id, semaphore):
        async with semaphore:
            payload = self.generate_payload(request_id)
            start = time.perf_counter()
            try:
                async with session.post(f"{self.proxy_url}/v1/chat/completions",
                                        json=payload,
                                        headers={"Content-Type": "application/json"}) as resp:
                    latency_ms = (time.perf_counter() - start) * 1000
                    status = resp.status
                    cache_hit = resp.headers.get("X-Cache-Status", "") == "HIT"
                    if status == 200:
                        await resp.text()  # consume response
                        self.results.append((latency_ms, status, cache_hit))
                    else:
                        self.errors += 1
                        # Still record error with latency
                        self.results.append((latency_ms, status, False))
            except Exception as e:
                self.errors += 1

    async def run(self):
        connector = aiohttp.TCPConnector(limit=self.concurrency, limit_per_host=self.concurrency)
        async with aiohttp.ClientSession(connector=connector) as session:
            semaphore = asyncio.Semaphore(self.concurrency)
            tasks = []
            for i in range(self.total):
                task = asyncio.create_task(self.fetch(session, i, semaphore))
                tasks.append(task)
            await asyncio.gather(*tasks)

    def compute_stats(self):
        if not self.results:
            print("No successful requests recorded.")
            return

        latencies = [r[0] for r in self.results if r[1] == 200]
        if not latencies:
            print("No successful requests to compute latency.")
            return

        latencies.sort()
        total_time = max([r[0] for r in self.results]) / 1000.0  # approximate total runtime
        qps = len(self.results) / total_time if total_time > 0 else 0

        def percentile(p):
            return latencies[int(len(latencies) * p)]

        p50 = percentile(0.5)
        p95 = percentile(0.95)
        p99 = percentile(0.99)

        cache_hits = sum(1 for r in self.results if r[2])
        hit_rate = (cache_hits / len(self.results)) * 100 if self.results else 0

        print("\n========== Benchmark Results ==========")
        print(f"Mode:           {self.mode}")
        print(f"Concurrency:    {self.concurrency}")
        print(f"Total requests: {self.total}")
        print(f"Successful:     {len(self.results)}")
        print(f"Errors:         {self.errors}")
        print(f"Error rate:     {self.errors/self.total*100:.2f}%")
        print(f"Cache hit rate: {hit_rate:.1f}%")
        print(f"QPS (approx):   {qps:.1f}")
        print(f"P50 latency:    {p50:.2f} ms")
        print(f"P95 latency:    {p95:.2f} ms")
        print(f"P99 latency:    {p99:.2f} ms")
        print("=======================================\n")

async def main():
    parser = argparse.ArgumentParser(description="Benchmark llm-proxy")
    parser.add_argument('--proxy', default='http://localhost:8080', help='Proxy URL')
    parser.add_argument('--concurrency', type=int, default=50, help='Number of concurrent workers')
    parser.add_argument('--total', type=int, default=10000, help='Total number of requests')
    parser.add_argument('--mode', choices=['cache_hit', 'cache_miss', 'mixed'], default='cache_hit',
                        help='Test scenario')
    parser.add_argument('--prompt-length', type=int, default=20, help='Length of unique prompt (for miss mode)')
    parser.add_argument('--hit-ratio', type=float, default=0.7, help='Hit ratio in mixed mode (0-1)')
    args = parser.parse_args()

    runner = BenchmarkRunner(
        proxy_url=args.proxy,
        concurrency=args.concurrency,
        total_requests=args.total,
        mode=args.mode,
        prompt_length=args.prompt_length,
        mixed_hit_ratio=args.hit_ratio
    )
    print(f"Starting benchmark: {args.mode} mode, {args.concurrency} workers, {args.total} requests")
    start_time = time.perf_counter()
    await runner.run()
    elapsed = time.perf_counter() - start_time
    print(f"Total elapsed time: {elapsed:.2f} seconds")
    runner.compute_stats()

if __name__ == '__main__':
    asyncio.run(main())