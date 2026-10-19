# IES Round 3 回归测试报告

- 测试者：严过关（QA）
- 测试对象：IES（CAD 勘察出图 ARX 插件 + Python 网关）P0 修复
- 被测版本：HEAD `0db5b34`（3 个 P0 提交：ARX `a5f8523` / 网关 `4b9156d` / 文档 `0db5b34`）
- 测试时间：2026-10-14
- 策略：ARX 静态代码审计（无 ObjectARX SDK，不编译）；网关**真实起服务 + requests 打 HTTP**；文档版本/内容核验
- **不修代码、不 push**（仅验证）

---

## 测试概要

| 端 | 用例数 | 通过 | 失败 | 备注 |
|---|---|---|---|---|
| ARX（静态审计） | 6 | 6 | 0 | 1 项 MINOR 观察（死代码函数体） |
| 网关（真起服 + HTTP） | 14 | 14 | 0 | 另 SSRF 单元 9/9 通过 |
| 网关（SSRF 单元） | 9 | 9 | 0 | |
| 文档（版本 + 内容） | 8 | 7 | 1 | 见缺陷 D1（接口文档残留旧措辞） |
| **合计** | **37** | **36** | **1** | |

**整体判定：PASS（核心代码全过）** —— ARX 与网关端 0 失败；文档端 1 处**残留矛盾措辞**待回修（非源码 Bug）。

---

## 一、ARX 端（静态代码审计）

| # | 验证项 | 期望 | 实际 | 结论 |
|---|--------|------|------|------|
| A1 | `ImportDwgAsBlock` 6 参重载（定义+实现） | 存在 | `CreateBaseView.h:26-31` 声明 + `CreateBaseView.cpp:61` 实现；`CreatePlanView.cpp:1015` 以 6 实参调用 | ✅ PASS |
| A2 | BASEPOINT 前修正甲方案 `ptInsert' = ptInsert + s·R(θ)·B` | 已应用 | `CreatePlanView.cpp:1002-1012`：`dx=s·(cosθ·Bx−sinθ·By)`、`dy=s·(sinθ·Bx+cosθ·By)`，`ptInsert.x/y += d` | ✅ PASS |
| A3 | `ApplyPlotScale` 残留 BASEPOINT 特判分支 | 无 | 仅统一矩阵 `m=scaling(k,O)`，无 `mImport` 复合分支（`mImport` 仅出现在注释 L1138） | ✅ PASS |
| A4 | `CUtils::FileExists` 残留 | 0 处 | 全库 0 处；`PathFileExists` 在 `CreatePlanView.cpp:900` 恰 1 处（`CreateColumnView.cpp:111-112` 为既有先例） | ✅ PASS |
| A5 | 新增平面图方法标识符齐全 | 齐全 | 9 个均定义：`DrawBaseMap/ImportBaseMap/ApplyPlotScale/EraseBaseMapLayers/DimBaseMapReference/BuildImportMatrix/GetJsonNumber/ParseRegistration/ComputeRegistration` | ✅ PASS |
| A6 | 图例字符串去"及层数" | 0 出现 | 全 `cad/` 0 处"及层数" | ✅ PASS |
| A7 | 相邻模块未受污染 | 未改动 | ARX 提交仅改 `CreatePlanView.cpp/.h`；`CreateSectionView`、`CreateColumnView` 未改 | ✅ PASS |

**MINOR 观察（不阻断）**：`BuildImportMatrix()` 函数体仍返回旧矩阵 `mT*mR*mS*mTp*mTn`（`CreatePlanView.cpp:881`），与详设 V1.7 展示的 `T(s·R(θ)·B)`（详设 L521）不一致——但 `grep` 全树确认该函数**无任何调用点**（作基类兼容接口保留的死代码），对出图行为零影响。建议后续勘误或加 `[[maybe_unused]]`/注释说明，非本轮阻断项。

---

## 二、网关端（真实起服务 + requests）

**环境**：`cad_server/draw_gateway.py`（Flask 3.1.3）子进程真实启动于 `127.0.0.1:7200`；mock CAD 于 `127.0.0.1:7100`；`ARX_DIR` 指向隔离目录；`requests` 打真实 HTTP。

| # | 验证项 | 期望 | 实际 | 结论 |
|---|--------|------|------|------|
| G1 | `POST /api/draw/column` | 200 + 必要字段 | 200；`status=success`，`results.column` 存在 | ✅ PASS |
| G2 | `POST /api/draw/section` | 200 | 200；`status=success`，`results.section` 存在 | ✅ PASS |
| G3 | `POST /api/draw/plan` 无底图 | 200 + `degraded_to` 字段 | 200；`degraded_to="process1_no_basemap"`，四字段齐全 | ✅ PASS |
| G4 | 旧 `POST /api/draw` 仍可用 | 200 | 200；`status=success`，`results{plan,section,column}` | ✅ PASS |
| G5 | D1：`/api/draw/plan` JSON body 含 `base_map_url`/`base_map` → 400 | 400 | 400（两键分别测均 400，文案正确） | ✅ PASS |
| G6 | SSRF：`base_map_url=http://127.0.0.1/x.dwg` 被拒 | 拒绝 | 200 + `warnings=["底图未被采用：base_map_url 校验未通过（详见网关日志）"]` + `degraded_to=process1_no_basemap` | ✅ PASS |
| G7 | `base_map_url` 非 `.dwg` → 400 | 400 | 400 | ✅ PASS |
| G8 | `base_map` 与 `base_map_url` 同传 → 400 | 400 | 400 | ✅ PASS |
| G9 | multipart 上传 `.dwg` → 流程二 | 200 + `process2_with_basemap` | 200；`degraded_to=process2_with_basemap` | ✅ PASS |
| G10 | `borehole_data` 非法 JSON → 400 | 400 | 400 | ✅ PASS |
| G11 | `/health` | 200 | 200 `{gateway:ok, cad:true}` | ✅ PASS |
| G12 | 流程一不残留 `plan_base.json` | 不存在 | 不存在（`_remove_if_exists` 生效） | ✅ PASS |

**SSRF 单元（直接 import 模块）**：`_is_loopback('127.0.0.1'/'localhost')`→True、`_ssrf_check('127.0.0.1')`→False、`_ssrf_check('8.8.8.8')`→True、非 http(s) 协议→ValueError、白名单生效时公网被拒/白名单命中放行/回环被拒 —— **9/9 PASS**。

**静态核对（真实起服不可覆盖的点）**：
- `_save_base_map()` 调用在 `_lock` **外**：调用点 `L648`，`with _lock:` 在 `L669` —— ✅ 确认锁外。
- `degraded_to` 4 字段（`status/results/warnings/degraded_to`）在**成功响应（L706-711）、504（L684-689）、500（L691-696）三处**均齐全 —— ✅ 确认。

**NOTE（非缺陷，记录口径差异）**：
1. **D1 作用域**：D1 方案 A 按蓝图 §3.4 仅落在 `draw_plan()`（`/api/draw/plan`，L620-632）。任务备忘写的"`POST /api/draw` 含 base_map_url → 400"与之不符——实测旧 `/api/draw` 对 `base_map_url` **返 200 并忽略**（该接口按 D3"原样保留"、从无底图参数）。**结论：D1 实现与蓝图一致；旧接口不校验属设计预期**。
2. **SSRF 函数命名**：任务备忘列 `_is_safe_host`/`_is_private_ip`，实际为 `_is_loopback`/`_ssrf_whitelist_check`/`_ssrf_check`/`_http_get_with_redirects`——功能等价（默认禁回环、`--ssrf-whitelist-cidr` 白名单、逐跳校验证）。且按架构师 P0-8 表态**不做内网黑名单**（`127.0.0.1` 由 loopback 判定拦截，非 private-ip 判定）——符合设计。

---

## 三、文档端

| # | 验证项 | 期望 | 实际 | 结论 |
|---|--------|------|------|------|
| D-1 | 需求文档版本 | V2.4 | V2.4 | ✅ PASS |
| D-2 | 需求规格说明书版本 | V1.3 | V1.3 | ✅ PASS |
| D-3 | 接口文档版本 | V1.1 | V1.1 | ✅ PASS |
| D-4 | 概要设计版本 | V0.8 | V0.8 | ✅ PASS |
| D-5 | 详细设计版本 | V1.7 | V1.7 | ✅ PASS |
| D-6 | 4 份评审抬头"实施状态（2026-10-14）" | 全部存在 | 设计评审 / 对于设计评审的建 / 第二轮设计评审 / 需规与概设-第二轮评审汇总 均存在（含 P0-1~P0-3/P0-4 收口表注） | ✅ PASS |
| D-7 | 接口文档 §7.2 D1 措辞明确"JSON body 即 400" | 明确 | L362 明确"JSON body 含底图键 → HTTP 400（…仅支持 multipart/form-data 提交）" | ✅ PASS |
| D-8 | 接口文档 §11.2 排查表删"自承漏洞"行 | 已删/已改 | §11.2（L892）旧"静默忽略/无任何反应"行已删，改为 D1 400 行（L902）；§12 变更记录 L926 明示删除 | ✅ PASS |
| D-9 | 详设 §5.3.1 BASEPOINT 改为甲方案，未保留旧矩阵字面实现 | 已改 | L479 `ptInsert' = ptInsert + s·R(θ)·B`；L486-504 新代码块为"前修正"；旧 V1.6 无效公式仅作勘误说明（L472） | ✅ PASS |
| D-10 | 详设 §12 含本次 P0 + D6 勘误条目 | 含 | 顶部 V1.7 修订说明含 D6 甲方案勘误（L37）+ P0 条目（L21-26/44/47）；§12 遗留项含 P1-4"V1.7 已勘误"行（L1337）+ P0-1 行 | ✅ PASS |
| D-11 | 概设 V0.8 同步 | 已同步 | §3.4/§5.3 BASEPOINT（L35/384/398）、§7.2.5 新增 D1 决策行（L566）、§11 P1-4 标"V0.8 已同步勘误"（L653） | ✅ PASS |
| **D-12** | **接口文档残留旧措辞一致性** | **与 D1 无矛盾** | **L604（§7.11.3）、L832（§10.4）仍写"用 application/json 提交该字段会被静默忽略（并按流程一继续出图）"——与 D1「JSON body 含底图键 → 400」直接矛盾** | **❌ FAIL（缺陷 D1）** |

### 缺陷 D1（文档，MEDIUM）
- **位置**：`docs/CAD勘察出图插件-接口文档.md` L604（§7.11.3 流程二 URL 底图）与 L832（§10.4 形态 B URL 底图）
- **现象**：两处仍保留**修复前旧措辞**："`base_map_url` 只从 `request.form` 读取…用 `application/json` 提交该字段**会被静默忽略**并按流程一继续出图"。
- **冲突**：与同文档 §7.2 L362 / §8.2 第 5 行 L648 / §11.2 L902（均声明 JSON body 含底图键 → **HTTP 400**）自相矛盾；P0 修复正是"消灭静默忽略路径"，此两处属**未同步清理的旧口径**。
- **影响**：文档内部不一致，可能误导对接方（以为 JSON 提交是"静默走流程一"而非"400 报错"）。
- **建议**：将 L604 / L832 改为"用 `application/json` 提交底图键将返回 **HTTP 400**（`base_map_url requires multipart/form-data`），必须改用 `multipart/form-data`"。属文档回修，非源码 Bug。

---

## 智能路由判定

- [ ] Engineer（源码 Bug）→ **无**：ARX 与网关源码全部通过，无源码缺陷。
- [ ] QA-self（测试代码 Bug）→ **无**：首轮 2 处失败（G6 漏传必填字段、G12 状态污染）均为测试脚本自身问题，已自修并复跑通过。
- [x] NoOne（核心全 PASS，无源码缺陷）**+ 1 项文档待修（缺陷 D1）** → 请主理人分派**文档回修**（接口文档 L604/L832 旧措辞与 D1 对齐）。

**结论：核心 P0 修复（ARX 代码 + 网关代码）回归 100% 通过；仅文档侧 1 处残留矛盾措辞需回修。**
