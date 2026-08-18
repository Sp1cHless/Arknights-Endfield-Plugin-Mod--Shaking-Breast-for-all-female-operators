#pragma once
// diagnostics/transform_recorder.h — CSV recorder for breast localRotation
// (baseline record_test.txt marker behavior; spec §19.4).  Worker-triggered,
// never inside the hot path.
#include <cstdio>
#include "../character/active_character.h"
#include "../common/logger.h"
#include "../common/safe_unity.h"

static FILE *g_recFile = nullptr;
static int g_recFrames = 0;

static void TransformRecorderEnsureInit(const ActiveCharacterRuntime &c) {
  if (g_recFile) return;
  g_recFile = fopen("plugin/breast_record.csv", "w");
  if (g_recFile) {
    fprintf(g_recFile, "frame,time_ms,gait,Rx,Ry,Rz,Rw,Lx,Ly,Lz,Lw\n");
    g_recFrames = 0;
  }
}

static void TransformRecorderFrame(const ActiveCharacterRuntime &c) {
  if (!g_recFile || !c.bones.breastR || !c.bones.breastL) return;
  if (g_recFrames >= 1800) {  // ~30s at 60fps then auto-stop
    fclose(g_recFile);
    g_recFile = nullptr;
    ProbeLog("[REC] auto-stop after %d frames\n", g_recFrames);
    return;
  }
  Quat r = SafeGetLocalRotation(c.bones.breastR);
  Quat l = SafeGetLocalRotation(c.bones.breastL);
  fprintf(g_recFile, "%d,%lu,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
          g_recFrames, NowMs(), c.currentGait, r.x, r.y, r.z, r.w,
          l.x, l.y, l.z, l.w);
  g_recFrames++;
  if ((g_recFrames % 300) == 0)
    ProbeLog("[REC] frames=%d\n", g_recFrames);
}

static void TransformRecorderStop() {
  if (g_recFile) {
    fclose(g_recFile);
    g_recFile = nullptr;
  }
}
