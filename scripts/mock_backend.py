#!/usr/bin/env python3
"""
Mock backend for llm-proxy performance testing.
Simulates a real LLM API endpoint with configurable delay and error rate.
"""

import time
import random
import argparse
import json
from flask import Flask, request, jsonify

app = Flask(__name__)

# Global configuration
DELAY_MS = 0
ERROR_RATE = 0.0

@app.route('/v1/chat/completions', methods=['POST'])
def chat_completions():
    # Simulate processing delay
    if DELAY_MS > 0:
        time.sleep(DELAY_MS / 1000.0)

    # Simulate random errors
    if ERROR_RATE > 0 and random.random() < ERROR_RATE:
        return "Internal Server Error", 500

    # Parse request (optional, just to echo)
    try:
        data = request.get_json()
        model = data.get('model', 'unknown')
        messages = data.get('messages', [])
        user_msg = messages[0].get('content', '') if messages else ''
    except:
        user_msg = ''

    # Fixed mock response (OpenAI-compatible)
    response = {
        "id": "mock-" + str(random.randint(1000, 9999)),
        "object": "chat.completion",
        "created": int(time.time()),
        "model": model,
        "choices": [{
            "index": 0,
            "message": {
                "role": "assistant",
                "content": f"Mock reply to: {user_msg[:50]}"
            },
            "finish_reason": "stop"
        }],
        "usage": {
            "prompt_tokens": len(user_msg) // 4,
            "completion_tokens": 50,
            "total_tokens": len(user_msg) // 4 + 50
        }
    }
    return jsonify(response), 200

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Mock LLM backend")
    parser.add_argument('--port', type=int, default=11434, help='Port to listen on')
    parser.add_argument('--delay', type=int, default=0, help='Fixed delay in milliseconds')
    parser.add_argument('--error-rate', type=float, default=0.0, help='Probability of 5xx error (0-1)')
    parser.add_argument('--host', default='0.0.0.0', help='Host to bind')
    args = parser.parse_args()

    DELAY_MS = args.delay
    ERROR_RATE = args.error_rate
    print(f"Starting mock backend on {args.host}:{args.port}")
    print(f"Delay: {DELAY_MS}ms, Error rate: {ERROR_RATE*100:.1f}%")
    app.run(host=args.host, port=args.port, threaded=True)