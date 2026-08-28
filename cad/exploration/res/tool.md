# CAD 辅助功能插件功能说明文档

## 1. 项目概述

本项目是一个基于 **AutoCAD ARX (ObjectARX)** 的二次开发插件，能在AUTOCAD2007低版本下运行。插件的核心目标是：

1. **AI Tools 对接**：将 CAD 操作封装为结构化的 JSON 命令接口（基于 cJSON），供 AI 智能体 / 外部程序调用，实现 AI 驱动的 CAD 自动化。
2. **定制业务功能**：提供行业定制功能（如 `CLineHolePlacer` 排孔等），扩展原生 CAD 能力。
3. **给勘探结论生成提供铺垫**：让图纸中的勘探成果（孔位布置、勘探线等）成为 AI 可读、可复用的结构化数据，为后续自动生成勘探结论 / 报告打底。铺垫体现在三个层面：

   | 层面 | 依托能力 | 对结论生成的意义 |
   |------|----------|------------------|
   | **图纸数据结构化** | 查询类 Tools（`list_entities` / `entity_get` / `get_drawing_units`）+ 图层管理（12 个） | 将孔位圆、勘探线、标注等图元按图层语义提取为 JSON，作为 LLM 的输入上下文，替代"截图 + 人工描述" |
   | **业务语义规整** | 定制排孔三件套（`CLineHolePlacer` 直线排孔、`CPolylineMarker` 顶点标记、`CCircleFilter` 圆过滤） | 保证图纸按统一规则绘制（统一孔径来自 `profile.hole_diameter`、统一红色孔圆标记、按图层归类），使提取的数据语义稳定、可机器判读 |
   | **AI 会话链路** | 面板内 `AiChat` 流式对话（`Chat` / `ChatRetry` / `ReplyContinue`）+ tools.json Function Calling | 模型既可"读数"（查询工具结果作上下文），也可"看图"（`get_screenshot` 视口截图），会话管理已就绪，可直接在其上追加结论生成的提示词模板与输出回填 |

   **目标链路（待建设）：**

   ```
   绘制/规整图纸（排孔工具）→ 结构化提取（查询类 Tools）
       → AI 上下文组装（图层语义 + 视口截图）
       → AiChat / LLM → 勘探结论初稿 → 回填面板供人工确认
   ```

## 2. 整体架构

### 2.1 命令调度模式

所有工具类均遵循统一的调度约定：

- **单例模式**：如 `ToolView::getInstance()`，工具类全局唯一。
- **基类继承**：工具类继承自 `ToolDrawBase` 等基类，复用生命周期管理。
- **生命周期钩子**：
  - `actionBefore()`：执行前置检查，非 0 则终止。
  - `actionEnd()`：执行收尾清理。
- **统一数据结构 `AiToolCommandData`**：

| 字段 | 说明 |
|------|------|
| `jsonRoot` | 入参 JSON（cJSON 解析后的根节点） |
| `resultJson` | 出参 JSON（返回给调用方的结果） |
| `ret` | 返回码，`0` 表示成功，`1` 表示失败 |

### 2.2 调用流程

```
外部调用方 → JSON 命令 → 工具分发 → 具体工具类方法
    → 解析 jsonRoot 参数 → 执行 CAD 操作 → 组装 resultJson → 返回 ret
```

### 2.3 tools.json 格式说明

`res/tools.json` 采用 **OpenAI Function Calling** 标准格式（`type: "function"`），每个工具包含 `name`（工具名，与代码方法一一对应）、`description`（AI 可读的功能描述）和 `parameters`（JSON Schema 参数定义），可直接注入 AI 模型的 tools 列表。

## 3. AI Tools 功能清单（tools.json）

共 **62 个工具**，按功能分为八大类。

### 3.1 绘图类（15 个）

创建基本图形实体，均返回新实体的 `entityID`（句柄）。

| 工具名 | 功能 | 主要参数 | 备注 |
|--------|------|----------|------|
| `create_line` | 画直线 | `x1,y1,x2,y2`，可选 `width`（线宽，单位 0.01mm） | 线宽默认 ByLayer |
| `create_circle` | 画圆 | `x,y,radius` | 半径必须为正 |
| `create_arc` | 画圆弧 | `x,y,radius,start_angle,end_angle`（弧度） | — |
| `create_ellipse` | 画椭圆 | `x,y,major_x,major_y,ratio` | 主轴矢量定义半径，ratio∈(0,1] |
| `create_rectangle` | 画矩形 | `x1,y1,width,height`，可选 `line_width` | 生成闭合多段线 |
| `create_polyline` | 画多段线 | `points[]`（≥2 点），可选 `closed,width` | — |
| `create_spline` | 画样条曲线 | `points[]`（≥3 点，拟合点方式） | — |
| `create_hatch` | 创建填充 | `points[]`（闭合边界），可选 `pattern`（默认 SOLID）、`scale` | 支持 ANSI31 等图案 |
| `create_text` | 创建单行文本 | `x,y,text`，可选 `height`（默认 1.0）、`rotation`（弧度） | — |
| `create_mtext` | 创建多行文本 | `x,y,text`，可选 `height`、`width`（默认 100） | — |
| `create_leader` | 创建引线标注 | `points[]`（≥2 点），可选 `text` | — |
| `create_dimension_linear` | 线性（旋转）标注 | `x1,y1,x2,y2,dim_x,dim_y`，可选 `rotation` | — |
| `create_dimension_aligned` | 对齐标注 | `x1,y1,x2,y2,dim_x,dim_y` | 标注线平行于两定义点连线 |
| `create_dimension_angular` | 三点角度标注 | `center_x,center_y,x1,y1,x2,y2`，可选 `arc_x,arc_y` | — |
| `create_dimension_radius` | 半径标注 | `center_x,center_y,radius,angle`，可选 `leader_length` | — |

### 3.2 实体编辑类（11 个）

对已有实体（按 handle 句柄定位）做几何变换与修改。

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `entity_move` | 移动实体 | `handle,dx,dy` |
| `entity_copy` | 复制实体 | `handle,dx,dy` |
| `entity_rotate` | 旋转实体 | `handle,x,y,angle`（弧度，绕基点） |
| `entity_scale` | 缩放实体 | `handle,x,y,scale` |
| `entity_mirror` | 镜像实体 | `handle,x1,y1,x2,y2`（镜像轴），可选 `delete_original` |
| `entity_offset` | 偏移曲线 | `handle,distance`（正负决定方向） |
| `entity_fillet` | 圆角 | `handle1,handle2,radius` |
| `entity_chamfer` | 倒角 | `handle1,handle2,dist1,dist2` |
| `entity_array` | 矩形阵列 | `handle,num_rows,num_cols,row_spacing,col_spacing` |
| `entity_set_color` | 设置颜色 | `handles[]`（批量）或 `handle`，`color`（ACI 0-256） |
| `delete_entities` | 删除实体 | `handles[]` |

### 3.3 查询类（5 个）

| 工具名 | 功能 | 主要参数 / 返回 |
|--------|------|----------|
| `list_entities` | 列出模型空间一级实体（含块内实体） | 可选 `handle`（块引用句柄）→ 返回 handle/type/color/layer 数组 |
| `entity_get` | 获取实体详细属性 | `handle` → 按类型返回几何属性（见下方说明） |
| `get_selected_entity` | 获取当前选中集 | 无 → JSON 数组 |
| `set_selected_entity` | 设置选中集（高亮夹点） | `handles[]` |
| `get_drawing_units` | 获取图形单位信息 | 无 → insunits/measurement/lunits/luprec 等 |

**`entity_get` 按类型返回的属性：**

| 类型 | 返回属性 |
|------|----------|
| Line | start_point, end_point |
| Polyline | points, closed |
| Circle | center, radius |
| Arc | center, radius, start_angle, end_angle |
| Ellipse | center, major_axis, ratio |
| Spline | control_points, degree |
| Text / MText | position, text, height, rotation / width |
| BlockReference | block_name, position, scale_x, scale_y, rotation |

### 3.4 图层管理类（12 个）

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `create_layer` | 创建图层（存在则跳过） | `name` |
| `list_layers` | 列出所有图层名 | 无 |
| `query_layer` | 查询指定图层（含颜色索引） | `name` |
| `query_all_layers` | 查询所有图层详情（颜色/可见/冻结/锁定） | 无 |
| `get_current_layer` | 获取当前图层名 | 无 |
| `set_current_layer` | 设置当前图层 | `name` |
| `get_current_layer_color` | 获取当前图层颜色索引 | 无 |
| `layer_set_properties` | 设置图层属性 | `name`，可选 `color/linetype/lineweight` |
| `layer_freeze` / `layer_thaw` | 冻结 / 解冻图层 | `name` |
| `layer_lock` / `layer_unlock` | 锁定 / 解锁图层 | `name` |

### 3.5 块操作类（6 个）

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `block_list` | 列出所有块定义（名称/句柄/标志） | 无 |
| `block_define` | 定义新块，可复制现有实体入块 | `name`，可选 `base_x,base_y,entities[]` |
| `block_insert` | 插入块引用 | `name,x,y`，可选 `scale`（默认 1.0）、`rotation` |
| `block_insert_with_attributes` | 插入块并填充属性值 | 同上 + `attributes`（键值对） |
| `block_get_attributes` | 读取块引用属性值 | `handle` |
| `block_update_attributes` | 更新块引用属性值 | `handle,attributes` |

### 3.6 样式管理类（10 个）

**文字样式（5 个）：**

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `add_text_style` | 创建文字样式 | `name`，可选 `font/height/width_factor/oblique_angle/upside_down/backward` |
| `modify_text_style` | 修改文字样式 | 同上 |
| `delete_text_style` | 删除文字样式（Standard 不可删） | `name` |
| `list_text_styles` | 列出所有文字样式 | 无 |
| `get_text_styles` | 按名称批量查询属性 | `names[]` |

**标注样式（5 个）：**

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `add_dim_style` | 创建标注样式 | `name`，可选 `arrow_size/text_height/dim_scale/decimal_places/text_color/dim_line_color/extension_line_offset/extension_line_extend/text_gap/text_style` |
| `modify_dim_style` | 修改标注样式 | 同上 |
| `delete_dim_style` | 删除标注样式（Standard 不可删） | `name` |
| `list_dim_styles` | 列出所有标注样式 | 无 |
| `get_dim_styles` | 按名称批量查询属性 | `names[]` |

### 3.7 视图控制类（2 个）

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `zoom_extents` | 缩放至图形范围（ZOOM E） | 无 |
| `zoom_window` | 缩放至指定矩形窗口 | `x1,y1,x2,y2` |

**实现细节（ToolView.cpp）：**
- 通过 `acDocManager->sendStringToExecute()` 向当前文档发送 AutoCAD 命令字符串实现。
- 代码中另实现有 `get_screenshot`（截取视口图像，GDI BitBlt + Base64 编码，返回 BMP 数据），**但未在 tools.json 中注册**，如需暴露给 AI 请补充声明。

### 3.8 系统命令类（1 个）

| 工具名 | 功能 | 主要参数 |
|--------|------|----------|
| `dispatch_command` | 直接执行任意 AutoCAD 命令 | `cmd`（如 `_u`、`_r`），可选 `args[]`（字符串/数字） |

> ⚠️ `dispatch_command` 为兜底通道，可执行任何 CAD 命令，AI 调用时需注意命令合法性校验。

## 4. 定制业务功能

> 定制功能由 HTML 面板（`CDataPalette`）触发，经 `HandleJsCommand(szCommand, szParam)` 分发（见 `src/CPalette.cpp`），目前**不在** `tools.json` 的 AI 工具清单内。业务方向为**排孔加工**：按面板配置的孔径，在图元上自动生成孔位标记。

### 4.1 公共配置：孔径参数（SetProfile / SetDiameter）

- 面板 `SetProfile` 命令将 JSON 参数解析后合并进配置根节点的 `profile` 对象，并持久化（`CMyPaletteSet::SavePaletteSet`）。
- 关键字段 `profile.hole_diameter`（字符串）。`CDataPalette::SetDiameter(CBaseMarker&)` 读取该值注入各标记器，实现"一次配置、多处生效"。

### 4.2 CLineHolePlacer（直线排孔）

- **入口**：面板命令 `PreSelectLine`。
- **流程**：`CUtils::FocusAcadDrawing()` 聚焦图形区 → `SetDiameter()` 注入孔径 → `Execute()` 进入交互流程。
- **功能**：选取直线后，沿线路径按设定孔径自动布置孔位。
- ⚠️ 类内部实现（孔间距、偏移方式等参数）以 `LineHolePlacer.h/.cpp` 为准。

### 4.3 CPolylineMarker（多段线/块顶点排孔标记）

- **入口**：面板命令 `PreSelectBlock`。
- **类结构**：继承 `CBaseMarker`，主入口 `ExecuteSelectionAndMark()`。
- **流程**：
  1. `ExecuteSelectionAndMark()` 提示用户选择对象（支持块引用）；
  2. `ProcessPolylineVertices(pPline, xform)` 遍历多段线顶点，`xform` 矩阵支持块引用坐标系变换（块内实体顶点可正确映射到模型空间）；
  3. `DrawRedCircle(center)` 在每个顶点绘制红色圆作为孔位标记，孔径取自 `profile.hole_diameter`。

### 4.4 CCircleFilter（圆过滤）

- **入口**：面板命令 `PreSelectCircle`。
- **功能**：选择过滤，只保留**当前图层**上的 Circle 实体（用于从杂图中快速筛出本层孔圆）。

### 4.5 CPreSelectScale（预选缩放 / 图纸缩放）

- **入口**：面板命令 `PreSelectScale`（区域缩放 `Execute()`）、`PreSelectScaleWhole`（整图纸张转换 `ExecuteModelScale()`）。
- **流程**（`Execute()`）：
  1. `selectRegion()`：框选区域并计算边界（`m_minPt`/`m_maxPt`）；
  2. `promptPaperSize()`：由 `determineSmartPaperSize(width, height)` 智能匹配最合适的标准纸张并提示确认；
  3. 缩放二选一：
     - 克隆式：`cloneAndScaleObjects()` 生成新对象 → `deleteOldObjects()` 删除旧对象；
     - 原地式：`scaleObjectsDirectly()` 直接缩放原对象。
- **用途**：将选中图形（或整图）按标准纸张归一化比例缩放，便于打印/下料。

### 4.6 CLineJig（交互式画线拖拽）

- 自定义 `AcEdJig`，模拟原生 LINE 命令的交互与橡皮筋预览。
- **状态机**：`m_nStep`（0=等待第一点，1=等待下一点），支持连续取点与闭合（`m_bClosed`）。
- **接口**：`getStartPoint()` / `getEndPoint()` 供调用方取结果；`appendLineToDatabase()` 将确认线段写入模型空间。

### 4.7 AI 对话集成（AiChat / AiApiRequest）

- **入口命令**：`Chat`（新建会话并发送）、`ChatNew`（销毁会话）、`ChatRetry` / `ReplyContinue`（继续生成）。
- **链路**：HTML 面板 → `AiChat::Execute()` → `AiApiRequest`（`OnCreate` 时启动的本地请求服务）→ 流式回调。
- **回传**：经 `WM_USER_CHAT` / `WM_USER_CHAT_SEND_ERROR` 消息回到 `CDataPalette`，再通过 `CallJsFunction` 调用页面回调：`onReply` / `onReplyContinue` / `onReplyStop` / `onChatError`。


## 5. 目录结构（部分）

```
exploration/
├── tools/
│   ├── ToolView.cpp/.h      # 视图控制工具
│   ├── ToolDrawBase.*       # 工具基类
│   └── ...
├── res/
│   ├── tools.json           # AI Tools 能力清单（62 个工具）
│   └── tool.md              # 本文档
├── CLineHolePlacer.*        # 定制排孔功能
├── utils.*                  # 公用工具函数
└── cJSON.*                  # JSON 解析库
```

## 6. 已知问题与建议

1. **工具清单与代码不同步**：`ToolView::get_screenshot` 已实现但未注册到 tools.json，建议补充；同时建议建立"代码方法 ↔ tools.json 条目"的对照检查机制。
2. `get_screenshot` 返回原始 24 位 BMP 的 Base64，体积较大，建议改为 PNG 编码（GDI+）以减小传输量。
3. `zoom_window` 等方法直接解引用 `cJSON_GetObjectItem` 返回值，参数缺失时会空指针崩溃，建议统一增加判空校验。
4. `entity_set_color` 同时支持 `handle` 和 `handles` 两种传参，建议在 description 中明确二选一的优先级（当前推荐批量 `handles`）。

## 7. 下一步建议
对接实际业务人员需求

