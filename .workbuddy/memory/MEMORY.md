---
summary: "IES 项目长期约定（跨会话必读）"
read_when:
  - 每次会话开始
  - 准备提交代码/文档改动前
---

# IES 项目长期约定

## 自动提交版本（用户拍板 2026-10-14）

用户要求：**之后每次改动，由主理人自动提交版本**。约定如下：

- **时机**：每完成一个逻辑单元（一次任务/一批文档修订/一次代码实现）后，立即提交；
- **范围**：**全部相关改动**（文档 + 代码 + 脚本等本次任务涉及的文件，按逻辑单元归入同一 commit）；
- **动作**：`git add` → `git commit` → **`git push` 到 `origin/dev_nby`**（本地提交 + 推送远程，用户明确选择）；
- **排除**：`.workbuddy/`（AI 工作目录，仅含 memory/，已加入 `.gitignore`），不得提交进仓库；
- **commit message**：中文简述本次改动的实质内容（沿用仓库既有风格，如"文档：xxx（规格书V1.2/需求V2.2/…）"）。

## 分支与远程

- 当前分支：`dev_nby`
- 远程：`origin` = https://gitee.com/ks_2026/ies.git

## ARX 写库铁律（2026-10-19 排 320 错误得出，写 CAD 代码必看）

- **文档写锁只在 `ToolDrawBase::actionBefore()` 内部获取**（`acDocManager->lockDocument(pDoc)`），
  `actionEnd()` 解锁。`startTrans()` / `commitTrans()` 只管 ARX 事务，**不负责文档锁**。
- 因此**所有写库操作都必须排在 `actionBefore()` 之后**：
  建层（`getLayerTable(kForWrite)`）、append 实体、改对象等。
  排在它之前 → CAD 返回 `320 = eWasOpenForRead`（这是个误导性错误码，真实原因是缺写锁）。
- **加载/创建线型同样算写库**（2026-10-20 修 `KS_LONGGDASH_DOT` / `swx` 警告得出）：
  `acdbLoadLineTypeFile` 与写线型表也必须排在 `actionBefore()` 之后，否则静默失败，
  表现为实体 `setLinetype` 找不到线型 → 回落 ByLayer。
- 正确范例：`tools/ToolLayer.cpp::create_layer()` —— 先 `actionBefore()`，再 `getLayerTable(kForWrite)`。
- 排查同类问题时的最快路径：**对比同项目里功能正常的同类接口，看它 `actionBefore()` 放在第几行**。

## 构建与运行

- 编译（**必须用 PowerShell 调**，Git Bash 里调 devenv 会静默不干活、产物不更新）：
  `Set-Location D:\project\ies\cad\exploration; & "D:\Program Files (x86)\Microsoft Visual Studio 8\Common7\IDE\devenv.com" exploration.sln /Build "Debug|Win32" /Project exploration /Out <log>`
  （VS2005；产物 `Debug\KsExploration.arx`，编译后核对 .arx 时间戳确认真的重建了）
- CAD 从 `Debug\KsExploration.arx` 加载（`plugin/load_plugin.scr`，注意里面写的是旧机器路径 `D:/project/kt/...`）。
- 端口：网关 `7200`（Python `cad_server/draw_gateway.py`），ARX 侧 `7100`。
- `_CAD2005` 宏决定 `StdAfx.h` 走 `StdAfx2005.h`（含 `acdocman.h` → 有 `AcApDocument`）
  还是 `StdAfx2007.h`（**不含** → 无 `AcApDocument`）。写跨版本代码时避免直接用 `AcApDocument`。

## 平面图坐标系约定（2026-10-20 定稿，改平面图代码必看）

- **映射不写死，由代码自动判定**：`CreatePlanView::Execute` 里预扫描全部孔的原始 (X,Y)，
  分别算「直接对应」和「X/Y 互换」两种映射下**数据包围盒中心与底图包围盒中心的偏移**，取偏移小的。
  底图范围不可用时默认按用户约定互换。日志会打印选了哪种。
- **为什么必须自动判定**：数据源 X/Y 顺序在两轮请求里**不一样**
  （请求 A：数据 X=3652万 / 数据 Y=274万；请求 B：反之）。写死任何一种都会出错。
- 底图实测 X=36524934.955 / Y=2747316.644（高斯含带号，不做去带号/两点配准）。
- 判断同类问题的快速手段：模型空间 X 同时存在 274万 与 3652万 两套量级 → 映射搞错了
  （典型症状：图例跑到 X≈1963 万，即两个量级的中点）。
- 底图由用户手工在 CAD 打开并初始化插件，插件只叠加钻孔（详设里的底图导入/两点配准/出图比例 plan_base 未实现）。

## 地层编号是分孔流水号，跨孔不可比（2026-10-20 修 ZK4/ZK5 连线得出，写跨孔逻辑必看）

- 同一编号在不同孔里可能完全不同岩性：ZK4 的 `3-2-0`=强风化泥岩，ZK5 的 `3-2-0`=泥灰岩。
- 因此**跨孔关联（地层连线/尖灭/合并连续段）一律以「地层名称」为主键**，编号只能当名称缺失时的兜底。
- 另一条硬规则：孔底（末层底）**不能作为分界线的落点**——连线连到孔底就是错图。
- 排查此类问题最快手段：拿 `cad/exploration/7100_request.log` 里最后一条 `create_section_view`
  请求的真实数据，在 Python 里复算 `drawLineDrill` 逻辑（合并→A 侧→B 侧→尖灭），
  比在 CAD 里反复出图快得多。

## 改 GBK 源码必须用 Python，别用 Edit 工具（2026-10-20 踩坑）

- `features/*.cpp` 是 **GBK + CRLF**。Edit 工具大多时候能自动转码，但**写入含 ★/→/（）等字符的
  中文注释时会落成非 GBK 字节** → 编译报 `C4819` + 一串 `C2001: newline in constant`。
- 可靠做法：用 Python `read(bytes).decode('gbk')` → 字符串 replace → `encode('gbk')` 写回，
  换行统一 `\r\n`；写完 `decode('gbk')` 自检一遍。出错先 `git checkout --` 回滚再重做。
- 底图由用户手工在 CAD 打开并初始化插件，插件只叠加钻孔（详设里的底图导入/两点配准/出图比例 plan_base 未实现）。

## 推送凭据（2026-10-14，待用户首次手动 push 后生效）

- 本机 Windows 凭据管理器**无 Gitee 条目**，`git push` 在非交互环境下报 `could not read Username for 'https://gitee.com'`，主理人无法自动登录。
- 用户选择：**自己手动 push 一次**（在项目目录执行 `git push origin dev_nby`，弹窗登录 Gitee）。成功后凭据由 GCM 缓存，之后主理人可全自动 `push`。
- **凭据缓存前**：每次改动先 `git commit` 到本地；push 交由用户完成首次登录后恢复自动化。

## ARX 项目隔离与编号口径（2026-10-21 起生效）
- 数据文件由 **ARX 自己写**（src/AiApiRequest.cpp），不经网关；文件名 {项目名}-钻探编录表.json / {项目名}-剖面图参数.json，路径 GetArxFolder()+project/
- 项目名来源：body 顶层 `projectName`（旧 project_name）/工程名称 优先，否则孔内「工程名称」且要求全孔一致，不一致报错；项目名未定时禁止读写任何项目文件
- **请求体顶层字段一律小驼峰**：`projectName` / `boreholeData` / `sectionParams`；下划线旧名仍可读但打 `[7100] WARN`
- **`boreholeData` 是钻孔对象数组** `[{...}, {...}]`（2026-10-22 定稿，旧的 `{孔号: 对象}` 仍兼容）：
  主键 = **`钻孔编号` 字段**（查询、块名 `孔_<编号>`、柱状图块名全用它）；缺失 → `KsHole_<序号>` + WARN
- 取孔节点统一走 `CUtils::FindHoleNode`（TCHAR）/ `FindHoleNodeUtf8`（char*，剖面组引用是 char*），
  **禁止再写 `cJSON_GetObjectItem(s_pJsonData, 孔号)`**（数组形态下必落空）；
  取主键/块名统一走 `CUtils::GetHoleKey(item, idx)`，禁止用 `item->string`（数组里是 NULL）
- 钻孔符号解析优先级（CUtils::ResolveHoleBlock）：block_define.json 国标 32 条 → **理正习惯叫法别名表** → 兜底 GKJBK + WARN
  理正别名（2026-10-21 用户给定）：技术孔/控制钻孔 → GKkzzk（控制钻孔）；一般钻孔/鉴别孔 → GKybzk（一般钻孔）；
  工程观测孔 → GKgck；普通钻孔 → GKzk。注意 block_define 已有「鉴别孔→GKJBK」且国标表优先，别名表的该项实际不可达
- 平面图与图例必须共用 CUtils::ResolveHoleBlock，禁止各自调用 FindBlockNameByHoleName
- explore/debug 提示：改 GBK 源码必须用 Python 按 GBK 读写；AiApiRequest.cpp 是 UTF-8 BOM + LF，与其它文件不同

## 维护范围（用户 2026-10-22 拍板）
- **只维护 ARX 插件**（`cad/exploration`）+ ARX 直连接口文档（`docs/接口文档1.1.md` 为权威口径）。
- **不再改 7200 网关 `cad_server/draw_gateway.py` 与 `cad_client/`**；网关侧已有的驼峰改动留作历史，新需求一律落 ARX。

## EnsureTextStyle 传参铁律（2026-10-23）
- 第 2 参是 **typeface 字体名**（宋体/仿宋/黑体/Times New Roman），**不是 .ttf 文件名**。传 "simfang.ttf" 会导致 setFont 找不到字体 → 汉字全部渲染 "?"。
- 该函数查到同名样式直接返回**不修复字体**：样式一旦带坏字体存进 dwg，只能换样式名强制重建。
