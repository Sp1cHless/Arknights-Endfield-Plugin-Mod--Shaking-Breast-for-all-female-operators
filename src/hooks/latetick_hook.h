#pragma once
// hooks/latetick_hook.h — NPCCPUAnimator.LateTick hook (OPTIONAL
// reinforcement).  Baseline: orig first, then amplify_native (Mode 1) and
// idempotent replay of the synthetic target.
#include "../common/logger.h"
#include "../diagnostics/hook_health.h"
#include "../il2cpp/il2cpp_api.h"
#include "../motion/motion_engine.h"
#include "prelate_hook.h"  // GetEngineForPreLate

typedef void(__fastcall *LateTickFn)(void *self, float deltaTime);
static LateTickFn g_origLateTick = nullptr;

static void __fastcall Hooked_LateTick(void *self, float deltaTime) {
  HookHealthCount(1);
  if (g_origLateTick) g_origLateTick(self, deltaTime);
  MotionEngine *eng = GetEngineForPreLate();
  if (!eng) return;
  __try {
    eng->OnLateTick();      // amplify_native mode (baseline Mode 1 timing)
    eng->ReplayTarget();    // synthetic idempotent replay (baseline)
  } __except (1) {
  }
}

static bool InstallLateTickHook(void *fn, MotionEngine &engine) {
  if (g_origLateTick) return true;
  MH_STATUS st = MH_OK;
  __try {
    st = MH_CreateHook(fn, (void *)Hooked_LateTick, (void **)&g_origLateTick);
  } __except (1) {
    Log("[HOOK] LateTick MH_CreateHook exception");
    return false;
  }
  if (st != MH_OK || !g_origLateTick) {
    Log("[HOOK] LateTick MH_CreateHook failed: %d", st);
    return false;
  }
  __try {
    st = MH_EnableHook(fn);
  } __except (1) {
    Log("[HOOK] LateTick MH_EnableHook exception");
    return false;
  }
  if (st != MH_OK) {
    Log("[HOOK] LateTick MH_EnableHook failed: %d", st);
    return false;
  }
  Log("[HOOK] NPCCPUAnimator.LateTick hooked at %p", fn);
  return true;
}
