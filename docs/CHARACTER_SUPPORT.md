# 角色支持指南 (CHARACTER_SUPPORT)

版本：Architecture v1.0（2026-08-17）

## 已确认骨型家族（基线实测）

| 家族 | 骨名 | 角色系 | 轴 | 缩放 | 候选表序 |
|---|---|---|---|---|---|
| girl | `breast_R/L_01_jnt` | 伊冯等 | Z | 1.0 | 0 |
| lady | `R/L_breast_01_jnt` | 奥罗拉等 | Z | 1.0 | 1 |
| xiong | `xiong_R/L_0_skin_jnt` | 庄方宜/莱万汀等 | Y | 0.4 | 2 |
| 变体 | `breast_R/L_01` | — | Z | 1.0 | 3 |
| 变体 | `R/L_breast_01` | — | Z | 1.0 | 4 |
| 变体 | `xiong_R/L_0_skin` | — | Y | 0.4 | 5 |

- xiong 是末端皮肤骨（杠杆臂大）→ 同角度视觉幅度大 → 0.4 缩放。
- xiong 的 X 轴=乳尖外指（像拧螺丝，错误），Z=左右晃（错误），Y=前后摆（正确）。

## 已知角色（default.json 已配置）

| 角色 id | 名字 | 家族 | 轴/缩放 |
|---|---|---|---|
| `chr_0014_aurora` | 奥罗拉 | lady (R_breast_01_jnt) | Z / 1.0 |
| `chr_0017_yvonne` | 伊冯 | girl (breast_R_01_jnt) | Z / 1.0 |
| `chr_0003_endminf` | 安多恩/endminf | xiong | Y / 0.4 |
| `chr_0030_zhuangfy` | 庄方宜 | xiong | Y / 0.4 |
| `chr_0016_laevat` | 莱万汀 | xiong | Y / 0.4 |

单人测试建议：**奥罗拉**（lady 系，基线多次验证）。

## 角色识别（重要修正）

游戏运行时角色的 GameObject 名带 `_postmodel` 后缀：
`chr_0003_endminf_postmodel` → canonical id `chr_0003_endminf`。

- 插件自动剥离 `_postmodel` / `(Clone)` / `#数字` 后缀再查 preset。
- 日志 `[CHAR] GO="..." id="..."` 同时打印原始名与规范化 id。

## 新角色更新流程（你自用的方法）

不用等任何人，一次游戏会话即可：

1. **采集真实 id**：进游戏切一圈想加的角色（队伍切换/替换），
   `plugin/secondary_motion/known_characters.json` 自动记录所有出现过的
   原始 GO 名（2s 刷新，去重）。
2. **确认骨骼**：创建 `plugin/bone_scan_test.txt` → 日志
   `[BONE-SCAN]` 输出当前角色骨架树（含 breast 关键字行）。确认胸骨名
   属于哪个家族。
3. **试轴**：创建 `plugin/axis_test.txt` → 3 秒固定角度测试
   （`diagnostics.json` 的 `axis_tester` 可改轴/角度/符号）。看晃动方向：
   - 前后摆 = 轴正确（再验 sign）
   - 左右晃 = 换轴；拧转 = 轴错
4. **加 preset**：复制 `default.json` 里 `_template_character` 块，把键名
   改成真实 id，`axis`/`amplitude_scale` 填实测值（或按家族默认：
   girl/lady→Z/1.0，xiong→Y/0.4）。
5. **重启验证**：日志应有 `[CHAR] id=.. profile=FOUND` + `[BONE] FOUND`，
   runtime_status.json 的 active_character 显示该 id。

预设更新 = 改 JSON + 重启游戏，无热重载。每次新角色多复制一个块即可。

## 已知注意事项

- 同骨名在不同角色上轴/杠杆臂可能仍有差异 → 按角色独立配置是硬要求。
- `spring_base_*_jnt` 是衣服弹簧骨骼，不是胸骨（勿误配）。
- 未配置角色 = fail-closed（原生），不会乱晃，但也不会生效 —— 这是设计。
- 四人队：V1 保持 `legacy_four_stack`（×0.25）；精确修复是 V2 研究项。
