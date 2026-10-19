# -*- coding: utf-8 -*-
"""Mock CAD host for IES gateway regression tests.

Stands in for the real CAD plugin HTTP endpoint (default http://localhost:7100).
Every POST returns 200 + a small JSON echo so the gateway's _call_cad() succeeds.
"""
from flask import Flask, request, jsonify

app = Flask(__name__)


@app.route("/", methods=["GET"])
def root():
    return jsonify({"mock_cad": "ok"})


@app.route("/<path:sub>", methods=["POST", "GET"])
def echo(sub):
    return jsonify({"mock_cad": "ok", "route": "/" + sub})


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=7100, debug=False, use_reloader=False)
