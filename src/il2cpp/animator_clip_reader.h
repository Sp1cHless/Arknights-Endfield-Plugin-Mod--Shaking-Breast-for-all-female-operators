#pragma once
// il2cpp/animator_clip_reader.h — Animator.GetCurrentAnimatorClipInfo layer-0
// reader.  Business code sees only ClipSample{clip, weight, name}; all IL2CPP
// ABI details (+0x20 + i*0x18 element layout, boxed reads) stay inside this
// class (architecture spec §16.4).  Reflection-resolved, no fixed RVAs.
#include <cstring>
#include "../common/logger.h"
#include "il2cpp_api.h"

struct ClipSample {
  void *clip = nullptr;
  float weight = 0.0f;
  float durationSec = 0.0f;  // AnimationClip.length (loop period), 0 if unread
  char name[128] = {0};
};

// FindMethod variant requiring the first parameter type name (baseline
// ProbeFindMethodWithFirstParamType semantics — the plain FindMethod(…,1)
// variant returned null for GetCurrentAnimatorClipInfo on this game).
static void *FindMethodWithFirstParamType(void *klass, const char *name,
                                          const char *firstParamType) {
  if (!klass) return nullptr;
  void *it = nullptr, *m;
  while ((m = il2cpp_class_get_methods(klass, &it))) {
    const char *mn = il2cpp_method_get_name(m);
    if (!mn || strcmp(mn, name) != 0) continue;
    uint32_t pc = il2cpp_method_get_param_count(m);
    if (pc < 1) continue;
    void *param = il2cpp_method_get_param(m, 0);
    const char *tn = param ? il2cpp_type_get_name(param) : nullptr;
    if (tn && strcmp(tn, firstParamType) == 0) return m;
  }
  return nullptr;
}

class AnimatorClipReader {
public:
  // Resolve the three methods once (from the animator's own class).
  bool EnsureResolved(void *animator) {
    if (mCount_ && mInfo_ && mId2Clip_) return true;
    void *domain = il2cpp_domain_get();
    size_t ac = 0;
    void **asms = domain ? il2cpp_domain_get_assemblies(domain, &ac) : nullptr;
    if (!asms || !ac) return false;

    __try {
      void *klass = il2cpp_object_get_class(animator);
      if (!klass) return false;
      if (!mCount_)
        mCount_ = il2cpp_class_get_method_from_name(
            klass, "GetCurrentAnimatorClipInfoCount", 1);
      if (!mInfo_)
        mInfo_ = FindMethodWithFirstParamType(klass,
                                              "GetCurrentAnimatorClipInfo",
                                              "System.Int32");
      if (!mId2Clip_) {
        // FindClass needs the assembly list (nullptr fails — SOT-era bug fix)
        void *cic = FindClass("UnityEngine", "AnimatorClipInfo", asms, ac);
        if (cic)
          mId2Clip_ = FindMethod(cic, "InstanceIDToAnimationClipPPtr", 1);
      }
    } __except (1) {
      return false;
    }
    if (!mCount_ || !mInfo_ || !mId2Clip_)
      ProbeLog("[GAIT] resolve fail c=%p i=%p id=%p\n",
               (void *)mCount_, (void *)mInfo_, (void *)mId2Clip_);
    return mCount_ && mInfo_ && mId2Clip_;
  }

  // Read layer-0 clips into out[] (max capacity).  Returns count read.
  bool ReadLayer0(void *animator, ClipSample *out, size_t capacity,
                  size_t &count) {
    count = 0;
    if (!animator || capacity == 0) return false;
    if (!EnsureResolved(animator)) return false;
    __try {
      int layer = 0;
      void *args[] = {&layer};
      void *exc = nullptr;

      void *countRes = il2cpp_runtime_invoke(mCount_, animator, args, &exc);
      int n = UnboxInt32(countRes);
      if (n <= 0 || n > 64) return false;

      void *infoArr = il2cpp_runtime_invoke(mInfo_, animator, args, &exc);
      if (!infoArr || exc) return false;

      size_t nRead = 0;
      for (int i = 0; i < n && nRead < capacity; i++) {
        int instanceId = 0;
        // 0x0 is the real instanceId (verified: id0 valid, id10 garbage) —
        // try 0x0 FIRST, then 0x10 as fallback probe.
        for (int off : {0x0, 0x10}) {
          __try {
            instanceId = *(int *)((char *)infoArr + IL2CPP_ARRAY_DATA +
                                  (size_t)i * 0x18 + off);
          } __except (1) {
            instanceId = 0;
          }
          if (instanceId) break;
        }
        if (!instanceId) continue;

        void *idArgs[] = {&instanceId};
        void *clip = il2cpp_runtime_invoke(mId2Clip_, nullptr, idArgs, &exc);
        if (!clip) continue;

        ClipSample &s = out[nRead];
        s.clip = clip;
        s.name[0] = 0;
        s.durationSec = ReadClipDuration(clip);  // cheap: cached method
        if (g_object_get_name) {
          void *ns = Invoke(g_object_get_name, clip);
          if (ns) ReadStrUtf8(ns, s.name, sizeof(s.name));
        }
        __try {
          s.weight = *(float *)((char *)infoArr + IL2CPP_ARRAY_DATA +
                                (size_t)i * 0x18 + 8);
        } __except (1) {
          s.weight = 0.0f;
        }
        nRead++;
      }
      count = nRead;
      return true;
    } __except (1) {
      return false;
    }
  }

  // Animator.speed (global playback multiplier).  The game does NOT play
  // locomotion clips at 1.0 — effective loop period = clip.length / speed.
  int32_t NormOffset() const { return normOff_; }
  void *StateInfoMethod() const { return mStateInfo_; }
  float ReadAnimatorSpeed(void *animator) {
    if (!animator) return 1.0f;
    __try {
      void *klass = il2cpp_object_get_class(animator);
      if (!klass) return 1.0f;
      void *m = il2cpp_class_get_method_from_name(klass, "get_speed", 0);
      if (!m) return 1.0f;
      void *exc = nullptr;
      void *res = il2cpp_runtime_invoke(m, animator, nullptr, &exc);
      if (!res || exc) return 1.0f;
      float s = *(float *)((char *)res + IL2CPP_BOXED_DATA);
      return (s > 0.01f && s < 10.0f) ? s : 1.0f;
    } __except (1) {
      return 1.0f;
    }
  }

  // Current state normalizedTime via Animator.GetCurrentAnimatorStateInfo(0).
  // Returns the RAW normalized time (0..1, wraps) or -1 if unavailable.
  // The 2s-sampled DELTA of this value = actual loop frequency (Hz), which
  // already includes state-speed multipliers that Animator.speed misses.
  float ReadNormalizedTime(void *animator) {
    if (!animator) return -1.0f;
    if (!mStateInfo_ || normOff_ < 0) {
      __try {
        void *klass = il2cpp_object_get_class(animator);
        if (!klass) return -1.0f;
        if (!mStateInfo_)
          mStateInfo_ = il2cpp_class_get_method_from_name(
              klass, "GetCurrentAnimatorStateInfo", 1);
        if (normOff_ < 0) {
          void *domain = il2cpp_domain_get();
          size_t ac = 0;
          void **asms = domain ? il2cpp_domain_get_assemblies(domain, &ac) : nullptr;
          void *asi = asms ? FindClass("UnityEngine", "AnimatorStateInfo", asms, ac) : nullptr;
          if (asi) {
            void *it = nullptr, *f;
            while ((f = il2cpp_class_get_fields(asi, &it))) {
              const char *fn = il2cpp_field_get_name(f);
              if (fn && strcmp(fn, "m_NormalizedTime") == 0) {
                normOff_ = (int32_t)il2cpp_field_get_offset(f);
                break;
              }
            }
          }
          // Unity's AnimatorStateInfo layout is stable across 2019-2023:
          // m_NormalizedTime is the 4th field (offset 0xC).  Fallback only.
          if (normOff_ < 0) normOff_ = 0xC;
        }
      } __except (1) {
        return -1.0f;
      }
      if (!mStateInfo_ || normOff_ < 0) return -1.0f;
    }
    __try {
      int layer = 0;
      void *args[] = {&layer};
      void *exc = nullptr;
      void *res = il2cpp_runtime_invoke(mStateInfo_, animator, args, &exc);
      if (!res || exc) return -1.0f;
      // IL2CPP returns value-type results UNBOXED: res points straight at
      // the struct data (no object header).  normOff_ is the field offset.
      return *(float *)((char *)res + normOff_);
    } __except (1) {
      return -1.0f;
    }
  }

private:
  void *mStateInfo_ = nullptr;
  int32_t normOff_ = -1;

  // AnimationClip.length (loop period in seconds) via get_length method.
  // Method cached per AnimationClip class; ~1 invoke per clip sample.
  float ReadClipDuration(void *clip) {
    if (!clip) return 0.0f;
    __try {
      void *klass = il2cpp_object_get_class(clip);
      if (!klass) return 0.0f;
      void *m = il2cpp_class_get_method_from_name(klass, "get_length", 0);
      if (!m) return 0.0f;
      void *exc = nullptr;
      void *res = il2cpp_runtime_invoke(m, clip, nullptr, &exc);
      if (!res || exc) return 0.0f;
      // boxed float: object header + 4 bytes payload (same as UnboxInt32)
      return *(float *)((char *)res + IL2CPP_BOXED_DATA);
    } __except (1) {
      return 0.0f;
    }
  }

  // Animator.speed (global playback multiplier).  The game does NOT play
  // locomotion clips at 1.0 — effective loop period = clip.length / s
  void *mCount_ = nullptr;
  void *mInfo_ = nullptr;
  void *mId2Clip_ = nullptr;
};
