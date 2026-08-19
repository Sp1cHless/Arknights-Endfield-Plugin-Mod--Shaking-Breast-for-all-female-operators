// verify_tests.cpp — V2 automated verification (architecture §40).
// Standalone console test; not part of the plugin.  Run via verify.bat.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

#include "src/common/quat.h"
#include "src/config/json_mini.h"
#include "src/config/config_types.h"
#include "src/config/config_loader.h"
#include "src/motion/gait_classifier.h"
#include "src/motion/locomotion_envelope.h"
#include "src/motion/party_compensator.h"

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(name, cond)                                                     \
  do {                                                                        \
    if (cond) {                                                               \
      g_pass++;                                                               \
      printf("  PASS %s\n", name);                                            \
    } else {                                                                  \
      g_fail++;                                                               \
      printf("  FAIL %s\n", name);                                            \
    }                                                                         \
  } while (0)

static bool Near(float a, float b, float eps = 1e-4f) {
  return fabsf(a - b) <= eps;
}

// ---------- JSON parse ----------
static void TestJson() {
  printf("[JSON]\n");
  const char *files[] = {
      "SecondaryMotion/data/characters.default.json",
      "SecondaryMotion/presets/Default.json",
      "SecondaryMotion/presets/User.json",
      "SecondaryMotion/runtime/config.json",
  };
  for (const char *f : files) {
    jsonmini::Value root;
    CHECK(f, jsonmini::LoadJsonFile(f, root));
  }
}

// ---------- character DB + preset merge ----------
static void TestDatabase() {
  printf("[DATABASE]\n");
  ConfigSnapshot snap;
  CHECK("db load", LoadCharacterDatabase(
                       "SecondaryMotion/data/characters.default.json", snap));
  if (snap.characters.empty()) return;
  CHECK("db has aurora", snap.characters.count("chr_0014_aurora") == 1);
  CHECK("db has yvonne", snap.characters.count("chr_0017_yvonne") == 1);
  CHECK("db aurora axis explicit Z",
        snap.characters["chr_0014_aurora"].axisExplicit &&
            snap.characters["chr_0014_aurora"].axis.axis == Axis::Z);
  CHECK("db endminf scale 1.0 (family 0.4 automatic)",
        Near(snap.characters["chr_0003_endminf"].amplitudeScale, 1.0f));
  CHECK("db aurora run 8.5",
        Near(snap.characters["chr_0014_aurora"].run.amplitudeDeg, 8.5f));
  CHECK("db jump default off",
        !snap.characters["chr_0014_aurora"].jump.enabled);

  // preset merge
  CHECK("preset apply", ApplyUserPreset("SecondaryMotion/presets/User.json",
                                        snap));
  CHECK("preset disables chen", !snap.characters["chr_0005_chen"].enabled);
  CHECK("preset tunes yvonne run 10.0",
        Near(snap.characters["chr_0017_yvonne"].run.amplitudeDeg, 10.0f));
  CHECK("preset keeps aurora bones",
        snap.characters["chr_0014_aurora"].bones.rightName ==
            "R_breast_01_jnt");

  // unknown fail-closed: not in DB -> not in snapshot
  ConfigSnapshot snap2;
  LoadCharacterDatabase("SecondaryMotion/data/characters.default.json", snap2);
  CHECK("unknown char absent", snap2.characters.count("chr_9999_xx") == 0);
}

// ---------- gait classifier ----------
static void TestGait() {
  printf("[GAIT]\n");
  CHECK("run loop -> 2", ClassifyClipName("A_actor_lady_run_loop") == 2);
  CHECK("sprint loop -> 3", ClassifyClipName("A_actor_girl_sprint_loop") == 3);
  CHECK("walk loop -> 1", ClassifyClipName("A_actor_girl_walk_loop") == 1);
  CHECK("idle loop -> 0", ClassifyClipName("A_actor_girl_idle_loop") == 0);
  CHECK("jump -> 2 (baseline)", ClassifyClipName("idle_jump_start_l") == 2);
  CHECK("run_stop -> 1", ClassifyClipName("run_stop_r") == 1);
  CHECK("sprint_stop -> 1", ClassifyClipName("sprint_stop_l") == 1);
  CHECK("run_to_walk -> 1", ClassifyClipName("run_to_walk_r") == 1);
  CHECK("run_to_sprint -> 3", ClassifyClipName("run_to_sprint_l") == 3);
  CHECK("walk_start -> 1", ClassifyClipName("walk_start_l_0_r") == 1);
  CHECK("sprint_start -> 3", ClassifyClipName("sprint_start_l") == 3);
  CHECK("zipline start -> 4", ClassifyClipName("A_actor_lady_interact_zipline_start") == 4);
  CHECK("zipline slide -> 4", ClassifyClipName("A_actor_lady_interact_zipline_sp_02") == 4);
  CHECK("zipline stop -> 4", ClassifyClipName("A_actor_lady_interact_zipline_stop") == 4);
  CHECK("unknown monster idle -> 0 (contains idle, baseline)",
        ClassifyClipName("A_actor_monster_hound_idle") == 0);
  GaitClassification c = ClassifyClipNameFull("run_stop_r");
  CHECK("stop -> toIdle flag", c.transitionToIdle);
  GaitClassification j = ClassifyClipNameFull("idle_jump_land_l");
  CHECK("land clip detected", j.landingDetected);
  GaitClassification i = ClassifyClipNameFull("idle_jump_start_l");
  CHECK("jump start NOT landing", !i.landingDetected);
}

// ---------- envelope ----------
// FrameGate needs >0.5ms between advances (game frames); sleep to simulate.
static void EnvAdvance(LocomotionEnvelope &env, float amp, float down,
                       float freq, bool toIdle, float attack, float release,
                       float freqTau) {
  Sleep(2);
  env.Advance(amp, down, freq, toIdle, attack, release, freqTau);
}

static void TestEnvelope() {
  printf("[ENVELOPE]\n");
  LocomotionEnvelope env;
  // run target: 8.5 deg -> rad 0.1484
  float target = DegToRad(8.5f);
  for (int i = 0; i < 300; i++)
    EnvAdvance(env, target, target, 1.7f, false, 0.15f, 0.015f, 0.20f);
  CHECK("amp converges to run", Near(env.Amplitude(), target, 1e-3f));
  CHECK("freq converges to 1.7", Near(env.freqEnv, 1.7f, 1e-3f));
  // phase bounded
  CHECK("phase in [0,2pi)", env.Phase() >= 0.0f && env.Phase() < 6.2832f);
  // to-idle fast release
  LocomotionEnvelope env2;
  for (int i = 0; i < 120; i++)
    EnvAdvance(env2, target, target, 1.7f, false, 0.15f, 0.015f, 0.20f);
  for (int i = 0; i < 30; i++)
    EnvAdvance(env2, 0.0f, 0.0f, 1.7f, true, 0.15f, 0.015f, 0.20f);
  CHECK("to-idle releases fast", env2.Amplitude() < 0.02f);
  // asymmetric down channel
  LocomotionEnvelope env3;
  float downT = DegToRad(5.0f);
  for (int i = 0; i < 300; i++)
    EnvAdvance(env3, target, downT, 1.7f, false, 0.15f, 0.015f, 0.20f);
  CHECK("down amp converges separately", Near(env3.DownAmplitude(), downT, 1e-3f));
  // GaitDownAmplitude fallback semantics (0 = symmetric = up); inlined here
  // because synthetic_motion.h pulls IL2CPP deps that verify cannot link.
  CharacterProfile p0;
  p0.run.amplitudeDeg = 8.5f;
  p0.run.amplitudeDownDeg = 0.0f;
  CHECK("down=0 -> symmetric", Near(
      p0.run.amplitudeDownDeg > 0.0f ? p0.run.amplitudeDownDeg : p0.run.amplitudeDeg,
      8.5f, 1e-4f));
  p0.run.amplitudeDownDeg = 5.0f;
  CHECK("down>0 -> explicit", Near(
      p0.run.amplitudeDownDeg > 0.0f ? p0.run.amplitudeDownDeg : p0.run.amplitudeDeg,
      5.0f, 1e-4f));
}

// ---------- quaternion ----------
static void TestQuat() {
  printf("[QUAT]\n");
  Quat a = QuatAxisAngle(2, DegToRad(8.5f));  // Z +8.5 deg
  Quat id = QuatIdentity();
  CHECK("axisangle w positive", a.w > 0.9f);
  CHECK("mul identity", Near(QuatMul(a, id).w, a.w) &&
                            Near(QuatMul(a, id).z, a.z));
  Quat inv = QuatMul(a, QuatInv(a));
  CHECK("inv product identity", Near(inv.w, 1.0f, 1e-5f) &&
                                    Near(inv.x, 0.0f, 1e-5f) &&
                                    Near(inv.y, 0.0f, 1e-5f) &&
                                    Near(inv.z, 0.0f, 1e-5f));
  Quat pk1 = QuatPowK(a, 1.0f);
  CHECK("powk 1 identity", Near(pk1.w, a.w, 1e-4f) && Near(pk1.z, a.z, 1e-4f));
  Quat pk2 = QuatPowK(a, 2.0f);
  CHECK("powk 2 doubles", pk2.w < a.w);  // 17° -> half-angle 8.5° -> w smaller
  CHECK("slerp 0 = a", Near(QuatSlerp(a, id, 0.0f).w, a.w));
  CHECK("slerp 1 = b", Near(QuatSlerp(a, id, 1.0f).w, 1.0f));
}

// ---------- compensation formula ----------
static void TestCompensation() {
  printf("[COMPENSATION]\n");
  PartyCompensationConfig cfg;
  cfg.enabled = true;
  cfg.alpha = 1.0f;
  cfg.transitionTauSec = 0.001f;  // near-instant ease for the test
  LegacyCallbackStackCompensation comp;
  // repeated calls to converge (ease with tiny tau)
  float f1 = 0, f2 = 0, f3 = 0, f4 = 0;
  for (int i = 0; i < 60; i++) {
    f1 = comp.GetFactor(cfg, 1);
    f2 = comp.GetFactor(cfg, 2);
    f3 = comp.GetFactor(cfg, 3);
    f4 = comp.GetFactor(cfg, 4);
  }
  CHECK("N=1 -> 1.0", Near(f1, 1.0f, 1e-2f));
  CHECK("N=2 -> 1.0 (scene NPCs do NOT compress)", Near(f2, 1.0f, 1e-2f));
  CHECK("N=3 -> 1.0 (scene NPCs do NOT compress)", Near(f3, 1.0f, 1e-2f));
  CHECK("N=4 -> 0.25 (real party)", Near(f4, 0.25f, 1e-2f));
  cfg.alpha = 0.0f;
  for (int i = 0; i < 60; i++) f4 = comp.GetFactor(cfg, 4);
  CHECK("alpha=0 -> 1.0", Near(f4, 1.0f, 1e-2f));
  cfg.enabled = false;
  for (int i = 0; i < 60; i++) f4 = comp.GetFactor(cfg, 4);
  CHECK("disabled -> 1.0", Near(f4, 1.0f, 1e-2f));
}

// ---------- hot reload ----------
static void TestHotReload() {
  printf("[HOT-RELOAD]\n");
  RuntimePathsSetRootForTest("SecondaryMotion");
  RuntimeConfigFile rc;
  CHECK("runtime config parse",
        LoadRuntimeConfigFile(rc) && rc.activePreset == "Default");
  // ConfigReloadIfChanged with a fake revision: reload reads the real files
  // (revision 0 on disk) -> snapshot revision must equal the requested one.
  bool ok = ConfigReloadIfChanged(7);
  const ConfigSnapshot *s = ConfigAcquire();
  CHECK("reload installs", ok && s && s->revision == 7);
  CHECK("reload has chars", s && s->characters.size() >= 6);
  if (s) {
    CHECK("reload yvonne present", s->characters.count("chr_0017_yvonne") == 1);
    // config.json active_preset=Default -> Default.json overrides apply
    CHECK("reload uses Default preset (chen enabled)",
          s->characters.count("chr_0005_chen") == 1 &&
              s->characters.at("chr_0005_chen").enabled);
    CHECK("reload Default keeps aurora run amplitude from preset",
          s->characters.at("chr_0014_aurora").run.amplitudeDeg > 0.0f);
  }
  // reloading the same revision is a no-op
  CHECK("same revision no-op", !ConfigReloadIfChanged(7));
}

int main() {
  printf("=== SecondaryMotion V2 verification ===\n\n");
  TestJson();
  TestDatabase();
  TestGait();
  TestEnvelope();
  TestQuat();
  TestCompensation();
  TestHotReload();

  printf("\n=== RESULT: %d passed, %d failed ===\n", g_pass, g_fail);
  if (g_fail == 0) {
    printf("VERIFICATION: PASS\n");
    return 0;
  }
  printf("VERIFICATION: FAIL\n");
  return 1;
}
