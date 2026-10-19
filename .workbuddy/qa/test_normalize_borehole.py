# -*- coding: utf-8 -*-
"""IES P2 修复 —— borehole_data 二维数组归一化层 单元测试

覆盖：
  - X/Y 坐标交换
  - test 字段全部 string 化
  - 孔号双校验 400（row[4][0]!="钻孔编号" 或 row[4][1]≠顶层 key）
  - 跳过"终止行"（col3=="终孔"）
  - 跳过"负责人行"（col0 含"项目负责人"）
  - sample 皆空不输出
  - test.type 空不输出 test
  - 形态A（原样保留）+ 形态B（归一化）分流
  - 实仓 ZK1 数据对账

直接 import cad_server/draw_gateway.py，不启动 app.run。

用法：  .venv\\Scripts\\python.exe test_normalize_borehole.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GATEWAY_DIR = os.path.abspath(os.path.join(HERE, "..", "..", "cad_server"))
sys.path.insert(0, GATEWAY_DIR)

import draw_gateway as gw   # noqa: E402

RESULTS = []


def record(tid, desc, expect, actual, ok):
    RESULTS.append((tid, desc, ok))
    print(f"[{'PASS' if ok else 'FAIL'}] {tid} {desc} | expect={expect} | actual={actual}")


# ============================================================
# 工具：构造一份符合形态B 11+ 行模板的"伪 row 列表"
# ============================================================
def _mk_row0():       # row 0：公司名（忽略）
    return ["某勘察公司"] + [None] * 19


def _mk_row1():       # row 1：表标题（忽略）
    return ["钻探编录表"] + [None] * 19


def _mk_row2(elev=100.0, depth=20.0, hole_type="技术孔", proj_name="测试工程"):
    # col 0=工程名称 / col 1=proj_name / col 10=孔口高程标签 / col 11=elev
    # col 12=钻孔类型标签 / col 13=None / col 14=hole_type
    # col 16=终孔深度标签 / col 17=None / col 18=depth
    row = [None] * 20
    row[0]  = "工程名称"
    row[1]  = proj_name
    row[10] = "孔口高程（m）"
    row[11] = elev
    row[12] = "钻孔类型"
    row[13] = None
    row[14] = hole_type
    row[16] = "终孔深度（m）"
    row[17] = None
    row[18] = depth
    return row


def _mk_row3(site="测试地点", date_start="2025-01-01", wl_initial=None):
    # col 0=工程地点 / col 1=site / col 10=开孔日期标签 / col 11=date_start
    # col 12=初见水位标签 / col 14=wl_initial
    row = [None] * 20
    row[0]  = "工程地点"
    row[1]  = site
    row[10] = "开孔日期"
    row[11] = date_start
    row[12] = "初见水位（m）"
    row[14] = wl_initial
    return row


def _mk_row4(hole_no="ZK99", y_north=1000.0, x_east=2000.0,
             date_end="2025-01-02", wl_stable=None):
    # col 0=钻孔编号 / col 1=hole_no / col 3=X坐标标签 / col 4=y_north
    # col 8=Y坐标标签 / col 9=x_east / col 10=终孔日期标签 / col 11=date_end
    # col 12=稳定水位标签 / col 14=wl_stable
    row = [None] * 20
    row[0]  = "钻孔编号"
    row[1]  = hole_no
    row[3]  = "X坐标（m）"
    row[4]  = y_north
    row[8]  = "Y坐标（m）"
    row[9]  = x_east
    row[10] = "终孔日期"
    row[11] = date_end
    row[12] = "稳定水位（m）"
    row[14] = wl_stable
    return row


def _mk_row5():       # row 5：地层表头起首（"地层编号"）
    row = [None] * 20
    row[0] = "地层编号"
    return row


def _mk_row6():       # row 6：地层字段表头
    row = [None] * 20
    row[0]  = None
    row[1]  = "自"
    row[2]  = "至"
    row[3]  = "地层名称"
    row[4]  = "颜色"
    row[5]  = "状态"
    row[6]  = "密实度"
    row[7]  = "湿度"
    row[8]  = "成分及其他"
    row[13] = "编号"
    row[14] = "取样深度"
    row[15] = "类型"
    row[16] = "起（m）"
    row[17] = "止（m）"
    row[18] = "杆长（m）"
    row[19] = "锤击数（击）"
    return row


def _mk_layer(layer_id="1-0-0", f=0.0, t=5.0, name="素填土",
              color="", state="", density="", moisture="", desc="",
              s_no="", s_dep="",
              t_type="", t_from=None, t_to=None, t_rod=None, t_hits=None):
    row = [None] * 20
    row[0]  = layer_id
    row[1]  = f
    row[2]  = t
    row[3]  = name
    row[4]  = color
    row[5]  = state
    row[6]  = density
    row[7]  = moisture
    row[8]  = desc
    row[13] = s_no
    row[14] = s_dep
    row[15] = t_type
    row[16] = t_from
    row[17] = t_to
    row[18] = t_rod
    row[19] = t_hits
    return row


def _mk_terminator_row():  # row N：终止行
    row = [None] * 20
    row[0] = None
    row[1] = 19.2
    row[2] = 19.2
    row[3] = "终孔"
    return row


def _mk_manager_row():     # row N-1：项目负责人行
    row = [None] * 20
    row[0] = "项目负责人：某某"
    return row


def _mk_hole_rows(hole_no, **kw):
    """构造一份合法的 11+ 行 borehole_data 单孔 rows。"""
    rows = [
        _mk_row0(),
        _mk_row1(),
        _mk_row2(),
        _mk_row3(),
        _mk_row4(hole_no=hole_no, **kw),
        _mk_row5(),
        _mk_row6(),
    ]
    return rows


def main():
    # ============================================================
    # N1 形态A/形态B 分流
    # ============================================================
    record("N1a", "_is_2d_array_format: dict of dict → False",
           "False", gw._is_2d_array_format({"ZK1": {"x": 1}}),
           gw._is_2d_array_format({"ZK1": {"x": 1}}) is False)
    record("N1b", "_is_2d_array_format: dict of list → True",
           "True", gw._is_2d_array_format({"ZK1": [[1, 2, 3]]}),
           gw._is_2d_array_format({"ZK1": [[1, 2, 3]]}) is True)
    record("N1c", "_is_2d_array_format: empty dict → False",
           "False", gw._is_2d_array_format({}),
           gw._is_2d_array_format({}) is False)

    # ============================================================
    # N2 X/Y 坐标交换（核心：测量X=北→y；测量Y=东→x）
    # ============================================================
    rows = _mk_hole_rows("ZK1", y_north=2747346.764, x_east=36524930.708)
    rows += [_mk_layer("1-0-0", 0, 7.5, "素填土")]
    raw = {"ZK1": rows}
    out = gw._normalize_borehole(raw)
    n1 = out["ZK1"]
    record("N2a", "X/Y 交换: output x = 测量Y=东",
           "36524930.708", n1["x"], n1["x"] == 36524930.708)
    record("N2b", "X/Y 交换: output y = 测量X=北",
           "2747346.764", n1["y"], n1["y"] == 2747346.764)

    # ============================================================
    # N3 test 字段全部 string 化
    # ============================================================
    rows = _mk_hole_rows("ZK1")
    rows += [_mk_layer("1-0-0", 0, 7.5, "素填土",
                       t_type="标贯", t_from=9.15, t_to=9.25,
                       t_rod=10, t_hits=2)]
    out = gw._normalize_borehole({"ZK1": rows})
    test = out["ZK1"]["layers"][0]["test"]
    record("N3a", "test.type str", "<str>", type(test["type"]).__name__,
           isinstance(test["type"], str))
    record("N3b", "test.from str('9.15')", "'9.15'", test["from"],
           test["from"] == "9.15" and isinstance(test["from"], str))
    record("N3c", "test.to str('9.25')", "'9.25'", test["to"],
           test["to"] == "9.25" and isinstance(test["to"], str))
    record("N3d", "test.rod_length str('10')", "'10'", test["rod_length"],
           test["rod_length"] == "10" and isinstance(test["rod_length"], str))
    record("N3e", "test.hits str('2')", "'2'", test["hits"],
           test["hits"] == "2" and isinstance(test["hits"], str))

    # ============================================================
    # N4 孔号双校验 400
    # ============================================================
    # 4a) row[4][0] != "钻孔编号"
    rows = _mk_hole_rows("ZK1")
    rows[4][0] = "孔号"  # 篡改
    try:
        gw._normalize_borehole({"ZK1": rows})
        ok = False
        msg = "未抛异常"
    except ValueError as e:
        ok = "ZK1" in str(e)
        msg = str(e)
    record("N4a", "孔号双校验: row[4][0]!='钻孔编号' → ValueError 含孔号",
           "含 'ZK1'", msg[:60], ok)

    # 4b) row[4][1] != 顶层 key
    rows = _mk_hole_rows("ZK1")
    rows[4][1] = "ZK_WRONG"  # 篡改
    try:
        gw._normalize_borehole({"ZK1": rows})
        ok = False
        msg = "未抛异常"
    except ValueError as e:
        ok = "ZK1" in str(e)
        msg = str(e)
    record("N4b", "孔号双校验: row[4][1]≠顶层 key → ValueError 含孔号",
           "含 'ZK1'", msg[:60], ok)

    # ============================================================
    # N5 跳过"终止行"（col3=="终孔"）
    # ============================================================
    rows = _mk_hole_rows("ZK1")
    rows += [
        _mk_layer("1-0-0", 0, 7.5, "素填土"),
        _mk_layer("2-0-0", 7.5, 19.2, "黏土"),
        _mk_terminator_row(),
    ]
    out = gw._normalize_borehole({"ZK1": rows})
    n_layers = len(out["ZK1"]["layers"])
    record("N5", "跳过终止行: 仅 2 层（不含 '终孔' 行）",
           "2", n_layers, n_layers == 2)

    # ============================================================
    # N6 跳过"负责人行"（col0 含"项目负责人"）
    # ============================================================
    rows = _mk_hole_rows("ZK1")
    rows += [
        _mk_layer("1-0-0", 0, 7.5, "素填土"),
        _mk_layer("2-0-0", 7.5, 19.2, "黏土"),
        _mk_manager_row(),
    ]
    out = gw._normalize_borehole({"ZK1": rows})
    n_layers = len(out["ZK1"]["layers"])
    record("N6", "跳过负责人行: 仅 2 层（不含 '项目负责人' 行）",
           "2", n_layers, n_layers == 2)

    # ============================================================
    # N7 sample 皆空不输出
    # ============================================================
    rows = _mk_hole_rows("ZK1")
    rows += [_mk_layer("1-0-0", 0, 7.5, "素填土")]  # 无 sample
    out = gw._normalize_borehole({"ZK1": rows})
    has_sample = "sample" in out["ZK1"]["layers"][0]
    record("N7", "sample 皆空 → 不输出 sample",
           "False (key not in layer)", "absent" if not has_sample else "present",
           has_sample is False)

    # ============================================================
    # N8 test.type 空不输出 test
    # ============================================================
    rows = _mk_hole_rows("ZK1")
    rows += [_mk_layer("1-0-0", 0, 7.5, "素填土",
                       t_type="", t_from=None, t_to=None, t_rod=None, t_hits=None)]
    out = gw._normalize_borehole({"ZK1": rows})
    has_test = "test" in out["ZK1"]["layers"][0]
    record("N8", "test.type 空 → 不输出 test",
           "False (key not in layer)", "absent" if not has_test else "present",
           has_test is False)

    # ============================================================
    # N9 形态A（原样保留，不归一化）
    # ============================================================
    shape_a = {
        "ZK1": {
            "hole_no": "ZK1",
            "x": 1.0, "y": 2.0, "elevation": 10.0, "depth": 5.0,
            "layers": [{"layer_id": "1", "from": 0, "to": 5, "name": "土"}],
        }
    }
    # 直接 _translate_borehole 应成功（形态A）
    try:
        gw._translate_borehole(shape_a)
        ok = True; msg = "translate OK"
    except Exception as e:
        ok = False; msg = f"{type(e).__name__}: {e}"
    record("N9", "形态A 走 _translate_borehole（原样保留）",
           "成功", msg, ok)

    # ============================================================
    # N10 实仓数据对账（cad_client/阳光幸福里-钻探编录表.json）
    # ============================================================
    real = r"D:\project\ies\cad_client\阳光幸福里-钻探编录表.json"
    if os.path.exists(real):
        with open(real, "r", encoding="utf-8") as f:
            raw_real = json.load(f)
        out_real = gw._normalize_borehole(raw_real)
        zk1 = out_real["ZK1"]
        record("N10a", "实仓 ZK1: x=36524930.708", "36524930.708", zk1["x"],
               zk1["x"] == 36524930.708)
        record("N10b", "实仓 ZK1: y=2747346.764", "2747346.764", zk1["y"],
               zk1["y"] == 2747346.764)
        record("N10c", "实仓 ZK1: elevation=206.05", "206.05", zk1["elevation"],
               zk1["elevation"] == 206.05)
        record("N10d", "实仓 ZK1: depth=19.2", "19.2", zk1["depth"],
               zk1["depth"] == 19.2)
        record("N10e", "实仓 ZK1: 3 layers", "3", len(zk1["layers"]),
               len(zk1["layers"]) == 3)
        t1 = zk1["layers"][0]["test"]
        record("N10f", "实仓 ZK1 layer1 test 全 string",
               "{type:str,from:str,to:str,rod_length:str,hits:str}",
               {k: type(v).__name__ for k, v in t1.items()},
               all(isinstance(v, str) for v in t1.values()))
        record("N10g", "实仓 ZK1 layer1 test 内容",
               "{type='标贯',from='9.15',to='9.25',rod_length='10',hits='2'}",
               str(t1), str(t1) == str({
                   "type": "标贯", "from": "9.15", "to": "9.25",
                   "rod_length": "10", "hits": "2"}))
    else:
        print(f"[SKIP] N10x: 实仓文件 {real} 不存在")

    # ============================================================
    # N11 _translate_borehole（英文→中文）走通归一化结果
    # ============================================================
    if os.path.exists(real):
        out_real = gw._normalize_borehole(json.load(open(real, "r", encoding="utf-8")))
        try:
            cn = gw._translate_borehole(out_real)
            zk1cn = cn["ZK1"]
            ok = (zk1cn["钻孔编号"] == "ZK1"
                  and zk1cn["X坐标(m)"] == 36524930.708
                  and zk1cn["Y坐标(m)"] == 2747346.764
                  and zk1cn["孔口高程(m)"] == 206.05
                  and zk1cn["终孔深度(m)"] == 19.2
                  and len(zk1cn["地层列表"]) == 3
                  and zk1cn["地层列表"][0]["原位测试"]["起(m)"] == "9.15"
                  and zk1cn["地层列表"][0]["原位测试"]["止(m)"] == "9.25"
                  and zk1cn["地层列表"][0]["原位测试"]["杆长(m)"] == "10"
                  and zk1cn["地层列表"][0]["原位测试"]["锤击数(击)"] == "2")
            msg = "translate OK" if ok else "translate FAIL"
        except Exception as e:
            ok = False; msg = f"{type(e).__name__}: {e}"
        record("N11", "归一化 → _translate_borehole 全链路",
               "成功 + 关键字段一致", msg[:60], ok)

    # ============================================================
    # 汇总
    # ============================================================
    total = len(RESULTS)
    passed = sum(1 for *_, ok in RESULTS if ok)
    fails = [r for r in RESULTS if not r[2]]
    print("\n============ 归一化层单元汇总：{}/{} 通过 ============".format(passed, total))
    for tid, desc, ok in RESULTS:
        if not ok:
            print(f"  FAIL {tid}: {desc}")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
