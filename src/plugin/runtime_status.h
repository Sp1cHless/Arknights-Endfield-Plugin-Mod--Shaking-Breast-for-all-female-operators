#pragma once
// plugin/runtime_status.h — V2 lightweight runtime_status.json (§38).
// Written by the worker thread only (never from animation callbacks).
//   SecondaryMotion/runtime/runtime_status.json
#include <cstdio>
#include <cstring>
#include "../character/active_character.h"
#include "../common/logger.h"
#include "../common/time_utils.h"
#include "../config/config_loader.h"
#include "../runtime/runtime_paths.h"

enum class PluginState {
  STARTING,
  CONFIG_LOADED,
  SYMBOLS_RESOLVED,
  HOOKS_INSTALLED,
  READY,
  DEGRADED,       // required hooks OK, optional reinforcement missing
  DISABLED_SAFE   // fatal startup failure — NO WRITE
};

static const char *PluginStateName(PluginState s) {
  switch (s) {
    case PluginState::STARTING: return "STARTING";
    case PluginState::CONFIG_LOADED: return "CONFIG_LOADED";
    case PluginState::SYMBOLS_RESOLVED: return "SYMBOLS_RESOLVED";
    case PluginState::HOOKS_INSTALLED: return "HOOKS_INSTALLED";
    case PluginState::READY: return "READY";
    case PluginState::DEGRADED: return "DEGRADED";
    case PluginState::DISABLED_SAFE: return "DISABLED_SAFE";
  }
  return "?";
}

static const char *GaitName(int gait) {
  switch (gait) {
    case 0: return "idle";
    case 1: return "walk";
    case 2: return "run";
    case 3: return "sprint";
    default: return "none";
  }
}

static const char *ModeName(int mode) {
  return mode == 1 ? "synthetic" : mode == 2 ? "amplify_native" : "off";
}

static void WriteRuntimeStatus(PluginState state, int revisionOverride,
                               const ActiveCharacterRuntime &active,
                               const char *lastError) {
  const ConfigSnapshot *cfg = ConfigAcquire();
  int revision = revisionOverride;
  if (revision == 0 && cfg) revision = cfg->revision;

  DWORD lastWriteAge = 0;
  if (active.replay.bonesVerifiedMs)
    lastWriteAge = NowMs() - active.replay.bonesVerifiedMs;

  char path[512];
  RuntimePath(path, sizeof(path), "runtime\\runtime_status.json");
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "{\n");
  fprintf(f, "  \"state\": \"%s\",\n", PluginStateName(state));
  fprintf(f, "  \"applied_revision\": %d,\n", revision);
  fprintf(f, "  \"character\": \"%s\",\n",
          active.characterId[0] ? active.characterId : "");
  fprintf(f, "  \"profile\": %s,\n", active.profileFound ? "true" : "false");
  fprintf(f, "  \"bones\": %s,\n", active.bones.found ? "true" : "false");
  fprintf(f, "  \"mode\": \"%s\",\n",
          ModeName(active.profile ? (int)active.profile->motionMode : 0));
  fprintf(f, "  \"gait\": \"%s\",\n", GaitName(active.currentGait));
  fprintf(f, "  \"last_write_ms\": %lu,\n", (unsigned long)lastWriteAge);
  fprintf(f, "  \"error\": \"%s\"\n", lastError ? lastError : "");
  fprintf(f, "}\n");
  fclose(f);
}
