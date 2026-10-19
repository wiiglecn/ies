# -*- coding: utf-8 -*-
"""IES P0 修复 —— 网关端回归测试（Round 3）

策略：真实起服务。
  1) 起 mock CAD（127.0.0.1:7100，werkzeug 线程）
  2) 以子进程启动 draw_gateway.py（--port 7200，ARX_DIR 指向隔离目录）
  3) 用 requests 打真实 HTTP，逐条断言

用法： .venv\\Scripts\\python.exe test_gateway.py
"""
import os
import sys
import time
import json
import socket
import subprocess
import threading

import requests
from werkzeug.serving import make_server

HERE = os.path.dirname(os.path.abspath(__file__))
GATEWAY_DIR = os.path.abspath(os.path.join(HERE, "..", "..", "cad_server"))
GATEWAY_PY = os.path.join(GATEWAY_DIR, "draw_gateway.py")

MOCK_PORT = 7100
GW_PORT = 7200
GW_BASE = f"http://127.0.0.1:{GW_PORT}"
ARX_DIR = os.path.join(HERE, "arx")          # 隔离落盘目录，避免污染实仓 Debug/

BH = {"ZK1": {"x": 1.0, "y": 2.0, "elevation": 10.0, "depth": 5.0,
              "hole_type": "鉴别孔"}}

RESULTS = []   # (id, desc, expect, actual, pass_bool)


def record(tid, desc, expect, actual, ok):
    RESULTS.append((tid, desc, expect, actual, ok))
    flag = "PASS" if ok else "FAIL"
    print(f"[{flag}] {tid} {desc} | expect={expect} | actual={actual}")


def wait_port(host, port, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except OSError:
            time.sleep(0.2)
    return False


def start_mock_cad():
    sys.path.insert(0, HERE)
    import mock_cad
    srv = make_server("127.0.0.1", MOCK_PORT, mock_cad.app, threaded=True)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    return srv


def start_gateway(extra_args=None):
    env = dict(os.environ)
    env["ARX_DIR"] = ARX_DIR
    env["CAD_HTTP"] = f"http://127.0.0.1:{MOCK_PORT}"
    args = [sys.executable, GATEWAY_PY, "--port", str(GW_PORT), "--host", "127.0.0.1"]
    if extra_args:
        args += extra_args
    proc = subprocess.Popen(args, cwd=GATEWAY_DIR, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return proc


def post(path, **kw):
    return requests.post(GW_BASE + path, timeout=30, **kw)


def main():
    mock = start_mock_cad()
    if not wait_port("127.0.0.1", MOCK_PORT):
        print("!! mock CAD 未就绪")
        return 2

    gw = start_gateway()
    if not wait_port("127.0.0.1", GW_PORT):
        out = gw.stdout.read().decode("utf-8", "replace") if gw.stdout else ""
        print("!! 网关未就绪；输出:\n", out)
        gw.kill()
        return 2
    time.sleep(0.5)

    try:
        # ---------------- G1 /api/draw/column ----------------
        r = post("/api/draw/column", json={"borehole_data": BH})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G1", "/api/draw/column 200+必要字段", "200 & status=success & results.column",
               f"{r.status_code} status={j.get('status')} keys={list(j.keys())}",
               r.status_code == 200 and j.get("status") == "success" and "column" in j.get("results", {}))

        # ---------------- G2 /api/draw/section ----------------
        r = post("/api/draw/section", json={"borehole_data": BH})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G2", "/api/draw/section 200", "200 & status=success & results.section",
               f"{r.status_code} status={j.get('status')} keys={list(j.keys())}",
               r.status_code == 200 and j.get("status") == "success" and "section" in j.get("results", {}))

        # ---------------- G3 /api/draw/plan + degraded_to ----------------
        r = post("/api/draw/plan", json={"borehole_data": BH})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G3", "/api/draw/plan 无底图 degraded_to", "200 & degraded_to=process1_no_basemap & 四字段",
               f"{r.status_code} degraded_to={j.get('degraded_to')} fields={sorted(j.keys())}",
               r.status_code == 200 and j.get("degraded_to") == "process1_no_basemap"
               and {"status", "results", "warnings", "degraded_to"}.issubset(j.keys()))

        # ---------------- G4 旧 /api/draw 仍可用 ----------------
        r = post("/api/draw", json={"borehole_data": BH})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G4", "旧 /api/draw 保留可用", "200 & status=success & results 三视图",
               f"{r.status_code} status={j.get('status')} results={sorted(j.get('results', {}).keys())}",
               r.status_code == 200 and j.get("status") == "success"
               and {"plan", "section", "column"}.issubset(j.get("results", {}).keys()))

        # ---------------- G5 D1：JSON body 含底图键 → 400 ----------------
        r = post("/api/draw/plan", json={"borehole_data": BH, "base_map_url": "http://example.com/x.dwg"})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G5a", "/api/draw/plan JSON 含 base_map_url → 400", "400",
               f"{r.status_code} msg={str(j.get('message'))[:40]}",
               r.status_code == 400)
        r = post("/api/draw/plan", json={"borehole_data": BH, "base_map": {"dwg": "x.dwg"}})
        record("G5b", "/api/draw/plan JSON 含 base_map → 400", "400", f"{r.status_code}", r.status_code == 400)
        # 旧接口行为（信息性，用于核对 D1 作用域）
        r = post("/api/draw", json={"borehole_data": BH, "base_map_url": "http://example.com/x.dwg"})
        record("G5c", "[信息] 旧 /api/draw JSON 含 base_map_url", "200(旧接口不改, D3) 或 400",
               f"{r.status_code}", True)

        # ---------------- G6 SSRF：multipart base_map_url=127.0.0.1 被拒 ----------------
        r = post("/api/draw/plan",
                 data={"borehole_data": json.dumps(BH),
                       "base_map_url": "http://127.0.0.1/x.dwg"})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        warn = j.get("warnings", [])
        record("G6", "SSRF 回环 base_map_url 被拒", "200 & warnings 非空 & degraded_to=process1_no_basemap",
               f"{r.status_code} warnings={warn} degraded_to={j.get('degraded_to')}",
               r.status_code == 200 and len(warn) > 0
               and j.get("degraded_to") == "process1_no_basemap")

        # ---------------- G7 扩展名非 .dwg → 400 ----------------
        r = post("/api/draw/plan", data={"base_map_url": "http://example.com/x.txt"})
        record("G7", "base_map_url 非 .dwg → 400", "400", f"{r.status_code}", r.status_code == 400)

        # ---------------- G8 同传 base_map + base_map_url → 400 ----------------
        r = post("/api/draw/plan",
                 files={"base_map": ("a.dwg", b"DWG")},
                 data={"base_map_url": "http://example.com/x.dwg"})
        record("G8", "base_map 与 base_map_url 同传 → 400", "400", f"{r.status_code}", r.status_code == 400)

        # ---------------- G9 multipart 上传 .dwg → 流程二 ----------------
        r = post("/api/draw/plan",
                 data={"borehole_data": json.dumps(BH)},
                 files={"base_map": ("base.dwg", b"FAKEDWG")})
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G9", "multipart 上传 .dwg → degraded_to=process2_with_basemap",
               "200 & process2_with_basemap",
               f"{r.status_code} degraded_to={j.get('degraded_to')} warnings={j.get('warnings')}",
               r.status_code == 200 and j.get("degraded_to") == "process2_with_basemap")

        # ---------------- G10 非法 JSON 字段 → 400 ----------------
        r = post("/api/draw/column", json={"borehole_data": "{not-json"})
        record("G10", "borehole_data 非法 JSON → 400", "400", f"{r.status_code}", r.status_code == 400)

        # ---------------- G11 health ----------------
        r = requests.get(GW_BASE + "/health", timeout=10)
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        record("G11", "/health（mock CAD 在线）", "200 & gateway=ok",
               f"{r.status_code} {j}", r.status_code == 200 and j.get("gateway") == "ok")

        # ---------------- G12 落盘文件核对（先跑一次流程一，再检查旧配置被清） ----------------
        post("/api/draw/plan", json={"borehole_data": BH})   # 流程一：应清掉旧 plan_base.json
        pb = os.path.join(ARX_DIR, "project", "plan_base.json")
        record("G12", "流程一不残留 plan_base.json", "不存在", "存在" if os.path.exists(pb) else "不存在",
               not os.path.exists(pb))

    finally:
        gw.kill()
        try:
            mock.shutdown()
        except Exception:
            pass

    # ---------------- 汇总 ----------------
    total = len(RESULTS)
    passed = sum(1 for *_, ok in RESULTS if ok)
    fails = [r for r in RESULTS if not r[4]]
    print("\n================ 网关端汇总 ================")
    print(f"用例 {passed}/{total} 通过；失败 {len(fails)}")
    for tid, desc, exp, act, ok in fails:
        print(f"  FAIL {tid}: {desc} expect={exp} actual={act}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
