#pragma once
// motion/synthetic_motion.h — synthetic gait oscillation computation
// (baseline Mode 2 / ProbeSpringAdvance math, review #11 semantics):
//   angle = ampEnv(t) * sin(phase(t))          [or jump curve]
//   target = currentNative × dq(axis, angle)
// The caller (MotionEngine) owns read/write timing; this module is pure math
// + envelope state so the "unconditional full compute per callback" rule
// stays intact without duplicating logic.
#include "../character/active_character.h"
#include "../common/logger.h"
#include "../config/config_types.h"
#include "jump_controller.h"
#include "locomotion_envelope.h"

struct SyntheticMotion {
  LocomotionEnvelope envelope;
  JumpController jump;

  // Compute the output angle (radians, axis-sign applied) for this callback.
  // Advances envelope at most once per frame (multi-instance safe).
  // `squadFactor` is applied to the AMPLITUDE TARGET before the envelope
  // (baseline: ampTarget *= factor, then envelope advances) — NOT to the
  // output angle.
  float ComputeAngle(ActiveCharacterRuntime &c, const CharacterProfile &p,
                     float squadFactor) {
    int gait = c.currentGait;
    // baseline kGaitAmp is RADIANS; config stores degrees -> convert here
    bool validGait = gait >= GaitIdle && gait <= GaitZipline;
    float ampTarget = validGait ? DegToRad(GaitAmplitude(p, gait)) : 0.0f;
    float downTarget = validGait ? DegToRad(GaitDownAmplitude(p, gait)) : 0.0f;
    float freqTarget = validGait ? GaitFrequency(p, gait) : 1.5f;

    // Jump animations (any clip containing "jump", including landings) are
    // classified as Run and would write large run amplitudes during the
    // jump.  When the jump feature is OFF, keep jump clips fully native:
    // zero the targets so the envelope eases out and nothing is written.
    if (c.jumpDetected && !p.jump.enabled) {
      ampTarget = 0.0f;
      downTarget = 0.0f;
    }

    ampTarget *= c.boneAmplitudeScale;  // family × profile scale
    ampTarget *= squadFactor;           // squad compensation (pre-envelope)
    downTarget *= c.boneAmplitudeScale;
    downTarget *= squadFactor;

    if (c.transitionToIdle) {
      ampTarget = 0.0f;   // to-idle fast release
      downTarget = 0.0f;
    }

    envelope.Advance(ampTarget, downTarget, freqTarget, c.transitionToIdle,
                     p.envelope.amplitudeAttackTauSec,
                     p.envelope.toIdleReleaseTauSec,
                     p.envelope.frequencyTauSec);

    float angle;
    if (c.jumpActive) {
      // landing-only damped settle (baseline simplified jump).  Baseline
      // does NOT apply the squad factor to the jump curve — same here.
      angle = jump.Tick(true, p.jump, c.boneAmplitudeScale);
    } else {
      jump.Tick(false, p.jump, c.boneAmplitudeScale);
      float amp = envelope.Amplitude();
      float down = envelope.DownAmplitude();
      float ph = envelope.Phase();
      // asymmetric gait: upper half-cycle swings with `amp`, lower
      // half-cycle with `down` (symmetric when down config is 0)
      if (amp > 0.001f || down > 0.001f)
        angle = ph < 3.14159265358979f ? amp * sinf(ph) : down * sinf(ph);
      else
        angle = 0.0f;
    }
    angle *= c.axisSign;
    return angle;
  }

  // Compose the absolute target quaternion: currentNative × dq(axis, angle).
  // Reads the CURRENT live rotation (baseline: read fresh every callback).
  static void ComposeTargets(ActiveCharacterRuntime &c, float angleRad,
                             Quat &outR, Quat &outL) {
    Quat curR = SafeGetLocalRotation(c.bones.breastR);
    Quat curL = SafeGetLocalRotation(c.bones.breastL);
    c.synthetic.lastNativeR = curR;
    c.synthetic.lastNativeL = curL;
    Quat dq = QuatAxisAngle((int)c.axis, angleRad);
    outR = QuatMul(curR, dq);
    outL = QuatMul(curL, dq);
  }

  static float GaitAmplitude(const CharacterProfile &p, int gait) {
    switch (gait) {
      case GaitIdle: return p.idle.amplitudeDeg;
      case GaitWalk: return p.walk.amplitudeDeg;
      case GaitRun: return p.run.amplitudeDeg;
      case GaitSprint: return p.sprint.amplitudeDeg;
      case GaitZipline: return p.zipline.amplitudeDeg;
      default: return 0.0f;
    }
  }
  // Down (-direction) amplitude; 0 in config = symmetric (= up value).
  static float GaitDownAmplitude(const CharacterProfile &p, int gait) {
    float d = 0.0f;
    switch (gait) {
      case GaitIdle: d = p.idle.amplitudeDownDeg; break;
      case GaitWalk: d = p.walk.amplitudeDownDeg; break;
      case GaitRun: d = p.run.amplitudeDownDeg; break;
      case GaitSprint: d = p.sprint.amplitudeDownDeg; break;
      case GaitZipline: d = p.zipline.amplitudeDownDeg; break;
      default: return 0.0f;
    }
    return d > 0.0f ? d : GaitAmplitude(p, gait);
  }
  static float GaitFrequency(const CharacterProfile &p, int gait) {
    switch (gait) {
      case GaitIdle: return p.idle.frequencyHz;
      case GaitWalk: return p.walk.frequencyHz;
      case GaitRun: return p.run.frequencyHz;
      case GaitSprint: return p.sprint.frequencyHz;
      case GaitZipline: return p.zipline.frequencyHz;
      default: return 1.5f;
    }
  }
};
