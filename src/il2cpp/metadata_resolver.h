#pragma once
// il2cpp/metadata_resolver.h — resolve game classes/methods/fields via
// IL2CPP metadata (architecture spec §16.3).  Every symbol is logged with a
// PASS/FAIL diagnostic.  Historical baseline values (m_animator=0x70,
// SyncCalc RVA 0x06DDEF9C) are used ONLY for diagnostic comparison and are
// never the production path (§16.5: reflect -> compare -> PASS -> switch).
#include "../common/logger.h"
#include "il2cpp_api.h"
#include "runtime_symbols.h"

// Historical baseline values (diagnostic comparison only — spec §16.5).
static const int kHistoricalAnimatorFieldOffset = 0x70;
static const uintptr_t kHistoricalSyncCalcRva = 0x06DDEF9C;

static bool PtrInGameAssembly(void *p) {
  if (!p || !g_gaMod) return false;
  return (uintptr_t)p >= (uintptr_t)g_gaMod;
}

static void *MethodPointer(const void *methodInfo) {
  if (!methodInfo) return nullptr;
  // MethodInfo offset 0 = methodPointer (stable on Unity 2021.3).
  __try {
    return *(void **)methodInfo;
  } __except (1) {
    return nullptr;
  }
}

// Resolve everything the hook chain needs.  Returns false only when the
// REQUIRED PreLateTick symbol is missing (plugin must go DISABLED_SAFE).
static bool ResolveRuntimeSymbols(RuntimeSymbols &s) {
  void *domain = il2cpp_domain_get();
  size_t ac = 0;
  void **asms = domain ? il2cpp_domain_get_assemblies(domain, &ac) : nullptr;
  if (!asms || !ac) {
    Log("[REFLECT] no assemblies -> DISABLED");
    return false;
  }

  // ---- classes ----
  s.animatorMonoClass =
      FindClass("Beyond.Gameplay.View.Animation", "AnimatorMono", asms, ac);
  s.npcCpuAnimatorClass =
      FindClass("Beyond.NPC.Animation", "NPCCPUAnimator", asms, ac);
  s.syncMonoClass = FindClass("Beyond.NPC.Animation",
                              "ScriptAnimationJobSyncMono", asms, ac);
  s.animatorClass = FindClass("UnityEngine", "Animator", asms, ac);
  s.animatorClipInfoClass = FindClass("UnityEngine", "AnimatorClipInfo", asms, ac);

  // ---- AnimatorMono.m_animator field (was self+0x70) ----
  // STRICT (V2 §36): reflection failure = unsupported game version, no
  // write.  No fallback to the historical offset.
  if (s.animatorMonoClass) {
    s.animatorFieldOffset =
        FindFieldOffsetInHierarchy(s.animatorMonoClass, "m_animator");
    if (s.animatorFieldOffset >= 0) {
      s.animatorFieldPass = true;
      Log("[REFLECT] AnimatorMono.m_animator offset=0x%X (historical 0x%X) %s",
          s.animatorFieldOffset, kHistoricalAnimatorFieldOffset,
          (s.animatorFieldOffset == kHistoricalAnimatorFieldOffset)
              ? "MATCH"
              : "DIFFERS");
    } else {
      Log("[REFLECT] AnimatorMono.m_animator NOT FOUND -> STRICT FAIL "
          "(unsupported game version)");
      return false;  // required symbol missing -> DISABLED_SAFE
    }
  } else {
    Log("[REFLECT] AnimatorMono class NOT found -> STRICT FAIL");
    return false;
  }

  // ---- AnimatorMono.PreLateTick (REQUIRED) ----
  if (s.animatorMonoClass) {
    __try {
      s.preLateTick = il2cpp_class_get_method_from_name(
          s.animatorMonoClass, "PreLateTick", 1);
    } __except (1) {
      s.preLateTick = nullptr;
    }
    if (s.preLateTick) {
      s.preLateTickFn = MethodPointer(s.preLateTick);
      s.preLateTickPass = s.preLateTickFn && PtrInGameAssembly(s.preLateTickFn);
      Log("[REFLECT] AnimatorMono.PreLateTick fn=%p %s", s.preLateTickFn,
          s.preLateTickPass ? "PASS" : "FAIL(outside GameAssembly)");
    } else {
      Log("[REFLECT] AnimatorMono.PreLateTick NOT found");
    }
  }

  // ---- NPCCPUAnimator.LateTick (OPTIONAL) ----
  if (s.npcCpuAnimatorClass) {
    __try {
      s.lateTick = il2cpp_class_get_method_from_name(
          s.npcCpuAnimatorClass, "LateTick", 1);
    } __except (1) {
      s.lateTick = nullptr;
    }
    if (s.lateTick) {
      s.lateTickFn = MethodPointer(s.lateTick);
      s.lateTickPass = s.lateTickFn && PtrInGameAssembly(s.lateTickFn);
      Log("[REFLECT] NPCCPUAnimator.LateTick fn=%p %s", s.lateTickFn,
          s.lateTickPass ? "PASS" : "FAIL(outside GameAssembly)");
    } else {
      Log("[REFLECT] NPCCPUAnimator.LateTick NOT found (optional)");
    }
  }

  // ---- ScriptAnimationJobSyncMono.CalcLayerMainStream (OPTIONAL) ----
  // Production path = resolved method pointer.  The historical RVA is only
  // compared for a PASS diagnostic (spec §16.5).
  if (s.syncMonoClass) {
    __try {
      s.calcLayerMainStream = il2cpp_class_get_method_from_name(
          s.syncMonoClass, "CalcLayerMainStream", 1);
    } __except (1) {
      s.calcLayerMainStream = nullptr;
    }
    if (s.calcLayerMainStream) {
      s.syncCalcFn = MethodPointer(s.calcLayerMainStream);
      s.syncCalcPass = s.syncCalcFn && PtrInGameAssembly(s.syncCalcFn);
      uintptr_t rva = s.syncCalcFn
                          ? (uintptr_t)s.syncCalcFn - (uintptr_t)g_gaMod
                          : 0;
      Log("[REFLECT] CalcLayerMainStream fn=%p rva=0x%08llX (historical "
          "0x06DDEF9C) %s",
          s.syncCalcFn, (unsigned long long)rva,
          (s.syncCalcPass && rva == kHistoricalSyncCalcRva) ? "MATCH" : "DIFFERS");
    } else {
      Log("[REFLECT] CalcLayerMainStream NOT found (optional)");
    }
  }

  return s.preLateTickPass;
}