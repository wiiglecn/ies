# CAD 勘察出图插件 接口文档

| 项目 | 内容 |
| --- | --- |
| 文档名称 | CAD 勘察出图插件（KsExploration.arx）对外接口文档 |
| 版本 | **V1.2** |
| 日期 | 2026-10-15 |
| 适用 | AutoCAD 2005 / 2007 + 勘察三图（平面图 / 柱状图 / 剖面图）出图 |
| 基线 | 《勘探点平面布置图·底图导入与出图比例 详细设计》**V1.6**；《CAD 勘察出图插件·勘察三图生成 需求规格说明书》**V1.3**；《CAD 勘察出图插件·勘察三图生成 需求文档》**V2.4** |
| 编写目的 | 作为业务系统/调用方对接出图能力的统一接口文档；**同时覆盖当前已实现形态与目标待实现形态**，对每条接口明确标注实现状态（✅ 已实现 / 🔜 待实现） |

> **形态说明（请先看）**
> - **形态 A（现状已实现）**：ARX 插件内嵌 HTTP 服务监听 `:7100`，业务系统直连，**单接口** `POST /api/draw`，一次调用可画多图。
> - **形态 B（目标待实现）**：独立 Flask 网关 `cad_server/draw_gateway.py` 提供**三个独立接口** `POST /api/draw/column`、`/api/draw/section`、`/api/draw/plan`；平面图接口支持**底图文件上传**与**底图可访问链接**两种提交方式，由网关端下载底图并落盘后转交 CAD 出图。
>
> 两种形态**不并存**：部署侧按需选其一；本版按主理人拍板，文档同步描述二者，由调用方按本节"接口清单总表"对号入座。

---

## 目录

1. [架构总览](#1-架构总览)
2. [通用约定](#2-通用约定)
3. [接口清单总表](#3-接口清单总表)
4. [`POST /api/draw`（形态 A · ✅ 已实现）](#4-post-apidraw形态-a---已实现)
5. [`POST /api/draw/column`（形态 B · 🔜 待实现）](#5-post-apidrawcolumn形态-b--待实现)
6. [`POST /api/draw/section`（形态 B · 🔜 待实现）](#6-post-apidrawsection形态-b--待实现)
7. [`POST /api/draw/plan`（形态 B · 🔜 待实现）](#7-post-apidrawplan形态-b--待实现)
8. [状态码与错误处理](#8-状态码与错误处理)
9. [`borehole_data` 数据格式（形态 B 二维数组 · 推荐；形态 A 结构化 · 遗留）](#9-borehole_data-数据格式形态-b-二维数组--推荐形态-a-结构化--遗留)
10. [完整 curl 调用示例](#10-完整-curl-调用示例)
11. [错误排查](#11-错误排查)
12. [变更记录](#12-变更记录)

---

## 1. 架构总览

### 1.1 形态 A · 现状（已实现）

```
业务系统 / 客户集成
   │  HTTP，application/json (UTF-8, 英文 snake_case key)
   ▼
┌──────────────────────────────────────────┐
│  AutoCAD 2007 + KsExploration.arx        │
│  内嵌 HTTP 服务监听 :7100                 │
│  收英文 JSON → 自动转中文键 → 落盘         │
│  → 调画出图（柱状图每 10 孔 yield）        │
└──────────────────────────────────────────┘
```

- **无独立网关**；业务系统、CAD、插件在同一台机器，直接打 `localhost`。
- 客户端只传英文 key，C++ 端落盘时自动转中文 key 供内部绘制逻辑使用（`cad_server/API.md` v2.1）。
- 单接口 `POST /api/draw`，一次调用可同时画平面图/柱状图/剖面图（`views` 数组）。

### 1.2 形态 B · 目标（待实现）

```
业务系统 / 客户集成
   │  HTTP，application/json 或 multipart/form-data
   ▼
┌──────────────────────────────────────────┐
│  Flask 网关 cad_server/draw_gateway.py   │
│  · 参数解析（JSON body 优先 / form 兜底） │
│  · borehole_data 二维数组→英文键（_normalize_borehole）│
│  · borehole_data 英文键→中文键（_translate_borehole）│
│  · 底图下载/落盘（仅网关端 · 不阻塞 CAD）│
│  · 单项目/单 CAD 实例，_lock 串行         │
└──────────────────┬───────────────────────┘
                   │  HTTP（现状 arx 内嵌 :7100 / 后续 plan 视图 300s）
                   ▼
┌──────────────────────────────────────────┐
│  AutoCAD 2005 / 2007 + KsExploration.arx  │
│  读 project/*.json 与 support/底图/*.dwg  │
│  → 出图（平面/柱状/剖面）                 │
└──────────────────────────────────────────┘
```

- **三接口独立**：柱状图、剖面图、平面图各自一个端点，按需调用。
- **`borehole_data` 入参为二维数组 JSON**（顶层 key=孔号，value=xlsx 行数组）；网关内 `_normalize_borehole()` 自动归一化为英文键结构化 JSON，再走 `_translate_borehole()` 英译中键落盘。调用方**无需**自行结构化转换。详见 §9。
- **平面图底图支持**：DWG 文件上传（`multipart/form-data` 字段 `base_map`）或可访问链接（`base_map_url`，http/https）二选一，**均由网关端下载并落盘到 `ARX_DIR/support/底图/`**，插件端**不联网**。
- **告警必达**：底图不可用**绝不**返回 5xx，统一 `HTTP 200 + warnings[] + degraded_to`，由 CAD 端显式告知用户（避免静默）。
- **SSRF 启动参数**：网关支持 `--ssrf-whitelist-cidr <CIDR列表>` 启动参数（默认空 = 全放行；非空时仅放行白名单内；回环地址一律拒），由部署层按需配置。

> 实现状态：在 `cad_server/draw_gateway.py` 当前代码（`draw_gateway.py:184`）仍为单接口 `/api/draw`；形态 B 三接口为详设 V1.7 §9.3 定稿，**本版实施中**（A1~A7/G1~G5 任务派单已发，详见 `评审-P0 改造蓝图-2026-10-14.md`）。

---

## 2. 通用约定

### 2.1 字符与编码

| 项 | 约定 |
| --- | --- |
| 字符编码 | 全部 UTF-8，JSON 无 BOM |
| 字段命名 | **英文 `snake_case`**（如 `borehole_data`、`hole_no`、`water_level_initial`） |
| 中文落盘 | C++/ARX 端落盘时自动将英文 key 转换为中文 key；客户端无需关心 |

### 2.2 提交方式（两种 Content-Type，形态 B 适用）

形态 B 三接口支持**两种 Content-Type**，由网关共用解析函数 `_resolve_form_json()` 统一处理：

| Content-Type | 适用场景 | 解析规则（`borehole_data` / `plan_base` / `section_params`） | `auto_zoom` |
| --- | --- | --- | --- |
| `application/json; charset=utf-8` | 纯数据提交（无底图文件） | **优先**从 JSON body 取该字段 | 从 JSON body 取 |
| `multipart/form-data` | 需上传底图文件 / 客户端封装更方便 | body 取不到则从对应 `form` 字段读字符串并 `json.loads` 解析 | 从 `request.form.get("auto_zoom")` 取 |

> **非法 JSON 报错**：解析失败抛 `ValueError` → 网关统一返回 `HTTP 400 + {"status":"error","message":...}`。
> **必填校验**：形态 B 中 `borehole_data` 为必填，缺失或非法 JSON 一律 400。

### 2.3 无状态与部署假设

- **无状态**：系统不保存项目目录/项目编码/镜像；每次调用独立落盘标准位置（`ARX_DIR/project/*.json`），后一次调用**覆盖**前一次。
- **单项目 / 单 CAD 实例**：按详设 FR-6.5，本版假设同一时刻仅服务一个项目、一个 CAD 实例；多项目并行须由**部署层**保证串行或按项目分目录，**本版不承诺多项目数据隔离**。
- **并发**：网关侧 `_lock` 串行；并发调用**排队**，按到达顺序执行（与《需规》§6 验收 15 一致）。

### 2.4 超时建议

| 视图 | 形态 A 实测耗时参考 | 形态 B 网关 `_call_cad` 超时 | 客户端建议 |
| --- | --- | --- | --- |
| 柱状图 | 352 孔 ≈ 25.3s | 120s（`COLUMN_TIMEOUT`） | ≥ 120s |
| 剖面图 | — | 120s（`COLUMN_TIMEOUT` 沿用） | ≥ 120s |
| 平面图（含底图导入 + 对齐 + 出图比例） | — | **300s**（`PLAN_TIMEOUT`，详设 V1.6 §9.1） | ≥ 300s |

### 2.5 体积上限

| 项 | 上限 | 触发 |
| --- | --- | --- |
| 单次请求体（Flask `MAX_CONTENT_LENGTH`） | **50 MB** | multipart/form-data 与 JSON body 均受限 |
| 底图 DWG（`DWG_MAX_BYTES`） | **50 MB** | 超限按"底图不可用"走 200 + warning，不返 5xx |

### 2.6 流程一 / 流程二（平面图特有，并列主流程）

> 与《需规》V1.3 §2.1、§3.5 R22、§6 验收 13-14 对齐。本节口径**不可替换为"降级/兜底"**。

| 流程 | 何时走 | 调用方提交 | 插件侧动作 |
| --- | --- | --- | --- |
| **流程一（委托方没有底图）** | 委托方无现成底图 / 流程二调用中底图不可用 | `borehole_data`（必填）+ `plan_base`（可省略） | **跳过底图导入 + 底图对齐两步**；自动布孔 + 标注 + 图例；`plot_scale` 配置则缩放"钻孔落点 + 图例"两层 |
| **流程二（委托方有底图）** | 委托方有现成底图（文件或链接） | `borehole_data` + `base_map` 或 `base_map_url`（二选一）+ `plan_base`（可选） | 底图导入 + 对齐 + 出图比例 + 布孔 + 图例；`plot_scale` 配置则缩放"底图几何 + 钻孔 + 图例"三层 |

- **两条流程地位对等**——流程一不是流程二的"降级/兜底/次选/退化路径"。
- **告警必达**：在流程二调用中，若调用方**尝试**给底图但**不可用**（文件不存在 / >50MB / 下载失败 / 超时 / SSRF 拒回环 / 落盘失败），**绝不**返 5xx，统一返回 `HTTP 200 + warnings[]`（文案"底图未被采用：xxx"）+ `degraded_to="process1_no_basemap"`（标识按流程一继续出图，与"降级"无关）；CAD 端**必须**用对话框/命令行**显式告知用户**（禁止仅打印到日志窗口）。
- **只有调用方式错误**（`base_map` 与 `base_map_url` 同传 / 扩展名非 `.dwg` / 必填缺失或非法 JSON）才返回 400。

---

## 3. 接口清单总表

| # | 接口 | 形态 | 状态 | Content-Type | 必填字段 | 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `POST /api/draw` | A | **✅ 已实现**（`cad_server/API.md` v2.2） | `application/json` | `borehole_data`（**结构化 dict**，形态 A 遗留） | 单接口一次调用多图；视图由 `views` 数组选择 |
| 2 | `POST /api/draw/column` | B | 🛠 **实施中** | `application/json` / `multipart/form-data` | `borehole_data`（**二维数组 JSON**，网关自动归一化） | 柱状图端点；本版实施中（详设 V1.7 §9.3，代码任务 G1/G3 派单已发） |
| 3 | `POST /api/draw/section` | B | 🛠 **实施中** | `application/json` / `multipart/form-data` | `borehole_data`（**二维数组 JSON**，网关自动归一化） | 剖面图端点；本版实施中 |
| 4 | `POST /api/draw/plan` | B | 🛠 **实施中**（**重点**） | `application/json` / `multipart/form-data` | `borehole_data`（**二维数组 JSON**，网关自动归一化）；`base_map` 或 `base_map_url` 二选一（流程二；底图字段**仅**支持 `multipart/form-data`，详见 §7.2） | 平面图端点；支持底图两种提交方式；详情见第 7 章 |

> **注**：第 2~4 行三接口的"🛠 实施中"状态与 **P0-1**（`ImportDwgAsBlock` 6 参重载用语核验）、**P0-3**（`base_map_url` JSON body 静默忽略——已由 §7.2 D1 决策落地）关联——网关落地时同步消除。
> 形态 A 现状实现位于 ARX 内嵌 HTTP 服务；形态 B 现状代码仍为单接口 `/api/draw`，三接口为详设 V1.7 定稿，**改造在 `cad_server/draw_gateway.py` 落地**。

---

## 4. `POST /api/draw`（形态 A · ✅ 已实现）

### 4.1 端点

```
POST http://<host>:7100/api/draw
Content-Type: application/json; charset=utf-8
```

### 4.2 前置条件

1. AutoCAD 2007 已启动；
2. 命令行加载插件：
   ```
   (arxload "D:/project/ies/cad/exploration/Debug/KsExploration.arx")
   ```
3. 命令行启动 HTTP 服务：
   ```
   SLINIT
   ```
   看到 `[CAD DEBUG] Listening started` 即可。

### 4.3 请求字段

| 字段 | 类型 | 必填 | 默认 | 说明 |
| --- | --- | --- | --- | --- |
| `views` | string[] | 否 | `["plan"]` | 要画哪几种：`plan`=平面图，`column`=柱状图，`section`=剖面图 |
| `borehole_data` | object | **是** | — | 钻孔数据，结构见第 9 章。顶层 key = 孔号（如 `"ZK1"`） |
| `section_params` | object | 否 | — | 剖面图参数，仅 `views` 含 `section` 时需要 |

### 4.4 响应

```json
{
  "status": "success",
  "results": {
    "plan":    { "status": "success" },
    "column":  { "status": "success" },
    "section": { "status": "success" }
  },
  "errors": {}
}
```

### 4.5 性能参考（实测，形态 A）

| 孔数 | views | 耗时 |
| --- | --- | --- |
| 5 孔 | `["plan"]` | ≈ 3s |
| 352 孔 | `["plan","column"]` | **25.3s** |

> 客户端超时建议 ≥ 300s；插件内部每 10 个柱状图 yield 一次消息泵，352 孔实测不崩。

### 4.6 完整请求示例

```bash
curl -X POST http://localhost:7100/api/draw \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "views": ["plan", "column"],
    "borehole_data": {
      "ZK1": {
        "hole_no": "ZK1",
        "project_name": "宜州城区地下管网改造提升项目",
        "company": "广西昇龙工程勘察设计检测有限公司",
        "site": "广西壮族自治区河池市宜州市",
        "elevation": 138.27,
        "depth": 7.7,
        "hole_type": "技术孔",
        "x": 2711077.24,
        "y": 562014.73,
        "date_start": "2025年12月10日",
        "date_end": "2025年12月10日",
        "page_current": 1,
        "page_total": 1,
        "water_level_initial": "",
        "water_level_stable": "",
        "layers": [
          {
            "layer_id": "1-0-0",
            "from": 0, "to": 1.0,
            "name": "填土-素填土",
            "color": "灰黑色",
            "density": "松散",
            "description": "第四纪人工填土，主要成份粘性土",
            "sample": {"no": "", "depth": ""},
            "test":   {"type": "标贯", "from": "3.15", "to": "3.25", "rod_length": "5.2", "hits": "3"}
          },
          {
            "layer_id": "2-0-0",
            "from": 1.0, "to": 7.7,
            "name": "黏性土-粉质黏土",
            "color": "黄褐色",
            "state": "可塑",
            "description": "第四纪残积，干强度中等",
            "sample": {"no": "", "depth": ""},
            "test":   {"type": "标贯", "from": "3.25", "to": "3.35", "rod_length": "5.2", "hits": "3"}
          }
        ]
      }
    }
  }'
```

---

## 5. `POST /api/draw/column`（形态 B · 🔜 待实现）

### 5.1 端点

```
POST http://<gateway>/api/draw/column
Content-Type: application/json; charset=utf-8
   或   multipart/form-data
```

### 5.2 请求字段

| 字段 | 类型 | 必填 | 提交位置（JSON body / form 字段） | 默认 | 说明 |
| --- | --- | --- | --- | --- | --- |
| `borehole_data` | object | **是** | JSON body 或 form 字段（`json.loads` 兜底） | — | 钻孔数据，第 9 章 |
| `auto_zoom` | bool | 否 | JSON body 或 form 字段 | `true` | 出图后是否调用 `/ai/tool/zoom_extents` 自动全图缩放 |

### 5.3 响应

```json
{
  "status": "success",
  "results": {"column": {"status": "success"}}
}
```

`column` 字段值由 `_call_cad(VIEW_ROUTES["column"])` 返回体透传。

### 5.4 行为

1. 解析 `borehole_data`（缺失或非法 JSON → 400）；
2. 进入 `_lock`：英文键 → 中文键 → 落盘 `ARX_DIR/project/钻探编录表.json` → 调 CAD `_call_cad(VIEW_ROUTES["column"], timeout=120)`；
3. 若 `auto_zoom=true`，锁外调用 `/ai/tool/zoom_extents`（30s）；
4. 成功返回 `HTTP 200`；CAD 异常透传为 `status` / `errors`。

---

## 6. `POST /api/draw/section`（形态 B · 🔜 待实现）

### 6.1 端点

```
POST http://<gateway>/api/draw/section
Content-Type: application/json; charset=utf-8
   或   multipart/form-data
```

### 6.2 请求字段

| 字段 | 类型 | 必填 | 提交位置（JSON body / form 字段） | 默认 | 说明 |
| --- | --- | --- | --- | --- | --- |
| `borehole_data` | object | **是** | JSON body / form | — | 钻孔数据，第 9 章 |
| `section_params` | object | 否 | JSON body / form | — | 剖面参数（剖面线选孔与布置），缺省按现状 |
| `auto_zoom` | bool | 否 | JSON body / form | `true` | 出图后是否自动全图缩放 |

### 6.3 响应

```json
{
  "status": "success",
  "results": {"section": {"status": "success"}}
}
```

### 6.4 行为

1. 解析 `borehole_data`（必填）和 `section_params`（可选）；缺失或非法 JSON → 400；
2. 进入 `_lock`：英文键 → 中文键 → 落盘 `钻探编录表.json`；若提供 `section_params` 则落盘 `剖面图参数.json` → 调 CAD `_call_cad(VIEW_ROUTES["section"], timeout=120)`；
3. 若 `auto_zoom=true`，锁外调用 `/ai/tool/zoom_extents`（30s）；
4. 成功返回 `HTTP 200`。

---

## 7. `POST /api/draw/plan`（形态 B · 🔜 待实现 · 重点）

> 本接口为详设 V1.6 §9.3 `draw_plan()` 定稿实现；当前代码仍为单接口 `/api/draw`，本接口落地后会替代其平面图能力。

### 7.1 端点

```
POST http://<gateway>/api/draw/plan
Content-Type: multipart/form-data        # 推荐：文件上传场景
   或   application/json; charset=utf-8  # 纯数据提交（流程一）
```

### 7.2 请求字段总览

| 字段 | 类型 | 必填 | 提交位置 | 默认 | 说明 |
| --- | --- | --- | --- | --- | --- |
| `borehole_data` | object | **是** | JSON body / form 字段 | — | 钻孔数据，第 9 章 |
| `base_map` | file (`.dwg`) | **二选一** | **`multipart/form-data` 文件字段** | — | 上传的底图 DWG 文件（流程二） |
| `base_map_url` | string | **二选一** | **仅 form 字段（`multipart/form-data`）** | — | 底图可访问链接，http/https（流程二） |
| `plan_base` | object | 否 | JSON body / form 字段 | — | 平面图配置；含底图对齐参数、出图比例等；详见 §7.5 |
| `auto_zoom` | bool | 否 | JSON body / form 字段 | `true` | 出图后是否自动全图缩放 |

> **`base_map` 与 `base_map_url` 二选一**：同时提交 → `HTTP 400 + {"status":"error","message":"base_map 与 base_map_url 互斥，二选一"}`。
> **底图字段仅支持 `multipart/form-data`**：`base_map`（文件）与 `base_map_url`（string）**只从 `request.form` / `request.files` 读取**（详设 V1.7 §9.2 `_save_base_map()`）；`application/json` 提交时 `request.form` 为空——**若 JSON body 中仍包含 `base_map` / `base_map_url` 键，视为"调用方式错误"，网关返 `HTTP 400 + {"status":"error","message":"base_map / base_map_url 仅支持 multipart/form-data 提交"}`**（与"同传 / 扩展名错误"同类处理，详见 §8.2 第 5 行；本项即 **D1 决策**——`base_map_url requires multipart/form-data`，属"调用方式错误"语义，**不是流程一降级**）。**调用方若需提供底图，必须使用 `multipart/form-data`**。
> **未提供底图与 `plan_base`** → 走流程一，响应 `degraded_to="process1_no_basemap"`（流程一正常路径，与"降级"无关）。
> **`plan_base`** 可与底图同传；亦可不传（流程一不走底图、不走对齐，插件按当前数据出图）。

### 7.3 响应（V1.6 同日小修订 · **告警必达**）

`draw_plan` 响应体**永远包含**以下四个字段（无论调用方式如何、底图是否可用）：

| 字段 | 类型 | 语义 |
| --- | --- | --- |
| `status` | string | `"success"` / `"error"`；底图不可用**不**降为 `"error"` |
| `results` | object | `{"plan": <CAD 返回体>}`（成功时为透传；错误时可能为 `null`） |
| `warnings` | string[] | **告警必达**——文案以"底图未被采用：xxx"开头；空数组表示无告警 |
| `degraded_to` | string \| null | **本次实际走的流程标识**——`null` 或 `"process1_no_basemap"`；详见 §7.4 |

#### 7.3.1 成功响应（流程二 · 底图可用）

```json
{
  "status": "success",
  "results": {"plan": {"status": "success"}},
  "warnings": [],
  "degraded_to": null
}
```

#### 7.3.2 成功响应（流程一 · 调用方未给底图 / 底图不可用）

```json
{
  "status": "success",
  "results": {"plan": {"status": "success"}},
  "warnings": ["底图未被采用：文件上传读取失败（FileNotFoundError: [Errno 2] No such file or directory）"],
  "degraded_to": "process1_no_basemap"
}
```

#### 7.3.3 错误响应（调用方式错误）

```json
{
  "status": "error",
  "message": "base_map 与 base_map_url 互斥，二选一"
}
```

> 调用方式错误（`base_map` + `base_map_url` 同传、扩展名非 `.dwg`、`borehole_data` 缺失或非法 JSON）才返 `HTTP 400`；底图不可用一律走 `HTTP 200` + `warnings[]`，**绝不**返 5xx。

### 7.4 `degraded_to` 语义（与"降级"无关）

| 取值 | 含义 | 何时出现 |
| --- | --- | --- |
| `null` | 未走流程一（即本次出图按流程二路径，底图已生效） | 底图成功落盘且生效 |
| `"process1_no_basemap"` | 本次按流程一继续出图 | 调用方**根本没给底图**，或**给了但不可用**（命中告警必达） |

- `degraded_to` 字段语义为"本次实际走的流程标识"，**与"降级/兜底"无关**——流程一与流程二**地位对等**（详设 V1.7 §1.1、《需规》V1.3 §2.1）。
- 即使调用方未给底图（流程一正常路径），仍返回 `"process1_no_basemap"`，不是错误。

### 7.5 `plan_base` 字段表（详设 V1.6 §2）

`plan_base` 内容（若提供）覆盖 `ARX_DIR/project/plan_base.json`，由 C++ 端 `CreatePlanView` 解析。

#### 7.5.1 整体结构

```json
{
  "base_map": {
    "dwg": "support/底图/05阳光幸福里总平面规划图104(1).dwg",
    "insert_x": 0.0,
    "insert_y": 0.0,
    "scale": 1.0,
    "rotation": 0.0,
    "layer": "底图-现状",
    "dim": false,
    "plot_scale": 500,
    "registration": {
      "source_points": [[2747350.0, 36524890.0], [2747650.0, 36524890.0]],
      "target_points": [[2747450.0, 36524990.0], [2747750.0, 36524990.0]]
    }
  }
}
```

#### 7.5.2 字段规则

| 字段 | 类型 | 必填 | 默认 | 规则 |
| --- | --- | --- | --- | --- |
| `base_map.dwg` | string | 否 | 空 | 相对插件目录（`strFilePath + "/" + dwg`）。空/文件不存在 → 跳过底图导入 |
| `base_map.insert_x` | number | 否 | `0.0` | 底图块插入点 X（米），**坐标对齐**用 |
| `base_map.insert_y` | number | 否 | `0.0` | 底图块插入点 Y（米），**坐标对齐**用 |
| `base_map.scale` | number | 否 | `1.0` | **坐标对齐**用插入比例（同坐标系同单位时为 `1.0`）；**与 `plot_scale` 不同，见 §7.6** |
| `base_map.rotation` | number | 否 | `0.0` | 插入旋转角（弧度） |
| `base_map.layer` | string | 否 | `"底图-现状"` | 底图块参照所在图层 |
| `base_map.dim` | bool | 否 | `false` | `true` 时底图块参照整体设色 253（置灰） |
| `base_map.plot_scale` | number | 否 | 空 | **出图比例分母**（如 `500` 表示 1:500）；配置后对整体成图施加统一缩放；**空/未配置 → 不施加比例缩放** |
| `base_map.registration` | object | 否 | 无 | 两点配准（见 §7.7）；存在时计算对齐参数并**覆盖** `insert_x/y`、`scale`、`rotation` |

#### 7.5.3 `registration.source_points` / `registration.target_points`

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `source_points` | array | 配准时必填 | 底图（DWG 内部坐标）上已知点；**`[x, y]` 数组**格式（如 `[[x1, y1], [x2, y2]]`），1 或 2 个点，单位米 |
| `target_points` | array | 配准时必填 | 上述点在钻孔坐标系下的对应坐标；数组格式同上，数量与 `source_points` 一致，单位米 |

> ⚠️ 与详设 V1.7 §2.1 JSON 定稿对齐（V1.6 P0-2 修复）：`registration` 中点为 **`[x, y]` 数组**，**不是** `{"x":..,"y":..}` 对象。**坐标对齐**（消除底图与钻孔的坐标系/单位差异）相关字段与 §7.6 "对齐 `scale`" 对应。

### 7.6 出图比例 `plot_scale` 语义（与对齐 `scale` 的区别）

| 项 | 对齐 `scale`（`base_map.scale` / `registration` 计算值） | 出图比例（`base_map.plot_scale`） |
| --- | --- | --- |
| 用途 | **坐标对齐**：消除底图与钻孔的坐标系/单位差异 | **成图输出**：设定成图比例（如 1:500） |
| 同坐标系同单位时 | `= 1.0` | 由出图比例决定（`≠1`） |
| 作用对象 | 仅底图（使其内部坐标落到钻孔坐标系） | **整体成图**（底图几何 + 钻孔落点 + 成果要素） |
| 执行顺序 | 第一步 | 第二步，**串联、顺序固定、互不覆盖** |

**串联关系**：

```
原始底图坐标 p
  ──[第一步：坐标对齐 A]──▶ 钻孔坐标 q = A(p) = T + s_align·R(θ_align)·p
  ──[第二步：出图比例 P]──▶ 成图坐标 q' = P(q) = O + k_plot·(q − O)
```

- `s_align`：对齐缩放系数（registration 或显式 `scale`）；
- `k_plot`：出图比例因子，由 `plot_scale` 换算；
- 底图块参照最终 `scale = s_align × k_plot`；**出图比例不覆盖对齐结果**。

> **`plot_scale` 两条流程都适用**（详设 V1.6 §5.4 / §6）：
> - 流程二（有底图）缩放"底图几何 + 钻孔 + 图例"三层；
> - 流程一（无底图）缩放"钻孔落点 + 图例"两层（"底图-现状"层在流程一不存在，循环自然跳过）；
> - 流程一**只跳过"底图导入 + 底图对齐"两步，不跳过出图比例**；仅当未配置 `plot_scale` 时才不缩放。

### 7.7 两点配准（`registration`）模式

| 模式 | source/target 点数 | 计算内容 | 适用场景 |
| --- | --- | --- | --- |
| 两点配准（推荐） | 2 对 | **scale + rotation + insert_x/insert_y 三项全算**（相似变换：均匀缩放、整体旋转、平移） | 底图坐标系与钻孔坐标系存在平移/旋转/比例差异 |
| 单点配准 | 1 对 | 仅算 **insert_x/insert_y（平移）**；`scale`/`rotation` 取显式配置值（默认 `1.0` / `0.0`） | 底图与钻孔仅存在平移错位、比例旋转已一致 |

- 配准点应取底图上**稳定、可辨认、间距足够大**的已知坐标点（如已有建筑角点、道路交点、图框角点）；
- 两点距离过近（`<1e-6` 米）或点对数量不匹配 → 打印告警并**回退显式 `insert_x/y` / `scale` / `rotation`**（若亦未显式配置，则**不施加变换**，默认同一坐标系导入）；
- 配准结果只在本次导入使用，**不修改底图 DWG 文件本身**（底图只读）。

### 7.8 行为（形态 B · 一次完整执行）

1. **必填校验**（锁外）：`_resolve_form_json("borehole_data", required=True)` 与 `_resolve_form_json("plan_base", required=False)`；失败 → `HTTP 400`。
2. **底图获取**（锁外，不阻塞 CAD）：
   - 解析 `base_map`（file）与 `base_map_url`（string）；同传 → 400；
   - 文件上传：扩展名 `.dwg`；体积 ≤ 50MB；同名覆盖落盘到 `ARX_DIR/support/底图/`；失败/超限/空文件 → 走 §7.9 告警；
   - URL 下载：仅 http/https；SSRF 校验（拒回环 `127.0.0.0/8` + `::1`）；私网/公网默认放行；`--ssrf-whitelist-cidr` 启动参数生效；限重定向 3 次；超时 30s；体积 ≤ 50MB；扩展名 `.dwg`；失败 → 走 §7.9 告警。
3. **`degraded_to` 判定**：
   - 调用方未给底图 → `degraded_to="process1_no_basemap"`（流程一正常路径）；
   - 调用方给了底图但**不可用** → `degraded_to="process1_no_basemap"`（告警已记录）；
   - 底图落盘成功 → `degraded_to=null`。
4. **`_lock` 内**：英文键 → 中文键 → 落盘 `钻探编录表.json`；按 `plan_base` / 底图状态决定是否落盘 `plan_base.json`；调 CAD `_call_cad(VIEW_ROUTES["plan"], timeout=300)`；超时 → `HTTP 504`。
5. **`auto_zoom`**（锁外）：`true` → 调 `/ai/tool/zoom_extents`（30s）。
6. **响应**：始终包含 `status` / `results` / `warnings` / `degraded_to` 四个字段。

### 7.9 底图不可用告警文案（告警必达 · 不返 5xx）

| 触发条件 | 文案 |
| --- | --- |
| 文件上传 IO 异常 | `底图未被采用：文件上传读取失败（<异常类型>：<消息>）` |
| 上传 > 50MB | `底图未被采用：base_map 超过 50MB 上限（实际 N 字节）` |
| 空文件 | `底图未被采用：base_map 为空文件` |
| 落盘失败 | `底图未被采用：落盘失败（<OS 错误>）` |
| URL 下载失败/超时/DNS 失败/连接错误 | `底图未被采用：URL 下载失败（<异常类型>：<消息>）` |
| URL 响应为空 | `底图未被采用：base_map_url 响应为空` |
| URL 内容 > 50MB | `底图未被采用：<size message>` |
| URL 重定向超过 3 次 | `底图未被采用：too many redirects (>3)` |
| SSRF 命中回环 / 协议非 http(s) | `底图未被采用：base_map_url 校验未通过（详见网关日志）`（不写明拒因，避免给攻击者反馈；详见 §7.10） |

> **CAD 端行为**（详设 V1.6 §9.3 / §9.4 告警必达口径）：`_call_cad` 成功返回后**必须**解析 `warnings[]` 并通过 `acedAlert` / 命令行 `acutPrintf` **显式**告知用户；**禁止仅打印到 CAD 日志窗口**。

### 7.10 SSRF（详设 V1.6 §9.4）

| 项 | 设计 |
| --- | --- |
| 协议 | 仅 `http://` / `https://` |
| 回环地址 | **一律拒**（`127.0.0.0/8` + `::1`，经 `socket.getaddrinfo` 解析后 `ipaddress.is_loopback` 比对） |
| 私网/公网 | **默认全放行**（`SSRF_DEFAULT_ALLOW_ALL=True`）；启动参数 `--ssrf-whitelist-cidr 10.x/172.16-31.x/192.168.x` 传入后**只放行白名单内** |
| 重定向 | 最多 3 次，逐跳独立 SSRF 校验 + 累计字节限流 |
| 超时 | 30s（`URL_TIMEOUT`） |
| 大小 | ≤ 50MB（`DWG_MAX_BYTES`） |
| 插件端 | **不联网**；底图下载全部发生在网关端（NFR-8） |
| 攻击行为告警 | SSRF 命中内网/回环时，不在 `warnings[]` 中明写拒因，避免给攻击者反馈；统一文案"base_map_url 校验未通过（详见网关日志）" |

### 7.11 请求示例

#### 7.11.1 流程一无底图（JSON body）

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "borehole_data": {
      "ZK1": {
        "hole_no": "ZK1",
        "elevation": 138.27,
        "depth": 7.7,
        "x": 2711077.24,
        "y": 562014.73,
        "layers": []
      }
    },
    "auto_zoom": true
  }'
```

**响应**：
```json
{
  "status": "success",
  "results": {"plan": {"status": "success"}},
  "warnings": [],
  "degraded_to": "process1_no_basemap"
}
```

> 调用方未给底图 → `degraded_to="process1_no_basemap"`（**非错误，是流程一正常路径**）。

#### 7.11.2 流程二 multipart 上传底图

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -F 'borehole_data={"ZK1":{"hole_no":"ZK1","elevation":138.27,"depth":7.7,"x":2711077.24,"y":562014.73,"layers":[]}};type=application/json' \
  -F 'base_map=@"/path/to/05阳光幸福里总平面规划图104(1).dwg";type=application/octet-stream' \
  -F 'plan_base={"base_map":{"dim":true,"plot_scale":500,"registration":{"source_points":[[2747350.0,36524890.0],[2747650.0,36524890.0]],"target_points":[[2747450.0,36524990.0],[2747750.0,36524990.0]]}}};type=application/json'
```

**响应**（底图成功落盘且生效）：
```json
{
  "status": "success",
  "results": {"plan": {"status": "success"}},
  "warnings": [],
  "degraded_to": null
}
```

#### 7.11.3 流程二 URL 底图（multipart/form-data）

> ⚠️ `base_map_url` **只从 `request.form` 读取**（详设 V1.6 §9.2），**必须使用 `multipart/form-data`**；用 `application/json` 提交底图键将返回 **HTTP 400**（`base_map / base_map_url 仅支持 multipart/form-data 提交`，须改用 `multipart/form-data`；D1 决策，详见 §8.2 第 5 行 / §7.2）。

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -F 'borehole_data={"ZK1":{"hole_no":"ZK1","elevation":138.27,"depth":7.7,"x":2711077.24,"y":562014.73,"layers":[]}};type=application/json' \
  -F 'base_map_url=https://internal.example.com/sitedata/05阳光幸福里总平面规划图104(1).dwg' \
  -F 'plan_base={"base_map":{"dim":true,"plot_scale":500}};type=application/json'
```

#### 7.11.4 告警必达示例（底图不可用，模拟落盘失败）

```json
{
  "status": "success",
  "results": {"plan": {"status": "success"}},
  "warnings": ["底图未被采用：文件上传读取失败（FileNotFoundError: [Errno 2] No such file or directory）"],
  "degraded_to": "process1_no_basemap"
}
```

> CAD 端**必须**用 `acedAlert` 或 `acutPrintf` 显式弹出"底图未被采用：xxx"并告知用户；**禁止仅记日志**。

---

## 8. 状态码与错误处理

### 8.1 状态码总览

| 状态码 | 触发条件 | 响应形态 | 说明 |
| --- | --- | --- | --- |
| `200 OK` | 正常（含底图不可用走告警必达） | `{status, results, warnings, degraded_to}`（plan）/ `{status, results}`（column / section） | 底图不可用**绝不**返 5xx（PM 决策 9 加粗：告警必达是本系统硬约束） |
| `400 Bad Request` | 调用方式错误 | `{status:"error", message:...}` | 仅以下场景：① `base_map` 与 `base_map_url` 同传；② 文件扩展名非 `.dwg`；③ `borehole_data` / `plan_base` / `section_params` 缺失或非法 JSON |
| `413 Request Entity Too Large` | multipart 体超过 50MB（Flask `MAX_CONTENT_LENGTH`） | Flask 默认错误页 | 见详设 V1.6 §9.1 |
| `504 Gateway Timeout` | `draw_plan` 调用 `_call_cad` 超时（300s） | `{status:"error", message:"plan view timed out (300s)", warnings, degraded_to}` | 仅 plan 视图可能；column/section 默认 120s |
| `5xx` | 网关或 CAD 内部异常 | 由 `_call_cad` 透传 | 出现于请求格式正确但 CAD 端崩溃 / 数据库异常等不可恢复错误 |

### 8.2 调用方式错误明细

| # | 触发 | 状态码 | 响应 `message` |
| --- | --- | --- | --- |
| 1 | `base_map` 与 `base_map_url` 同时提交 | 400 | `base_map 与 base_map_url 互斥，二选一` |
| 2 | `base_map` / `base_map_url` 扩展名非 `.dwg` | 400 | `only .dwg allowed` / `base_map_url must point to .dwg` |
| 3 | `borehole_data` 缺失或非法 JSON | 400 | `missing required field: borehole_data` / `borehole_data invalid JSON in body` / `... in form` |
| 4 | `plan_base` / `section_params` 非法 JSON | 400 | `<field> invalid JSON in body/form` |
| 5 | `application/json` 请求 body 含 `base_map` / `base_map_url` 键（V1.1 新增 · **D1 决策**） | 400 | `base_map / base_map_url 仅支持 multipart/form-data 提交` |

### 8.3 底图不可用枚举（200 + `warnings[]`）

详见 §7.9 表格。**绝不**返 5xx。

---

## 9. `borehole_data` 数据格式（形态 B 二维数组 · 推荐；形态 A 结构化 · 遗留）

> **V1.2 修订要点**（产品经理 Alice 同步，基于 `docs/评审-转换层下沉网关-增量设计-V1.1.md` 落地 D-A / D-B 决策）：
>
> - **形态 B 三接口**（`POST /api/draw/{column,section,plan}` · Flask 网关）的 `borehole_data` 入参**改为"二维数组 JSON"**：顶层 key = 孔号，value = 该孔 xlsx 原始行数组（`{"ZK1": [[row0], [row1], ...]}`）。
> - 网关侧 `cad_server/draw_gateway.py::_normalize_borehole()` **自动**将二维数组归一化为英文键结构化 JSON（`hole_no / x / y / elevation / depth / layers[]`），再走既有 `_translate_borehole()` 英译中键落盘；**调用方无需自行结构化转换**。
> - **形态 A**（`POST /api/draw` · ARX 内嵌 :7100）保留**结构化英文键 dict 输入**作为遗留兼容（详见 §9.7），**新接入推荐走形态 B**。
> - 详细行义表 / 字段映射表 / 容错规则 / 完整 ZK1 样例（二维数组 + 归一化对照）：见 §9.1.2 ~ §9.1.3 / §9.3 / §9.6；详细转换契约（函数签名 / 输入输出 / 集成到 4 接口）：见 `评审-转换层下沉网关-增量设计-V1.1 §3`。

### 9.1 二维数组 JSON 输入格式（形态 B 网关 · 推荐）

#### 9.1.1 顶层结构

```json
{
  "<孔号 1>": [[row_0], [row_1], [row_2], ...],
  "<孔号 2>": [[row_0], [row_1], [row_2], ...],
  ...
}
```

| 约束 | 规则 |
| --- | --- |
| 顶层 | object，顶层 key = 孔号（字符串，**原样保留**——不规范化、不强转大写） |
| 每个 value | list[list[any]]——每行是一个 list，列宽不固定（按 xlsx 实际列数） |
| 行数 | 每孔 ≥ 7 行（基本信息 4 行 + 表头 1 行 + 字段表头 1 行 + 至少 1 个地层） |
| 单元格 | string / number / null 三种；数字格按 JS Number（int / float）解析；空缺为 `null` |
| 字符编码 | UTF-8，JSON 无 BOM |
| 顺序 | 顶层 key 顺序**不要求**；地层行顺序按 zk 内部 row 顺序处理 |

#### 9.1.2 单孔二维数组的行义（基于 `cad_client/阳光幸福里-钻探编录表.json` 真实 ZK1 反推 · V1.1 勘误后）

| 行索引 | 内容 | 中文标签位置（列号 → 标签） | 用途 |
| --- | --- | --- | --- |
| **Row 0** | 公司名称 | col 0 = "广西昇龙工程勘察设计检测有限公司"（或同类公司名） | 仅作存在性校验，**不进入 borehole_data** |
| **Row 1** | 表名 + 页码 | col 0 = "工 程 勘 察 钻 探 现 场 编 录 表"；col 18 = "第 N 页共 M 页" | 仅作存在性校验 |
| **Row 2** | 工程基本信息 | col 0 = "工程名称" / col 1 = `<工程名称值>`；col 10 = "孔口高程（m）" / col 11 = `<高程值>`；col 12 = "钻孔类型" / col 14 = `<类型值>`（"技术孔"/"鉴别孔"）；col 16 = "终孔深度（m）" / col 18 = `<深度值>` | **基础信息行**，4 个核心字段 + 工程名称 |
| **Row 3** | 工程地点 + 开孔日期 + 初见水位 | col 0 = "工程地点" / col 1 = `<地点值>`；col 10 = "开孔日期" / col 11 = `<日期值>`；col 12 = "初见水位（m）" / col 14 = `<值或空字符串>` | 工程地点、开孔日期、可选水位 1 |
| **Row 4** | 孔号 + 测量学 X/Y + 终孔日期 + 稳定水位 | col 0 = "钻孔编号" / col 1 = `<孔号值>`；col 3 = "X坐标（m）" / col 4 = `<测量 X = 北 N 值>`；col 8 = "Y坐标（m）" / col 9 = `<测量 Y = 东 E 值>`；col 10 = "终孔日期" / col 11 = `<日期值>`；col 12 = "稳定水位（m）" / col 14 = `<值或空字符串>` | **孔号 + 坐标（关键）** + 终孔日期 + 可选水位 2 |
| **Row 5** | 地层表头起首 + 字段表头汇总 | col 0 = "地层编号" / col 1 = "进尺（m）" / col 3 = "岩性描述" / col 12 = "岩芯\n采取率（%）" / col 13 = "取岩（土、水）样" / col 15 = "原位测试" | 仅作存在性校验 |
| **Row 6** | 地层字段表头（详细列名） | col 1 = "自" / col 2 = "至" / col 3 = "地层名称" / col 4 = "颜色" / col 5 = "状态" / col 6 = "密实度" / col 7 = "湿度" / col 8 = "成分及其他" / col 13 = "编号" / col 14 = "取样深度" / col 15 = "类型" / col 16 = "起（m）" / col 17 = "止（m）" / col 18 = "杆长（m）" / col 19 = "锤击数（击）" | **字段表头定稿**——所有后续地层行按此表头取列偏移 |
| **Row 7~N** | 每个地层一行 | 按 Row 6 表头列偏移取字段值 | 地层数据，N 至少为 7 |
| **Row N+1** | 终止行 | col 1 / col 2 为终止进尺数字；col 3 = "终孔" | 网关**跳过**，不进入 borehole_data |
| **Row N+2** | 项目负责人 / 机长 / 编录员 | col 0 = "项目负责人：xxx  钻探机长：xxx  编录员：xxx" | **跳过**（不进入 borehole_data） |

#### 9.1.3 字段映射表（中文标签 → 结构化字段名 → 列位置 → 必填 · V1.1 勘误后）

> **约定**：列位置指单孔二维数组中的列索引（0-based）；"必填"指**进入 borehole_data 必须存在**（缺则 400）。

| # | 中文标签 | 二维数组位置 | 必填 | 结构化字段（英） | 类型 | 转换/取值说明 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | "钻孔编号" / `<孔号值>` | Row 4 col 0 / col 1 | **是** | `hole_no` | string | 取 row[4][1]；与顶层 key 双校验（应一致） |
| 2 | "X坐标（m）" / `<测量 X>` | Row 4 col 3 / col 4 | 平面必填 | `y`（注意映射） | number | **测量学 X = 北 N → AutoCAD Y = 纵**；row[4][4] 转 float |
| 3 | "Y坐标（m）" / `<测量 Y>` | Row 4 col 8 / col 9 | 平面必填 | `x`（注意映射） | number | **测量学 Y = 东 E → AutoCAD X = 横**；row[4][9] 转 float |
| 4 | "孔口高程（m）" / `<值>` | Row 2 col 10 / col 11 | **是** | `elevation` | number | row[2][11] 转 float |
| 5 | "终孔深度（m）" / `<值>` | Row 2 col 16 / col 18 | **是** | `depth` | number | row[2][18] 转 float |
| 6 | "钻孔类型" / `<值>` | Row 2 col 12 / col 14 | 否 | `hole_type` | string | row[2][14] 转 str（值仍传中文："技术孔"/"鉴别孔"） |
| 7 | "工程名称" / `<值>` | Row 2 col 0 / col 1 | 否 | `project_name` | string | row[2][1] 转 str |
| 8 | "工程地点" / `<值>` | Row 3 col 0 / col 1 | 否 | `site` | string | row[3][1] 转 str |
| 9 | "开孔日期" / `<值>` | Row 3 col 10 / col 11 | 否 | `date_start` | string | row[3][11] 转 str（值仍传中文："2025年09月27日"） |
| 10 | "终孔日期" / `<值>` | Row 4 col 10 / col 11 | 否 | `date_end` | string | row[4][11] 转 str |
| 11 | "初见水位（m）" / `<值>` | Row 3 col 12 / col 14 | 否 | `water_level_initial` | number \| string | row[3][14]：数字则转 float；空字符串/null 则 null |
| 12 | "稳定水位（m）" / `<值>` | Row 4 col 12 / col 14 | 否 | `water_level_stable` | number \| string | row[4][14]：数字则转 float；空字符串/null 则 null |
| 13 | "地层编号" / `<值>` | Row 7+ col 0 | **是**（地层行） | `layer_id` | string | row[ri][0] 转 str |
| 14 | 进尺"自" / `<值>` | Row 7+ col 1 | **是**（地层行） | `from` | number | row[ri][1] 转 float（层顶深度） |
| 15 | 进尺"至" / `<值>` | Row 7+ col 2 | **是**（地层行） | `to` | number | row[ri][2] 转 float（层底深度） |
| 16 | "地层名称" / `<值>` | Row 7+ col 3 | **是**（地层行） | `name` | string | row[ri][3] 转 str（用于匹配填充图案） |
| 17 | "颜色" / `<值>` | Row 7+ col 4 | 否 | `color` | string | row[ri][4] |
| 18 | "状态" / `<值>` | Row 7+ col 5 | 否 | `state` | string | row[ri][5] |
| 19 | "密实度" / `<值>` | Row 7+ col 6 | 否 | `density` | string | row[ri][6] |
| 20 | "湿度" / `<值>` | Row 7+ col 7 | 否 | `moisture` | string | row[ri][7] |
| 21 | "成分及其他" / `<值>` | Row 7+ col 8 | 否 | `description` | string | row[ri][8] |
| 22 | "编号" / 取样编号 | Row 7+ col 13 | 否 | `sample.no` | string | row[ri][13]；空值不输出 sample 子对象 |
| 23 | "取样深度" | Row 7+ col 14 | 否 | `sample.depth` | string | row[ri][14] |
| 24 | "类型" / 原位类型 | Row 7+ col 15 | 否 | `test.type` | string | row[ri][15]（"标贯"/"重型动探"）；空则 test 不输出 |
| 25 | "起（m）" | Row 7+ col 16 | 否 | `test.from` | **string** | row[ri][16] 转 str（接口 §9.7.3.2 明确为 string，不是 number） |
| 26 | "止（m）" | Row 7+ col 17 | 否 | `test.to` | **string** | row[ri][17] 转 str |
| 27 | "杆长（m）" | Row 7+ col 18 | 否 | `test.rod_length` | **string** | row[ri][18] 转 str |
| 28 | "锤击数（击）" | Row 7+ col 19 | 否 | `test.hits` | **string** | row[ri][19] 转 str（数字需 str(2) → "2"） |

#### 9.1.4 网关自动归一化流程

```
二维数组 JSON（§9.1.1 输入）
   │
   ▼
`_normalize_borehole(raw)`  ← cad_server/draw_gateway.py（新增）
                              §9.1.3 字段映射表逐列取数 + 关键标签校验
                              V1.1 勘误后常量：
                                ROW_STRATA_START = 7
                                COL_ELEVATION / COL_DATE_START = 11
   │
   ▼
英文键结构化 JSON（与 §9.7.2 同结构——`hole_no / x / y / elevation / depth / layers[]`）
   │
   ▼
`_translate_borehole()`  ← 既有代码，未动
   │
   ▼
中文键结构化 JSON → 落盘 ARX 端 `project/钻探编录表.json`
```

> 调用方**只关心** §9.1.1（顶层二维数组结构）+ §9.1.2（行义表，正确填 xlsx 即可）+ §9.1.3（字段映射，知道每个字段来自哪一列）。其余归一化 / 翻译 / 落盘流程由网关和 ARX 端透明完成。

#### 9.1.5 完整二维数组样例（ZK1 · `cad_client/阳光幸福里-钻探编录表.json`）

```json
{
  "ZK1": [
    ["广西昇龙工程勘察设计检测有限公司", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null],
    ["工  程  勘  察  钻  探  现  场  编  录  表", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, "第1页共37页", null],
    ["工程名称", "阳光幸福里", null, null, null, null, null, null, null, null, "孔口高程（m）", 206.05, "钻孔类型", null, "技术孔", null, "终孔深度（m）", null, 19.2, null],
    ["工程地点", "广西壮族自治区河池市环江毛南族自治县思恩镇民族路东侧", null, null, null, null, null, null, null, null, "开孔日期", "2025年09月27日", "初见水位（m）", null, "", null, "初见水位观测时间", null, null, null],
    ["钻孔编号", "ZK1", null, "X坐标（m）", 2747346.764, null, null, null, "Y坐标（m）", 36524930.708, "终孔日期", "2025年09月28日", "稳定水位（m）", null, "", null, "稳定水位观测时间", null, null, null],
    ["地层编号", "进尺（m）", null, "岩性描述", null, null, null, null, null, null, null, null, "岩芯\n采取率（%）", "取岩（土、水）样", null, "原位测试", null, null, null, null],
    [null, "自", "至", "地层名称", "颜色", "状态", "密实度", "湿度", "成分及其他", null, null, null, null, "编号", "取样深度", "类型", "起（m）", "止（m）", "杆长（m）", "锤击数（击）"],
    ["1-0-0", 0, 7.5, "填土-素填土", "褐黄色", "", "松散", "", "地层年代:第四纪,堆积年代:近十年堆积,地层成因:人工填土,均匀性:一般,主要成份:粘性土,次要成份:碎砖,", null, null, null, null, "QY-001", "13-13.3", "标贯", 9.15, 9.25, 10, 2],
    ["2-0-0", 7.5, 11.7, "黏性土-黏土", "褐黄色", "可塑", "", "", "地层年代:第四纪,地层成因:残积,刀切面光滑，中干强度，韧性中等，无摇振反应", null, null, null, null, null, null, "标贯", 9.25, 9.35, 10, 3],
    ["4-0-0", 11.7, 19.2, "岩石-泥灰岩", "灰黑色", "", "", "", "地层年代:石炭纪,坚硬程度:较硬岩,基本质量等级:III级,风化程度:中风化,可挖行:干钻不易钻进,结构类型:层状,泥质结构，中厚层状构造，节理裂隙较发育，岩体较完整，岩芯呈短柱状、块状，具易崩解性，浸水易软化，采取率为70%～82%，RQD=55%～63%。", null, null, null, null, null, null, "标贯", 9.35, 9.45, 10, 3]
  ]
}
```

#### 9.1.6 curl 示例（形态 B · 平面图 · 二维数组入参 · `application/json`）

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "borehole_data": {
      "ZK1": [
        ["广西昇龙工程勘察设计检测有限公司", null, ..., null],
        ["工  程  勘  察  钻  探  现  场  编  录  表", null, ..., "第1页共37页", null],
        ["工程名称", "阳光幸福里", null, ..., "孔口高程（m）", 206.05, "钻孔类型", null, "技术孔", null, "终孔深度（m）", null, 19.2, null],
        ["工程地点", "广西壮族自治区河池市环江毛南族自治县思恩镇民族路东侧", null, ..., "开孔日期", "2025年09月27日", ...],
        ["钻孔编号", "ZK1", null, "X坐标（m）", 2747346.764, ..., "Y坐标（m）", 36524930.708, ...],
        ["地层编号", "进尺（m）", ...],
        [null, "自", "至", "地层名称", "颜色", ...],
        ["1-0-0", 0, 7.5, "填土-素填土", "褐黄色", "", "松散", "", "...", ..., "标贯", 9.15, 9.25, 10, 2]
      ]
    },
    "auto_zoom": true
  }'
```

> 实际提交时建议用 `python -m json.tool` 校验二维数组 JSON 的合法性；网关侧的 `ValueError` 会以 `HTTP 400 + {"status":"error","message":...}` 透传。multipart/form-data 提交方式见 §10.3 / §10.4（形态 B 柱状/剖面/平面接口同样支持）。

### 9.2 地层列表组装规则（地层 / 取样 / 原位测试三段 · 网关归一化时按此组装）

按 §9.1.3 字段映射表 13~28 行，地层行 `r` 从 `ROW_STRATA_START (= 7)` 开始扫描，到 `len(rows) - 1` 结束：

1. **空行跳过**：`row` 中所有单元格均为 `null` / 空字符串 / 仅空白 → 跳过；
2. **终止行跳过**：col 3 == "终孔" → 跳过；
3. **无效进尺跳过**：`s_from == null and s_to == null` → 跳过（防御非法行）；
4. **地层基本段**（必填+可选）：`layer_id, from, to, name, color, state, density, moisture, description` —— 即使空字符串也输出（**保持字段存在性**），仅 `null` 才省略；
5. **取样段**：仅当 `sample.no` 或 `sample.depth` **任一非空**时输出 `sample: { no, depth }`；
6. **原位测试段**：仅当 `test.type` 或 `test.from/to/rod_length/hits` **任一非空**时输出 `test: { type, from, to, rod_length, hits }`；
7. **结构（与 §9.7.3 / §9.7.3.2 对齐）**：

```json
{
  "layer_id": "1-0-0",
  "from": 0,
  "to": 7.5,
  "name": "填土-素填土",
  "color": "褐黄色",
  "state": "",
  "density": "松散",
  "moisture": "",
  "description": "地层年代:第四纪,堆积年代:近十年堆积,...",
  "sample": { "no": "QY-001", "depth": "13-13.3" },
  "test":   { "type": "标贯", "from": "9.15", "to": "9.25", "rod_length": "10", "hits": "2" }
}
```

### 9.3 容错规则（二维数组 JSON · 网关侧 `_normalize_borehole`）

| 异常 | 处理 | HTTP 状态 |
| --- | --- | --- |
| 顶层不是 object | `borehole_data must be a JSON object` | **400** |
| 顶层为空 object（无孔） | 静默跳过（落空 borehole_data.json）；日志 `[WARN] no holes in borehole_data` | 200 |
| 某孔行数 `< ROW_STRATA_START (=7)` | `borehole_data['<孔号>']: 行数 < 7，模板异常` | **400** |
| 关键标签不存在（如 row 4 col 0 != "钻孔编号"） | `borehole_data['<孔号>']: row 4 col 0 标签缺失（期望"钻孔编号"）` | **400** |
| `hole_no`（row 4 col 1）与顶层 key 不一致 | `borehole_data['<孔号>']: row 4 col 1 '<值>' 与 key '<key>' 不一致` | **400**（**双校验**，防调用方把孔号搞错） |
| `elevation` / `depth` 非数字 | `borehole_data['<孔号>']: 字段 <字段名> 非数字（"<raw>"）` | **400** |
| `x` / `y` 测量学坐标非数字 | `borehole_data['<孔号>']: 测量 <X|Y>(m) 非数字` | **400**（平面必填） |
| 地层行 `from` / `to` 非数字且非"终孔" | 跳过该行 + 日志 `[WARN] layer row skipped: from/to 非数字` | 200（仅警告） |
| `sample.no/depth` / `test.type/from/to/rod_length/hits` 全部为空 | 不输出 `sample` / `test` 子对象 | 200 |
| `test.from/to/rod_length/hits` 为数字 | **强制 str() 转字符串**（接口契约 §9.7.3.2 明确） | 200 |
| 终止行（col 3 == "终孔"）| 跳过 | 200 |
| 负责人/机长/编录员行 | 跳过 | 200 |

### 9.4 网关注入实现（V1.1 §3.1 / §3.5 定稿 · 概要）

新增函数：

```python
def _normalize_borehole(raw: dict[str, list[list[Any]]]) -> dict[str, dict[str, Any]]:
    """二维数组 borehole_data JSON → 英文键结构化 borehole_data JSON。"""
```

行/列偏移常量（详见 `评审-转换层下沉网关-增量设计-V1.1 §3.5`）：

```python
# 行索引常量
ROW_HEADER, ROW_INFO_1, ROW_INFO_2, ROW_INFO_3 = 0, 2, 3, 4
ROW_STRATA_HEADER, ROW_STRATA_COL, ROW_STRATA_START = 5, 6, 7

# 基础信息列偏移（V1.1 勘误：COL_ELEVATION / COL_DATE_START 由 12 → 11）
COL_HOLE_NO, COL_X_SURVEY, COL_Y_SURVEY, COL_ELEVATION, COL_DEPTH = 1, 4, 9, 11, 18
COL_HOLE_TYPE, COL_PROJ_NAME, COL_PROJ_LOC, COL_DATE_START, COL_DATE_END = 14, 1, 1, 11, 11
COL_WATER_INIT, COL_WATER_STABLE = 14, 14

# 地层字段列偏移（与 Row 6 表头对齐）
COL_STRATA_ID, COL_STRATA_FROM, COL_STRATA_TO = 0, 1, 2
COL_STRATA_NAME, COL_STRATA_COLOR, COL_STRATA_STATE = 3, 4, 5
COL_STRATA_DENS, COL_STRATA_MOIST, COL_STRATA_DESC = 6, 7, 8
COL_SAMPLE_NO, COL_SAMPLE_DEPTH = 13, 14
COL_TEST_TYPE, COL_TEST_FROM, COL_TEST_TO = 15, 16, 17
COL_TEST_ROD, COL_TEST_HITS = 18, 19
```

调用链改造（V1.1 §3.2 / §3.3）：

```
4 接口 (column / section / plan)
  → _resolve_form_json("borehole_data")     # 拿到二维数组 JSON
  → _normalize_borehole(二维数组)            # 新增，本设计核心
  → _translate_borehole(英文键)              # 既有代码，未动
  → _write_json(BOREHOLE_FILE, 中文键)
  → _call_cad
```

形态 A（`POST /api/draw`）**保留**结构化 dict 输入兼容；详见 §9.7。

### 9.5 错误处理（详细）

| 异常源 | 文案 | HTTP | 文档引用 |
| --- | --- | --- | --- |
| `raw` 非 object | `borehole_data must be a JSON object` | 400 | §9.3 第 1 行 |
| `raw[zk_id]` 非 list | `borehole_data['<zk>']: 必须为 list（实际 <type>）` | 400 | §9.3 第 2 行 |
| 行数不足 | `borehole_data['<zk>']: 行数 <n> < 7，模板异常` | 400 | §9.3 第 3 行 |
| 关键标签缺失 | `borehole_data['<zk>']: row <r> col <c> 标签缺失（期望"<label>"）` | 400 | §9.3 第 4 行 |
| `hole_no` 与 key 不一致 | `borehole_data['<zk>']: row 4 col 1 '<v>' 与 key '<k>' 不一致` | 400 | §9.3 第 5 行 |
| 必填字段非数字（elevation/depth/x/y） | `borehole_data['<zk>']: 字段 <field> 非数字或缺失` | 400 | §9.3 第 6/7 行 |

> **非阻塞警告**（不返 4xx/5xx，仅日志）：
> - 地层行 `from/to` 非法且非"终孔" → 日志 `[WARN] layer row skipped: from/to 非数字`，跳过该行；
> - `test.from/to/rod_length/hits` 为数字 → **强制 str()**，不警告（契约要求）。

### 9.6 与设计文档的引用

- 行义表 / 字段映射表 / 容错规则 / ZK1 真实样例（二维数组 + 结构化对照）的**定稿版**：见 `docs/评审-转换层下沉网关-增量设计-V1.1.md` §2（含 V1.1 勘误，列号与行号已与真实数据自洽）。
- 函数签名 / 输入输出契约 / 关键路径伪代码 / 集成到 4 接口的代码段：见同文档 §3。
- 与形态 A 的兼容关系：见 §9.7。
- 文档影响面清单：见 `评审-转换层下沉网关-增量设计-V1.1.md` §6。

### 9.7 兼容：结构化 dict 输入（形态 A 遗留 · 不推荐新接入）

> ⚠️ **本节描述的是形态 A（`POST /api/draw` · ARX 内嵌 :7100）遗留输入格式**。新接入请使用 §9.1 ~ §9.6 描述的二维数组 JSON 输入；网关自动归一化为等价结构。
>
> 形态 A 与形态 B 的关系：形态 A 是已实现的 ARX 内嵌 HTTP 服务，**无独立网关**——转换逻辑由调用方保证；形态 B 是新增的 Flask 网关，转换逻辑由网关 `_normalize_borehole` 透明承担。两种形态不并存；详细理由见 `评审-转换层下沉网关-增量设计-V1.1.md` §4.2 / §4.3。

#### 9.7.1 整体结构

```
borehole_data (object)
├── "ZK1"            ← 顶层 key = 钻孔编号（字符串，原样）
│   ├── hole_no, x, y, elevation, depth
│   ├── layers[]
│   │   ├── layer_id, from, to, name
│   │   ├── color, state, density, moisture
│   │   ├── description
│   │   ├── sample { no, depth }
│   │   └── test   { type, from, to, rod_length, hits }
├── "ZK2": { ... }
└── ...
```

> JSON 编码：UTF-8，无 BOM。字段名英文 snake_case。

#### 9.7.2 单孔字段

| 字段名 | 类型 | 必填 | 平面 | 柱状 | 备注（落盘中文 key） |
| --- | --- | --- | --- | --- | --- |
| `hole_no` | string | **是** | ✓ | ✓ | 钻孔编号 |
| `x` | number | 平面必填 | ✓ | — | X 坐标 (m)，大地坐标 X |
| `y` | number | 平面必填 | ✓ | — | Y 坐标 (m)，大地坐标 Y |
| `elevation` | number | **是** | ✓ | ✓ | 孔口高程 (m) |
| `depth` | number | 柱状必填 | — | ✓ | 终孔深度 (m) |
| `hole_type` | string | 建议 | ✓ | — | 钻孔类型（值仍传中文，如 `"技术孔"`） |
| `project_name` | string | 柱状建议 | — | ✓ | 工程名称 |
| `company` | string | 否 | — | ✓ | 公司名称 |
| `site` | string | 否 | — | — | 工程地点 |
| `date_start` | string | 否 | — | ✓ | 开孔日期，如 `"2025年12月10日"` |
| `date_end` | string | 否 | — | ✓ | 终孔日期 |
| `page_current` | int | 否 | — | ✓ | 当前页，默认 1 |
| `page_total` | int | 否 | — | ✓ | 总页码，默认 1 |
| `water_level_initial` | string | 否 | — | — | 初见水位 (m)，无则 `""` |
| `water_level_stable` | string | 否 | — | — | 稳定水位 (m)，无则 `""` |
| `manager` | string | 否 | — | — | 项目负责人 |
| `driller` | string | 否 | — | — | 钻探机长 |
| `recorder` | string | 否 | — | — | 编录员 |
| `layers` | array | 柱状必填 | — | ✓ | 地层列表，见 §9.7.3 |

#### 9.7.3 `layers[]` 地层字段

| 字段名 | 类型 | 必填 | 备注（落盘中文 key） |
| --- | --- | --- | --- |
| `layer_id` | string | **是** | 地层编号，如 `"1-0-0"` |
| `from` | number | **是** | 进尺自 (m)，层顶深度 |
| `to` | number | **是** | 进尺至 (m)，层底深度 |
| `name` | string | **是** | 地层名称，如 `"填土-素填土"`，用于匹配填充图案 |
| `color` | string | 否 | 颜色 |
| `state` | string | 否 | 状态（可塑 / 中风化等） |
| `density` | string | 否 | 密实度（松散 / 中密等） |
| `moisture` | string | 否 | 湿度 |
| `description` | string | 否 | 成分及其他（柱状图描述栏） |
| `sample` | object | 否 | 取岩(土、水)样，见 §9.7.3.1 |
| `test` | object | 否 | 原位测试，见 §9.7.3.2 |

##### 9.7.3.1 `sample` 取样子对象

| 字段名 | 类型 | 备注 |
| --- | --- | --- |
| `no` | string | 编号，如 `"QT-001"`，无则 `""` |
| `depth` | string | 取样深度，如 `"13-13.3"`，无则 `""` |

##### 9.7.3.2 `test` 原位测试子对象

| 字段名 | 类型 | 备注 |
| --- | --- | --- |
| `type` | string | 类型值仍传中文：`"标贯"` / `"重型动探"`，无则 `""` |
| `from` | string | 起 (m) |
| `to` | string | 止 (m) |
| `rod_length` | string | 杆长 (m) |
| `hits` | string | 锤击数 (击) |

#### 9.7.4 完整请求示例（形态 A · `POST /api/draw`）

```bash
curl -X POST http://localhost:7100/api/draw \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "views": ["plan", "column"],
    "borehole_data": {
      "ZK1": {
        "hole_no": "ZK1",
        "project_name": "宜州城区地下管网改造提升项目",
        "company": "广西昇龙工程勘察设计检测有限公司",
        "site": "广西壮族自治区河池市宜州市",
        "elevation": 138.27,
        "depth": 7.7,
        "hole_type": "技术孔",
        "x": 2711077.24,
        "y": 562014.73,
        "date_start": "2025年12月10日",
        "date_end": "2025年12月10日",
        "page_current": 1,
        "page_total": 1,
        "water_level_initial": "",
        "water_level_stable": "",
        "layers": [
          {
            "layer_id": "1-0-0",
            "from": 0, "to": 1.0,
            "name": "填土-素填土",
            "color": "灰黑色",
            "density": "松散",
            "description": "第四纪人工填土，主要成份粘性土",
            "sample": {"no": "", "depth": ""},
            "test":   {"type": "标贯", "from": "3.15", "to": "3.25", "rod_length": "5.2", "hits": "3"}
          },
          {
            "layer_id": "2-0-0",
            "from": 1.0, "to": 7.7,
            "name": "黏性土-粉质黏土",
            "color": "黄褐色",
            "state": "可塑",
            "description": "第四纪残积，干强度中等",
            "sample": {"no": "", "depth": ""},
            "test":   {"type": "标贯", "from": "3.25", "to": "3.35", "rod_length": "5.2", "hits": "3"}
          }
        ]
      }
    }
  }'
```

---

## 10. 完整 curl 调用示例

### 10.1 形态 A · `POST /api/draw`（一次出多图，application/json · 结构化 dict 遗留）

```bash
curl -X POST http://localhost:7100/api/draw \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "views": ["plan", "column"],
    "borehole_data": { "ZK1": { /* 结构化 dict，见 §9.7 */ } }
  }'
```

### 10.2 形态 B · `POST /api/draw/plan` 流程一无底图（application/json · 二维数组 JSON）

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "borehole_data": {
      "ZK1": [
        ["广西昇龙工程勘察设计检测有限公司", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null],
        ["工  程  勘  察  钻  探  现  场  编  录  表", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, "第1页共37页", null],
        ["工程名称", "宜州城区地下管网改造提升项目", null, null, null, null, null, null, null, null, "孔口高程（m）", 138.27, "钻孔类型", null, "技术孔", null, "终孔深度（m）", null, 7.7, null],
        ["工程地点", "广西壮族自治区河池市宜州市", null, null, null, null, null, null, null, null, "开孔日期", "2025年12月10日", "初见水位（m）", null, "", null, "初见水位观测时间", null, null, null],
        ["钻孔编号", "ZK1", null, "X坐标（m）", 2711077.24, null, null, null, "Y坐标（m）", 562014.73, "终孔日期", "2025年12月10日", "稳定水位（m）", null, "", null, "稳定水位观测时间", null, null, null],
        ["地层编号", "进尺（m）", null, "岩性描述", null, null, null, null, null, null, null, null, "岩芯\n采取率（%）", "取岩（土、水）样", null, "原位测试", null, null, null, null],
        [null, "自", "至", "地层名称", "颜色", "状态", "密实度", "湿度", "成分及其他", null, null, null, null, "编号", "取样深度", "类型", "起（m）", "止（m）", "杆长（m）", "锤击数（击）"],
        ["1-0-0", 0, 7.7, "黏性土-粉质黏土", "黄褐色", "可塑", "", "", "第四纪残积，干强度中等", null, null, null, null, null, null, null, null, null, null, null]
      ]
    },
    "auto_zoom": true
  }'
```

> 完整二维数组 JSON 行义表与字段映射表见 §9.1.2 / §9.1.3；网关自动归一化为结构化 dict 后走既有 `_translate_borehole` 落盘。完整 ZK1 真实样例见 §9.1.5。

### 10.3 形态 B · `POST /api/draw/plan` 流程二 multipart 上传底图（二维数组 JSON）

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -F 'borehole_data={"ZK1":[["广西昇龙工程勘察设计检测有限公司",null,...],["工  程  勘  察  钻  探  现  场  编  录  表",null,...,"第1页共37页",null],["工程名称","宜州城区地下管网改造提升项目",null,...,"孔口高程（m）",138.27,"钻孔类型",null,"技术孔",null,"终孔深度（m）",null,7.7,null],["工程地点","广西壮族自治区河池市宜州市",null,...,"开孔日期","2025年12月10日",...],["钻孔编号","ZK1",null,"X坐标（m）",2711077.24,...,"Y坐标（m）",562014.73,...],["地层编号","进尺（m）",...],[null,"自","至","地层名称","颜色","状态","密实度","湿度","成分及其他",...],["1-0-0",0,7.7,"黏性土-粉质黏土","黄褐色","可塑","","","第四纪残积",...,null,null,null,null,null,null,null,null,null,null]]};type=application/json' \
  -F 'base_map=@"/path/to/05阳光幸福里总平面规划图104(1).dwg";type=application/octet-stream' \
  -F 'plan_base={"base_map":{"dim":true,"plot_scale":500,"registration":{"source_points":[[2747350.0,36524890.0],[2747650.0,36524890.0]],"target_points":[[2747450.0,36524990.0],[2747750.0,36524990.0]]}}};type=application/json'
```

> 上例中 `borehole_data` 为省略版（实际提交时按 §9.1.5 完整二维数组填齐 8 行）。完整字段映射见 §9.1.3；行义见 §9.1.2。

### 10.4 形态 B · `POST /api/draw/plan` 流程二 URL 底图（multipart/form-data · 二维数组 JSON）

> ⚠️ `base_map_url` **只从 `request.form` 读取**（详设 V1.6 §9.2），**必须使用 `multipart/form-data`**；用 `application/json` 提交底图键将返回 **HTTP 400**（`base_map / base_map_url 仅支持 multipart/form-data 提交`，须改用 `multipart/form-data`；D1 决策，详见 §8.2 第 5 行）。

```bash
curl -X POST http://<gateway>/api/draw/plan \
  -F 'borehole_data={"ZK1":[["广西昇龙工程勘察设计检测有限公司",null,...],...,["1-0-0",0,7.7,"黏性土-粉质黏土","黄褐色","可塑","","","第四纪残积",...,null,null,null,null,null,null,null,null,null,null]]};type=application/json' \
  -F 'base_map_url=https://internal.example.com/sitedata/05阳光幸福里总平面规划图104(1).dwg' \
  -F 'plan_base={"base_map":{"dim":true,"plot_scale":500}};type=application/json'
```

### 10.5 形态 B · `POST /api/draw/column`（application/json · 二维数组 JSON）

```bash
curl -X POST http://<gateway>/api/draw/column \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "borehole_data": {
      "ZK1": [
        ["广西昇龙工程勘察设计检测有限公司", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null],
        ["工  程  勘  察  钻  探  现  场  编  录  表", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, "第1页共1页", null],
        ["工程名称", "宜州城区地下管网改造提升项目", null, null, null, null, null, null, null, null, "孔口高程（m）", 138.27, "钻孔类型", null, "技术孔", null, "终孔深度（m）", null, 7.7, null],
        ["工程地点", "广西壮族自治区河池市宜州市", null, null, null, null, null, null, null, null, "开孔日期", "2025年12月10日", "初见水位（m）", null, "", null, "初见水位观测时间", null, null, null],
        ["钻孔编号", "ZK1", null, "X坐标（m）", 2711077.24, null, null, null, "Y坐标（m）", 562014.73, "终孔日期", "2025年12月10日", "稳定水位（m）", null, "", null, "稳定水位观测时间", null, null, null],
        ["地层编号", "进尺（m）", null, "岩性描述", null, null, null, null, null, null, null, null, "岩芯\n采取率（%）", "取岩（土、水）样", null, "原位测试", null, null, null, null],
        [null, "自", "至", "地层名称", "颜色", "状态", "密实度", "湿度", "成分及其他", null, null, null, null, "编号", "取样深度", "类型", "起（m）", "止（m）", "杆长（m）", "锤击数（击）"],
        ["1-0-0", 0, 7.7, "黏性土-粉质黏土", "黄褐色", "可塑", "", "", "第四纪残积，干强度中等", null, null, null, null, "", "", "", "", "", "", ""]
      ]
    },
    "auto_zoom": true
  }'
```

### 10.6 形态 B · `POST /api/draw/section`（multipart · 二维数组 JSON）

```bash
curl -X POST http://<gateway>/api/draw/section \
  -F 'borehole_data={"ZK1":[["广西昇龙工程勘察设计检测有限公司",null,...],...,["1-0-0",0,7.7,"黏性土-粉质黏土","黄褐色","可塑","","","第四纪残积",...,null,null,null,null,null,null,null,null,null,null]]};type=application/json' \
  -F 'section_params={"sections":[{"id":"A-A","holes":["ZK1","ZK2"],"azimuth":90.0}]};type=application/json' \
  -F 'auto_zoom=true'
```

---

## 11. 错误排查

### 11.1 连接 / 形态 A 启动问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 连接拒绝 | CAD 没起 / ARX 没加载 / 没 `SLINIT` | 重启 CAD，`(arxload ...)`，`SLINIT` |
| 400 Bad Request | JSON 格式错或缺 `borehole_data` | 检查请求 body 与字段名 |
| HTTP 200 但 CAD 里没东西 | 形态 A 缺 `x`/`y`/`elevation` 或字段名拼错；形态 B 二维数组行/列号未对齐（`cad_client` xlsx 模板换版） | 形态 A 对照 §9.7.2 字段表；形态 B 对照 §9.1.2 行义表 + §9.1.3 字段映射表 + §9.3 容错规则 |
| 柱图无填充图案 | `layers[].name` 未在 `pat_define.json` 配置 | 补图案映射 |
| CAD 长时间无响应 | 孔数极多（>500），画图中 | 等待；插件每 10 孔自动 yield |

### 11.2 形态 B 网关相关（新增）

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 400 `base_map 与 base_map_url 互斥，二选一` | 两个字段同时提交 | 改为只提交其中一个 |
| 400 `only .dwg allowed` / `base_map_url must point to .dwg` | 扩展名非 `.dwg` | 确认底图文件名 / URL 路径以 `.dwg` 结尾 |
| 400 `borehole_data invalid JSON in body/form` | JSON 体或 form 字段非合法 JSON | 用 `python -m json.tool` 校验 |
| 413 Request Entity Too Large | 上传超过 50MB（Flask `MAX_CONTENT_LENGTH`） | 压缩或拆分底图；详设 V1.6 §9.1 |
| 504 `plan view timed out (300s)` | 大底图 + 配准 + 出图比例超 300s | 客户端重试；适当缩放或简化底图 |
| 200 + `warnings: ["底图未被采用：..."]` | 底图不可用，按流程一继续出图 | 检查底图路径 / 链接 / 体积 / 网络可达性；**CAD 端已显式告知用户**（详设 V1.7 §9.3） |
| 400 `base_map / base_map_url 仅支持 multipart/form-data 提交` | `application/json` 请求 body 含底图键（V1.1 D1 决策） | 改用 `multipart/form-data` 提交（见 §7.11.2 / §7.11.3 / §10.3 / §10.4） |
| `warnings: ["底图未被采用：base_map_url 校验未通过（详见网关日志）"]` | SSRF 命中回环 / 协议非 http(s) | 在网关 `--ssrf-whitelist-cidr` 配置或改用 http(s) |
| `degraded_to: "process1_no_basemap"` 且 `warnings: []` | 调用方**没给底图**，走流程一正常路径 | 非错误，是流程一对等路径 |
| 并发调用耗时叠加 | 网关 `_lock` 串行 | 按需由部署层分项目分目录；本版不承诺多项目隔离 |
| `_lock` 内 _save_base_map 阻塞 | V1.6 之前版本遗留 bug | 升级到 V1.6；下载已移出锁 |

### 11.3 平面图特有

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 底图未生效但图正常出 | 底图不可用，已走流程一 | 检查 `warnings[]` 内容；按 §11.2 处理底图来源 |
| 底图与钻孔错位 | `registration` 点对配错 / `scale`/`rotation` 与实际不一致 | 对照 §7.7 校验配准点；或显式 `insert_x/y` / `scale` / `rotation` |
| 出图比例未生效 | 未配置 `plot_scale` 或配置为 0 | 在 `plan_base.base_map.plot_scale` 提供正整数（如 `500`） |
| 出图比例与坐标对齐"打架" | 误将出图比例值放到对齐 `scale` | 区分两者：见 §7.6 表格 |
| 图例被底图"绑架" | 详设 V1.5 之前版本（已修复） | 升级到 V1.6，按"勘察点方案 + 勘察点图例"层定位 |

---

## 12. 变更记录

| 日期 | 版本 | 内容 |
| --- | --- | --- |
| 2026-10-14 | **V1.0** | 初版。整合 `cad_server/API.md` v2.1（形态 A：单接口 `POST /api/draw` 已实现）与详设 V1.6 §9（形态 B：三接口 `POST /api/draw/{column,section,plan}` 待实现）。覆盖平面图流程一/流程二并列、底图两种提交方式、SSRF 策略、告警必达（绝不返 5xx + `warnings[]` + `degraded_to`）、出图比例 `plot_scale` 与对齐 `scale` 的区别、单项目/单 CAD 实例假设。基线：详设 V1.6 / 需规 V1.3 / 需求文档 V2.4。 |
| 2026-10-14 | **V1.0**（初版核验修正） | 主理人核验发现两处与基线不符：① `base_map_url` 提交位置笔误——基线 `_save_base_map()` 仅从 `request.form` 读取，文档误写为"JSON body / form 字段"。修正：§7.2 表格行改为"仅 form 字段（multipart/form-data）"，新增"底图字段仅支持 multipart/form-data"表注（含"JSON body 提交会被静默忽略并按流程一继续出图"漏洞场景说明）；§3 接口清单总表第 4 行备注同步；§7.11.3 / §10.4 URL 底图示例由 `application/json` 改为 `multipart/form-data`；§11.2 排查表新增"静默忽略"行。② §1.2 ASCII 架构图"（仅网关端 · · 不阻塞 CAD）"重复点号。版本号维持 V1.0。 |
| 2026-10-15 | **V1.2** | **borehole_data 入参改为二维数组 JSON（产品经理 Alice 同步 · 基于 `评审-转换层下沉网关-增量设计-V1.1` 落地 D-A / D-B 决策）**：① §9 整章重写——新增 §9.1 二维数组 JSON 输入格式（形态 B 网关主路径 · 推荐），含 §9.1.1 顶层结构 / §9.1.2 单孔二维数组行义表（V1.1 勘误后：Row 7+ = 地层数据行）/ §9.1.3 字段映射表（28 行，含 V1.1 勘误：`elevation = row[2][11]`、`date_start = row[3][11]`）/ §9.1.4 网关自动归一化流程 / §9.1.5 完整 ZK1 真实样例（基于 `cad_client/阳光幸福里-钻探编录表.json`）/ §9.1.6 curl 示例 / §9.2 地层组装规则 / §9.3 容错规则 / §9.4 网关注入实现 / §9.5 错误处理 / §9.6 与设计文档的引用 / §9.7 兼容：结构化 dict 输入（形态 A 遗留 · 不推荐新接入），将原 §9.1 ~ §9.4 内容下移为 §9.7.1 ~ §9.7.4；② §1.2 形态 B 架构段补"`_normalize_borehole()` 二维数组→英文键"+ "二维数组 JSON" 入参说明；③ §3 接口清单总表 borehole_data 字段标注 "二维数组 JSON（网关自动归一化）"；④ §10.2 ~ §10.6 形态 B curl 示例由结构化 dict 改为二维数组 JSON；⑤ §10.1 形态 A curl 示例注释改指 §9.7；⑥ §11.1 排查表 "对照 §9.2 字段表" 拆分为形态 A（§9.7.2）+ 形态 B（§9.1.2 / §9.1.3 / §9.3）双路径；⑦ 文档元信息版本号 V1.1 → V1.2、日期 2026-10-14 → 2026-10-15。基线：详设 V1.7 / 需规 V1.3 / 需求文档 V2.4 / `评审-转换层下沉网关-增量设计-V1.1`。**未改动**：形态 A 接口定义、§3 接口清单总表接口形态/状态、§4-§8 既有流程与底图策略、§11.2 形态 B 错误排查表。 |

---

*本接口文档完成后，下游对接**直接按本版 §3 接口清单总表对号入座**：形态 A 调用方对接 `POST /api/draw`；形态 B 调用方按需调用 `POST /api/draw/{column,section,plan}`，平面图按"是否持有底图"选流程一/流程二，底图不可用告警必达不返 5xx。*
