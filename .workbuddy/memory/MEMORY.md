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

## 推送凭据（2026-10-14，待用户首次手动 push 后生效）

- 本机 Windows 凭据管理器**无 Gitee 条目**，`git push` 在非交互环境下报 `could not read Username for 'https://gitee.com'`，主理人无法自动登录。
- 用户选择：**自己手动 push 一次**（在项目目录执行 `git push origin dev_nby`，弹窗登录 Gitee）。成功后凭据由 GCM 缓存，之后主理人可全自动 `push`。
- **凭据缓存前**：每次改动先 `git commit` 到本地；push 交由用户完成首次登录后恢复自动化。
