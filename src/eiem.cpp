#define _CRT_SECURE_NO_WARNINGS
// eiem.cpp — EIEM-compatible plugin entry.  This build contains ONLY the
// secondary-motion tool (architecture spec v1.0); the MMD/GUI features of
// the upstream EIEM are not part of this project.
#include <windows.h>
#include <cstdint>

#include "plugin/plugin_main.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    g_hEiemModule = hModule;  // runtime path derivation source
    PluginStart();
  }
  return TRUE;
}
