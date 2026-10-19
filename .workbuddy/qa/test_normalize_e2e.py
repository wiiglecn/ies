# -*- coding: utf-8 -*-
"""IES P2 修复 —— 归一化层端到端集成测试（4 个出图入口）

策略：
  1) 起 mock CAD
  2) 起网关（隔离 ARX_DIR）
  3) 用 cad_client/阳光幸福里-钻探编录表.json（形态B）打 4 个出图入口
  4) 读落盘文件验证中文字段
"""
import json
import os
import socket
import subprocess
import sys
import threading
import time

import requests
from werkzeug.serving import make_server

HERE = os.path.dirname(os.path.abspath(__file__))
GATEWAY_DIR = os.path.abspath(os.path.join(HERE, "..", "..", "cad_server"))
GATEWAY_PY = os.path.join(GATEWAY_DIR, "draw_gateway.py")

MOCK_PORT = 7100
GW_PORT = 7201  # 与 test_gateway.py 的 7200 错开
GW_BASE = f"http://127.0.0.1:{GW_PORT}"
ARX_DIR = os.path.join(HERE, "arx_norm_e2e")
BOREHOLE_FILE = os.path.join(ARX_DIR, "project", "钻探编录表.json")
REAL_DATA = r"D:\project\ies\cad_client\阳光幸福里-钻探编录表.json"

RESULTS = []


def record(tid, desc, expect, actual, ok):
    RESULTS.append((tid, desc, ok))
    print(f"[{'PASS' if ok else 'FAIL'}] {tid} {desc} | expect={expect} | actual={actual}")


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


def start_gateway():
    env = dict(os.environ)
    env["ARX_DIR"] = ARX_DIR
    env["CAD_HTTP"] = f"http://127.0.0.1:{MOCK_PORT}"
    args = [sys.executable, GATEWAY_PY, "--port", str(GW_PORT), "--host", "127.0.0.1"]
    return subprocess.Popen(args, cwd=GATEWAY_DIR, env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    mock = start_mock_cad()
    if not wait_port("127.0.0.1", MOCK_PORT):
        print("!! mock CAD 未就绪")
        return 2
    gw = start_gateway()
    if not wait_port("127.0.0.1", GW_PORT):
        print("!! 网关未就绪")
        gw.kill()
        return 2
    time.sleep(0.5)

    if not os.path.exists(REAL_DATA):
        print(f"!! 缺真实数据 {REAL_DATA}")
        gw.kill(); mock.shutdown()
        return 2

    with open(REAL_DATA, "r", encoding="utf-8") as f:
        bdata = json.load(f)

    try:
        # ---- 形态B 通过 4 个入口 ----
        # E1: /api/draw/column
        r = requests.post(GW_BASE + "/api/draw/column",
                          json={"borehole_data": bdata}, timeout=30)
        record("E1", "/api/draw/column 形态B → 200",
               "200", r.status_code, r.status_code == 200)

        # E2: /api/draw/section
        r = requests.post(GW_BASE + "/api/draw/section",
                          json={"borehole_data": bdata}, timeout=30)
        record("E2", "/api/draw/section 形态B → 200",
               "200", r.status_code, r.status_code == 200)

        # E3: /api/draw/plan
        r = requests.post(GW_BASE + "/api/draw/plan",
                          json={"borehole_data": bdata}, timeout=30)
        record("E3", "/api/draw/plan 形态B → 200",
               "200", r.status_code, r.status_code == 200)

        # E4: 旧 /api/draw
        r = requests.post(GW_BASE + "/api/draw",
                          json={"borehole_data": bdata}, timeout=30)
        record("E4", "/api/draw 形态B → 200",
               "200", r.status_code, r.status_code == 200)

        # 落盘中文 borehole_data 对账
        if os.path.exists(BOREHOLE_FILE):
            with open(BOREHOLE_FILE, "r", encoding="utf-8") as f:
                cn = json.load(f)
            zk1 = cn.get("ZK1", {})
            record("E5", "落盘 ZK1.X坐标(m)=36524930.708",
                   "36524930.708", zk1.get("X坐标(m)"),
                   zk1.get("X坐标(m)") == 36524930.708)
            record("E6", "落盘 ZK1.Y坐标(m)=2747346.764",
                   "2747346.764", zk1.get("Y坐标(m)"),
                   zk1.get("Y坐标(m)") == 2747346.764)
            record("E7", "落盘 ZK1.孔口高程(m)=206.05",
                   "206.05", zk1.get("孔口高程(m)"),
                   zk1.get("孔口高程(m)") == 206.05)
            record("E8", "落盘 ZK1.终孔深度(m)=19.2",
                   "19.2", zk1.get("终孔深度(m)"),
                   zk1.get("终孔深度(m)") == 19.2)
            layers = zk1.get("地层列表", [])
            record("E9", "落盘 ZK1.地层列表 3 层",
                   "3", len(layers), len(layers) == 3)
            if layers:
                test = layers[0].get("原位测试", {})
                record("E10", "落盘 ZK1 layer1.原位测试.起(m)='9.15'",
                       "'9.15'", test.get("起(m)"),
                       test.get("起(m)") == "9.15")
                record("E11", "落盘 ZK1 layer1.原位测试.杆长(m)='10'",
                       "'10'", test.get("杆长(m)"),
                       test.get("杆长(m)") == "10")
                record("E12", "落盘 ZK1 layer1.原位测试.锤击数(击)='2'",
                       "'2'", test.get("锤击数(击)"),
                       test.get("锤击数(击)") == "2")

        # ---- 形态A（dict of dict）走老路径 ----
        shape_a = {"ZK1": {"hole_no": "ZK1", "x": 1.0, "y": 2.0, "elevation": 10.0, "depth": 5.0,
                          "hole_type": "技术孔", "layers": []}}
        r = requests.post(GW_BASE + "/api/draw/column",
                          json={"borehole_data": shape_a}, timeout=30)
        record("E13", "/api/draw/column 形态A 仍可用 → 200",
               "200", r.status_code, r.status_code == 200)

        # ---- 校验 400：孔号不一致 ----
        bad = {"WRONG": list(bdata["ZK1"])}  # 顶层 key 与 row[4][1] 不一致
        # 修改 row[4][1] 让它不匹配顶层 key（双重不匹配必 400）
        bad_rows = [list(r) for r in bad["WRONG"]]
        bad_rows[4][1] = "WRONG"  # 顶层 key 是 WRONG，与 row[4][1]==WRONG 一致
        # 实际上 row[4][1]==WRONG==顶层 key，应通过校验。要触发 400，把 row[4][0] 改掉
        bad_rows[4][0] = "孔号"
        bad["WRONG"] = bad_rows
        r = requests.post(GW_BASE + "/api/draw/column",
                          json={"borehole_data": bad}, timeout=30)
        j = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        ok = r.status_code == 400 and "WRONG" in str(j.get("message", ""))
        record("E14", "孔号双校验失败 → 400 含孔号",
               "400 + 含 WRONG",
               f"status={r.status_code} msg={str(j.get('message'))[:60]}",
               ok)

    finally:
        gw.kill()
        try:
            mock.shutdown()
        except Exception:
            pass

    total = len(RESULTS)
    passed = sum(1 for *_, ok in RESULTS if ok)
    print(f"\n============ E2E 汇总：{passed}/{total} 通过 ============")
    for tid, desc, ok in RESULTS:
        if not ok:
            print(f"  FAIL {tid}: {desc}")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
