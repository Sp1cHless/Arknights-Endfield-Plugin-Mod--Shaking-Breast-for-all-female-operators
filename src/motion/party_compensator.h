#pragma once
// motion/party_compensator.h — callback-stack amplitude compensation.
// V2 formal name: LegacyCallbackStackCompensation (§11) — compensates
// multiple Animator callbacks repeatedly applying the same target to the
// main character's bones (NOT "party physics").
//
// V2 formula (§12):
//   factor = 1 / (1 + alpha * (effectiveStackCount - 1))
//   default alpha = 1.0  ->  N=1:1.0  N=2:0.5  N=3:0.333  N=4:0.25
// effectiveStackCount keeps the V1 verified detection (per-frame callback
// count grouped by Unity frameCount).  We do NOT assume it equals party
// size unless measured.
#include <cmath>
#include "../config/config_types.h"

class IPartyCompensator {
public:
  virtual ~IPartyCompensator() {}
  virtual float GetFactor(const PartyCompensationConfig &cfg,
                          int stackCountThisFrame) = 0;
};

class LegacyCallbackStackCompensation : public IPartyCompensator {
public:
  LegacyCallbackStackCompensation() {}

  // stackCountThisFrame from the MotionEngine frame counter (Unity
  // frameCount-grouped, baseline g_sqCalls semantics).  cfg is the CURRENT
  // snapshot's compensation config (hot-reload safe).
  //
  // V2 fix (scene artifacts): compensation applies ONLY to real parties
  // (4+ animator callbacks in the frame).  N=2/3 is common for a solo
  // player + scene NPCs and previously halved the amplitude, making the
  // same preset feel different between scenes.  Solo gameplay must be
  // scene-independent: N<=3 -> factor 1.0.  N>=4 keeps the V1-equivalent
  // 4-man compression (N=4 -> 0.25).
  float GetFactor(const PartyCompensationConfig &cfg,
                  int stackCountThisFrame) override {
    if (!cfg.enabled) return 1.0f;
    if (stackCountThisFrame <= 3) {
      EaseTo(1.0f, cfg);
      return level_;
    }
    float n = (float)stackCountThisFrame;
    float target = 1.0f / (1.0f + cfg.alpha * (n - 1.0f));
    if (target < 0.05f) target = 0.05f;  // sanity floor
    EaseTo(target, cfg);
    return level_;
  }

  void Reset() { level_ = 1.0f; }

private:
  void EaseTo(float target, const PartyCompensationConfig &cfg) {
    // per-frame exponential smoothing (V1 easing preserved)
    float k = 1.0f - expf(-(1.0f / 60.0f) / cfg.transitionTauSec);
    if (k > 1.0f) k = 1.0f;
    level_ += (target - level_) * k;
  }

  float level_ = 1.0f;  // smoothed factor
};
