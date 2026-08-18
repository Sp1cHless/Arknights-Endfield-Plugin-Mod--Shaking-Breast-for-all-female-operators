#pragma once
// character/character_identity.h — canonical character-id resolution and the
// PlayerController capture hook that feeds the entity-refresh chain.
// Identity priority (architecture spec §7.1):
//   1. "chr_*" in Animator/root GameObject name
//   2. verified player-control chain root/entity name
//   (clip names are only auxiliary, never sole source)
#include <cstring>
#include "../common/logger.h"
#include "../common/safe_unity.h"
#include "../il2cpp/il2cpp_api.h"
#include "../runtime/runtime_paths.h"

// ---- player-control chain state (baseline SafeRefreshEntity) ----
static void *g_playerController = nullptr;
static void *g_mainCharEntity = nullptr;
static void *g_cachedAnimator = nullptr;  // current main-character Animator

// Field offsets resolved via reflection (STRICT — no fallback; failure of
// this chain means main-character detection unavailable -> no write).
static int OFF_pcEntity = -1;             // PlayerController -> entity
static int OFF_entityComplexAnim = -1;    // Entity -> complexAnimComp
static int OFF_complexAnimAnimator = -1;  // complexAnimComp -> animator

// STRICT (V2 §36): a failed resolution returns -1; the caller fails closed
// (main-character refresh unavailable -> no sampling -> no write).
static inline int SafeOff(int resolved, const char *name) {
  if (resolved < 0) {
    static DWORD s_lastWarn = 0;
    if (RateLimit(s_lastWarn, 5000))
      Log("[WARN] Reflection failed for %s (no fallback — strict)", name);
  }
  return resolved;
}

// Lazy-resolve ComplexAnimComp -> animator field.  Called whenever we hold a
// non-null complexAnimCom (SetMainCharacter hook AND refresh path — the
// component may not be initialized at SetMainCharacter time, so the refresh
// path is the reliable point, matching baseline behavior).
static void ResolveComplexAnimatorOffset(void *entity, void *complexAnimCom) {
  if (OFF_complexAnimAnimator >= 0 || !complexAnimCom) return;
  __try {
    void *cacClass = il2cpp_object_get_class(complexAnimCom);
    if (cacClass) {
      const char *animNames[] = {"animator", "m_animator", "_animator",
                                 "<animator>k__BackingField"};
      OFF_complexAnimAnimator =
          FindFieldOffsetInHierarchyNames(cacClass, animNames, 4);
      Log("[REFLECT] ComplexAnimComp.animator offset=%d (strict)",
          OFF_complexAnimAnimator);
    }
  } __except (1) {
  }
}

// Resolve the three player-control-chain field offsets via IL2CPP metadata.
static void ResolvePlayerChainOffsets() {
  void *domain = il2cpp_domain_get();
  if (!domain) return;
  size_t ac = 0;
  void **asms = il2cpp_domain_get_assemblies(domain, &ac);
  if (!asms || !ac) return;

  void *pcClass = FindClass("Beyond.Gameplay.Core", "PlayerController", asms, ac);
  if (pcClass) {
    const char *entityNames[] = {"mainCharacter", "m_entity", "_entity",
                                 "m_mainCharacter", "entity", "m_controlledEntity",
                                 "controlledEntity"};
    OFF_pcEntity = FindFieldOffsetInHierarchyNames(pcClass, entityNames, 7);
    Log("[REFLECT] PlayerController.entity offset=%d (strict)",
        OFF_pcEntity);
  } else {
    Log("[REFLECT] PlayerController class NOT found (strict: main-character "
        "refresh unavailable)");
  }

  // Entity -> ComplexAnim offset is resolved lazily from the captured entity
  // (class known only at runtime), same for ComplexAnim -> Animator.
}

// Hooked PlayerController.SetMainCharacter(self, entity, flag):
// captures the player controller + main entity (baseline behavior).
typedef void(__fastcall *SetMainCharacterFn)(void *self, void *entity, bool flag);
static SetMainCharacterFn g_origSetMainCharacter = nullptr;

static void __fastcall Hooked_SetMainCharacter(void *self, void *entity, bool flag) {
  if (g_origSetMainCharacter)
    g_origSetMainCharacter(self, entity, flag);
  if (self && !g_playerController) {
    g_playerController = self;
    Log("[CHAR] captured PlayerController=%p", self);
  }
  if (entity) {
    g_mainCharEntity = entity;
    // lazy resolve Entity -> complexAnimComp field
    if (OFF_entityComplexAnim < 0) {
      __try {
        void *entClass = il2cpp_object_get_class(entity);
        if (entClass) {
          const char *caNames[] = {"<animatorCom>k__BackingField", "animatorCom",
                                   "complexAnimationComponent",
                                   "m_complexAnimationComponent"};
          OFF_entityComplexAnim =
              FindFieldOffsetInHierarchyNames(entClass, caNames, 4);
          Log("[REFLECT] Entity.complexAnim offset=%d (strict)",
              OFF_entityComplexAnim);
        }
      } __except (1) {
      }
    }
    // lazy resolve ComplexAnimComp -> animator field (component may be null
    // at this early point; the refresh path retries)
    if (OFF_complexAnimAnimator < 0) {
      __try {
        int caOff = SafeOff(OFF_entityComplexAnim, "entityComplexAnim");
        if (caOff >= 0) {
          void *complexAnimCom = *(void **)((char *)entity + caOff);
          ResolveComplexAnimatorOffset(entity, complexAnimCom);
        }
      } __except (1) {
      }
    }
  }
}

// Install the SetMainCharacter hook (optional — entity refresh falls back to
// bone re-discovery on animator change when it is absent).
static bool InstallPlayerControllerHook() {
  if (g_origSetMainCharacter) return true;
  void *domain = il2cpp_domain_get();
  if (!domain) return false;
  size_t ac = 0;
  void **asms = il2cpp_domain_get_assemblies(domain, &ac);
  if (!asms || !ac) return false;
  void *pcClass = FindClass("Beyond.Gameplay.Core", "PlayerController", asms, ac);
  if (!pcClass) {
    Log("[CHAR] PlayerController class not found — entity refresh degraded");
    return false;
  }
  void *method = FindMethod(pcClass, "SetMainCharacter", 2);
  if (!method) {
    Log("[CHAR] PlayerController.SetMainCharacter not found");
    return false;
  }
  void *fn = *(void **)method;
  if (!fn || !g_gaMod || (uintptr_t)fn < (uintptr_t)g_gaMod) {
    Log("[CHAR] SetMainCharacter fn invalid %p", fn);
    return false;
  }
  MH_STATUS st = MH_CreateHook(fn, (void *)Hooked_SetMainCharacter,
                               (void **)&g_origSetMainCharacter);
  if (st != MH_OK || !g_origSetMainCharacter) {
    Log("[CHAR] SetMainCharacter hook failed: %d", st);
    return false;
  }
  st = MH_EnableHook(fn);
  Log("[CHAR] SetMainCharacter hooked=%d fn=%p", st == MH_OK, fn);
  return st == MH_OK;
}

// ---- canonical character id ----
// Extract "chr_xxxx_name" from a GameObject name and strip runtime suffixes:
//   chr_0003_endminf_postmodel        -> chr_0003_endminf
//   chr_0003_endminf_postmodel(Clone) -> chr_0003_endminf
//   chr_0003_endminf_postmodel#123    -> chr_0003_endminf
// (architecture spec §7.1: remove "_postmodel(Clone)#123" style suffixes.)
static void NormalizeCharacterId(const char *goName, char *out, int sz) {
  out[0] = 0;
  if (!goName || !*goName) return;
  const char *start = strstr(goName, "chr_");
  if (!start) {
    snprintf(out, sz, "%s", goName);
    return;
  }
  char tmp[192];
  snprintf(tmp, sizeof(tmp), "%s", start);
  // strip at the first known runtime suffix (case-insensitive)
  static const char *kSuffixes[] = {"_postmodel", "(clone)", "#"};
  size_t cut = strlen(tmp);
  for (const char *sfx : kSuffixes) {
    // case-insensitive search
    const char *p = tmp;
    while ((p = strstr(p, sfx)) != nullptr) {
      // verify the whole suffix matches case-insensitively at p
      bool match = true;
      for (size_t i = 0; sfx[i]; i++) {
        char a = p[i], b = sfx[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) { match = false; break; }
      }
      if (match) {
        size_t pos = (size_t)(p - tmp);
        if (pos < cut) cut = pos;
        break;
      }
      p++;
    }
  }
  tmp[cut] = 0;
  snprintf(out, sz, "%s", tmp);
}

// ---- known-character collector ----
// Records every main-character GameObject name seen (raw, with runtime
// suffixes) so the user can build their preset from the real id list.
// Written from the PreLateTick path (append-only, no allocation); the
// worker thread flushes it to developer/discovered_characters.json.
#define KNOWN_CHARS_MAX 128
static char g_knownChars[KNOWN_CHARS_MAX][192];
static volatile LONG g_knownCharCount = 0;
static CRITICAL_SECTION g_knownCharLock;

static void KnownCharactersInit() {
  InitializeCriticalSection(&g_knownCharLock);
}

static void KnownCharacterNote(const char *goName) {
  if (!goName || !*goName) return;
  EnterCriticalSection(&g_knownCharLock);
  LONG n = g_knownCharCount;
  for (LONG i = 0; i < n; i++) {
    if (strcmp(g_knownChars[i], goName) == 0) {
      LeaveCriticalSection(&g_knownCharLock);
      return;  // already recorded
    }
  }
  if (n < KNOWN_CHARS_MAX) {
    snprintf(g_knownChars[n], 192, "%s", goName);
    InterlockedIncrement(&g_knownCharCount);
  }
  LeaveCriticalSection(&g_knownCharLock);
}

// Flush the collected names to developer/discovered_characters.json (worker).
static void KnownCharactersFlush() {
  EnterCriticalSection(&g_knownCharLock);
  LONG n = g_knownCharCount;
  if (n == 0) {
    LeaveCriticalSection(&g_knownCharLock);
    return;
  }
  char path[512];
  if (!RuntimePathsInit()) {
    LeaveCriticalSection(&g_knownCharLock);
    return;
  }
  RuntimePath(path, sizeof(path), "developer\\discovered_characters.json");
  FILE *f = fopen(path, "w");
  if (f) {
    fprintf(f, "{\n  \"discovered_characters\": [\n");
    for (LONG i = 0; i < n; i++) {
      fprintf(f, "    %s\"%s\"%s\n", i ? "," : "", g_knownChars[i],
              (i == n - 1) ? "" : ",");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
  }
  LeaveCriticalSection(&g_knownCharLock);
}
