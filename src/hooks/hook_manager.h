#pragma once
// hooks/hook_manager.h — MinHook install orchestration + health (spec §17).
//   REQUIRED:  AnimatorMono.PreLateTick  (install failure -> DISABLED_SAFE)
//   OPTIONAL:  NPCCPUAnimator.LateTick, CalcLayerMainStream (failure ->
//              DEGRADED, PreLateTick continues)
#include "../common/logger.h"
#include "../il2cpp/metadata_resolver.h"
#include "latetick_hook.h"
#include "prelate_hook.h"
#include "synccalc_hook.h"

struct HookHealth {
  bool preLateTickInstalled = false;
  bool lateTickInstalled = false;
  bool syncCalcInstalled = false;
};

class HookManager {
public:
  // Returns true when the REQUIRED PreLateTick hook is live.
  bool InstallAll(const RuntimeSymbols &s, MotionEngine &engine) {
    health_ = HookHealth();
    if (!s.preLateTickPass || !s.preLateTickFn) {
      Log("[HOOK] PreLateTick unavailable -> DISABLED_SAFE");
      return false;
    }
    if (!InstallPreLateTickHook(s.preLateTickFn, engine)) {
      Log("[HOOK] PreLateTick install failed -> DISABLED_SAFE");
      return false;
    }
    health_.preLateTickInstalled = true;
    Log("[HOOK] PreLateTick PASS");

    // Optional reinforcements
    if (s.lateTickPass && s.lateTickFn) {
      if (InstallLateTickHook(s.lateTickFn, engine)) {
        health_.lateTickInstalled = true;
        Log("[HOOK] LateTick PASS");
      } else {
        Log("[HOOK] LateTick OPTIONAL_FAIL (continuing)");
      }
    } else {
      Log("[HOOK] LateTick OPTIONAL_FAIL (symbol unresolved)");
    }
    if (s.syncCalcPass && s.syncCalcFn) {
      if (InstallSyncCalcHook(s.syncCalcFn, engine)) {
        health_.syncCalcInstalled = true;
        Log("[HOOK] SyncCalc PASS");
      } else {
        Log("[HOOK] SyncCalc OPTIONAL_FAIL (continuing)");
      }
    } else {
      Log("[HOOK] SyncCalc OPTIONAL_FAIL (symbol unresolved)");
    }
    return true;
  }

  const HookHealth &Health() const { return health_; }

private:
  HookHealth health_;
};
