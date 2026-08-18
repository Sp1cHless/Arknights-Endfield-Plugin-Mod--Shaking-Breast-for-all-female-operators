#pragma once
// diagnostics/diagnostics_manager.h — development diagnostics (spec §19).
// Default OFF; enabled only via config/diagnostics/diagnostics.json or a
// legacy marker.  Never affects the production write path.
#include "../common/logger.h"
#include "../config/json_mini.h"
#include "../il2cpp/il2cpp_api.h"
#include "../runtime/runtime_paths.h"

struct DiagnosticsConfig {
  bool enabled = false;
  bool boneScanner = false;
  bool clipInspector = false;
  bool transformRecorder = false;
  bool axisTester = false;
  bool hookHealth = false;
  float axisTestAngleDeg = 10.0f;
  int axisTestAxis = 2;      // 0=X 1=Y 2=Z
  float axisTestSign = 1.0f;
};

static DiagnosticsConfig g_diagCfg;

static void LoadDiagnosticsConfig() {
  char path[512];
  if (!RuntimePathsInit()) return;
  RuntimePath(path, sizeof(path), "developer\\diagnostics.json");
  jsonmini::Value root;
  if (!jsonmini::LoadJsonFile(path, root)) {
    g_diagCfg = DiagnosticsConfig();  // all off
    return;
  }
  g_diagCfg.enabled = root.GetBool("enabled", false);
  if (const jsonmini::Value *m = root.Find("modules")) {
    g_diagCfg.boneScanner = m->GetBool("bone_scanner", false);
    g_diagCfg.clipInspector = m->GetBool("clip_inspector", false);
    g_diagCfg.transformRecorder = m->GetBool("transform_recorder", false);
    g_diagCfg.axisTester = m->GetBool("axis_tester", false);
    g_diagCfg.hookHealth = m->GetBool("hook_health", false);
  }
  if (const jsonmini::Value *at = root.Find("axis_tester")) {
    g_diagCfg.axisTestAngleDeg = (float)at->GetNumber("test_angle_deg", 10.0f);
    g_diagCfg.axisTestAxis = (int)at->GetNumber("axis", 2);
    g_diagCfg.axisTestSign = (float)at->GetNumber("sign", 1.0f);
  }
  Log("[DIAG] diagnostics=%s scanner=%d clip=%d recorder=%d axis=%d health=%d",
      g_diagCfg.enabled ? "ON" : "OFF", g_diagCfg.boneScanner,
      g_diagCfg.clipInspector, g_diagCfg.transformRecorder,
      g_diagCfg.axisTester, g_diagCfg.hookHealth);
}
