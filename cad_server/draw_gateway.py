# -*- coding: utf-8 -*-
"""
出图网关（三接口 + 底图 + SSRF）P0 修复版

启动：
    .venv\\Scripts\\python.exe draw_gateway.py [--ssrf-whitelist-cidr CIDR1,CIDR2,...]

三接口路由（蓝图 §1.2）：
    POST /api/draw/column   柱状图
    POST /api/draw/section  剖面图
    POST /api/draw/plan     平面图（含底图，二选一 file/url）

旧接口（保留并存，D3）：
    POST /api/draw          一次调用出全部图（status/results/errors/files 形态不变）

P0 关键决策落地：
    D1 方案 A：application/json 请求体中含 base_map / base_map_url 键 → 400
    D2 仅同名覆盖：support/底图/ 异名遗留由部署侧清理（详见详设 V1.6 §11.5）
    D3 旧接口保留：状态码 200 + 旧响应体不变
    D7 ARX_DIR 默认值改实仓：D:\\project\\ies\\cad\\exploration\\Debug
    D8 --ssrf-whitelist-cidr 启动参数：见 __main__
"""
import os
import sys
import json
import argparse
import threading
import ipaddress
import socket
import urllib.request
import urllib.parse
import urllib.error
import requests
from flask import Flask, request, jsonify


# ============================================================
# 配置
# ============================================================

# ARX 端（CAD + 插件）落盘根目录（D7：默认实仓）
ARX_DIR = os.environ.get(
    "ARX_DIR",
    r"D:\project\ies\cad\exploration\Debug",
)

CAD_HTTP = os.environ.get("CAD_HTTP", "http://localhost:7100")

# 三图落盘标准位置（详设 V1.6 §9.1）
PROJECT_DIR = os.path.join(ARX_DIR, "project")
BOREHOLE_FILE = os.path.join(PROJECT_DIR, "钻探编录表.json")
SECTION_FILE = os.path.join(PROJECT_DIR, "剖面图参数.json")
PLAN_BASE_FILE = os.path.join(PROJECT_DIR, "plan_base.json")
BASE_MAP_DIR = os.path.join(ARX_DIR, "support", "底图")

# 视图路由
VIEW_ROUTES = {
    "plan":    "/ai/feature/create_plan_view",
    "section": "/ai/feature/create_section_view",
    "column":  "/ai/feature/create_column_view",
}

# 超时（秒）
URL_TIMEOUT     = 30      # 底图 URL 下载
PLAN_TIMEOUT    = 300     # plan 视图 _call_cad
COLUMN_TIMEOUT  = 120     # column 视图 _call_cad
SECTION_TIMEOUT = 180     # section 视图 _call_cad

# 体积上限（字节）
DWG_MAX_BYTES = 50 * 1024 * 1024   # 50MB

# SSRF 配置（默认全放行；仅禁回环；启动参数 --ssrf-whitelist-cidr 可叠加白名单）
SSRF_DENY_LOOPBACK = True
SSRF_WHITELIST_CIDR = []  # list[ipaddress.IPv4Network | IPv6Network]

# Flask 全局请求体上限（multipart 与 JSON body 均受限）
app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = DWG_MAX_BYTES


# ============================================================
# 英中字段映射（对外英文 -> 落盘中文；既有代码原样复用）
# ============================================================
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
SAMPLE_KEY_MAP = {
    "no":    "编号",
    "depth": "取样深度",
}
TEST_KEY_MAP = {
    "type":      "类型",
    "from":      "起(m)",
    "to":        "止(m)",
    "rod_length":"杆长(m)",
    "hits":      "锤击数(击)",
}
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


# ============================================================
# CAD 主线程单线程串行锁
# ============================================================
_lock = threading.Lock()


# ============================================================
# 文件 / CAD 调用
# ============================================================
def _write_json(path, obj):
    """UTF-8 无 BOM 写 JSON，C++ 端 LoadJsonFile 按 UTF-8 读。"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)


def _remove_if_exists(path):
    """删除文件（若存在）；不存在不报错。用于 plan_base 流程一路径清旧配置。"""
    if os.path.exists(path):
        try:
            os.remove(path)
        except OSError:
            pass


def _call_cad(path, timeout=120):
    r = requests.post(CAD_HTTP + path, json={}, timeout=timeout)
    r.raise_for_status()
    return r.json()


# ============================================================
# 参数解析（V1.6 P0-1 修复）
# ============================================================
def _resolve_form_json(field_name, required=False):
    """共用参数解析（G1 验收条款）：
       1) 优先从 JSON body 取（application/json 提交时）
       2) 否则从 form 字段取字符串，json.loads 解析（multipart 提交时）
       3) 非法 JSON 抛 ValueError → 外层捕获回 400；required=True 且全无值抛 ValueError。

       返回值可能是 dict/list/str/原值；None 表示字段不存在（required=False 时）。
    """
    # 1) JSON body
    try:
        body = request.get_json(silent=True) or {}
    except Exception:
        body = {}
    if isinstance(body, dict) and field_name in body and body[field_name] is not None:
        v = body[field_name]
        if isinstance(v, (dict, list)):
            return v
        if isinstance(v, str):
            try:
                return json.loads(v)
            except Exception:
                raise ValueError(f"{field_name} invalid JSON in body")
        return v
    # 2) form 字段（multipart）
    raw = request.form.get(field_name)
    if raw:
        try:
            return json.loads(raw)
        except Exception:
            raise ValueError(f"{field_name} invalid JSON in form")
    # 3) required 检查
    if required:
        raise ValueError(f"missing required field: {field_name}")
    return None


def _resolve_auto_zoom():
    """auto_zoom 解析：form 字段优先 → JSON body 兜底 → 默认 True（bool，无需 json.loads）。

    接受 '1'/'0'/'true'/'false'/'yes'/'no'/'on'/'off'（大小写不敏感）的字符串，
    其他非空字符串视为 True，空字符串视为 None（走 JSON body 兜底）。
    """
    raw_form = request.form.get("auto_zoom")
    if raw_form is not None and raw_form != "":
        if isinstance(raw_form, bool):
            return raw_form
        s = str(raw_form).strip().lower()
        if s in ("0", "false", "no", "off"):
            return False
        return s in ("1", "true", "yes", "on")
    try:
        body = request.get_json(silent=True) or {}
    except Exception:
        body = {}
    if not isinstance(body, dict):
        return True
    if "auto_zoom" in body and body["auto_zoom"] is not None:
        v = body["auto_zoom"]
        if isinstance(v, bool):
            return v
        if isinstance(v, (int, float)):
            return bool(v)
        if isinstance(v, str):
            s = v.strip().lower()
            if s in ("0", "false", "no", "off"):
                return False
            return s in ("1", "true", "yes", "on")
        return bool(v)
    return True


# ============================================================
# SSRF 三函数（G2：ssrf_safe / ssrf_whitelist_check / resolve_redirects）
# ============================================================
def _is_loopback(host):
    """回环地址解析：hostname → 所有解析到的 IP，任一为回环即 True。

    涵盖 IPv4 127.0.0.0/8 与 IPv6 ::1。
    """
    if not host:
        return False
    try:
        infos = socket.getaddrinfo(host, None)
    except Exception:
        return False
    for fam, *_, sockaddr in infos:
        ip_str = sockaddr[0]
        try:
            ip_obj = ipaddress.ip_address(ip_str)
        except ValueError:
            continue
        if ip_obj.is_loopback:
            return True
    return False


def _ssrf_whitelist_check(ip):
    """与 --ssrf-whitelist-cidr 比对：任一 CIDR 命中即 True。

    用于在 _ssrf_check 内部判定"白名单命中→豁免回环黑名单"。
    """
    if not SSRF_WHITELIST_CIDR:
        return False
    try:
        ip_obj = ipaddress.ip_address(ip)
    except ValueError:
        return False
    for net in SSRF_WHITELIST_CIDR:
        try:
            if ip_obj in net:
                return True
        except (TypeError, ValueError):
            continue
    return False


def _ssrf_check(host):
    """SSRF 校验：
       - 仅允许 hostname 非空；
       - 白名单生效时（SSRF_WHITELIST_CIDR 非空）：解析所有 IP，任一命中白名单即 True；否则 False；
       - 否则：禁回环（is_loopback）即 False；
       - 默认：True（私网/公网放行）。
    """
    if not host:
        return False

    # 解析所有 IP
    try:
        infos = socket.getaddrinfo(host, None)
    except Exception:
        return False
    ips = []
    for fam, *_, sockaddr in infos:
        try:
            ipaddress.ip_address(sockaddr[0])
            ips.append(sockaddr[0])
        except ValueError:
            continue
    if not ips:
        return False

    # 1) 白名单生效：仅放行白名单
    if SSRF_WHITELIST_CIDR:
        for ip in ips:
            if _ssrf_whitelist_check(ip):
                return True
        return False

    # 2) 默认放行；但禁回环
    if SSRF_DENY_LOOPBACK:
        for ip in ips:
            try:
                if ipaddress.ip_address(ip).is_loopback:
                    return False
            except ValueError:
                continue
    return True


def _http_get_with_redirects(url, timeout):
    """手动重定向跟随（最多 3 次），每跳独立 SSRF 校验 + 累计字节限流 50MB。

    抛 ValueError：
       - "only http(s) allowed"        协议非 http(s)
       - "ssrf denied host: <host>"    SSRF 校验失败
       - "redirect missing Location"   重定向无 Location 头
       - "base_map too large (>50MB)"  累计字节超限
       - "too many redirects (>3)"     重定向次数超限
    其余异常（socket.timeout / DNS / connection refused 等）让上层捕获作为"URL 下载失败"。
    """
    LIMIT_REDIRECTS = 3
    cur = url
    for _ in range(LIMIT_REDIRECTS + 1):
        u = urllib.parse.urlparse(cur)
        if u.scheme not in ("http", "https"):
            raise ValueError("only http(s) allowed")
        if not _ssrf_check(u.hostname):
            raise ValueError(f"ssrf denied host: {u.hostname}")

        # 禁用自动重定向（自定义跟随逻辑）
        class _NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self, *a, **kw):  # noqa: N802
                return None

        opener = urllib.request.build_opener(_NoRedirect())
        req = urllib.request.Request(cur)
        with opener.open(req, timeout=timeout) as r:
            status = r.status
            if status in (301, 302, 303, 307, 308):
                nxt = r.headers.get("Location", "")
                if not nxt:
                    raise ValueError("redirect missing Location")
                cur = urllib.parse.urljoin(cur, nxt)
                continue
            # 非重定向：流式读取，限流 50MB
            data = bytearray()
            while len(data) <= DWG_MAX_BYTES:
                chunk = r.read(64 * 1024)
                if not chunk:
                    return bytes(data)
                data.extend(chunk)
            raise ValueError("base_map too large (>50MB)")
    raise ValueError("too many redirects (>3)")


# ============================================================
# 底图获取（G2：移出 _lock）
# ============================================================
def _save_base_map():
    """返回 (rel_path | None, warnings: list[str]) 二元组。

    - rel_path  底图相对路径（相对 ARX_DIR，正斜杠）；未提供/不可用 → None
    - warnings  告警列表；任何底图不可用都会进 warnings（告警必达，禁止仅日志/静默）

    错误分类（蓝图 §1.2.5 / 详设 §9.2）：
      a) 调用方式错误（400，warnings 不写）：同传 base_map + base_map_url、扩展名非 .dwg
      b) 底图不可用（200，warnings 必达）：
         - 文件上传 >50MB / 空文件 / IO 异常 / 落盘失败
         - URL 下载失败 / 超时 / DNS 失败 / 重定向过多 / SSRF 拒回环 / 响应为空 / 落盘失败
      c) 未提供（None + 无 warnings，走流程一）

    SSRF 命中（攻击行为）警告文案**不外明拒因**，统一 "base_map_url 校验未通过（详见网关日志）"。
    """
    f = request.files.get("base_map")
    url = (request.form.get("base_map_url") or "").strip()

    # 互斥同传 → 400
    if f and f.filename and url:
        raise ValueError("base_map 与 base_map_url 互斥，二选一")

    # 文件上传
    if f and f.filename:
        safe = os.path.basename(f.filename)
        if not safe.lower().endswith(".dwg"):
            raise ValueError("only .dwg allowed")
        try:
            data = f.read(DWG_MAX_BYTES + 1)
        except Exception as e:
            return None, [f"底图未被采用：文件上传读取失败（{type(e).__name__}：{e}）"]
        if len(data) > DWG_MAX_BYTES:
            return None, [f"底图未被采用：base_map 超过 50MB 上限（实际 {len(data)} 字节）"]
        if len(data) == 0:
            return None, ["底图未被采用：base_map 为空文件"]
        os.makedirs(BASE_MAP_DIR, exist_ok=True)
        dst = os.path.join(BASE_MAP_DIR, safe)  # 同名即覆盖（D2）
        try:
            with open(dst, "wb") as w:
                w.write(data)
        except OSError as e:
            return None, [f"底图未被采用：落盘失败（{e}）"]
        return f"support/底图/{safe}", []

    # URL 下载
    if url:
        name = os.path.basename(urllib.parse.urlparse(url).path) or "base_map.dwg"
        if not name.lower().endswith(".dwg"):
            raise ValueError("base_map_url must point to .dwg")
        try:
            data = _http_get_with_redirects(url, URL_TIMEOUT)
        except ValueError as e:
            msg = str(e)
            if "ssrf denied" in msg or "only http(s)" in msg:
                # 攻击行为：不写明告警文案（避免给攻击者反馈）
                return None, ["底图未被采用：base_map_url 校验未通过（详见网关日志）"]
            return None, [f"底图未被采用：{msg}"]
        except (urllib.error.URLError, socket.timeout, TimeoutError) as e:
            return None, [f"底图未被采用：URL 下载失败（{type(e).__name__}：{e}）"]
        except Exception as e:
            return None, [f"底图未被采用：URL 下载失败（{type(e).__name__}：{e}）"]
        if not data:
            return None, ["底图未被采用：base_map_url 响应为空"]
        os.makedirs(BASE_MAP_DIR, exist_ok=True)
        dst = os.path.join(BASE_MAP_DIR, name)
        try:
            with open(dst, "wb") as w:
                w.write(data)
        except OSError as e:
            return None, [f"底图未被采用：URL 内容落盘失败（{e}）"]
        return f"support/底图/{name}", []

    # 未提供 → 流程一
    return None, []


# ============================================================
# 路由
# ============================================================
@app.route("/health", methods=["GET"])
def health():
    try:
        requests.get(CAD_HTTP + "/", timeout=3)
        cad_ok = True
    except Exception as e:
        return jsonify({"gateway": "ok", "cad": False, "error": str(e)}), 503
    return jsonify({"gateway": "ok", "cad": True, "cad_http": CAD_HTTP})


@app.route("/api/draw/column", methods=["POST"])
def draw_column():
    """柱状图：JSON 或 multipart 双通道。

    请求：borehole_data（必填）；auto_zoom（可选，默认 True）。
    响应：{status, results: {column: <CAD>}}。
    """
    try:
        bh = _resolve_form_json("borehole_data", required=True)
    except ValueError as e:
        return jsonify({"status": "error", "message": str(e)}), 400
    if not isinstance(bh, dict):
        return jsonify({"status": "error",
                        "message": "borehole_data must be a JSON object"}), 400
    az = _resolve_auto_zoom()

    with _lock:
        try:
            _write_json(BOREHOLE_FILE, _translate_borehole(bh))
            result = _call_cad(VIEW_ROUTES["column"], timeout=COLUMN_TIMEOUT)
        except requests.exceptions.Timeout:
            return jsonify({"status": "error",
                            "message": f"column view timed out ({COLUMN_TIMEOUT}s)"}), 504
        except Exception as e:
            return jsonify({"status": "error",
                            "message": f"{type(e).__name__}: {e}"}), 500

    if az:
        try:
            _call_cad("/ai/tool/zoom_extents", timeout=30)
        except Exception:
            pass
    return jsonify({"status": "success", "results": {"column": result}}), 200


@app.route("/api/draw/section", methods=["POST"])
def draw_section():
    """剖面图：JSON 或 multipart 双通道。

    请求：borehole_data（必填）；section_params（可选）；auto_zoom（可选，默认 True）。
    响应：{status, results: {section: <CAD>}}。
    """
    try:
        bh = _resolve_form_json("borehole_data", required=True)
        sp = _resolve_form_json("section_params", required=False)
    except ValueError as e:
        return jsonify({"status": "error", "message": str(e)}), 400
    if not isinstance(bh, dict):
        return jsonify({"status": "error",
                        "message": "borehole_data must be a JSON object"}), 400
    az = _resolve_auto_zoom()

    with _lock:
        try:
            _write_json(BOREHOLE_FILE, _translate_borehole(bh))
            if sp is not None:
                _write_json(SECTION_FILE, _translate_section(sp))
            result = _call_cad(VIEW_ROUTES["section"], timeout=SECTION_TIMEOUT)
        except requests.exceptions.Timeout:
            return jsonify({"status": "error",
                            "message": f"section view timed out ({SECTION_TIMEOUT}s)"}), 504
        except Exception as e:
            return jsonify({"status": "error",
                            "message": f"{type(e).__name__}: {e}"}), 500

    if az:
        try:
            _call_cad("/ai/tool/zoom_extents", timeout=30)
        except Exception:
            pass
    return jsonify({"status": "success", "results": {"section": result}}), 200


@app.route("/api/draw/plan", methods=["POST"])
def draw_plan():
    """平面图：JSON 或 multipart 双通道；底图字段仅 multipart（D1 方案 A）。

    请求（详设 V1.6 §9.3 / 蓝图 §1.2.2）：
      必填：borehole_data
      可选：base_map(file) ∥ base_map_url(string) 二选一；plan_base(object)；auto_zoom(bool，默认 True)

    响应（详设 V1.6 同日小修订·告警必达；永远包含四字段）：
      {
        "status": "success" | "error",
        "results": {"plan": <CAD>} | null,
        "warnings": [<告警文案>...],
        "degraded_to": null | "process1_no_basemap" | "process2_with_basemap"
      }

    语义：
      - degraded_to 是"本次实际流程标识"，与"降级"无关
      - 底图生效 → "process2_with_basemap"
      - 调用方未给底图 → "process1_no_basemap"（流程一正常路径）
      - 给了但不可用 → "process1_no_basemap"（告警必达路径，仍按流程一继续出图）
      - 底图不可用**绝不**返 5xx；超时（300s）才返 504
    """
    # ===== D1 方案 A：JSON body 含底图键即 400（蓝图 §3.4） =====
    if (request.mimetype or "").startswith("application/json"):
        try:
            body_for_check = request.get_json(silent=True) or {}
        except Exception:
            body_for_check = {}
        if isinstance(body_for_check, dict) and (
            "base_map_url" in body_for_check or "base_map" in body_for_check
        ):
            return jsonify({
                "status": "error",
                "message": "base_map / base_map_url 仅支持 multipart/form-data 提交，"
                           "请在 JSON body 中移除该字段并改用 multipart"
            }), 400

    # ===== 必填字段解析（锁外尽早校验） =====
    try:
        bh = _resolve_form_json("borehole_data", required=True)
        pb = _resolve_form_json("plan_base", required=False)
    except ValueError as e:
        return jsonify({"status": "error", "message": str(e)}), 400
    if not isinstance(bh, dict):
        return jsonify({"status": "error",
                        "message": "borehole_data must be a JSON object"}), 400

    # ===== 底图获取（锁外，告警必达） =====
    warnings = []
    degraded_to = None
    try:
        dwg, dwg_warnings = _save_base_map()
    except ValueError as e:
        # 调用方式错误（同传 / 扩展名）→ 400，不写 warnings
        return jsonify({"status": "error", "message": str(e)}), 400
    if dwg_warnings:
        warnings.extend(dwg_warnings)

    # ===== degraded_to 判定（蓝图 §1.2.3 / 详设 §9.3） =====
    base_map_provided = bool(
        request.files.get("base_map")
        or (request.form.get("base_map_url") or "").strip()
    )
    if dwg is not None:
        degraded_to = "process2_with_basemap"
    else:
        # 未给底图 或 给了但不可用 → 统一走流程一
        degraded_to = "process1_no_basemap"

    az = _resolve_auto_zoom()

    # ===== 锁内：落盘 + 调 CAD（plan 视图 300s） =====
    with _lock:
        try:
            _write_json(BOREHOLE_FILE, _translate_borehole(bh))
            if pb is not None:
                cfg = dict(pb)
                if dwg:
                    cfg.setdefault("base_map", {})["dwg"] = dwg
                _write_json(PLAN_BASE_FILE, cfg)
            elif dwg:
                _write_json(PLAN_BASE_FILE, {"base_map": {"dwg": dwg}})
            else:
                _remove_if_exists(PLAN_BASE_FILE)

            result = _call_cad(VIEW_ROUTES["plan"], timeout=PLAN_TIMEOUT)
        except requests.exceptions.Timeout:
            return jsonify({
                "status": "error",
                "message": f"plan view timed out ({PLAN_TIMEOUT}s)",
                "warnings": warnings,
                "degraded_to": degraded_to,
            }), 504
        except Exception as e:
            return jsonify({
                "status": "error",
                "message": f"{type(e).__name__}: {e}",
                "warnings": warnings,
                "degraded_to": degraded_to,
            }), 500

    # ===== auto_zoom 锁外 =====
    if az:
        try:
            _call_cad("/ai/tool/zoom_extents", timeout=30)
        except Exception:
            pass

    # ===== 永远返回四字段（蓝图 §1.2.3） =====
    return jsonify({
        "status": "success",
        "results": {"plan": result},
        "warnings": warnings,
        "degraded_to": degraded_to,
    }), 200


# 旧接口（D3：保留并存，状态码 200 + 旧响应体不变）
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


# ============================================================
# 启动（D8：--ssrf-whitelist-cidr）
# ============================================================
def _parse_args():
    parser = argparse.ArgumentParser(
        description="CAD 出图网关（三接口 + 底图 + SSRF，P0 修复版）",
    )
    parser.add_argument(
        "--ssrf-whitelist-cidr", type=str, default="",
        help=("SSRF 白名单 CIDR（逗号分隔），传入后仅放行白名单内网地址。"
              "默认全放行（仅禁回环）。例如：--ssrf-whitelist-cidr 10.0.0.0/8,192.168.0.0/16"),
    )
    parser.add_argument(
        "--port", type=int, default=7200,
        help="监听端口（默认 7200）",
    )
    parser.add_argument(
        "--host", type=str, default="0.0.0.0",
        help="监听地址（默认 0.0.0.0）",
    )
    return parser.parse_known_args()[0]


if __name__ == "__main__":
    args = _parse_args()
    if args.ssrf_whitelist_cidr:
        for cidr in args.ssrf_whitelist_cidr.split(","):
            cidr = cidr.strip()
            if not cidr:
                continue
            try:
                SSRF_WHITELIST_CIDR.append(ipaddress.ip_network(cidr, strict=False))
            except ValueError as e:
                print(f"[draw_gateway] 无效 CIDR '{cidr}': {e}", file=sys.stderr)
                sys.exit(2)

    print(f"[draw_gateway] ARX_DIR = {ARX_DIR}")
    print(f"[draw_gateway] CAD_HTTP = {CAD_HTTP}")
    print(f"[draw_gateway] MAX_CONTENT_LENGTH = {DWG_MAX_BYTES} bytes (50MB)")
    print(f"[draw_gateway] PLAN_TIMEOUT = {PLAN_TIMEOUT}s / COLUMN_TIMEOUT = {COLUMN_TIMEOUT}s / SECTION_TIMEOUT = {SECTION_TIMEOUT}s / URL_TIMEOUT = {URL_TIMEOUT}s")
    print(f"[draw_gateway] SSRF_WHITELIST_CIDR = {[str(n) for n in SSRF_WHITELIST_CIDR] or '(empty = 默认全放行)'}")
    print(f"[draw_gateway] BASE_MAP_DIR = {BASE_MAP_DIR}")
    app.run(host=args.host, port=args.port, debug=False)
