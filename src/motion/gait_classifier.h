#pragma once
// motion/gait_classifier.h — pure clip-name gait classification.
// V1 keeps the EXACT baseline classification semantics (architecture spec
// §9.3): priority jump > _to_ (by target) > start > stop > main loops.
#include <cstring>
#include "../config/config_types.h"

struct GaitClassification {
  int gait = GaitNone;          // Gait enum
  bool transitionToIdle = false;
  bool jumpDetected = false;
  bool landingDetected = false;
};

// Baseline rules (交接文档 §4.2, verified 2026-08-16):
//   jump          -> Run params (2)  [gate first: idle_jump_* contains "idle"]
//   _to_<target>  -> sprint=3 walk=1 run=2 idle=1 relax=0, unknown=1
//   <x>_start     -> sprint_start=3 run_start=2 walk_start=1
//   *_stop        -> 1 (stopping toward idle/walk)
//   sprint/run/walk|move/idle|relax -> 3/2/1/0
//   unknown       -> -1 (no oscillation)
static int ClassifyClipName(const char *name) {
  if (!name) return -1;
  if (strstr(name, "zipline")) return GaitZipline;  // slide on zipline
  if (strstr(name, "jump")) return GaitRun;  // baseline: jump uses Run params
  if (strstr(name, "_to_")) {
    const char *t = strstr(name, "_to_") + 4;
    if (strstr(t, "sprint")) return GaitSprint;
    if (strstr(t, "walk")) return GaitWalk;
    if (strstr(t, "run")) return GaitRun;
    if (strstr(t, "idle")) return GaitWalk;   // -> idle uses walk params
    if (strstr(t, "relax")) return GaitIdle;  // -> relax = idle family
    return GaitWalk;  // unknown target — conservative walk
  }
  if (strstr(name, "start")) {
    if (strstr(name, "sprint")) return GaitSprint;
    if (strstr(name, "run")) return GaitRun;
    return GaitWalk;
  }
  if (strstr(name, "stop")) return GaitWalk;  // stopping toward idle/walk
  if (strstr(name, "sprint")) return GaitSprint;
  if (strstr(name, "run")) return GaitRun;
  if (strstr(name, "walk") || strstr(name, "move")) return GaitWalk;
  if (strstr(name, "idle") || strstr(name, "relax")) return GaitIdle;
  return GaitNone;
}

static GaitClassification ClassifyClipNameFull(const char *name) {
  GaitClassification out;
  out.gait = ClassifyClipName(name);
  if (!name) return out;
  // to-idle fast release: stop clips and _to_idle transitions (baseline)
  if (strstr(name, "stop") || strstr(name, "_to_idle"))
    out.transitionToIdle = true;
  if (strstr(name, "jump")) out.jumpDetected = true;
  // landing-only simplified jump (baseline: only the LAND clip drives it)
  if (strstr(name, "jump") && strstr(name, "land"))
    out.landingDetected = true;
  return out;
}
