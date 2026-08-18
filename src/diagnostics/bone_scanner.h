#pragma once
// diagnostics/bone_scanner.h — dump the active root's Transform tree /
// search bone names (dev tool for new-character onboarding, spec §19.1).
// V2: full-tree dumps also land in developer/bone_dumps/<character>.json
// (Manager Developer page reads them back).
#include <cstdio>
#include <cstring>
#include "../character/active_character.h"
#include "../common/logger.h"
#include "../common/safe_unity.h"
#include "../runtime/runtime_paths.h"

static void BoneScannerDumpRecursive(void *transform, int depth, int maxDepth,
                                     const char *keyword, FILE *f) {
  if (!transform || depth > maxDepth) return;
  char name[256] = "";
  SafeGetObjectName(transform, name, sizeof(name));
  if (keyword && *keyword) {
    if (strstr(name, keyword))
      ProbeLog("[BONE-SCAN] %*s%s\n", depth * 2, "", name);
  } else {
    ProbeLog("[BONE-SCAN] %*s%s\n", depth * 2, "", name);
    if (f) fprintf(f, "{\"name\": \"%s\", \"depth\": %d}\n", name, depth);
  }
  __try {
    void *countBoxed = Invoke(g_transform_get_childCount, transform);
    int count = countBoxed ? *(int *)((char *)countBoxed + IL2CPP_BOXED_DATA) : 0;
    for (int i = 0; i < count && i < 200; i++) {
      void *params[] = {&i};
      void *child = Invoke(g_transform_GetChild, transform, params);
      if (child) BoneScannerDumpRecursive(child, depth + 1, maxDepth, keyword, f);
    }
  } __except (1) {
  }
}

// Dump (or keyword-search) the whole tree under the main animator's root.
static void BoneScannerRun(void *rootTransform, const char *keyword,
                           const char *characterId) {
  if (!rootTransform) return;
  ProbeLog("[BONE-SCAN] === root=%p keyword=\"%s\" ===\n",
           (void *)rootTransform, keyword ? keyword : "");

  // Full dumps (no keyword) also go to a file for the Manager.
  FILE *f = nullptr;
  if (!keyword || !*keyword) {
    char path[512];
    if (RuntimePathsInit()) {
      RuntimePath(path, sizeof(path), "developer\\bone_dumps");
      CreateDirectoryA(path, nullptr);
      char file[600];
      snprintf(file, sizeof(file), "%s\\%s.json", path,
               characterId && characterId[0] ? characterId : "unknown");
      f = fopen(file, "w");
      if (f) fprintf(f, "{\n  \"character\": \"%s\",\n  \"bones\": [\n",
                     characterId && characterId[0] ? characterId : "unknown");
    }
  }
  BoneScannerDumpRecursive(rootTransform, 0, 40, keyword, f);
  if (f) {
    fprintf(f, "  ]\n}\n");
    fclose(f);
    ProbeLog("[BONE-SCAN] dump written\n");
  }
  ProbeLog("[BONE-SCAN] === end ===\n");
}
