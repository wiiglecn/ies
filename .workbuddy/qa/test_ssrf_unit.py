# -*- coding: utf-8 -*-
"""IES P0 修复 —— 网关 SSRF 防御 单元测试（Round 3）

直接 import cad_server/draw_gateway.py（不启动 app.run），
对 SSRF 三函数与协议校验做隔离断言。
"""
import os
import sys
import ipaddress

HERE = os.path.dirname(os.path.abspath(__file__))
GATEWAY_DIR = os.path.abspath(os.path.join(HERE, "..", "..", "cad_server"))
sys.path.insert(0, GATEWAY_DIR)

import draw_gateway as gw   # noqa: E402

RESULTS = []


def record(tid, desc, expect, actual, ok):
    RESULTS.append((tid, desc, ok))
    print(f"[{'PASS' if ok else 'FAIL'}] {tid} {desc} | expect={expect} | actual={actual}")


def main():
    # ---- 默认态（无白名单，禁回环）----
    record("S1", "_is_loopback('127.0.0.1')", "True", gw._is_loopback("127.0.0.1"),
           gw._is_loopback("127.0.0.1") is True)
    record("S2", "_is_loopback('localhost')", "True", gw._is_loopback("localhost"),
           gw._is_loopback("localhost") is True)
    record("S3", "_ssrf_check('127.0.0.1') 禁回环", "False", gw._ssrf_check("127.0.0.1"),
           gw._ssrf_check("127.0.0.1") is False)
    record("S4", "_ssrf_check('8.8.8.8') 公网放行", "True", gw._ssrf_check("8.8.8.8"),
           gw._ssrf_check("8.8.8.8") is True)
    record("S5", "_ssrf_check('') 空 host", "False", gw._ssrf_check(""),
           gw._ssrf_check("") is False)

    # ---- 协议校验：非 http(s) ----
    try:
        gw._http_get_with_redirects("ftp://example.com/a.dwg", 5)
        proto_ok, proto_actual = False, "未抛异常"
    except ValueError as e:
        proto_ok, proto_actual = ("only http(s)" in str(e)), str(e)
    except Exception as e:
        proto_ok, proto_actual = False, f"{type(e).__name__}: {e}"
    record("S6", "非 http(s) 协议拒绝", "ValueError 'only http(s) allowed'", proto_actual, proto_ok)

    # ---- 白名单生效态：仅放行白名单 CIDR ----
    saved = gw.SSRF_WHITELIST_CIDR[:]
    try:
        gw.SSRF_WHITELIST_CIDR = [ipaddress.ip_network("10.0.0.0/8")]
        record("S7", "白名单生效时公网 8.8.8.8 被拒", "False", gw._ssrf_check("8.8.8.8"),
               gw._ssrf_check("8.8.8.8") is False)
        record("S8", "白名单命中 10.1.2.3 放行", "True", gw._ssrf_check("10.1.2.3"),
               gw._ssrf_check("10.1.2.3") is True)
        record("S9", "白名单生效时回环 127.0.0.1 被拒", "False", gw._ssrf_check("127.0.0.1"),
               gw._ssrf_check("127.0.0.1") is False)
    finally:
        gw.SSRF_WHITELIST_CIDR = saved

    total = len(RESULTS)
    passed = sum(1 for *_, ok in RESULTS if ok)
    print(f"\n============ SSRF 单元汇总：{passed}/{total} 通过 ============")
    for tid, desc, ok in RESULTS:
        if not ok:
            print(f"  FAIL {tid}: {desc}")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
