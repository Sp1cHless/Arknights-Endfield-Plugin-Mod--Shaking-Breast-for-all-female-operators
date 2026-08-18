#pragma once
// diagnostics/axis_tester.h — dev-mode fixed-angle write to confirm the
// correct axis/sign/lever arm for a character (spec §19.2).  Diagnostics
// ONLY: writes a fixed offset for a short burst, then restores by letting
// the game animate naturally (no persistent state, never in a preset).
//
// V2: parameters come from EITHER the diagnostics config (legacy markers /
// diagnostics.json) OR the live developer_command.json channel (Manager
// Developer page).  Command-driven tests carry an expiry so a crashed
// Manager can never leave a pose stuck.
#include "../character/active_character.h"
#include "../common/logger.h"
#include "../common/safe_unity.h"
#include "../common/time_utils.h"
#include "../config/config_types.h"
#include "../runtime/dev_command.h"
#include "diagnostics_manager.h"

struct AxisTester {
  bool armed = false;
  DWORD armTimeMs = 0;
  DWORD durationMs = 3000;   // burst length (config-driven fallback)
  DWORD cmdExpiresAt = 0;    // absolute expiry when driven by dev command

  void Arm() {
    armed = true;
    armTimeMs = NowMs();
    cmdExpiresAt = g_devCmd.expiresAt;  // command-driven expiry (0 = config burst)
    ProbeLog("[AXIS-TEST] armed: axis=%d sign=%+.0f angle=%.1fdeg for %lums\n",
             g_diagCfg.axisTestAxis, g_diagCfg.axisTestSign,
             g_diagCfg.axisTestAngleDeg, durationMs);
  }

  // Called from the motion engine's PreLateTick path while armed.
  void Tick(const ActiveCharacterRuntime &c) {
    if (!armed) return;

    // Command-driven test: check its own expiry first.
    int axis = g_diagCfg.axisTestAxis;
    float sign = g_diagCfg.axisTestSign;
    float angleDeg = g_diagCfg.axisTestAngleDeg;
    bool fromCmd = DevCommandAxisTestActive(axis, sign, angleDeg);
    if (fromCmd && cmdExpiresAt && NowMs() > cmdExpiresAt) {
      armed = false;
      cmdExpiresAt = 0;
      ProbeLog("[AXIS-TEST] done (expired, restored by natural animation)\n");
      return;
    }
    if (!fromCmd && NowMs() - armTimeMs > durationMs) {
      armed = false;
      cmdExpiresAt = 0;
      ProbeLog("[AXIS-TEST] done (restored by natural animation)\n");
      return;
    }

    if (!c.bones.breastR || !c.bones.breastL) return;
    float angle = DegToRad(angleDeg) * sign;
    Quat dq = QuatAxisAngle(axis, angle);
    __try {
      Quat r = SafeGetLocalRotation(c.bones.breastR);
      Quat l = SafeGetLocalRotation(c.bones.breastL);
      SafeSetLocalRotation(c.bones.breastR, QuatMul(r, dq));
      SafeSetLocalRotation(c.bones.breastL, QuatMul(l, dq));
    } __except (1) {
    }
  }
};
