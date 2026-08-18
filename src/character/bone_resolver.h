#pragma once
// character/bone_resolver.h — breast bone discovery from the Animator root
// and per-bone-family axis/scale selection (baseline candidate table +
// SetBreastAxisByName semantics).  Architecture spec §8.
#include <cstring>
#include "../common/logger.h"
#include "../common/safe_unity.h"
#include "character_identity.h"

// Candidate table in priority order (baseline verified list).
static const char *kBreastCandidates[][2] = {
    {"breast_R_01_jnt", "breast_L_01_jnt"},
    {"R_breast_01_jnt", "L_breast_01_jnt"},
    {"xiong_R_0_skin_jnt", "xiong_L_0_skin_jnt"},
    {"breast_R_01", "breast_L_01"},
    {"R_breast_01", "L_breast_01"},
    {"xiong_R_0_skin", "xiong_L_0_skin"},
};
static const int kBreastCandidateCount =
    (int)(sizeof(kBreastCandidates) / sizeof(kBreastCandidates[0]));

struct BoneResolution {
  void *breastR = nullptr;
  void *breastL = nullptr;
  char rightName[128] = {0};
  char leftName[128] = {0};
  Axis axis = Axis::Z;      // per-family (baseline SetBreastAxisByName)
  float ampScale = 1.0f;
  bool found = false;
};

// Axis/scale by bone family (baseline semantics):
//   xiong_* -> Y front-back sway, 0.4 scale (tip skin bone lever arm)
//   others  -> Z, 1.0
static void ApplyBoneFamilyDefaults(BoneResolution &res, const char *rightName) {
  if (rightName && strstr(rightName, "xiong")) {
    res.axis = Axis::Y;
    res.ampScale = 0.4f;
  } else {
    res.axis = Axis::Z;
    res.ampScale = 1.0f;
  }
}

// Search the whole candidate table under root.  Returns false when no pair
// is found (caller decides fail-closed behavior).
static bool ResolveBreastBones(void *rootTransform, BoneResolution &out,
                               bool logFound) {
  if (!rootTransform) return false;
  for (int i = 0; i < kBreastCandidateCount; i++) {
    void *r = SafeFindChildRecursive(rootTransform, kBreastCandidates[i][0], 40);
    void *l = SafeFindChildRecursive(rootTransform, kBreastCandidates[i][1], 40);
    if (r && l) {
      out.breastR = r;
      out.breastL = l;
      SafeGetObjectName(r, out.rightName, sizeof(out.rightName));
      SafeGetObjectName(l, out.leftName, sizeof(out.leftName));
      ApplyBoneFamilyDefaults(out, out.rightName);
      out.found = true;
      if (logFound)
        ProbeLog("[BONE] FOUND pattern[%d] R=\"%s\" L=\"%s\" axis=%d scale=%.2f\n",
                 i, out.rightName, out.leftName, (int)out.axis, out.ampScale);
      return true;
    }
  }
  out.found = false;
  return false;
}

// Resolve using explicit profile bone names first, then candidates.
// (Architecture spec §8.1 order.)
static bool ResolveBreastBonesWithProfile(void *rootTransform,
                                          const CharacterProfile &profile,
                                          BoneResolution &out) {
  if (rootTransform && !profile.bones.rightName.empty() &&
      !profile.bones.leftName.empty()) {
    void *r = SafeFindChildRecursive(rootTransform, profile.bones.rightName.c_str(), 40);
    void *l = SafeFindChildRecursive(rootTransform, profile.bones.leftName.c_str(), 40);
    if (r && l) {
      out.breastR = r;
      out.breastL = l;
      SafeGetObjectName(r, out.rightName, sizeof(out.rightName));
      SafeGetObjectName(l, out.leftName, sizeof(out.leftName));
      out.axis = profile.axis.axis;
      out.ampScale = profile.amplitudeScale;
      out.found = true;
      ProbeLog("[BONE] FOUND explicit R=\"%s\" L=\"%s\"\n",
               out.rightName, out.leftName);
      return true;
    }
  }
  if (!profile.bones.allowFallbackCandidates) return false;
  return ResolveBreastBones(rootTransform, out, true);
}
