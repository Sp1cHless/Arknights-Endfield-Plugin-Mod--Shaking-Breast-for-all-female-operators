# Secondary Motion 工具 — 配置指南 (CONFIG_SCHEMA)

版本：Architecture v1.0（2026-08-17）。所有配置**重启生效**（V1 无热重载）。

## 文件布局

```
plugin/secondary_motion/
├─ settings.json          （选 preset + 总开关）
├─ runtime_status.json    （插件自写：生效状态证明）
├─ presets/
│  ├─ default.json        （完整快照，基线值）
│  └─ user_example.json   （示例）
└─ diagnostics/
   └─ diagnostics.json    （开发诊断开关，默认全关）
```

## settings.json

| 字段 | 取值 | 说明 |
|---|---|---|
| `schema_version` | 1 | 不支持其他版本 → 插件 DISABLED_SAFE |
| `enabled` | true/false | false = 插件完全关闭（原生） |
| `active_preset` | 预设名 | 对应 `presets/<名>.json` |
| `config_reload` | `restart_only` | V1 仅支持重启 |

## preset 结构（完整快照，禁止 merge/继承）

### global

```json
"global": {
  "party_compensation": {
    "enabled": true,
    "strategy": "legacy_four_stack",   // V1 唯一策略（基线 workaround）
    "four_member_factor": 0.25,        // 四人队幅度补偿（已验证）
    "transition_tau_sec": 0.15         // 进出小队状态的平滑时间常数
  },
  "gait_sample_interval_ms": 50,       // 20Hz 采样
  "entity_refresh_interval_ms": 500,   // 主控 animator 刷新
  "replay_verify_window_ms": 150,      // 幂等重放验证窗口
  "legacy_marker_mode": true           // true=保留 spring/amplify marker 门控（基线行为）
}
```

### characters.<id>

```json
"chr_0017_yvonne": {
  "enabled": true,                      // false = 完全原生
  "motion_mode": "synthetic",           // off | synthetic | amplify_native（互斥）
  "bones": {
    "right": "breast_R_01_jnt",         // 显式骨名（空=用候选表）
    "left":  "breast_L_01_jnt",
    "allow_fallback_candidates": true   // 显式找不到时是否回退候选表
  },
  "axis": { "name": "Z", "sign": 1.0 }, // X/Y/Z；sign ±1（轴方向试错）
  "amplitude_scale": 1.0,               // 整体幅度缩放（xiong 族建议 0.4）
  "gait": {
    "idle":   { "amplitude_deg": 0.0,  "frequency_hz": 1.2 },
    "walk":   { "amplitude_deg": 3.6,  "frequency_hz": 1.5 },
    "run":    { "amplitude_deg": 8.5,  "frequency_hz": 1.7 },
    "sprint": { "amplitude_deg": 12.0, "frequency_hz": 2.0 }
  },
  "envelope": {
    "amplitude_attack_tau_sec": 0.15,   // 幅度进入
    "frequency_tau_sec": 0.20,          // 频率平滑
    "to_idle_release_tau_sec": 0.015    // 停步快速释放（~0.05s 归零）
  },
  "jump": {
    "enabled": false,                   // 产品默认建议 off
    "mode": "landing_damped",           // off | landing_damped
    "amplitude_deg": 10.0,
    "damping_tau_sec": 0.3,
    "frequency_hz": 1.5,
    "max_duration_sec": 1.2
  },
  "native_amplify": { "factor": 2.0 }   // amplify_native 模式的放大倍数 K
}
```

注意：
- `amplitude_scale` 与骨型家族默认缩放（xiong=0.4）**相乘**。
- `bones.right/left` 显式指定时**优先**于候选表。
- jump 的 `landing_damped` 就是基线简化版（仅 land clip，阻尼衰减）。
- default.json 中 jump 保留 `enabled=true + landing_damped` 以与基线行为一致；产品建议 `false`。

### unknown_character

```json
"unknown_character": { "enabled": false }
```
V1 固定 fail-closed：未在 `characters` 中配置的角色 → 原生动画，不写骨骼。

## 单位

角度一律 **degree**，频率 **Hz**，时间 **second**。运行时自动转换，用户不填 rad。

## 校验规则（启动时）

schema_version、preset 名、骨名对、轴 X/Y/Z、sign ±1、浮点有限、freq>0、tau>0、
amplitude_scale>=0、amplify factor>0、jump mode 已知、motion mode 已知。
**单个角色校验失败 → 该角色禁用 + 日志，其他角色继续，插件不崩。**

## 行为开关：legacy_marker_mode

- `true`（默认，= 基线）：`spring_test.txt` 存在才执行 synthetic 写回；
  `amplify_test.txt` 存在才执行 amplify_native。与旧版完全一致。
- `false`：完全由配置决定（enabled 角色即生效）。marker 仍可在 worker
  低频检测日志中看到，但不再门控。

marker 目录：`E:/GAMU/Hypergryph Launcher/games/Endfield Game/plugin/`
（与基线相同；诊断 marker：bone_scan_test.txt / clip_inspect_test.txt /
axis_test.txt / record_test.txt）。
