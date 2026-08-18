#pragma once
// motion/locomotion_envelope.h — amplitude/frequency envelope with phase
// integration (baseline review #11 semantics, user-confirmed "效果非常好"):
//   desiredAngle(t) = ampEnv(t) * sin(phase(t))
//   amp env: exponential approach, tau from profile (0.15s / 0.015s to-idle)
//   freq env: exponential approach, tau 0.20s
//   phase:   phase += 2π * freqEnv * dt; fmod 2π  — NEVER reset on gait change
#include <cmath>
#include "../common/time_utils.h"
#include "../config/config_types.h"

struct LocomotionEnvelope {
  float ampEnv = 0.0f;
  float downEnv = 0.0f;   // down-amplitude channel (asymmetric gait)
  float freqEnv = 1.5f;
  double phase = 0.0;
  FrameGate gate;  // frame-once advance (multi-instance safe)

  // Advance the envelope.  Returns true when a frame actually advanced.
  bool Advance(float ampTarget, float downTarget, float freqTarget,
               bool toIdle, float attackTauSec, float toIdleTauSec,
               float freqTauSec) {
    float dt = gate.Tick();
    if (dt <= 0.0) return false;
    float ampTau = toIdle ? toIdleTauSec : attackTauSec;
    float kA = 1.0f - expf(-dt / ampTau);
    float kF = 1.0f - expf(-dt / freqTauSec);
    ampEnv += (ampTarget - ampEnv) * kA;
    downEnv += (downTarget - downEnv) * kA;
    freqEnv += (freqTarget - freqEnv) * kF;
    phase += 2.0 * 3.14159265358979 * freqEnv * dt;
    phase = fmod(phase, 2.0 * 3.14159265358979);
    return true;
  }

  float Amplitude() const { return ampEnv; }
  float DownAmplitude() const { return downEnv; }
  float Phase() const { return (float)phase; }

  void Reset() {
    ampEnv = 0.0f;
    downEnv = 0.0f;
    freqEnv = 1.5f;
    phase = 0.0;
    gate = FrameGate();
  }
};
