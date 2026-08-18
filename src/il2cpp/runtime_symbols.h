#pragma once
// il2cpp/runtime_symbols.h — resolved game symbols (architecture spec §16.2).
// All pointers resolved once at startup via metadata; the motion core reads
// this struct and never hardcodes RVAs/offsets.
#include <cstddef>

struct RuntimeSymbols {
  // classes
  void *animatorMonoClass = nullptr;
  void *npcCpuAnimatorClass = nullptr;
  void *syncMonoClass = nullptr;
  void *animatorClass = nullptr;
  void *animatorClipInfoClass = nullptr;

  // methods
  void *preLateTick = nullptr;        // MethodInfo
  void *lateTick = nullptr;           // MethodInfo
  void *calcLayerMainStream = nullptr;  // MethodInfo

  // fields
  int animatorFieldOffset = -1;       // AnimatorMono.m_animator (resolved)

  // derived raw pointers (methodPointer)
  void *preLateTickFn = nullptr;
  void *lateTickFn = nullptr;
  void *syncCalcFn = nullptr;

  // resolution health
  bool animatorFieldPass = false;
  bool preLateTickPass = false;
  bool lateTickPass = false;
  bool syncCalcPass = false;
};
