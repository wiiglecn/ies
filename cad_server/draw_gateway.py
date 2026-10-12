# -*- coding: utf-8 -*-
"""
出图网关：一次 HTTP 调用完成"收参数 -> 落盘 -> 调 ARX 出图"。

对外接口字段全部使用英文 snake_case；落盘时自动转成 C++ 端需要的中文 key。

启动：
    .venv\\Scripts\\python.exe draw_gateway.py

调用：
    POST http://localhost:7200/api/draw
    {
      "views": ["plan","section","column"],        // 可选，默认全画
      "borehole_data": { ...英文 JSON... },          // 必填
      "section_params": { ...英文 JSON... },         // 可选
      "auto_zoom": true                              // 可选
    }
"""
import os
import json
import threading
import requests
from flask import Flask, request, jsonify

# ---------- 配置 ----------
CAD_HTTP = os.environ.get("CAD_HTTP", "http://localhost:7100")
ARX_DIR = os.environ.get(
    "ARX_DIR",
    r"D:\project\kt\ies\cad\exploration\Debug",
)
PROJECT_DIR = os.path.join(ARX_DIR, "project")
BOREHOLE_FILE = os.path.join(PROJECT_DIR, "钻探编录表.json")
SECTION_FILE = os.path.join(PROJECT_DIR, "剖面图参数.json")

VIEW_ROUTES = {
    "plan":    "/ai/feature/create_plan_view",
    "section": "/ai/feature/create_section_view",
    "column":  "/ai/feature/create_column_view",
}

# ---------- 英中字段映射（对外英文 -> 落盘中文） ----------
# 单孔顶层字段
HOLE_KEY_MAP = {
    "hole_no":            "钻孔编号",
    "x":                  "X坐标(m)",
    "y":                  "Y坐标(m)",
    "elevation":          "孔口高程(m)",
    "depth":              "终孔深度(m)",
    "hole_type":          "钻孔类型",
    "project_name":       "工程名称",
    "company":            "公司名称",
    "date_start":         "开孔日期",
    "date_end":           "终孔日期",
    "page_current":       "当前页",
    "page_total":         "总页码",
    "water_level_stable": "稳定水位(m)",
    "water_level_initial":"初见水位(m)",
    "site":               "工程地点",
    "manager":            "项目负责人",
    "recorder":           "编录员",
    "driller":            "钻探机长",
    "layers":             "地层列表",
}
# 地层列表每层
LAYER_KEY_MAP = {
    "layer_id":   "地层编号",
    "from":       "进尺自(m)",
    "to":         "进尺至(m)",
    "name":       "地层名称",
    "description":"成分及其他",
    "color":      "颜色",
    "state":      "状态",
    "density":    "密实度",
    "moisture":   "湿度",
    "sample":     "取岩(土、水)样",
    "test":       "原位测试",
}
# 取样子对象
SAMPLE_KEY_MAP = {
    "no":    "编号",
    "depth": "取样深度",
}
# 原位测试子对象
TEST_KEY_MAP = {
    "type":      "类型",
    "from":      "起(m)",
    "to":        "止(m)",
    "rod_length":"杆长(m)",
    "hits":      "锤击数(击)",
}
# 剖面图参数
SECTION_KEY_MAP = {
    "sections":   "剖面组",
    "h_scale":    "水平比例",
    "v_scale":    "垂直比例",
    "title":      "标题",
    "holes":      "勘察组",
    "zero_elev":  "零标高",
    "excav_elev": "开挖标高",
}


def _remap_keys(obj, key_map):
    """把 dict 的 key 按 key_map 重命名；未知 key 原样保留。"""
    if not isinstance(obj, dict):
        return obj
    out = {}
    for k, v in obj.items():
        out[key_map.get(k, k)] = v
    return out


def _translate_borehole(eng):
    """对外英文 borehole_data -> 落盘中文 borehole_data。

    顶层 key 是孔号（"ZK1"），保持不变；孔内字段、地层、取样、原位测试递归转。
    """
    out = {}
    for hole_no, hole in eng.items():
        if not isinstance(hole, dict):
            out[hole_no] = hole
            continue
        hole_cn = _remap_keys(hole, HOLE_KEY_MAP)
        layers = hole_cn.get("地层列表")
        if isinstance(layers, list):
            new_layers = []
            for layer in layers:
                if not isinstance(layer, dict):
                    new_layers.append(layer); continue
                layer_cn = _remap_keys(layer, LAYER_KEY_MAP)
                samp = layer_cn.get("取岩(土、水)样")
                if isinstance(samp, dict):
                    layer_cn["取岩(土、水)样"] = _remap_keys(samp, SAMPLE_KEY_MAP)
                test = layer_cn.get("原位测试")
                if isinstance(test, dict):
                    layer_cn["原位测试"] = _remap_keys(test, TEST_KEY_MAP)
                new_layers.append(layer_cn)
            hole_cn["地层列表"] = new_layers
        out[hole_no] = hole_cn
    return out


def _translate_section(eng):
    """对外英文 section_params -> 落盘中文。"""
    if not isinstance(eng, dict):
        return eng
    root = _remap_keys(eng, SECTION_KEY_MAP)
    groups = root.get("剖面组")
    if isinstance(groups, list):
        root["剖面组"] = [_remap_keys(g, SECTION_KEY_MAP) if isinstance(g, dict) else g
                          for g in groups]
    return root


# CAD 主线程单线程执行，必须串行
_lock = threading.Lock()

app = Flask(__name__)


def _write_json(path, obj):
    """UTF-8 无 BOM 写 JSON，C++ 端 LoadJsonFile 按 UTF-8 读。"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)


def _call_cad(path, timeout=120):
    r = requests.post(CAD_HTTP + path, json={}, timeout=timeout)
    r.raise_for_status()
    return r.json()


@app.route("/health", methods=["GET"])
def health():
    try:
        requests.get(CAD_HTTP + "/", timeout=3)
        cad_ok = True
    except Exception as e:
        return jsonify({"gateway": "ok", "cad": False, "error": str(e)}), 503
    return jsonify({"gateway": "ok", "cad": True, "cad_http": CAD_HTTP})


@app.route("/api/draw", methods=["POST"])
def draw():
    body = request.get_json(force=True, silent=True)
    if not body or not isinstance(body, dict):
        return jsonify({"status": "error", "message": "body must be a JSON object"}), 400

    borehole = body.get("borehole_data")
    if not borehole or not isinstance(borehole, dict):
        return jsonify({"status": "error",
                        "message": "missing required field: borehole_data"}), 400

    views = body.get("views") or ["plan", "section", "column"]
    unknown = [v for v in views if v not in VIEW_ROUTES]
    if unknown:
        return jsonify({"status": "error",
                        "message": f"unknown views: {unknown}, allowed: {list(VIEW_ROUTES)}"}), 400

    section_params = body.get("section_params")
    auto_zoom = body.get("auto_zoom", True)

    with _lock:
        # 1. 英->中 转换并落盘
        try:
            borehole_cn = _translate_borehole(borehole)
            _write_json(BOREHOLE_FILE, borehole_cn)
            if section_params is not None:
                section_cn = _translate_section(section_params)
                _write_json(SECTION_FILE, section_cn)
        except Exception as e:
            return jsonify({"status": "error", "message": f"failed to write files: {e}"}), 500

        # 2. 依次调插件出图
        results, errors = {}, {}
        for v in views:
            try:
                results[v] = _call_cad(VIEW_ROUTES[v])
            except Exception as e:
                errors[v] = f"{type(e).__name__}: {e}"

        # 3. zoom extents
        if auto_zoom and not errors:
            try:
                _call_cad("/ai/tool/zoom_extents", timeout=30)
            except Exception as e:
                errors["zoom_extents"] = str(e)

    return jsonify({
        "status": "success" if not errors else "partial",
        "results": results,
        "errors": errors,
        "files": {
            "borehole": BOREHOLE_FILE,
            "section": SECTION_FILE if section_params is not None else None,
        },
    }), 200 if not errors else 207


if __name__ == "__main__":
    print(f"[draw_gateway] ARX_DIR = {ARX_DIR}")
    print(f"[draw_gateway] CAD_HTTP = {CAD_HTTP}")
    app.run(host="0.0.0.0", port=7200, debug=False)
