# Endfield Secondary Motion Tool v1

终末地胸部二次运动（Secondary Motion）增强工具。架构规范 v1.0（2026-08-17）
的实现，基线为已验证的 `c73660fb` 行为。

## 构建

```
run_build.bat        （或 build.bat，自动找 MSVC）
输出：bin/eiem.dll + bin/d3dcompiler_47.dll + bin/vulkan-1.dll
```

## 部署（游戏必须退出）

```
deploy.bat [tag]     （自动备份当前 active DLL 为 eiem.dll.<tag>，默认 pre_v1）
```

或手动：复制 `bin/*` 到游戏目录（`plugin/eiem.dll`、根目录两个 proxy），
把 `config/` 内容复制到 `plugin/secondary_motion/`。

## 生效验证（部署后、进游戏前）

看 `plugin/secondary_motion/runtime_status.json`：

- `state` = READY（或 DEGRADED = 可选 hook 缺失，主功能仍工作）
- `symbols.*` 全 true
- `hooks.pre_late_tick` = installed
- 进游戏后 `active_character` 显示角色、`bones_found` = true

日志：`plugin/eiem_log.txt`（启动/反射/hook）+ `plugin/breast_probe_log.txt`
（运行期）。

## 使用

- 配置：`plugin/secondary_motion/presets/*.json`（完整快照，重启生效）
- 基线兼容：`legacy_marker_mode=true` 时 `spring_test.txt` / `amplify_test.txt`
  marker 门控与旧版一致（worker 低频缓存，非每帧 I/O）
- 角色配置/新角色接入/诊断工具：见 `docs/`

## 文档

- `docs/BASELINE.md` — 冻结基线（hash + 行为摘要）
- `docs/CONFIG_SCHEMA.md` — 配置 schema
- `docs/CHARACTER_SUPPORT.md` — 角色支持与接入流程
- `docs/DIAGNOSTICS.md` — 诊断工具

## V1 边界（不做）

- 无热重载（重启生效）、无 GUI
- 无 preset 继承/merge
- 四人队精确修复、event-driven jump、party-count 自动解析、state-hash
  gait 均为 V2
- 不含上游 EIEM 的 MMD/GUI 功能（本项目只做 secondary motion）
