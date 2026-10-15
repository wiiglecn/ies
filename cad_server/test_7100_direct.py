# -*- coding: utf-8 -*-
"""
直连 7100 测试脚本（k-v 结构化 JSON 版）
用法:
    python test_7100_direct.py                    # 默认测平面图，用阳光幸福里数据
    python test_7100_direct.py --view column      # 测柱状图
    python test_7100_direct.py --view section     # 测剖面图
    python test_7100_direct.py --hole ZK1         # 只测单孔

流程:
    读 xls 二维数组 → 转成结构化英文 k-v JSON → x/y 换回测量北/东 → 发送给 7100
    （X/Y 轴交换由 7100 服务端内部完成）

日志输出:
    请求记录.log — 记录每次发送的请求和收到的响应（追加写）
"""
import json
import time
import argparse
import os
import sys
from datetime import datetime

import requests

# 复用 Python 网关的归一化逻辑
sys.path.insert(0, os.path.dirname(__file__))
from draw_gateway import _normalize_borehole

CAD_HTTP = os.environ.get("CAD_HTTP", "http://localhost:7100")
DEFAULT_DATA = os.path.join(os.path.dirname(__file__), "..", "cad_client", "阳光幸福里-钻探编录表.json")
LOG_FILE = os.path.join(os.path.dirname(__file__), "请求记录.log")

VIEW_ROUTES = {
    "plan":    "/ai/feature/create_plan_view",
    "column":  "/ai/feature/create_column_view",
    "section": "/ai/feature/create_section_view",
}


def log(msg: str):
    """写请求日志（追加）。"""
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{ts}] {msg}\n"
    print(line, end="")
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(line)


def to_structured_kev(rows_borehole: dict) -> dict:
    """
    把二维数组 JSON 转成结构化英文 k-v JSON。
    注意：x = 测量北，y = 测量东（不交换，交给服务端做）。
    """
    # _normalize_borehole 输出的是 x=东、y=北，我们交换回来变成 x=北、y=东
    normalized = _normalize_borehole(rows_borehole)
    for hole_no, hole in normalized.items():
        if "x" in hole and "y" in hole:
            hole["x"], hole["y"] = hole["y"], hole["x"]
    return normalized


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--view", default="plan", choices=list(VIEW_ROUTES.keys()))
    parser.add_argument("--data", default=DEFAULT_DATA, help="borehole_data JSON 文件路径（二维数组）")
    parser.add_argument("--hole", default=None, help="只测指定孔号（如 ZK1），不传则全量")
    args = parser.parse_args()

    url = CAD_HTTP + VIEW_ROUTES[args.view]

    # 1. 读取数据
    data_path = os.path.abspath(args.data)
    log(f"=== TEST START ===")
    log(f"view: {args.view}")
    log(f"target: POST {url}")
    log(f"data file: {data_path}")

    with open(data_path, "r", encoding="utf-8") as f:
        rows_borehole = json.load(f)

    # 只取指定孔号
    if args.hole:
        rows_borehole = {args.hole: rows_borehole[args.hole]}
        log(f"filtered to hole: {args.hole}")

    hole_count = len(rows_borehole)
    log(f"hole count: {hole_count}")

    # 2. 二维数组 → 结构化 k-v JSON（x=北, y=东，不交换）
    structured = to_structured_kev(rows_borehole)
    log(f"converted to structured k-v JSON")

    # 打印单孔坐标核对
    for hole_no, hole in structured.items():
        log(f"  {hole_no}: x(北)={hole.get('x')}, y(东)={hole.get('y')}, elev={hole.get('elevation')}, depth={hole.get('depth')}")

    payload = {"borehole_data": structured}

    # 3. 记录请求 body
    body_str = json.dumps(payload, ensure_ascii=False)
    if len(body_str) > 5000:
        log(f"request body (first 5000 chars of {len(body_str)}):\n{body_str[:5000]}...")
    else:
        log(f"request body:\n{body_str}")

    # 4. 发送请求
    log(f"sending request...")
    t0 = time.time()
    try:
        resp = requests.post(url, json=payload, timeout=300)
        elapsed = time.time() - t0
        log(f"response status: {resp.status_code} ({elapsed:.1f}s)")
        log(f"response body:\n{resp.text}")
    except requests.exceptions.Timeout:
        elapsed = time.time() - t0
        log(f"REQUEST TIMEOUT after {elapsed:.1f}s")
        sys.exit(1)
    except Exception as e:
        log(f"REQUEST ERROR: {type(e).__name__}: {e}")
        sys.exit(1)

    log(f"=== TEST END ===\n")


if __name__ == "__main__":
    main()
