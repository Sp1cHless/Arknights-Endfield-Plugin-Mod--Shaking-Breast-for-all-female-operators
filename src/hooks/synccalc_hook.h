#pragma once
// hooks/synccalc_hook.h — ScriptAnimationJobSyncMono.CalcLayerMainStream hook
// (OPTIONAL reinforcement; production path = resolved method pointer, no
// RVA — spec §16.3).  Baseline: orig first, then idempotent replay.
#include "../common/logger.h"
#include "../diagnostics/hook_health.h"
#include "../il2cpp/il2cpp_api.h"
#include "../motion/motion_engine.h"
#include "prelate_hook.h"  // GetEngineForPreLate

typedef void(__fastcall *SyncCalcFn)(void *self, float deltaTime);
static SyncCalcFn g_origSyncCalc = nullptr;

static void __fastcall Hooked_SyncCalc(void *self, float deltaTime) {
  HookHealthCount(2);
  if (g_origSyncCalc) g_origSyncCalc(self, deltaTime);
  MotionEngine *eng = GetEngineForPreLate();
  if (!eng) return;
  static int s_syncCalls = 0;
  s_syncCalls++;
  if ((s_syncCalls % 300) == 1)
    ProbeLog("[SYNC-CALL] calls=%d\n", s_syncCalls);
  __try {
    eng->ReplayTarget();
  } __except (1) {
  }
}

static bool InstallSyncCalcHook(void *fn, MotionEngine &engine) {
  if (g_origSyncCalc) return true;
  MH_STATUS st = MH_OK;
  __try {
    st = MH_CreateHook(fn, (void *)Hooked_SyncCalc, (void **)&g_origSyncCalc);
  } __except (1) {
    Log("[HOOK] SyncCalc MH_CreateHook exception");
    return false;
  }
  if (st != MH_OK || !g_origSyncCalc) {
    Log("[HOOK] SyncCalc MH_CreateHook failed: %d", st);
    return false;
  }
  __try {
    st = MH_EnableHook(fn);
  } __except (1) {
    Log("[HOOK] SyncCalc MH_EnableHook exception");
    return false;
  }
  if (st != MH_OK) {
    Log("[HOOK] SyncCalc MH_EnableHook failed: %d", st);
    return false;
  }
  Log("[HOOK] CalcLayerMainStream hooked at %p", fn);
  return true;
}
