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
- 正确范例：`tools/ToolLayer.cpp::create_layer()` —— 先 `actionBefore()`，再 `getLayerTable(kForWrite)`。
- 排查同类问题时的最快路径：**对比同项目里功能正常的同类接口，看它 `actionBefore()` 放在第几行**。

## 构建与运行

- 编译：`cd D:\project\ies\cad\exploration` 后
  `"D:\Program Files (x86)\Microsoft Visual Studio 8\Common7\IDE\devenv.com" exploration.sln /Build "Debug|Win32" /Project exploration`
  （VS2005，需 `--%` 或完整路径调用；产物 `Debug\KsExploration.arx`）
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
