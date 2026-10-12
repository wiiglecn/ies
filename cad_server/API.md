# CAD 智能出图接口文档

> 版本：v1.2 ｜ 更新：2026-10-12
> 适用：AutoCAD 2007 + KsExploration.arx + draw_gateway 网关
> **字段命名约定**
>
> ：对外接口全部使用英文 
>
> `snake_case`
>
> ；网关落盘时自动转成中文 key 供 C++ 插件读取。下表 "备注" 列即为落盘后的中文原名，仅供对照，客户端不要传中文 key。



***

## 1. 架构与端口



```
客户端（Web/Postman/业务系统）
   │  HTTP, JSON (英文 key)
   ▼
┌─────────────────────────────┐
│  draw_gateway.py :7200      │
│  收英文 JSON → 转中文 → 落盘 │
└─────────────────────────────┘
   │
   ▼
┌─────────────────────────────┐
│  ARX 插件 :7100             │  读中文 key 的 JSON 文件
└─────────────────────────────┘
```



| 服务       | 地址                      | 用途                  |
| -------- | ----------------------- | ------------------- |
| **出图网关** | `http://<host>:7200`    | 业务系统对接（本接口文档描述的就是它） |
| 插件原生     | `http://localhost:7100` | 调试用，业务系统不直接调        |



***

## 2. 快速开始

### 2.1 前置条件



1. AutoCAD 2007 已启动并加载 `KsExploration.arx`

2. 网关已启动：



```
cd D:\project\kt\ies\cad_server
.venv\Scripts\python.exe draw_gateway.py
```



1. 健康检查：



```
curl http://localhost:7200/health
```

### 2.2 一行调用出图



```
curl -X POST http://localhost:7200/api/draw \
  -H "Content-Type: application/json; charset=utf-8" \
  -d @your_borehole.json
```



***

## 3. 网关接口（:7200）

### 3.1 `POST /api/draw` —— 一次调用出图

#### 请求体顶层字段



| 字段               | 类型        | 必填    | 默认                            | 说明                                              |
| ---------------- | --------- | ----- | ----------------------------- | ----------------------------------------------- |
| `borehole_data`  | object    | **是** | —                             | 钻孔数据，结构见 §4                                     |
| `views`          | string\[] | 否     | `["plan","section","column"]` | 要画哪几种图：`plan`= 平面图，`section`= 剖面图，`column`= 柱状图 |
| `section_params` | object    | 否     | 沿用上次                          | 剖面图参数，见 §4.4；不画 section 可不传                     |
| `auto_zoom`      | bool      | 否     | `true`                        | 出图后是否 zoom extents                              |

#### 响应



| HTTP  | 含义               |
| ----- | ---------------- |
| `200` | 全部成功             |
| `207` | 部分失败（看 `errors`） |
| `400` | 请求格式错            |
| `500` | 写文件失败            |



```
{
  "status": "success | partial",
  "results": { "plan": {"status":"success"}, "section": {...}, "column": {...} },
  "errors":  {},
  "files":   { "borehole": "<server path>", "section": "<server path or null>" }
}
```



***

### 3.2 `GET /health`



```
curl http://localhost:7200/health
# {"gateway":"ok","cad":true,"cad_http":"http://localhost:7100"}
```



***

## 4. `borehole_data` 数据格式

### 4.0 整体结构



```
borehole_data (object)
├── "ZK1"            ← 顶层 key = 钻孔编号，保持原样
│   ├── <hole fields>
│   └── layers[]     ← 地层列表
│       └── <layer fields>
│           ├── sample{}   ← 取样
│           └── test{}      ← 原位测试
├── "ZK2": { ... }
└── ...
```

> JSON 编码：UTF-8，无 BOM。



***

### 4.1 单孔字段（每个钻孔 object 内）



| 英文字段                  | 类型             | 必填        | 平面 | 剖面 | 柱状 | 备注（落盘中文 key）      |
| --------------------- | -------------- | --------- | -- | -- | -- | ----------------- |
| `hole_no`             | string         | 建议        | ✓  | ✓  | ✓  | 钻孔编号              |
| `x`                   | number         | 平面必填      | ✓  | 间接 | —  | X 坐标 (m)，大地坐标 X   |
| `y`                   | number         | 平面必填      | ✓  | 间接 | —  | Y 坐标 (m)，大地坐标 Y   |
| `elevation`           | number         | **是**     | ✓  | ✓  | ✓  | 孔口高程 (m)，单位米      |
| `depth`               | number         | 平面 / 柱状必填 | ✓  | —  | ✓  | 终孔深度 (m)，单位米      |
| `hole_type`           | string         | 建议        | ✓  | —  | —  | 钻孔类型，取值见 §4.3     |
| `project_name`        | string         | 柱状必填      | —  | —  | ✓  | 工程名称，图签           |
| `company`             | string         | 否         | —  | —  | ✓  | 公司名称，图签落款         |
| `date_start`          | string         | 否         | —  | —  | ✓  | 开孔日期              |
| `date_end`            | string         | 否         | —  | —  | ✓  | 终孔日期              |
| `page_current`        | int            | 否         | —  | —  | ✓  | 当前页，默认 1          |
| `page_total`          | int            | 否         | —  | —  | ✓  | 总页码，默认 1          |
| `water_level_stable`  | number\|string | 否         | —  | —  | ✓  | 稳定水位 (m)，无则传 `""` |
| `water_level_initial` | number\|string | 否         | —  | —  | —  | 初见水位 (m)          |
| `site`                | string         | 否         | —  | —  | —  | 工程地点              |
| `manager`             | string         | 否         | —  | —  | —  | 项目负责人             |
| `recorder`            | string         | 否         | —  | —  | —  | 编录员               |
| `driller`             | string         | 否         | —  | —  | —  | 钻探机长              |
| `layers`              | array          | 剖面 / 柱状必填 | —  | ✓  | ✓  | 地层列表，见 §4.2       |



***

### 4.2 `layers[]` 地层字段



| 英文字段          | 类型     | 必填    | 备注（落盘中文 key）                 |
| ------------- | ------ | ----- | ---------------------------- |
| `layer_id`    | string | **是** | 地层编号，如 `"1-0-0"`             |
| `from`        | number | **是** | 进尺自 (m)，层顶深度                 |
| `to`          | number | **是** | 进尺至 (m)，层底深度                 |
| `name`        | string | **是** | 地层名称，如 `"填土-素填土"`，用于匹配填充图案   |
| `description` | string | 否     | 成分及其他，柱状图描述栏                 |
| `color`       | string | 否     | 颜色                           |
| `state`       | string | 否     | 状态（可塑 / 中风化等）                |
| `density`     | string | 否     | 密实度（松散 / 中密等）                |
| `moisture`    | string | 否     | 湿度                           |
| `sample`      | object | 否     | 取岩 (土、水) 样，见 §4.2.1；无则传 `{}` |
| `test`        | object | 否     | 原位测试，见 §4.2.2；无则传 `{}`       |

#### 4.2.1 `sample` 取样子对象



| 英文字段    | 类型     | 备注                         |
| ------- | ------ | -------------------------- |
| `no`    | string | 编号，如 `"QT-001"`，无则 `""`    |
| `depth` | string | 取样深度，如 `"13-13.3"`，无则 `""` |

#### 4.2.2 `test` 原位测试子对象



| 英文字段         | 类型             | 备注                   |
| ------------ | -------------- | -------------------- |
| `type`       | string         | 类型：`"标贯"` 或 `"重型动探"` |
| `from`       | number\|string | 起 (m)                |
| `to`         | number\|string | 止 (m)                |
| `rod_length` | number\|string | 杆长 (m)               |
| `hits`       | number\|string | 锤击数 (击)              |



***

### 4.3 `hole_type` 可选值

> **注意**
>
> ：这个字段的
>
> **值**
>
> 仍是中文（因为要匹配块符号配置文件），客户端必须传下列取值之一，否则按 "鉴别孔" 画。



| 取值          | 块名      | 取值      | 块名     |
| ----------- | ------- | ------- | ------ |
| `鉴别孔`       | GKJBK   | `抽水试验孔` | GKCSK  |
| `取土试样钻孔`    | GKQTK   | `注水试验孔` | GKZSK  |
| `取水试样钻孔`    | GKQSK   | `压水试验孔` | GKYSK  |
| `取土、取水试样钻孔` | GKQTSK  | `旁压试验孔` | GKPYK  |
| `标准贯入试验孔`   | GKBGK   | `探槽`    | GKTC   |
| `取土标贯钻孔`    | GKQTBGK | `物探点`   | GKWTD  |
| `静力触探试验孔`   | GKJTK   | `小螺旋钻`  | GKxlxz |
| `圆锥动力触探试验孔` | GKDTK   |         |        |



***

### 4.4 `section_params` 剖面图参数

只在 `views` 含 `"section"` 时需要。



| 英文字段                    | 类型        | 必填    | 备注                          |
| ----------------------- | --------- | ----- | --------------------------- |
| `sections`              | array     | **是** | 剖面组，每条 = 一条剖面线              |
| `sections[].h_scale`    | int       | 否     | 水平比例，如 `250` 表示 1:250       |
| `sections[].v_scale`    | int       | 否     | 垂直比例，如 `200`                |
| `sections[].title`      | string    | 建议    | 标题                          |
| `sections[].holes`      | string\[] | **是** | 勘察组，孔号数组，顺序即从左到右            |
| `sections[].zero_elev`  | number    | 否     | 零标高（±0.00 对应绝对标高），无则 `null` |
| `sections[].excav_elev` | number    | 否     | 开挖标高（画基础底设计线），无则 `null`     |



***

### 4.5 完整请求示例



```
curl -X POST http://localhost:7200/api/draw \
  -H "Content-Type: application/json; charset=utf-8" \
  -d '{
    "views": ["plan", "section", "column"],
    "auto_zoom": true,
    "borehole_data": {
      "ZK1": {
        "hole_no": "ZK1",
        "project_name": "阳光幸福里",
        "company": "广西昇龙工程勘察设计检测有限公司",
        "elevation": 206.04,
        "depth": 19.02,
        "hole_type": "鉴别孔",
        "x": 2747354.342,
        "y": 36524941.52,
        "date_start": "2025年09月28日",
        "date_end": "2025年09月28日",
        "page_current": 1,
        "page_total": 1,
        "water_level_stable": "",
        "layers": [
          {
            "layer_id": "1-0-0",
            "from": 0,
            "to": 7.5,
            "name": "填土-素填土",
            "color": "褐黄色",
            "density": "松散",
            "description": "人工填土，主要成份粘性土",
            "sample": { "no": "QT-001", "depth": "13-13.3" },
            "test":   { "type": "标贯", "from": 9.15, "to": 9.25, "rod_length": 10, "hits": 2 }
          },
          {
            "layer_id": "2-0-0",
            "from": 7.5,
            "to": 11.7,
            "name": "黏性土-黏土",
            "color": "褐黄色",
            "state": "可塑",
            "description": "残积黏土",
            "sample": { "no": "", "depth": "" },
            "test":   { "type": "", "from": "", "to": "", "rod_length": "", "hits": "" }
          },
          {
            "layer_id": "4-0-0",
            "from": 11.7,
            "to": 19.02,
            "name": "岩石-泥灰岩",
            "color": "灰黑色",
            "description": "中风化泥灰岩",
            "sample": { "no": "", "depth": "" },
            "test":   { "type": "", "from": "", "to": "", "rod_length": "", "hits": "" }
          }
        ]
      }
    },
    "section_params": {
      "sections": [
        {
          "h_scale": 250,
          "v_scale": 200,
          "title": "1-1#住宅楼剖面",
          "holes": ["ZK1", "ZK2"],
          "zero_elev": 0,
          "excav_elev": null
        }
      ]
    }
  }'
```



***

## 5. 错误处理



| 现象                              | 原因                             | 处理             |
| ------------------------------- | ------------------------------ | -------------- |
| `/health` 503                   | AutoCAD 没开                     | 启动 CAD 并加载 arx |
| `errors` 里 `Connection refused` | CAD 崩了                         | 重启 CAD，重发请求    |
| 平面图上没孔位                         | 缺 `x`/`y` 或 `elevation`        | 检查请求体          |
| 孔位块符号不对                         | `hole_type` 值不在 §4.3           | 改成标准取值         |
| 柱状图无填充花纹                        | `layers[].name` 没在 pat 配置里     | 补映射            |
| 柱状图图签空白                         | 缺 `project_name`/`company`/ 日期 | 补 §4.1 字段      |



***

## 6. 部署注意



1. AutoCAD 必须保持打开，插件是进程内 DLL。

2. 同一时刻只能一个 CAD 实例加载该 arx。

3. 网关通过环境变量 `ARX_DIR` 定位资源，部署时改路径。

4. 网关串行锁已处理并发；客户端超时建议 ≥120s。

5. 当前网关无鉴权，对外暴露需加 Token。



***

## 7. 变更记录



| 日期         | 版本   | 内容                                                 |
| ---------- | ---- | -------------------------------------------------- |
| 2026-10-12 | v1.0 | 初版                                                 |
| 2026-10-12 | v1.1 | 细化中文 borehole\_data 字段表                            |
| 2026-10-12 | v1.2 | **对外字段全部改英文 snake\_case**；网关落盘时自动转中文 key；中文原名移到备注列 |