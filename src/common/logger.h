#pragma once
// common/logger.h — two log sinks: plugin/sbm_log.txt (general) and
// plugin/breast_probe_log.txt (secondary-motion).  Never called from hot
// animation callbacks except rate-limited diagnostics.
#include <windows.h>
#include <cstdarg>
#include <cstdio>

static HANDLE g_logHandle = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logLock;

static bool LogInit() {
  InitializeCriticalSection(&g_logLock);
  g_logHandle = CreateFileA("plugin\\sbm_log.txt", GENERIC_WRITE, FILE_SHARE_READ,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  return g_logHandle != INVALID_HANDLE_VALUE;
}

static void Log(const char *fmt, ...) {
  if (g_logHandle == INVALID_HANDLE_VALUE) return;
  EnterCriticalSection(&g_logLock);
  char buf[4096];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
  va_end(args);
  if (len < 0) len = 0;
  buf[len] = '\n';
  len++;
  DWORD written;
  WriteFile(g_logHandle, buf, len, &written, NULL);
  LeaveCriticalSection(&g_logLock);
}

static FILE *g_probeLog = nullptr;

static bool ProbeLogInit() {
  g_probeLog = fopen("plugin/breast_probe_log.txt", "w");
  return g_probeLog != nullptr;
}

// Rate-limit helper for callback diagnostics: returns true at most once per
// `intervalMs` (first call returns true).
static inline bool RateLimit(DWORD &lastMs, DWORD intervalMs) {
  DWORD now = GetTickCount();
  if (now - lastMs >= intervalMs) {
    lastMs = now;
    return true;
  }
  return false;
}

static void ProbeLog(const char *fmt, ...) {
  if (!g_probeLog) return;
  EnterCriticalSection(&g_logLock);
  va_list args;
  va_start(args, fmt);
  vfprintf(g_probeLog, fmt, args);
  va_end(args);
  fflush(g_probeLog);
  LeaveCriticalSection(&g_logLock);
}
