#pragma once
// runtime/dev_command.h — Developer Mode command channel (V2 §29).
// The Manager writes runtime/developer_command.json (atomic); the service
// worker polls it, validates, and converts it into a plain C++ command
// state.  Hook callbacks only consume the parsed state (no file I/O inside
// hooks).  axis_test commands carry an expiry so a crashed Manager can
// never leave a test pose stuck.
#include <cstring>
#include "../common/logger.h"
#include "../common/time_utils.h"
#include "../config/json_mini.h"
#include "../runtime/runtime_paths.h"

struct DevCommand {
  bool active = false;
  int revision = -1;
  char type[16] = {0};        // none|axis_test|bone_scan|clip_inspect|record
  char characterId[64] = {0};
  int axis = 2;               // 0=X 1=Y 2=Z
  float sign = 1.0f;
  float angleDeg = 5.0f;
  DWORD expiresAt = 0;        // absolute tick (0 = no expiry/manual stop)
  bool recordStart = false;
};

static DevCommand g_devCmd;

static void DevCommandClear() {
  g_devCmd.active = false;
  g_devCmd.revision = -1;
}

// Poll runtime/developer_command.json; called from the service worker only.
static void DevCommandPoll() {
  char path[512];
  if (!RuntimePathsInit()) return;
  RuntimePath(path, sizeof(path), "runtime\\developer_command.json");
  jsonmini::Value root;
  if (!jsonmini::LoadJsonFile(path, root)) {
    DevCommandClear();
    return;
  }
  int rev = (int)root.GetNumber("revision", 0);
  if (rev == g_devCmd.revision) return;  // unchanged since last poll
  g_devCmd.revision = rev;

  std::string cmd = root.GetString("command", "");
  g_devCmd.active = false;
  if (cmd.empty() || cmd == "none") return;

  if (cmd == "axis_test") {
    std::string axisName = root.GetString("axis", "Z");
    g_devCmd.axis = axisName == "X" ? 0 : axisName == "Y" ? 1 : 2;
    double sign = root.GetNumber("sign", 1.0);
    g_devCmd.sign = sign < 0 ? -1.0f : 1.0f;
    g_devCmd.angleDeg = (float)root.GetNumber("angle_deg", 5.0);
    double expiresMs = root.GetNumber("expires_ms", 0);
    g_devCmd.expiresAt = expiresMs > 0 ? GetTickCount() + (DWORD)expiresMs : 0;
    strncpy(g_devCmd.type, "axis_test", sizeof(g_devCmd.type) - 1);
    strncpy(g_devCmd.characterId,
            root.GetString("character_id", "").c_str(),
            sizeof(g_devCmd.characterId) - 1);
    g_devCmd.active = true;
    ProbeLog("[DEVCMD] axis_test axis=%d sign=%+.0f angle=%.1fdeg expires=%lums\n",
             g_devCmd.axis, g_devCmd.sign, g_devCmd.angleDeg,
             (unsigned long)(expiresMs > 0 ? expiresMs : 0));
  } else if (cmd == "bone_scan") {
    strncpy(g_devCmd.type, "bone_scan", sizeof(g_devCmd.type) - 1);
    g_devCmd.active = true;
    ProbeLog("[DEVCMD] bone_scan requested\n");
  } else if (cmd == "clip_inspect") {
    strncpy(g_devCmd.type, "clip_inspect", sizeof(g_devCmd.type) - 1);
    g_devCmd.active = true;
    ProbeLog("[DEVCMD] clip_inspect requested\n");
  } else if (cmd == "record") {
    g_devCmd.recordStart = root.GetBool("start", true);
    strncpy(g_devCmd.type, "record", sizeof(g_devCmd.type) - 1);
    g_devCmd.active = true;
    ProbeLog("[DEVCMD] record start=%d\n", g_devCmd.recordStart ? 1 : 0);
  }
}

// Hook-side: has an axis_test command expired?  (Cheap; called from the
// motion engine tick so a dead Manager can never leave a test pose.)
static inline bool DevCommandAxisTestActive(int &axisOut, float &signOut,
                                            float &angleDegOut) {
  if (!g_devCmd.active || strcmp(g_devCmd.type, "axis_test") != 0)
    return false;
  if (g_devCmd.expiresAt && NowMs() > g_devCmd.expiresAt) {
    DevCommandClear();
    ProbeLog("[DEVCMD] axis_test expired -> cleared\n");
    return false;
  }
  axisOut = g_devCmd.axis;
  signOut = g_devCmd.sign;
  angleDegOut = g_devCmd.angleDeg;
  return true;
}

// Hook-side: consume one-shot scan requests (bone_scan / clip_inspect).
static inline bool DevCommandConsumeOneShot(const char *type) {
  if (!g_devCmd.active || strcmp(g_devCmd.type, type) != 0) return false;
  g_devCmd.active = false;  // one-shot
  return true;
}
