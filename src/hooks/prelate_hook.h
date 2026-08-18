#pragma once
// hooks/prelate_hook.h — AnimatorMono.PreLateTick hook (REQUIRED).
// Baseline contract: call orig FIRST, then the motion engine.  No managed
// allocation, no file I/O, no JSON — everything SEH-wrapped.
#include "../common/logger.h"
#include "../diagnostics/hook_health.h"
#include "../il2cpp/il2cpp_api.h"
#include "../motion/motion_engine.h"

// Engine pointer + resolved field offset (set by plugin init).
static MotionEngine *g_engineForPreLate = nullptr;
static int g_animatorFieldOffset = -1;  // AnimatorMono.m_animator (reflection)

static inline MotionEngine *GetEngineForPreLate() {
  return g_engineForPreLate;
}

typedef void(__fastcall *PreLateTickFn)(void *self, float deltaTime,
                                        void *methodInfo);
static PreLateTickFn g_origPreLateTick = nullptr;

static void __fastcall Hooked_PreLateTick(void *self, float deltaTime,
                                          void *methodInfo) {
  HookHealthCount(0);
  if (!g_origPreLateTick) return;  // fail closed: never run without orig
  g_origPreLateTick(self, deltaTime, methodInfo);

  MotionEngine *eng = GetEngineForPreLate();
  if (!eng) return;
  // Baseline: read the animator from the resolved field (reflection offset;
  // falls back to the historical 0x70 only when resolution failed and the
  // plugin is DEGRADED).
  void *animator = nullptr;
  if (g_animatorFieldOffset >= 0) {
    __try {
      animator = *(void **)((char *)self + g_animatorFieldOffset);
    } __except (1) {
      animator = nullptr;
    }
  }
  eng->OnPreLateTick(animator);
}

static bool InstallPreLateTickHook(void *fn, MotionEngine &engine) {
  if (g_origPreLateTick) return true;
  g_engineForPreLate = &engine;
  MH_STATUS st = MH_OK;
  __try {
    st = MH_CreateHook(fn, (void *)Hooked_PreLateTick,
                       (void **)&g_origPreLateTick);
  } __except (1) {
    Log("[HOOK] PreLateTick MH_CreateHook exception");
    return false;
  }
  if (st != MH_OK || !g_origPreLateTick) {
    Log("[HOOK] PreLateTick MH_CreateHook failed: %d trampoline=%p", st,
        (void *)g_origPreLateTick);
    return false;
  }
  __try {
    st = MH_EnableHook(fn);
  } __except (1) {
    Log("[HOOK] PreLateTick MH_EnableHook exception");
    return false;
  }
  if (st != MH_OK) {
    Log("[HOOK] PreLateTick MH_EnableHook failed: %d", st);
    return false;
  }
  Log("[HOOK] AnimatorMono.PreLateTick hooked at %p", fn);
  return true;
}
