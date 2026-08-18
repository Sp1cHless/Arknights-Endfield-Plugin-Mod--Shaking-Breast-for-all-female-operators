#pragma once
// config/config_loader.h — V2 config loading: merge of
//   data/characters.default.json   (technical facts: bones/axis/scale/defaults)
//   presets/<active>.json          (user overrides: enable/mode/params)
//   runtime/config.json            (revision + active preset + global)
// into an immutable ConfigSnapshot.  Supports HOT RELOAD (Phase 1): the
// service thread polls runtime/config.json, rebuilds the snapshot on
// revision change and swaps the atomic pointer.  Old snapshots are retired
// two generations later (no hook callback can still be using them).
#include <atomic>
#include <cstdint>
#include <cstring>
#include <float.h>
#include "config_types.h"
#include "json_mini.h"
#include "../common/logger.h"
#include "../runtime/runtime_paths.h"

static const char *kDefaultPreset = "Default";

// Atomic config pointer consumed by the motion core every callback.
// Defined once (single-TU plugin); swap under the service thread.
static std::atomic<const ConfigSnapshot *> g_configPtr{nullptr};
static const ConfigSnapshot *g_retired[2] = {nullptr, nullptr};

static inline const ConfigSnapshot *ConfigAcquire() {
  return g_configPtr.load(std::memory_order_acquire);
}

// Retire the previous snapshot (call after a successful swap, from the
// service thread only).  Two-generation delay keeps in-flight callbacks
// safe without locks.
static void ConfigRetireOld(const ConfigSnapshot *oldSnap) {
  if (g_retired[0]) delete g_retired[0];
  g_retired[0] = g_retired[1];
  g_retired[1] = oldSnap;
}

// ---- helpers ----
static MotionMode ParseMotionMode(const std::string &s) {
  if (s == "synthetic") return MotionMode::Synthetic;
  if (s == "amplify_native") return MotionMode::AmplifyNative;
  return MotionMode::Off;
}

static Axis ParseAxis(const std::string &s) {
  if (s == "X") return Axis::X;
  if (s == "Y") return Axis::Y;
  return Axis::Z;
}

static bool IsFinite(double d) { return _finite(d) != 0; }

static bool ParseGaitParam(const jsonmini::Value *v, GaitParam &out,
                           float defAmp, float defFreq) {
  if (!v) {
    out.amplitudeDeg = defAmp;
    out.amplitudeDownDeg = 0.0f;  // 0 = symmetric (= up)
    out.frequencyHz = defFreq;
    return true;
  }
  out.amplitudeDeg = (float)v->GetNumber("amplitude_deg", defAmp);
  // down amplitude: explicit only; 0 / absent = symmetric
  out.amplitudeDownDeg = (float)v->GetNumber("amplitude_down_deg", 0.0);
  out.frequencyHz = (float)v->GetNumber("frequency_hz", defFreq);
  if (!IsFinite(out.amplitudeDeg) || !IsFinite(out.amplitudeDownDeg) ||
      !IsFinite(out.frequencyHz))
    return false;
  if (out.frequencyHz <= 0.0f) return false;
  return true;
}

// Parse the shared "params" shape used by both the default DB (as defaults)
// and user presets (as overrides): gait/envelope/jump/native_amplify.
static bool ParseParamBlock(const jsonmini::Value &v, CharacterProfile &p) {
  if (const jsonmini::Value *g = v.Find("gait")) {
    if (!ParseGaitParam(g->Find("idle"), p.idle, 0.0f, 1.2f)) return false;
    if (!ParseGaitParam(g->Find("walk"), p.walk, 3.6f, 1.5f)) return false;
    if (!ParseGaitParam(g->Find("run"), p.run, 8.5f, 1.7f)) return false;
    if (!ParseGaitParam(g->Find("sprint"), p.sprint, 12.0f, 2.0f))
      return false;
    if (!ParseGaitParam(g->Find("zipline"), p.zipline, 8.5f, 1.7f))
      return false;  // defaults = run-family values
  }
  if (const jsonmini::Value *env = v.Find("envelope")) {
    p.envelope.amplitudeAttackTauSec =
        (float)env->GetNumber("amplitude_attack_tau_sec", 0.15);
    p.envelope.frequencyTauSec =
        (float)env->GetNumber("frequency_tau_sec", 0.20);
    p.envelope.toIdleReleaseTauSec =
        (float)env->GetNumber("to_idle_release_tau_sec", 0.015);
  }
  if (!IsFinite(p.envelope.amplitudeAttackTauSec) ||
      !IsFinite(p.envelope.frequencyTauSec) ||
      !IsFinite(p.envelope.toIdleReleaseTauSec))
    return false;
  if (p.envelope.amplitudeAttackTauSec <= 0.0f ||
      p.envelope.frequencyTauSec <= 0.0f ||
      p.envelope.toIdleReleaseTauSec <= 0.0f)
    return false;
  if (const jsonmini::Value *j = v.Find("jump")) {
    p.jump.enabled = j->GetBool("enabled", false);  // V2: default OFF
    p.jump.mode = j->GetString("mode", "off");
    p.jump.amplitudeDeg = (float)j->GetNumber("amplitude_deg", 10.0f);
    p.jump.dampingTauSec = (float)j->GetNumber("damping_tau_sec", 0.3f);
    p.jump.frequencyHz = (float)j->GetNumber("frequency_hz", 1.5f);
    p.jump.maxDurationSec = (float)j->GetNumber("max_duration_sec", 1.2f);
  }
  if (p.jump.mode != "off" && p.jump.mode != "landing_damped") return false;
  if (const jsonmini::Value *na = v.Find("native_amplify")) {
    p.nativeAmplify.factor = (float)na->GetNumber("factor", 2.0f);
  }
  if (!IsFinite(p.nativeAmplify.factor) || p.nativeAmplify.factor <= 0.0f)
    return false;
  return true;
}

// ---- characters.default.json (technical facts) ----
// Each entry: display_name, bones{right,left,allow_fallback_candidates},
// axis{name,sign} (optional -> auto), bone_scale (family default unless
// written), defaults{ gait, envelope, jump, native_amplify }.
static bool LoadCharacterDatabase(const char *path, ConfigSnapshot &snap) {
  jsonmini::Value root;
  if (!jsonmini::LoadJsonFile(path, root)) {
    Log("[CFG] characters.default.json missing/unreadable: %s", path);
    return false;
  }
  if ((uint32_t)root.GetNumber("schema_version", 1) != 1) {
    Log("[CFG] characters.default.json schema unsupported");
    return false;
  }
  const jsonmini::Value *chars = root.Find("characters");
  if (!chars || chars->type != jsonmini::Value::Object) {
    Log("[CFG] characters.default.json has no characters object");
    return false;
  }
  for (auto &kv : chars->obj) {
    const std::string &id = kv.first;
    const jsonmini::Value &c = kv.second;
    CharacterProfile p;
    p.enabled = true;  // DB chars are supported; preset decides enable
    p.motionMode = MotionMode::Synthetic;
    if (const jsonmini::Value *b = c.Find("bones")) {
      p.bones.rightName = b->GetString("right", "");
      p.bones.leftName = b->GetString("left", "");
      p.bones.allowFallbackCandidates =
          b->GetBool("allow_fallback_candidates", true);
    }
    if (const jsonmini::Value *axis = c.Find("axis")) {
      p.axis.axis = ParseAxis(axis->GetString("name", "Z"));
      double sign = axis->GetNumber("sign", 1.0);
      p.axis.sign = (sign < 0.0f) ? -1.0f : 1.0f;
      p.axisExplicit = true;
    }
    p.amplitudeScale = (float)c.GetNumber("bone_scale", 1.0);
    if (!IsFinite(p.amplitudeScale) || p.amplitudeScale < 0.0f) continue;
    if (const jsonmini::Value *def = c.Find("defaults")) {
      if (!ParseParamBlock(*def, p)) {
        Log("[CFG] db character '%s' defaults invalid -> skipped", id.c_str());
        continue;
      }
    }
    snap.characters[id] = p;
  }
  Log("[CFG] character db loaded: %zu entries", snap.characters.size());
  return true;
}

// ---- user preset (overrides) ----
// Entries may be partial: only written fields override the DB.
static bool ApplyUserPreset(const char *path, ConfigSnapshot &snap) {
  jsonmini::Value root;
  if (!jsonmini::LoadJsonFile(path, root)) {
    Log("[CFG] preset missing/unreadable: %s", path);
    return false;
  }
  if ((uint32_t)root.GetNumber("schema_version", 1) != 1) {
    Log("[CFG] preset schema unsupported: %s", path);
    return false;
  }
  const jsonmini::Value *chars = root.Find("characters");
  if (!chars || chars->type != jsonmini::Value::Object) {
    Log("[CFG] preset has no characters object: %s", path);
    return false;
  }
  int overridden = 0;
  for (auto &kv : chars->obj) {
    const std::string &id = kv.first;
    const jsonmini::Value &v = kv.second;
    auto it = snap.characters.find(id);
    if (it == snap.characters.end()) {
      // preset-only character: build from scratch (bones/axis via family
      // auto; parameters from the preset)
      CharacterProfile p;
      p.enabled = true;
      p.motionMode = MotionMode::Synthetic;
      if (!ParseParamBlock(v, p)) continue;
      if (v.Find("axis")) {
        const jsonmini::Value *axis = v.Find("axis");
        p.axis.axis = ParseAxis(axis->GetString("name", "Z"));
        double sign = axis->GetNumber("sign", 1.0);
        p.axis.sign = (sign < 0.0f) ? -1.0f : 1.0f;
        p.axisExplicit = true;
      }
      p.amplitudeScale = (float)v.GetNumber("amplitude_scale", 1.0);
      if (p.amplitudeScale < 0.0f) continue;
      snap.characters[id] = p;
      overridden++;
      continue;
    }
    CharacterProfile &p = it->second;
    p.enabled = v.GetBool("enabled", p.enabled);
    std::string mode = v.GetString("motion_mode", "");
    if (!mode.empty()) p.motionMode = ParseMotionMode(mode);
    if (const jsonmini::Value *b = v.Find("bones")) {
      std::string r = b->GetString("right", "");
      std::string l = b->GetString("left", "");
      p.bones.rightName = r;
      p.bones.leftName = l;
      p.bones.allowFallbackCandidates =
          b->GetBool("allow_fallback_candidates", true);
    }
    if (const jsonmini::Value *axis = v.Find("axis")) {
      p.axis.axis = ParseAxis(axis->GetString("name", "Z"));
      double sign = axis->GetNumber("sign", 1.0);
      p.axis.sign = (sign < 0.0f) ? -1.0f : 1.0f;
      p.axisExplicit = true;
    }
    p.amplitudeScale = (float)v.GetNumber("amplitude_scale", p.amplitudeScale);
    if (!IsFinite(p.amplitudeScale) || p.amplitudeScale < 0.0f) continue;
    ParseParamBlock(v, p);  // partial gait/envelope/jump/native overrides
    overridden++;
  }
  Log("[CFG] preset applied: %d character overrides", overridden);
  return true;
}

// ---- runtime/config.json ----
struct RuntimeConfigFile {
  int revision = 0;
  bool enabled = true;
  std::string activePreset = kDefaultPreset;
  bool hasGlobal = false;
  jsonmini::Value global;
};

static bool LoadRuntimeConfigFile(RuntimeConfigFile &out) {
  char path[512];
  RuntimePath(path, sizeof(path), "runtime\\config.json");
  jsonmini::Value root;
  if (!jsonmini::LoadJsonFile(path, root)) return false;
  out.revision = (int)root.GetNumber("revision", 0);
  out.enabled = root.GetBool("enabled", true);
  std::string p = root.GetString("active_preset", "");
  if (!p.empty()) out.activePreset = p;
  if (const jsonmini::Value *g = root.Find("global")) {
    out.hasGlobal = true;
    out.global = *g;
  }
  return true;
}

static void ApplyGlobalOverride(ConfigSnapshot &snap,
                                const RuntimeConfigFile &rc) {
  if (!rc.hasGlobal) return;
  const jsonmini::Value &g = rc.global;
  if (const jsonmini::Value *pc = g.Find("party_compensation")) {
    snap.global.partyCompensation.enabled = pc->GetBool("enabled", true);
    snap.global.partyCompensation.strategy =
        pc->GetString("strategy", "legacy_four_stack");
    snap.global.partyCompensation.fourMemberFactor =
        (float)pc->GetNumber("four_member_factor", 0.25f);
    snap.global.partyCompensation.transitionTauSec =
        (float)pc->GetNumber("transition_tau_sec", 0.15f);
    snap.global.partyCompensation.alpha =
        (float)pc->GetNumber("alpha", 1.0f);  // V2 formula parameter
  }
  snap.global.gaitSampleIntervalMs =
      (uint32_t)g.GetNumber("gait_sample_interval_ms", 50);
  snap.global.entityRefreshIntervalMs =
      (uint32_t)g.GetNumber("entity_refresh_interval_ms", 500);
  snap.global.replayVerifyWindowMs =
      (uint32_t)g.GetNumber("replay_verify_window_ms", 150);
  snap.global.legacyMarkerMode = g.GetBool("legacy_marker_mode", false);
}

// ---- main entry (startup + hot reload) ----
// Builds a full snapshot from the three files.  On failure returns an
// invalid snapshot (caller keeps the last good one).
static ConfigSnapshot LoadConfigSnapshot(int revision) {
  ConfigSnapshot snap;
  snap.revision = revision;

  if (!RuntimePathsInit()) {
    Log("[CFG] runtime root unavailable -> INVALID");
    return snap;
  }

  char dbPath[512];
  RuntimePath(dbPath, sizeof(dbPath), "data\\characters.default.json");

  // runtime/config.json first (knows the active preset)
  RuntimeConfigFile rc;
  bool haveRuntime = LoadRuntimeConfigFile(rc);
  if (haveRuntime) {
    snap.presetName = rc.activePreset;
    // Global switch: enabled=false keeps loading config but marks the
    // snapshot disabled (MotionEngine -> no write).  Re-enabling is then
    // instant on the next revision bump.
    snap.pluginEnabled = rc.enabled;
  } else {
    snap.presetName = kDefaultPreset;
    snap.pluginEnabled = true;
  }

  // preset name sanity (no path traversal)
  for (char ch : snap.presetName) {
    if (ch == '.' || ch == '/' || ch == '\\' || ch == ':') {
      Log("[CFG] invalid preset name '%s' -> INVALID", snap.presetName.c_str());
      return snap;
    }
  }

  if (!LoadCharacterDatabase(dbPath, snap)) return snap;
  char presetPath[512];
  snprintf(presetPath, sizeof(presetPath), "%s\\presets\\%s.json",
           g_runtimeRoot, snap.presetName.c_str());
  ApplyUserPreset(presetPath, snap);
  if (haveRuntime) ApplyGlobalOverride(snap, rc);

  snap.schemaVersion = 1;
  snap.valid = true;
  Log("[CFG] snapshot ready preset=%s revision=%d chars=%zu", 
      snap.presetName.c_str(), revision, snap.characters.size());
  return snap;
}

// Hot-reload entry: called from the service thread on revision change.
// Returns true when a new valid snapshot was installed.
static bool ConfigReloadIfChanged(int newRevision) {
  const ConfigSnapshot *cur = ConfigAcquire();
  if (cur && cur->revision == newRevision) return false;
  ConfigSnapshot fresh = LoadConfigSnapshot(newRevision);
  if (!fresh.valid) {
    Log("[CFG] reload revision=%d INVALID -> keeping last good (rev=%d)",
        newRevision, cur ? cur->revision : -1);
    return false;
  }
  ConfigSnapshot *installed = new ConfigSnapshot(std::move(fresh));
  const ConfigSnapshot *old = g_configPtr.exchange(installed,
                                                   std::memory_order_acq_rel);
  ConfigRetireOld(old);
  Log("[CFG] reload installed revision=%d", newRevision);
  return true;
}

// Startup entry: installs the initial snapshot (valid required).
static bool ConfigInstallInitial(int revision) {
  ConfigSnapshot fresh = LoadConfigSnapshot(revision);
  if (!fresh.valid) return false;
  ConfigSnapshot *installed = new ConfigSnapshot(std::move(fresh));
  g_configPtr.store(installed, std::memory_order_release);
  return true;
}
