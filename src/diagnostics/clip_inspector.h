#pragma once
// diagnostics/clip_inspector.h — dump current layer-0 clips: name, weight,
// classified gait, transition/jump flags (spec §19.3; baseline GAITDBG).
#include "../common/logger.h"
#include "../il2cpp/animator_clip_reader.h"
#include "../motion/gait_classifier.h"

static void ClipInspectorRun(AnimatorClipReader &reader, void *animator) {
  if (!animator) return;
  static DWORD s_lastRun = 0;
  if (!RateLimit(s_lastRun, 3000)) return;  // rate-limited on the hot path
  ClipSample clips[8];
  size_t count = 0;
  if (!reader.ReadLayer0(animator, clips, 8, count)) {
    ProbeLog("[CLIP-INSP] read failed\n");
    return;
  }
  for (size_t i = 0; i < count; i++) {
    GaitClassification cls = ClassifyClipNameFull(clips[i].name);
    ProbeLog("[CLIP-INSP] #%zu w=%.3f gait=%d toIdle=%d jump=%d land=%d name=\"%s\"\n",
             i, clips[i].weight, cls.gait, cls.transitionToIdle ? 1 : 0,
             cls.jumpDetected ? 1 : 0, cls.landingDetected ? 1 : 0,
             clips[i].name);
  }
}
