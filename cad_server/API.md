# CAD 智能出图接口文档

> 版本：v2.1 ｜ 更新：2026-10-12
> 适用：AutoCAD 2007 + KsExploration.arx（一体化内置 HTTP，无独立网关）

---

## 1. 架构

```
业务系统（客户服务）
   │  HTTP, JSON (UTF-8, 英文 snake_case key)
   ▼
┌──────────────────────────────────────┐
│  AutoCAD 2007 + KsExploration.arx    │
│  内嵌 HTTP 服务监听 :7100             │
│  收英文 JSON → 自动转中文 → 落盘      │
│  → 调画出图（柱状图每 10 孔 yield）   │
└──────────────────────────────────────┘
```

**无独立 Python 网关**。业务系统、CAD、插件在同一台机器，直接打 localhost。客户端只传英文 key，C++ 端落盘时自动转成中文 key 供内部绘制逻辑使用。

| 服务 | 地址 | 用途 |
|---|---|---|
| **出图服务** | `http://<host>:7100` | 业务系统直接对接 |

---

## 2. 前置条件

1. AutoCAD 2007 已启动
2. 命令行加载插件：
   ```
   (arxload "D:/project/ies/cad/exploration/Debug/KsExploration.arx")
   ```
3. 命令行启动 HTTP 服务：
   ```
   SLINIT
   ```
   看到 `[CAD DEBUG] Listening started` 即可。

---

## 3. 接口

### 3.1 `POST /api/draw` —— 一次调用出图

#### 请求

```
POST http://<host>:7100/api/draw
Content-Type: application/json; charset=utf-8
```

| 字段 | 类型 | 必填 | 默认 | 说明 |
|---|---|---|---|---|
| `views` | string[] | 否 | `["plan"]` | 要画哪几种：`plan`=平面图，`column`=柱状图，`section`=剖面图 |
| `borehole_data` | object | **是** | — | 钻孔数据，结构见 §4。顶层 key = 孔号 |
| `section_params` | object | 否 | — | 剖面图参数，仅 `views` 含 `section` 时需要 |

#### 响应

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

#### 性能参考（实测）

| 孔数 | views | 耗时 |
|---|---|---|
| 5 孔 | plan | ~3s |
| 352 孔 | plan + column | **25.3s** |

客户端超时建议 ≥ 300s。

---

## 4. `borehole_data` 数据格式

### 4.0 整体结构

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

---

### 4.1 单孔字段

| 字段名 | 类型 | 必填 | 平面 | 柱状 | 备注（落盘中文 key） |
|---|---|---|---|---|---|
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
| `layers` | array | 柱状必填 | — | ✓ | 地层列表，见 §4.2 |

---

### 4.2 `layers[]` 地层字段

| 字段名 | 类型 | 必填 | 备注（落盘中文 key） |
|---|---|---|---|
| `layer_id` | string | **是** | 地层编号，如 `"1-0-0"` |
| `from` | number | **是** | 进尺自 (m)，层顶深度 |
| `to` | number | **是** | 进尺至 (m)，层底深度 |
| `name` | string | **是** | 地层名称，如 `"填土-素填土"`，用于匹配填充图案 |
| `color` | string | 否 | 颜色 |
| `state` | string | 否 | 状态（可塑 / 中风化等） |
| `density` | string | 否 | 密实度（松散 / 中密等） |
| `moisture` | string | 否 | 湿度 |
| `description` | string | 否 | 成分及其他（柱状图描述栏） |
| `sample` | object | 否 | 取岩(土、水)样，见 §4.2.1 |
| `test` | object | 否 | 原位测试，见 §4.2.2 |

#### 4.2.1 `sample` 取样子对象

| 字段名 | 类型 | 备注 |
|---|---|---|
| `no` | string | 编号，如 `"QT-001"`，无则 `""` |
| `depth` | string | 取样深度，如 `"13-13.3"`，无则 `""` |

#### 4.2.2 `test` 原位测试子对象

| 字段名 | 类型 | 备注 |
|---|---|---|
| `type` | string | 类型值仍传中文：`"标贯"` / `"重型动探"`，无则 `""` |
| `from` | string | 起 (m) |
| `to` | string | 止 (m) |
| `rod_length` | string | 杆长 (m) |
| `hits` | string | 锤击数 (击) |

---

### 4.3 完整请求示例

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

## 5. 错误排查

| 现象 | 原因 | 处理 |
|---|---|---|
| 连接拒绝 | CAD 没起 / arx 没加载 / 没 SLINIT | 重启 CAD，arxload，SLINIT |
| 400 Bad Request | JSON 格式错或缺 `borehole_data` | 检查 body |
| HTTP 200 但 CAD 里没东西 | 缺 `x`/`y`/`elevation` 或字段名拼错 | 对照 §4.1 字段表 |
| 柱图无填充图案 | `layers[].name` 未在 `pat_define.json` 配置 | 补图案映射 |
| CAD 长时间无响应 | 孔数极多（>500），画图中 | 等待，插件每 10 孔自动 yield |

---

## 6. 部署注意

1. AutoCAD 必须保持打开，插件是进程内 DLL。
2. 同一时刻只能一个 CAD 实例加载该 arx。
3. 无鉴权；业务系统与 CAD 同机，默认监听 localhost。跨机器访问需改绑 `0.0.0.0` 并加防火墙规则。
4. 客户端超时建议 ≥ 300s。
5. 一次 POST 可传任意孔数；插件内部每 10 个柱状图 yield 一次消息泵，352 孔实测不崩。

---

## 7. 变更记录

| 日期 | 版本 | 内容 |
|---|---|---|
| 2026-10-12 | v1.0 | 初版（Python 网关 :7200） |
| 2026-10-12 | v1.2 | 对外字段英文 snake_case |
| 2026-10-12 | v2.0 | 去掉 Python 网关，直接调 arx :7100 |
| 2026-10-12 | **v2.1** | **C++ 端内置英中映射：客户端传英文 key，落盘自动转中文；柱状图内部 yield 消息泵；352 孔实测 25.3s** |
