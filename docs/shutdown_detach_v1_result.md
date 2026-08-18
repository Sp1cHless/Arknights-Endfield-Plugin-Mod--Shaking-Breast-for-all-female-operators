# shutdown_detach_v1_result.md

**日期**：2026-08-17  
**实验**：IL2CPP Worker 生命周期单点实验（H1 验证）

## 版本 / hash

| 项 | 值 |
|---|---|
| source: plugin_main.h | 5443d68f732d5fbc |
| source: il2cpp_api.h | ebb776d30ecec0e3 |
| built DLL | 1e20f673f610c1948e8e250b681d6d9de6e36a0082a1e52675967e2e01aa57dd |
| deployed DLL | 1e20f673…（与 built 一致） |
| 上一版备份 | plugin/eiem.dll.shutdown_detach_v1 |

## 实验内容

单点改动：PluginWorker 在启动全部完成后执行 `il2cpp_thread_detach(attachedThread)`，worker 继续原 while(true) 服务循环。其余（Hook/Motion/Config/Log/DllMain）零改动。

前置静态审计：worker 服务循环仅 Win32/CRT/纯逻辑（marker 检查、status JSON、known-characters flush、diagnostics arm、Sleep），无任何 il2cpp_*/Unity API 调用 → 允许 detach。

## 日志证据

```
[SHUTDOWN-DIAG] il2cpp_thread_detach resolved PASS
[SHUTDOWN-DIAG] worker attached ptr=0x...
[SHUTDOWN-DIAG] startup complete ok=1
[SHUTDOWN-DIAG] worker IL2CPP DETACHED
[SHUTDOWN-DIAG] worker entering service loop detached
```

## 测试结果

- 功能回归：主控 synthetic motion 正常（walk/run/sprint 符合预期，无回归）
- 退出测试：游戏中 → 回登录界面 → 点击 Exit，正常退出，不再卡死

## Verdict

**PASS — H1 STRONGLY SUPPORTED**

永久 attached 的 PluginWorker 是游戏退出卡死的根因：
游戏点击 Exit 后进入 Unity/IL2CPP runtime teardown，worker 线程仍注册为
attached IL2CPP thread 且不退出，teardown 等待/枚举该线程 → 进程卡死。
detach 后 worker 退化为普通 Win32 线程，IL2CPP teardown 不再依赖它，
退出路径恢复畅通。

## 对 V2 的结论

1. 生命周期模式正式化：`attach → 初始化 → detach → 纯服务循环`。
2. worker 线程禁止持有 IL2CPP context；任何需要 IL2CPP 的周期性工作
   要么放主线程 hook，要么按需 attach/detach（需验证）。
3. 遗留项（不影响进程退出，V2 再议）：DLL_PROCESS_DETACH 无清理
   （hook 不卸载 / 日志不关 / 锁不销毁）。若未来支持游戏内卸载 DLL，
   必须先 MH_DisableHook + worker 退出后再 FreeLibrary，顺序不能反。
