#pragma once
// config/config_validator.h — startup validation pass over the snapshot.
// Invalid profiles are disabled with a log line; the rest continue.
// Architecture spec §21.
#include "config_types.h"
#include "../common/logger.h"

static void ValidateSnapshot(const ConfigSnapshot &snap) {
  for (auto &kv : snap.characters) {
    const CharacterProfile &p = kv.second;
    if (!p.enabled) {
      Log("[CFG] character '%s': profile DISABLED -> native only", kv.first.c_str());
      continue;
    }
    // Per-profile invariants (already partially enforced at load).
    bool ok = true;
    if (p.motionMode == MotionMode::Synthetic) {
      // explicit bone pair, if given, must be a pair
      if ((!p.bones.rightName.empty() && p.bones.leftName.empty()) ||
          (p.bones.rightName.empty() && !p.bones.leftName.empty()))
        ok = false;
    }
    if (p.axis.sign != 1.0f && p.axis.sign != -1.0f) ok = false;
    if (p.nativeAmplify.factor <= 0.0f) ok = false;
    if (!ok) {
      Log("[CFG] character '%s': validation FAILED -> disabled", kv.first.c_str());
      const_cast<CharacterProfile &>(p) = CharacterProfile();
    } else {
      Log("[CFG] character '%s': mode=%d axis=%d sign=%+.0f scale=%.2f",
          kv.first.c_str(), (int)p.motionMode, (int)p.axis.axis, p.axis.sign,
          p.amplitudeScale);
    }
  }
}
