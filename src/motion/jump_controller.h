#pragma once
// motion/jump_controller.h — landing-only damped settle (baseline simplified
// jump: idle_jump_land_* -> A·exp(-t/tau)·sin(2π·f·t), 1.2s cap).
// V1 modes: off | landing_damped (architecture spec §15).
// Event-driven jump is a V2 item — unsupported states pass through original.
#include <cmath>
#include "../common/time_utils.h"
#include "../config/config_types.h"

struct JumpController {
  bool active = false;
  float t = -1.0f;   // seconds since landing clip start, -1 = inactive
  DWORD lastTick = 0;

  // Call when a landing clip is detected.  Starts the curve from t=0
  // (sin(0)=0 -> smooth attack, no hard step).
  void OnLandingDetected() {
    if (!active) {
      active = true;
      t = 0.0f;
      lastTick = GetTickCount();
    }
  }

  // Call every frame while the character exists.  Returns current jump
  // angle (radians, includes amplitudeScale) or 0 when inactive.
  float Tick(bool landingActive, const JumpConfig &cfg, float ampScale) {
    if (!cfg.enabled || cfg.mode != "landing_damped") {
      active = false;
      t = -1.0f;
      return 0.0f;
    }
    if (landingActive) {
      if (!active) {
        active = true;
        t = 0.0f;
        lastTick = GetTickCount();
      }
    } else {
      active = false;
      t = -1.0f;
      return 0.0f;
    }
    if (!active) return 0.0f;

    DWORD now = GetTickCount();
    float dt = (float)(now - lastTick) / 1000.0f;
    lastTick = now;
    if (dt > 0.05f) dt = 0.05f;
    t += dt;

    float angle = 0.0f;
    if (t <= cfg.maxDurationSec) {
      angle = cfg.amplitudeDeg * expf(-t / cfg.dampingTauSec) *
              sinf(6.2831853f * cfg.frequencyHz * t);
    } else {
      active = false;
      t = -1.0f;
    }
    return DegToRad(angle) * ampScale;
  }

  bool EnabledAndActive() const { return active; }
};
