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

## 推送凭据（2026-10-14，待用户首次手动 push 后生效）

- 本机 Windows 凭据管理器**无 Gitee 条目**，`git push` 在非交互环境下报 `could not read Username for 'https://gitee.com'`，主理人无法自动登录。
- 用户选择：**自己手动 push 一次**（在项目目录执行 `git push origin dev_nby`，弹窗登录 Gitee）。成功后凭据由 GCM 缓存，之后主理人可全自动 `push`。
- **凭据缓存前**：每次改动先 `git commit` 到本地；push 交由用户完成首次登录后恢复自动化。
