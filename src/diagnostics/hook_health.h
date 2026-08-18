#pragma once
// diagnostics/hook_health.h — per-hook call counters (spec §19.5).
// Diagnostic only; never part of scheduling decisions.
#include "../common/logger.h"

struct HookHealthCounters {
  volatile LONG preCalls = 0;
  volatile LONG lateCalls = 0;
  volatile LONG syncCalls = 0;
  DWORD lastLogMs = 0;
};

static HookHealthCounters g_hookCounters;

// Call from each hook callback (cheap InterlockedIncrement).
static inline void HookHealthCount(int kind) {
  if (kind == 0) InterlockedIncrement(&g_hookCounters.preCalls);
  else if (kind == 1) InterlockedIncrement(&g_hookCounters.lateCalls);
  else InterlockedIncrement(&g_hookCounters.syncCalls);
}

// Rate-limited summary (worker thread).
static void HookHealthLog() {
  if (RateLimit(g_hookCounters.lastLogMs, 5000)) {
    ProbeLog("[HOOK-HEALTH] pre=%ld late=%ld sync=%ld per-5s\n",
             g_hookCounters.preCalls, g_hookCounters.lateCalls,
             g_hookCounters.syncCalls);
    InterlockedExchange(&g_hookCounters.preCalls, 0);
    InterlockedExchange(&g_hookCounters.lateCalls, 0);
    InterlockedExchange(&g_hookCounters.syncCalls, 0);
  }
}
