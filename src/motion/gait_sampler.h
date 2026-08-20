#pragma once
// motion/gait_sampler.h — main-character gait sampling (baseline
// ProbeSampleGait semantics):
//   - entity refresh every 500ms (squad-swap following)
//   - ONLY the main animator is sampled (PreLateTick fires for every member)
//   - 20Hz clip sampling via AnimatorClipReader, weight-max selection
//   - character switch -> reset active runtime + re-find bones
//   - bone discovery on demand
#include <cstring>
#include "../character/active_character.h"
#include "../character/bone_resolver.h"
#include "../common/logger.h"
#include "../config/config_types.h"
#include "../il2cpp/animator_clip_reader.h"
#include "gait_classifier.h"

class GaitSampler {
public:
  GaitSampler(AnimatorClipReader &reader) : reader_(reader) {}

  // Hot-reload / forced refresh: reset the active runtime but keep the
  // animator, then re-identify + re-bind profile from the NEW snapshot.
  // Runs on the main thread (PreLateTick).
  void ForceRefresh(ActiveCharacterRuntime &active, const ConfigSnapshot &cfg) {
    void *animator = active.animator;
    ProbeLog("[CFG] hot reload -> refreshing active character\n");
    ResetActiveCharacter(active);
    active.animator = animator;
    if (animator) {
      ResolveCharacterId(active);
      active.rootTransform = SafeGetComponentTransform(animator);
      BindProfile(active, cfg);
    }
    active.valid = animator != nullptr;
  }

  // Called from PreLateTick (per instance).  Internally filters to the main
  // character only.  (The per-frame squad call counter lives in
  // MotionEngine — it must count EVERY callback, not just the main one.)
  void ObserveCallback(void *callerAnimator, const ConfigSnapshot &cfg,
                       ActiveCharacterRuntime &active) {
    DWORD now = NowMs();

    // 500ms entity refresh (baseline)
    if (now - active.lastIdentityRefreshMs >=
        cfg.global.entityRefreshIntervalMs) {
      active.lastIdentityRefreshMs = now;
      RefreshMainCharacterAnimator();
    }

    // ONLY the main character (baseline filter)
    if (!g_cachedAnimator || callerAnimator != g_cachedAnimator) return;

    // Character switch detection: reset everything, re-find bones
    if (callerAnimator != active.animator) {
      bool isSwitch = (active.animator != nullptr);
      ProbeLog("[SWITCH] animator %p -> %p%s\n", (void *)active.animator,
               (void *)callerAnimator, isSwitch ? " (re-finding)" : "");
      ResetActiveCharacter(active);
      active.animator = callerAnimator;
      ResolveCharacterId(active);
      active.rootTransform = SafeGetComponentTransform(callerAnimator);
      BindProfile(active, cfg);
      active.valid = true;
    }

    // Bone discovery when missing (main character only)
    if (!active.bones.found && active.profile && active.profile->enabled &&
        active.rootTransform) {
      if (!ResolveBreastBonesWithProfile(active.rootTransform,
                                         *active.profile, active.bones)) {
        ProbeLog("[BONE] not found yet (animator=%p)\n",
                 (void *)active.animator);
      } else {
        // Axis: explicit preset value wins; otherwise the bone-family
        // default discovered from the actual bone name (spec §8.2 — family
        // defaults are the profile-generation rule, explicit config the
        // final authority).
        if (active.profile->axisExplicit) {
          active.axis = active.profile->axis.axis;
          active.axisSign = active.profile->axis.sign;
        } else {
          active.axis = active.bones.axis;
          active.axisSign = 1.0f;
        }
        active.boneAmplitudeScale =
            active.bones.ampScale * active.profile->amplitudeScale;
        ProbeLog("[BONE] R=%s L=%s axis=%d%s sign=%+.0f scale=%.2f\n",
                 active.bones.rightName, active.bones.leftName,
                 (int)active.axis, active.profile->axisExplicit ? "" : "(auto)",
                 active.axisSign, active.boneAmplitudeScale);
      }
    }

    // 20Hz sampling (baseline)
    if (now - active.lastGaitSampleMs < cfg.global.gaitSampleIntervalMs)
      return;
    active.lastGaitSampleMs = now;

    ClipSample clips[8];
    size_t count = 0;
    if (!reader_.ReadLayer0(callerAnimator, clips, 8, count)) return;

    int bestGait = GaitNone;
    float bestW = -1.0f;
    bool transToIdle = false;
    bool landing = false;
    active.jumpDetected = false;  // recomputed every sample
    for (size_t i = 0; i < count; i++) {
      GaitClassification cls = ClassifyClipNameFull(clips[i].name);
      if (cls.gait >= GaitIdle && clips[i].weight > bestW) {
        bestW = clips[i].weight;
        bestGait = cls.gait;
      }
      if (cls.transitionToIdle) transToIdle = true;
      if (cls.landingDetected) landing = true;
      if (cls.jumpDetected) active.jumpDetected = true;
    }
    active.currentGait = bestGait;
    active.transitionToIdle = transToIdle;
    active.jumpActive = landing;

    // Diagnostic: dump clip names (helps onboarding new gaits such as
    // zipline/slide).  Rate-limited to ~2s.
    static DWORD s_gaitLogT = 0;
    static DWORD s_clipLogT = 0;
    if (RateLimit(s_gaitLogT, 2000))
      ProbeLog("[GAIT] gait=%d count=%zu\n", bestGait, count);
    if (RateLimit(s_clipLogT, 2000)) {
      float spd = reader_.ReadAnimatorSpeed(callerAnimator);
      // actual loop frequency from normalizedTime delta (includes
      // state-speed multipliers that Animator.speed misses)
      static float s_lastNorm = -1.0f;
      static DWORD s_lastNormT = 0;
      float cyc = -1.0f;
      float norm = reader_.ReadNormalizedTime(callerAnimator);
      if (norm >= 0.0f && s_lastNorm >= 0.0f && s_lastNormT) {
        float dNorm = norm - s_lastNorm;
        if (dNorm < 0.0f) dNorm += 1.0f;  // wrapped
        float dt = (float)(NowMs() - s_lastNormT) / 1000.0f;
        if (dt > 0.1f) cyc = dNorm / dt;
      }
      s_lastNorm = norm;
      s_lastNormT = NowMs();
      static DWORD s_normDiagT = 0;
      if (RateLimit(s_normDiagT, 5000))
        ProbeLog("[NORM] norm=%.3f off=%d stateinfo=%p\n",
                 norm, (int)reader_.NormOffset(), (void *)reader_.StateInfoMethod());
      for (size_t i = 0; i < count && i < 8; i++)
        ProbeLog("[CLIP] %s dur=%.3fs spd=%.2f eff=%.3fs cyc=%.2f/s w=%.2f\n",
                 clips[i].name[0] ? clips[i].name : "(none)",
                 clips[i].durationSec, spd,
                 clips[i].durationSec > 0.001f
                     ? clips[i].durationSec / spd
                     : 0.0f,
                 cyc >= 0.0f ? cyc : -1.0f,
                 clips[i].weight);
    }
  }

private:
  // Bind the active character's profile from the current snapshot
  // (used on switch AND on hot reload).
  static void BindProfile(ActiveCharacterRuntime &active,
                          const ConfigSnapshot &cfg) {
    active.profile = nullptr;
    active.profileFound = false;
    if (cfg.characters.count(active.characterId) > 0) {
      active.profile = &cfg.characters.at(active.characterId);
      active.profileFound = true;
      if (!active.profile->enabled) {
        ProbeLog("[CHAR] id=%s profile=DISABLED -> native only\n",
                 active.characterId);
      } else {
        ProbeLog("[CHAR] id=%s profile=FOUND mode=%d\n",
                 active.characterId, (int)active.profile->motionMode);
      }
    } else {
      ProbeLog("[CHAR] id=%s UNSUPPORTED -> native only\n",
               active.characterId);
    }
  }

  AnimatorClipReader &reader_;
};
