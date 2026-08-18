# Phase 0 — 成功基线冻结 (2026-08-17)

## 基线身份

- **active = `c73660fb`** = 部署中 `bin/sbm.dll` 的 SHA-256 前 8 位
- 源码位置：`E:\ShakerMOD\breast-probe\`（git commit `851533b` + 未提交改动，与基线部署一致）
- 备份目录：`E:\ShakerMOD\breast-probe-backup-20260816-5461fe6a\`

## Hash 记录（部署时点）

| 文件 | SHA-256 |
|---|---|
| src/breast_probe.h | 260e21972068ecdc876b664ab1c55c97c74da7aca518aab0d3924db71bb4ac65 |
| src/sbm.cpp | 598a8c83b6b223324dcf796ae07300929fe631fe31bc3910367a8a56a2834755 |
| src/animation.h | 9864869a5b43b7a490d61bad2b274a12ba05e607687a27e6c624e0765585ec95 |
| src/globals.h | df9470b92e37f4541b8a859859f26594f0e1cca6ba99a0455e7a8d74ae678bba |
| src/il2cpp_api.h | 40497a53e80586bb81219fa9ff8dcb5de678e22259e9dbda170117b1bd44891e |
| bin/sbm.dll | c73660fb305966f0e805b5ef8e99cc3f041c20f5a281ee56c7f9d6b7758fb5dc |

## 基线行为摘要（必须保持）

1. Hook 链：`AnimatorMono.PreLateTick`（REQUIRED）→ orig → 主控过滤采样 + 无条件完整 read/compute/write；`NPCCPUAnimator.LateTick` + `ScriptAnimationJobSyncMono.CalcLayerMainStream`（OPTIONAL）→ 幂等重放，150ms 验证门控。
2. 唯一计算点：`target = native × dq(angle)`；其他写点只 SetLocalRotation(target)。
3. 四人补偿：帧内调用计数 ≥3 → 幅度 ×0.25（平滑 0.1/帧）。
4. gait：clip 名分类（jump→2 优先，_to_ 按目标，start/stop，主循环），20Hz 采样，weight 最大。
5. 切人：500ms 刷新玩家控制链 → animator 变化 → 清缓存重找骨骼；候选表 6 组；xiong→Y/0.4，其余 Z/1.0。
6. 包络：amp tau 0.15s / to-idle 0.015s / freq tau 0.20s；相位连续积分。
7. jump（land 段）：`10°·exp(-t/0.3)·sin(2π·1.5·t)`，1.2s 归零。
8. Mode 1 amplify：base Slerp(0.05) + `d=inv(base)*q; out=base*d^K`，K=2。
9. marker 驱动：`spring_test.txt`(Mode2) / `amplify_test.txt`(Mode1) 存在即启用。
10. 禁用即"停止自定义 SetLocalRotation"，游戏自然恢复。

## 部署链

- 构建：`run_build.bat` → `bin/sbm.dll`（+ d3dcompiler_47.dll / vulkan-1.dll proxy）
- 部署：游戏退出 → 备份 `plugin/sbm.dll.pre_*` → 覆盖 → 启动游戏
- 日志：`plugin/breast_probe_log.txt`（[GAIT]/[PRE]/[JUMP]/[SWITCH]/[SYNC-CALL]/[V19]/[MARKER] 等）
