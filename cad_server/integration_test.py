# -*- coding: utf-8 -*-
"""
阳光幸福里项目 · 集成测试脚本 V1.0
依据: docs/阳光幸福里项目-集成测试方案-V1.0-2026-10-14.md

用法:
    # 1) 先启动网关（另一个终端）:
    #    python draw_gateway.py --port 7200 --host 127.0.0.1
    # 2) 跑测试:
    python integration_test.py                 # 全量
    python integration_test.py --boundary      # 仅边界用例（不依赖 CAD :7100）
    python integration_test.py --cad           # 仅需 CAD 的用例（需 AutoCAD+SLINIT）

退出码: 0 = 全过; 1 = 有用例失败/跳过
"""
import argparse
import json
import os
import sys
import time
import io
import requests

# ---------- 配置 ----------
GATEWAY = os.environ.get("GATEWAY", "http://127.0.0.1:7200")
CAD_HTTP = os.environ.get("CAD_HTTP", "http://127.0.0.1:7100")
ROOT = os.path.dirname(os.path.abspath(__file__))
BOREHOLE_SRC = r"D:\project\ies\cad_client\阳光幸福里-钻探编录表.json"
BASEMAP = r"D:\project\ies\资料\阳光幸福里项目\1.甲方的项目资料\05阳光幸福里 总平面规划图10.4.dwg"

results = []  # (id, 形态, 目的, 状态, 说明)


# ---------- borehole_data 解析（xlsx 二维数组 -> 接口层级结构） ----------
def parse_boreholes(path):
    """把 cad_client/阳光幸福里-钻探编录表.json（每孔 11 行 x 20 列二维数组）
    翻译成接口文档 §9.1 的层级对象结构（英文 key）。"""
    raw = json.load(open(path, encoding="utf-8"))
    out = {}
    for hole_no, rows in raw.items():
        if not isinstance(rows, list) or len(rows) < 8:
            continue
        try:
            elev = rows[2][11]
            depth = rows[2][18]
            hole_type = rows[2][14] or ""
            date_start = rows[3][11] or ""
            x = rows[4][4]
            y = rows[4][8]
            date_end = rows[4][11] or ""
            wl_init = rows[3][14] if rows[3][14] is not None else ""
            wl_stab = rows[4][14] if rows[4][14] is not None else ""
        except IndexError:
            continue
        layers = []
        # rows[7..] 为地层数据行，直到遇到 None/页脚
        for r in rows[7:]:
            if not isinstance(r, list) or not r or r[0] is None:
                break
            lid = str(r[0]).strip()
            if "项目负责人" in lid or "钻探机长" in lid:
                break
            layer = {
                "layer_id": lid,
                "from": r[1] if r[1] is not None else 0,
                "to": r[2] if r[2] is not None else 0,
                "name": r[3] or "",
                "color": r[4] or "",
                "state": r[5] or "",
                "density": r[6] or "",
                "moisture": r[7] or "",
                "description": r[8] or "",
            }
            samp_no = r[13] if len(r) > 13 else None
            samp_depth = r[14] if len(r) > 14 else None
            if samp_no:
                layer["sample"] = {"no": str(samp_no), "depth": str(samp_depth or "")}
            t_type = r[15] if len(r) > 15 else None
            if t_type:
                layer["test"] = {
                    "type": str(t_type),
                    "from": r[16] if len(r) > 16 else 0,
                    "to": r[17] if len(r) > 17 else 0,
                    "rod_length": r[18] if len(r) > 18 else 0,
                    "hits": r[19] if len(r) > 19 else 0,
                }
            layers.append(layer)
        out[hole_no] = {
            "hole_no": hole_no,
            "x": x, "y": y,
            "elevation": elev, "depth": depth,
            "hole_type": hole_type,
            "project_name": "阳光幸福里",
            "date_start": date_start, "date_end": date_end,
            "water_level_initial": wl_init, "water_level_stable": wl_stab,
            "layers": layers,
        }
    return out


MIN_BH = {
    "ZK1": {"hole_no": "ZK1", "x": 2747346.764, "y": 36524930.708,
            "elevation": 206.05, "depth": 19.2, "layers": []}
}


# ---------- 工具 ----------
def record(tid, form, purpose, status, note=""):
    results.append((tid, form, purpose, status, note))
    mark = {"PASS": "✓", "FAIL": "✗", "SKIP": "·"}.get(status, "?")
    print(f"  [{mark}] {tid:<12} {status:<5} {note}")


def check(name, cond, detail=""):
    return bool(cond), (detail if cond else f"实际不符: {detail}")


def is_cad_up():
    try:
        r = requests.get(CAD_HTTP + "/", timeout=2)
        return True
    except Exception:
        return False


def wait_gateway():
    """网关存活即可（CAD 不通时 /health 返回 503，也算网关活着）。"""
    for _ in range(15):
        try:
            # /health 服务端会先探 CAD :7100（3s 超时），这里留足 6s
            r = requests.get(GATEWAY + "/health", timeout=6)
            try:
                return r.json()
            except Exception:
                return {"raw_status": r.status_code}
        except Exception:
            time.sleep(1)
    return None


# ---------- 用例 ----------
def t_health():
    print("\n== [前置] 网关健康检查 ==")
    h = wait_gateway()
    if h is None:
        record("HEALTH", "B", "网关 /health", "FAIL", f"{GATEWAY} 不可达")
        return False
    record("HEALTH", "B", "网关 /health", "PASS",
           f"cad={h.get('cad')} cad_http={h.get('cad_http')}")
    return True


def t_d01():
    """D01-B: JSON body 含 base_map_url → 400 (D1 方案A)"""
    print("\n== D01-B: JSON body 含底图键应 400 ==")
    try:
        r = requests.post(GATEWAY + "/api/draw/plan",
                          json={"borehole_data": MIN_BH,
                                "base_map_url": "https://internal.example.com/x.dwg"},
                          timeout=10)
        ok = r.status_code == 400 and "multipart" in r.json().get("message", "")
        record("D01-B", "B", "D1: JSON含底图键→400",
               "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} body={r.text[:120]}")
    except Exception as e:
        record("D01-B", "B", "D1", "FAIL", str(e))


def t_e01():
    """E01-B: 非法 JSON → 400"""
    print("\n== E01-B: 非法 JSON → 400 ==")
    try:
        r = requests.post(GATEWAY + "/api/draw/column",
                          data='{"borehole_data":{"ZK1":{"hole_no":"ZK1",',
                          headers={"Content-Type": "application/json"},
                          timeout=10)
        ok = r.status_code == 400
        record("E01-B", "B", "非法JSON→400", "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} body={r.text[:120]}")
    except Exception as e:
        record("E01-B", "B", "非法JSON", "FAIL", str(e))


def t_e02():
    """E02-B: 上传非 .dwg → 400"""
    print("\n== E02-B: 非 .dwg 上传 → 400 ==")
    try:
        files = {"base_map": ("test.png", b"dummy", "application/octet-stream")}
        data = {"borehole_data": json.dumps(MIN_BH)}
        r = requests.post(GATEWAY + "/api/draw/plan", files=files, data=data, timeout=10)
        ok = r.status_code == 400 and r.json().get("message") == "only .dwg allowed"
        record("E02-B", "B", "非.dwg→400", "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} body={r.text[:120]}")
    except Exception as e:
        record("E02-B", "B", "非.dwg", "FAIL", str(e))


def t_e03():
    """E03-B: base_map 与 base_map_url 同传 → 400"""
    print("\n== E03-B: file+url 同传 → 400 ==")
    try:
        files = {"base_map": ("x.dwg", b"ADOB", "application/octet-stream")}
        data = {"borehole_data": json.dumps(MIN_BH),
                "base_map_url": "https://internal.example.com/x.dwg"}
        r = requests.post(GATEWAY + "/api/draw/plan", files=files, data=data, timeout=10)
        ok = r.status_code == 400 and "互斥" in r.json().get("message", "")
        record("E03-B", "B", "file+url互斥→400", "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} body={r.text[:120]}")
    except Exception as e:
        record("E03-B", "B", "互斥", "FAIL", str(e))


def t_ss01():
    """SS01-B: base_map_url=127.0.0.1 → 200 + warnings + degraded_to=process1"""
    print("\n== SS01-B: SSRF 回环拦截 → 200 + warnings ==")
    if not is_cad_up():
        record("SS01-B", "B", "SSRF回环拦截", "SKIP", "CAD :7100 未启动（需CAD出图兜底）")
        return
    try:
        data = {"borehole_data": json.dumps(MIN_BH),
                "base_map_url": "http://127.0.0.1:80/anything.dwg"}
        r = requests.post(GATEWAY + "/api/draw/plan", data=data, timeout=60)
        j = r.json()
        ok = (r.status_code == 200
              and j.get("degraded_to") == "process1_no_basemap"
              and any("校验未通过" in w for w in j.get("warnings", [])))
        record("SS01-B", "B", "SSRF回环→200+warnings",
               "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} warnings={j.get('warnings')} degraded_to={j.get('degraded_to')}")
    except Exception as e:
        record("SS01-B", "B", "SSRF", "FAIL", str(e))


def t_p02(full_bh):
    """P02-B: 流程一无底图 → degraded_to=process1_no_basemap"""
    print("\n== P02-B: 平面图流程一（无底图） ==")
    if not is_cad_up():
        record("P02-B", "B", "平面流程一", "SKIP", "CAD :7100 未启动")
        return
    try:
        r = requests.post(GATEWAY + "/api/draw/plan",
                          json={"borehole_data": full_bh, "auto_zoom": False},
                          timeout=300)
        j = r.json()
        ok = (r.status_code == 200
              and j.get("degraded_to") == "process1_no_basemap"
              and j.get("warnings") == [])
        record("P02-B", "B", "平面流程一 degraded_to=process1",
               "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} degraded_to={j.get('degraded_to')} warnings={j.get('warnings')}")
    except Exception as e:
        record("P02-B", "B", "平面流程一", "FAIL", str(e)[:150])


def t_c01(full_bh):
    """C01-B: 柱状图"""
    print("\n== C01-B: 柱状图 37 孔 ==")
    if not is_cad_up():
        record("C01-B", "B", "柱状图", "SKIP", "CAD :7100 未启动")
        return
    try:
        t0 = time.time()
        r = requests.post(GATEWAY + "/api/draw/column",
                          json={"borehole_data": full_bh, "auto_zoom": False},
                          timeout=120)
        dt = time.time() - t0
        j = r.json()
        ok = r.status_code == 200 and j.get("status") == "success"
        record("C01-B", "B", f"柱状图37孔 ({dt:.1f}s)",
               "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} body={r.text[:150]}")
    except Exception as e:
        record("C01-B", "B", "柱状图", "FAIL", str(e)[:150])


def t_s01(full_bh):
    """S01-B: 剖面图"""
    print("\n== S01-B: 剖面图 ==")
    if not is_cad_up():
        record("S01-B", "B", "剖面图", "SKIP", "CAD :7100 未启动")
        return
    section_params = {
        "sections": [{"id": "A-A'", "holes": ["ZK1", "ZK2", "ZK3"], "azimuth": 90.0}],
        "h_scale": 200, "v_scale": 200, "title": "A-A' 剖面",
    }
    try:
        r = requests.post(GATEWAY + "/api/draw/section",
                          json={"borehole_data": full_bh,
                                "section_params": section_params,
                                "auto_zoom": False},
                          timeout=180)
        j = r.json()
        ok = r.status_code == 200 and j.get("status") == "success"
        record("S01-B", "B", "剖面图 A-A'", "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} body={r.text[:150]}")
    except Exception as e:
        record("S01-B", "B", "剖面图", "FAIL", str(e)[:150])


def t_p01_upload(full_bh):
    """P01-B-UPLOAD: 上传真实底图 → 流程二"""
    print("\n== P01-B-UPLOAD: 上传真实底图（流程二） ==")
    if not is_cad_up():
        record("P01-B-UP", "B", "平面流程二上传底图", "SKIP", "CAD :7100 未启动")
        return
    if not os.path.exists(BASEMAP):
        record("P01-B-UP", "B", "上传底图", "SKIP", f"底图不存在: {BASEMAP}")
        return
    try:
        with open(BASEMAP, "rb") as f:
            files = {"base_map": (os.path.basename(BASEMAP), f, "application/octet-stream")}
            data = {
                "borehole_data": json.dumps(full_bh, ensure_ascii=False),
                "plan_base": json.dumps({"base_map": {"dim": True, "plot_scale": 500}},
                                        ensure_ascii=False),
            }
            t0 = time.time()
            r = requests.post(GATEWAY + "/api/draw/plan", files=files, data=data, timeout=300)
            dt = time.time() - t0
        j = r.json()
        ok = (r.status_code == 200 and j.get("status") == "success"
              and j.get("degraded_to") in ("process2_with_basemap", None))
        record("P01-B-UP", "B", f"上传底图流程二 ({dt:.1f}s)",
               "PASS" if ok else "FAIL",
               f"HTTP {r.status_code} degraded_to={j.get('degraded_to')} warnings={j.get('warnings')}")
    except Exception as e:
        record("P01-B-UP", "B", "上传底图", "FAIL", str(e)[:150])


def t_m01(full_bh):
    """M01-B: 四接口并存冒烟"""
    print("\n== M01-B: 三接口+旧接口并存 ==")
    if not is_cad_up():
        record("M01-B", "B", "四接口并存", "SKIP", "CAD :7100 未启动")
        return
    codes = {}
    try:
        # 旧接口
        r = requests.post(GATEWAY + "/api/draw",
                          json={"views": ["plan"], "borehole_data": full_bh,
                                "auto_zoom": False}, timeout=300)
        codes["/api/draw"] = r.status_code
        ok = all(c == 200 for c in codes.values())
        record("M01-B", "B", "旧/api/draw并存", "PASS" if ok else "FAIL",
               f"status_codes={codes}")
    except Exception as e:
        record("M01-B", "B", "四接口并存", "FAIL", str(e)[:150])


# ---------- 主流程 ----------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--boundary", action="store_true", help="仅边界用例（不依赖CAD）")
    ap.add_argument("--cad", action="store_true", help="仅需CAD的用例")
    args = ap.parse_args()

    print(f"网关: {GATEWAY}   CAD: {CAD_HTTP}")
    gw_up = t_health()
    if not gw_up:
        print("\n[!] 网关未启动，退出。请先: python draw_gateway.py --port 7200")
        sys.exit(1)

    full_bh = {}
    if os.path.exists(BOREHOLE_SRC):
        try:
            full_bh = parse_boreholes(BOREHOLE_SRC)
            print(f"[i] 已加载 {len(full_bh)} 孔 borehole_data")
        except Exception as e:
            print(f"[!] borehole 解析失败: {e}")

    boundary_only = args.boundary
    cad_only = args.cad

    # 边界用例（不依赖 CAD）
    if not cad_only:
        t_d01()
        t_e01()
        t_e02()
        t_e03()

    # 需 CAD 的用例
    if not boundary_only:
        if not is_cad_up():
            print(f"\n[i] CAD :7100 未启动 → 需 CAD 用例将 SKIP。")
            print("    请在 AutoCAD 2007 中:")
            print('      (arxload "D:/project/ies/cad/exploration/Debug/KsExploration.arx")')
            print("      SLINIT   # 看到 [CAD DEBUG] Listening started")
        t_ss01()
        t_p02(full_bh)
        t_c01(full_bh)
        t_s01(full_bh)
        t_p01_upload(full_bh)
        t_m01(full_bh)

    # 汇总
    print("\n" + "=" * 70)
    print(f"{'用例':<12}{'形态':<6}{'状态':<6}说明")
    print("-" * 70)
    n_pass = n_fail = n_skip = 0
    for tid, form, purpose, status, note in results:
        print(f"{tid:<12}{form:<6}{status:<6}{purpose} | {note[:60]}")
        if status == "PASS": n_pass += 1
        elif status == "FAIL": n_fail += 1
        else: n_skip += 1
    print("-" * 70)
    print(f"合计: PASS={n_pass}  FAIL={n_fail}  SKIP={n_skip}")

    # 落盘报告
    report = os.path.join(ROOT, "test_report.json")
    json.dump([{"id": r[0], "form": r[1], "purpose": r[2],
                "status": r[3], "note": r[4]} for r in results],
              open(report, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print(f"报告已写入: {report}")
    sys.exit(0 if n_fail == 0 else 1)


if __name__ == "__main__":
    main()
