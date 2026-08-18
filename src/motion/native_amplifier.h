#pragma once
// motion/native_amplifier.h — legacy Mode 1: amplify the native animation
// by exponent K on the delta quaternion (baseline ProbeAmplifyApply +
// LateTick amplification math, 胸骨骼标注与研究文档 §5 corrected order):
//   d  = inv(base) * q
//   out = base * d^K
// base is a slow Slerp (0.05/frame) so breathing/pose is preserved.
// MUTUALLY EXCLUSIVE with synthetic (per-character mode, spec §14).
#include "../character/active_character.h"
#include "../common/logger.h"
#include "../common/quat.h"
#include "../common/safe_unity.h"
#include "../config/config_types.h"

struct NativeAmplifier {
  Quat baseR = QuatIdentity();
  Quat baseL = QuatIdentity();
  bool baseInit = false;
  bool enabled = false;  // marker/config gate (cached, not per-frame I/O)

  void Reset() {
    baseR = QuatIdentity();
    baseL = QuatIdentity();
    baseInit = false;
    enabled = false;
  }

  // One amplification step for both bones (LateTick timing, baseline).
  // Reads live rotations, updates base, writes amplified rotations.
  void Apply(ActiveCharacterRuntime &c, float factor) {
    if (!enabled || !c.bones.breastR || !c.bones.breastL) return;
    __try {
      if (!baseInit) {
        baseInit = true;
        baseR = SafeGetLocalRotation(c.bones.breastR);
        baseL = SafeGetLocalRotation(c.bones.breastL);
      }
      Quat q = SafeGetLocalRotation(c.bones.breastR);
      baseR = QuatSlerp(baseR, q, 0.05f);
      Quat d = QuatMul(QuatInv(baseR), q);
      Quat out = QuatMul(baseR, QuatPowK(d, factor));
      SafeSetLocalRotation(c.bones.breastR, out);

      Quat ql = SafeGetLocalRotation(c.bones.breastL);
      baseL = QuatSlerp(baseL, ql, 0.05f);
      Quat dl = QuatMul(QuatInv(baseL), ql);
      Quat outl = QuatMul(baseL, QuatPowK(dl, factor));
      SafeSetLocalRotation(c.bones.breastL, outl);
    } __except (1) {
    }
  }
};
