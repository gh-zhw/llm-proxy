#!/usr/bin/env python3
from flask import Flask, request, jsonify
import time
import random

app = Flask(__name__)

@app.route('/v1/chat/completions', methods=['POST'])
def chat():
    # time.sleep(2)  # simulate timeout
    # if random.random() < 0.3: return "Internal error", 500
    data = request.get_json()
    model = data.get('model', 'unknown')
    messages = data.get('messages', [])
    response = {
        "id": "mock-response",
        "object": "chat.completion",
        "created": 1234567890,
        "model": model,
        "choices": [{
            "index": 0,
            "message": {
                "role": "assistant",
                "content": f"Mock reply to: {messages[0]['content'] if messages else ''}"
            },
            "finish_reason": "stop"
        }]
    }
    return jsonify(response), 200

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=11434)