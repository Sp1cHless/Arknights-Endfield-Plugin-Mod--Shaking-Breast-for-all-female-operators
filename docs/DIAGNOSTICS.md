# 诊断工具指南 (DIAGNOSTICS)

版本：Architecture v1.0（2026-08-17）。诊断与普通用户路径完全隔离：
默认关闭时动画回调零额外工作。

## 开关

`plugin/secondary_motion/diagnostics/diagnostics.json`：

```json
{
  "enabled": false,          // 总开关
  "modules": {
    "bone_scanner": false,   // 骨架 dump / 关键字搜索
    "clip_inspector": false, // 当前 clip 详情
    "transform_recorder": false, // CSV 录制胸骨 localRotation
    "axis_tester": false,    // 固定角度轴测试
    "hook_health": false     // hook 调用计数
  },
  "axis_tester": { "test_angle_deg": 10.0, "axis": 2, "sign": 1.0 }
}
```

## 各模块

| 模块 | 输出 | 说明 |
|---|---|---|
| bone_scanner | 日志 `[BONE-SCAN]` | 主控 root 下骨架树，关键字 "breast" |
| clip_inspector | 日志 `[CLIP-INSP]`（3s 限频） | layer0 clips：weight/gait/toIdle/jump/land/name |
| transform_recorder | `plugin/breast_record.csv` | 每帧 localRotation R/L + gait，1800 帧自动停 |
| axis_tester | 游戏内固定角度 3 秒 | 确认轴/符号/杠杆臂；结束后自然恢复 |
| hook_health | 日志 `[HOOK-HEALTH]`（5s） | pre/late/sync 每秒调用数，证明 hook 活着 |

## legacy 开发 marker（不依赖 diagnostics.json）

在游戏 `plugin/` 目录创建（删除即停）：

- `bone_scan_test.txt` → 一次性骨架 dump
- `clip_inspect_test.txt` → clip 详情（3s 限频）
- `record_test.txt` → CSV 录制
- `axis_test.txt` → 3 秒轴测试
- `spring_test.txt` → synthetic 写回门控（legacy_marker_mode=true 时）
- `amplify_test.txt` → amplify_native 门控

所有 marker 由 worker 线程低频检查并缓存（250ms），**动画回调内零文件 I/O**
（基线是每帧 CreateFileA —— 已按规范 §22.2/§23 修正）。

## 日志速查

| 前缀 | 含义 |
|---|---|
| `[REFLECT]` | 反射解析 PASS/FAIL（m_animator、三个 hook 方法） |
| `[HOOK]` | hook 安装结果（PreLateTick REQUIRED，Late/Sync OPTIONAL） |
| `[CFG]` | 配置加载/校验 |
| `[CHAR]` | 角色识别/profile 查找（FOUND/DISABLED/UNSUPPORTED） |
| `[BONE]` | 骨骼发现与轴/缩放 |
| `[GAIT]` | 20Hz clip 采样分类 |
| `[PRE]` | 振荡器输出（2s 限频） |
| `[SYNC-CALL]` | SyncCalc hook 触发证明 |
| `[MARKER]` | marker 缓存状态（1s） |
| `[PLUGIN]` | 启动状态机（READY/DEGRADED/DISABLED_SAFE） |

## runtime_status.json（生效证明）

`plugin/secondary_motion/runtime_status.json`，1s 更新：

- `state`: READY / DEGRADED / DISABLED_SAFE
- `symbols.*`: 反射解析结果
- `hooks.*`: 三个 hook 安装状态
- `active_character` / `character_profile_found` / `bones_found` / `motion_mode`
- `config_hash`: 当前 preset 内容哈希（换 preset 会变 —— 生效证明）

部署后先看这个文件：state=READY + symbols 全 true + hooks.pre_late_tick
=installed，再进游戏。
