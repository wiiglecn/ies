#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
excel_to_borehole_data.py
========================

将 `cad_client/阳光幸福里-钻探编录表.xlsx`（或同类钻探编录 xlsx）转换为
`cad_server/draw_gateway.py` 与 `cad/exploration` ARX 端期望的 borehole_data JSON 结构。

输入格式（基于资料/阳光幸福里项目/3.导出的施工勘探数据/阳光幸福里-钻探编录表.xlsx 反推）
----------------------------------------------------------------
xlsx 中每个 sheet 对应一个孔号（"ZK1"/"ZK2"/…），每孔 11+ 行：

  Row 0         col0           工程公司名称
  Row 2         col1             工程名称（如"阳光幸福里"）
                col11            孔口高程（m）
                col18            终孔深度（m）
                col14            钻孔类型（技术孔/鉴别孔）
  Row 3         col1             工程地点
                col11            开孔日期
  Row 4         col1             孔号
                col4             X坐标(m)         ← **测量学口径：X = 北坐标 N**
                col9             Y坐标(m)         ← **测量学口径：Y = 东坐标 E**
                col11            终孔日期
  Row 5         col0             "地层"（表头起首）
                col1             "进尺自(m)"
                col3             "地层名称"
                col4             "颜色"
                col5             "状态"
                col6             "密实度"
                col7             "湿度"
                col8             "成分及其他"
                col13            "取样编号"
                col14            "取样深度"
                col15            "原位类型"
                col16            "原位起(m)"
                col17            "原位止(m)"
                col18            "原位击数"
                col19            "原位校正值"
  Row 6         重复表头
  Row 7~N+1   每个地层一行
  …
  Row N         col1=终止进尺（数字），col2=终止进尺（数字），col3="终孔"

输出格式（与 cad/exploration/Debug/project/钻探编录表.json 完全一致）
----------------------------------------------------------------
```
{
  "<孔号>": {
    "钻孔编号":        "<hole_number>",
    "X坐标(m)":        "xlsx col9 测量东坐标 = AutoCAD 横向 X",
    "Y坐标(m)":        "xlsx col4 测量北坐标 = AutoCAD 纵向 Y",
    "孔口高程(m)":     "xlsx row2 col11",
    "终孔深度(m)":     "xlsx row2 col18",
    "钻孔类型":        "xlsx row2 col14",
    "工程名称":        "xlsx row2 col1",
    "工程地点":        "xlsx row3 col1",
    "开孔日期":        "xlsx row3 col11",
    "终孔日期":        "xlsx row4 col11",
    "地层列表": [
      {
        "地层编号":        "xlsx rowN col0",
        "进尺自(m)":     "xlsx rowN col1",
        "进尺至(m)":     "xlsx rowN col2",
        "地层名称":      "xlsx rowN col3",
        "颜色":            "xlsx rowN col4",
        "状态":            "xlsx rowN col5",
        "密实度":        "xlsx rowN col6",
        "湿度":            "xlsx rowN col7",
        "成分及其他":    "xlsx rowN col8",
        "取岩(土、水)样": {
          "编号":         "xlsx rowN col13",
          "取样深度":    "xlsx rowN col14"
        },
        "原位测试": {
          "类型":         "xlsx rowN col15",
          "起(m)":        "xlsx rowN col16",
          "止(m)":        "xlsx rowN col17",
          "击数":         "xlsx rowN col18",
          "校正值(击)": "xlsx rowN col19"
        }
      },
      …
    ]
  }
}
```

⚠️ **坐标系关键点（X/Y 装反风险）**
- 测量学惯例：X = 北坐标 N, Y = 东坐标 E
- AutoCAD 惯例：X = 横向（=东）, Y = 纵向（=北）
- cad_client xlsx 列名按测量学 → 适配器必须显式做 X/Y 字段交换
- 任何"字段名直译"都会出错

用法
----------------------------------------------------------------
```bash
# 默认：转换资料/阳光幸福里项目/3.导出的施工勘探数据/阳光幸福里-钻探编录表.xlsx
python cad_client/excel_to_borehole_data.py

# 显式：
输入文件=资料/阳光幸福里项目/3.导出的施工勘探数据/阳光幸福里-钻探编录表.xlsx \\
python cad_client/excel_to_borehole_data.py

# 同时转换另一本（钻探记录表）：
输入文件=资料/阳光幸福里项目/3.导出的施工勘探数据/阳光幸福里-钻探记录表.xlsx \\
python cad_client/excel_to_borehole_data.py
```

输出：与 xlsx 同目录的 `_borehole.json` 文件（如阳光幸福里-钻探编录表_borehole.json）。

对账
----------------------------------------------------------------
跑完脚本后输出"对账报告"——与 `cad/exploration/Debug/project/钻探编录表.json`
对比 X/Y/孔口/终孔深度；如 4 项均相等（容差 1e-6），说明 borehole_data JSON
可放心覆盖 ARX 端 project/ 钻探编录表.json。
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

# 复用 cad_client/excel_to_json.py 的标准库 xlsx 解析（无需 openpyxl）
sys.path.insert(0, str(Path(__file__).resolve().parent))
from excel_to_json import parse_xlsx_with_stdlib  # noqa: E402


# ========================================================================
# 列 / 行偏移常量（基于 cad_client 阳光幸福里 xlsx 11 行模板反推）
# ========================================================================
ROW_HEADER        = 0   # 工程公司名
ROW_INFO_1        = 2   # 工程名称 / 孔口高程 / 终孔深度 / 钻孔类型
ROW_INFO_2        = 3   # 工程地点 / 开孔日期
ROW_INFO_3        = 4   # 孔号 / X坐标(m) / Y坐标(m) / 终孔日期
ROW_STRATA_HEADER = 5   # 地层表头起首（"地层"标题）
ROW_STRATA_H2     = 6   # 地层字段表头
ROW_STRATA_START  = 7    # 第一条地层数据

COL_HOLE          = 1   # 孔号（Row 4 col 1）
COL_X_SURVEY     = 4   # 测量 X（北）→ 映射到 JSON Y坐标(m)
COL_Y_SURVEY     = 9   # 测量 Y（东）→ 映射到 JSON X坐标(m)
COL_ELEVATION    = 11  # 孔口高程
COL_DEPTH        = 18  # 终孔深度
COL_HOLE_TYPE    = 14  # 钻孔类型
COL_PROJ_NAME    = 1   # 工程名称
COL_PROJ_LOC     = 1   # 工程地点
COL_DATE_START   = 11  # 开孔日期
COL_DATE_END     = 11  # 终孔日期

# 地层字段列
COL_STRATA_ID         = 1   # 地层编号（Row 7+ col 0；偏移 0 在 cell index）
COL_STRATA_FROM       = 1   # 进尺自(m) Row 7+ col 1
COL_STRATA_TO         = 2   # 进尺至(m) Row 7+ col 2
COL_STRATA_NAME       = 3   # 地层名称 Row 7+ col 3
COL_STRATA_COLOR      = 4   # 颜色 Row 7+ col 4
COL_STRATA_STATE      = 5   # 状态
COL_STRATA_DENSITY    = 6   # 密实度
COL_STRATA_MOISTURE   = 7   # 湿度
COL_STRATA_DESC       = 8   # 成分及其他
COL_SAMPLE_NO         = 13  # 取样编号
COL_SAMPLE_DEPTH      = 14  # 取样深度
COL_TEST_TYPE         = 15  # 原位类型
COL_TEST_FROM         = 16  # 原位起(m)
COL_TEST_TO           = 17  # 原位止(m)
COL_TEST_HITS         = 18  # 原位击数
COL_TEST_CORRECTED    = 19  # 原位校正值


def _get_str(row: list[Any], col: int) -> str:
    """取单元格值（字符串形式）。空值返回 ''。"""
    if col >= len(row):
        return ""
    v = row[col]
    if v is None:
        return ""
    return str(v).strip()


def _get_num(row: list[Any], col: int) -> float | None:
    """取单元格值（数字形式）。空/非数字返回 None。"""
    if col >= len(row):
        return None
    v = row[col]
    if v is None or v == "":
        return None
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def convert_sheet_to_borehole(zk_id: str, rows: list[list[Any]]) -> dict[str, Any]:
    """单孔 sheet 转换为 borehole_data 节点。

    rows: 该 sheet 所有行（list of list）。
    """
    if len(rows) < ROW_STRATA_START:
        raise ValueError(f"孔 {zk_id}: 行数 {len(rows)} < {ROW_STRATA_START}，模板异常")

    # ----- 基础信息（row 2/3/4） -----
    info2 = rows[ROW_INFO_1]
    info3 = rows[ROW_INFO_2]
    info4 = rows[ROW_INFO_3]

    project_name = _get_str(info2, COL_PROJ_NAME)
    elevation = _get_num(info2, COL_ELEVATION)
    depth = _get_num(info2, COL_DEPTH)
    hole_type = _get_str(info2, COL_HOLE_TYPE)
    project_loc = _get_str(info3, COL_PROJ_LOC)
    date_start = _get_str(info3, COL_DATE_START)
    date_end = _get_str(info4, COL_DATE_END)

    # ----- ⚠️ 坐标系交换 -----
    # xlsx 列名按"测量学"：col4=X(北) col9=Y(东)
    # borehole_data JSON 按"AutoCAD"：X=横向(东) Y=纵向(北)
    # 显式交换：JSON X ← xlsx col9 (东)；JSON Y ← xlsx col4 (北)
    x_cad = _get_num(info4, COL_Y_SURVEY)   # AutoCAD 横向 X = 测量东
    y_cad = _get_num(info4, COL_X_SURVEY)   # AutoCAD 纵向 Y = 测量北

    # ----- 地层列表（row 7 起） -----
    strata: list[dict[str, Any]] = []
    for r in range(ROW_STRATA_START, len(rows)):
        row = rows[r]
        # 空行跳过
        if not any(v is not None and str(v).strip() for v in row):
            continue
        # 终止行：col1/col2 为终止进尺数字、col3 == "终孔"
        if _get_str(row, 3) == "终孔":
            continue

        s_from = _get_num(row, COL_STRATA_FROM)
        s_to = _get_num(row, COL_STRATA_TO)
        # 跳过无有效进尺的行
        if s_from is None and s_to is None:
            continue

        layer: dict[str, Any] = {
            "地层编号":     _get_str(row, 0),
            "进尺自(m)":   s_from,
            "进尺至(m)":   s_to,
            "地层名称":     _get_str(row, COL_STRATA_NAME),
            "颜色":         _get_str(row, COL_STRATA_COLOR),
            "状态":         _get_str(row, COL_STRATA_STATE),
            "密实度":       _get_str(row, COL_STRATA_DENSITY),
            "湿度":         _get_str(row, COL_STRATA_MOISTURE),
            "成分及其他": _get_str(row, COL_STRATA_DESC),
        }
        # 取样（编号 + 取样深度）
        sample_no = _get_str(row, COL_SAMPLE_NO)
        sample_depth = _get_str(row, COL_SAMPLE_DEPTH)
        if sample_no or sample_depth:
            layer["取岩(土、水)样"] = {
                "编号": sample_no,
                "取样深度": sample_depth,
            }
        # 原位测试（类型 + 起止 + 击数 + 校正值）
        test_type = _get_str(row, COL_TEST_TYPE)
        test_from = _get_num(row, COL_TEST_FROM)
        test_to = _get_num(row, COL_TEST_TO)
        test_hits = _get_num(row, COL_TEST_HITS)
        test_corr = _get_num(row, COL_TEST_CORRECTED)
        if test_type or test_from is not None or test_to is not None or test_hits is not None:
            layer["原位测试"] = {
                "类型": test_type,
                "起(m)": test_from,
                "止(m)": test_to,
                "击数": test_hits,
                "校正值(击)": test_corr,
            }
        strata.append(layer)

    # ----- borehole_data 节点 -----
    node: dict[str, Any] = {
        "钻孔编号":     zk_id,
        "X坐标(m)":     x_cad,
        "Y坐标(m)":     y_cad,
        "孔口高程(m)":  elevation,
        "终孔深度(m)":  depth,
        "钻孔类型":     hole_type,
        "工程名称":     project_name,
        "工程地点":     project_loc,
        "开孔日期":     date_start,
        "终孔日期":     date_end,
        "地层列表":     strata,
    }
    return node


def convert_xlsx_to_borehole(xlsx_path: Path) -> dict[str, Any]:
    """整本 xlsx 转 borehole_data JSON。"""
    parsed = parse_xlsx_with_stdlib(xlsx_path)
    out: dict[str, Any] = {}
    for zk_id in sorted(parsed.keys()):
        rows = parsed[zk_id]
        if not isinstance(rows, list):
            raise ValueError(f"孔 {zk_id}: 行数据不是 list（实际 {type(rows).__name__}）")
        out[zk_id] = convert_sheet_to_borehole(zk_id, rows)
    return out


# ========================================================================
# 对账报告
# ========================================================================
def diff_vs_arx(generated: dict[str, Any],
                arx_path: Path,
                tol: float = 1e-6) -> dict[str, Any]:
    """与 cad/exploration/Debug/project/钻探编录表.json 对比关键字段。"""
    if not arx_path.exists():
        return {"status": "skip", "reason": f"{arx_path} 不存在"}

    with open(arx_path, "r", encoding="utf-8") as f:
        arx = json.load(f)

    report = {"status": "ok", "孔数对比": {}, "差异项": []}
    common_zk = set(generated.keys()) & set(arx.keys())
    only_in_gen = set(generated.keys()) - set(arx.keys())
    only_in_arx = set(arx.keys()) - set(generated.keys())
    report["孔数对比"] = {
        "generated": len(generated),
        "arx": len(arx),
        "共同": len(common_zk),
        "仅生成": sorted(only_in_gen),
        "仅ARX": sorted(only_in_arx),
    }

    fields = ["X坐标(m)", "Y坐标(m)", "孔口高程(m)", "终孔深度(m)"]
    for zk in sorted(common_zk):
        g = generated[zk]
        a = arx.get(zk, {})
        for f in fields:
            gv = g.get(f)
            av = a.get(f)
            if isinstance(gv, (int, float)) and isinstance(av, (int, float)):
                if abs(float(gv) - float(av)) > tol:
                    report["差异项"].append({
                        "孔": zk, "字段": f,
                        "generated": gv, "arx": av,
                        "diff": float(gv) - float(av),
                    })
            elif gv != av:
                report["差异项"].append({
                    "孔": zk, "字段": f,
                    "generated": gv, "arx": av,
                })

    if report["差异项"] or only_in_gen or only_in_arx:
        report["status"] = "diff"
    return report


# ========================================================================
# CLI 入口
# ========================================================================
DEFAULT_XLSX = Path("资料/阳光幸福里项目/3.导出的施工勘探数据/阳光幸福里-钻探编录表.xlsx")
ARX_REF = Path("cad/exploration/Debug/project/钻探编录表.json")


def main() -> int:
    xlsx_str = sys.argv[1] if len(sys.argv) > 1 else None
    xlsx = Path(xlsx_str) if xlsx_str else DEFAULT_XLSX
    if not xlsx.exists():
        print(f"[ERROR] xlsx 文件不存在：{xlsx}", file=sys.stderr)
        return 1

    print(f"[INFO] 解析：{xlsx}")
    borehole = convert_xlsx_to_borehole(xlsx)
    print(f"[INFO] 孔数：{len(borehole)}")

    # 写到 xlsx 同目录的 _borehole.json
    out_path = xlsx.with_name(f"{xlsx.stem}_borehole.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(borehole, f, ensure_ascii=False, indent=2)
    print(f"[INFO] 已生成：{out_path}")

    # 抽样打印 ZK1
    zk1 = borehole.get("ZK1")
    if zk1:
        print("\n[样本 ZK1]")
        for k, v in zk1.items():
            if k != "地层列表":
                print(f"  {k}: {v!r}")
        print(f"  地层列表: {len(zk1.get('地层列表', []))} 层")
        for L in zk1.get("地层列表", []):
            print(f"    - {L.get('地层编号')} {L.get('地层名称')} "
                  f"{L.get('进尺自(m)')}-{L.get('进尺至(m)')}m")

    # 对账
    print(f"\n[对账] vs {ARX_REF}")
    rep = diff_vs_arx(borehole, ARX_REF)
    print(json.dumps(rep, ensure_ascii=False, indent=2))

    if rep["status"] == "diff":
        print("\n[WARN] 与 ARX 端 borehole_data JSON 存在差异！")
        print("  → 检查 X/Y 是否装反（generated/expected 字段值差几个数量级即装反）")
        return 2

    print("\n[OK] 对账通过；生成的 borehole_data JSON 与 ARX 端 project/ 钻探编录表.json 关键字段一致。")
    print("→ 下一步由用户把本 JSON 拷贝到 cad/exploration/Debug/project/钻探编录表.json 覆盖。")
    return 0


if __name__ == "__main__":
    sys.exit(main())