#pragma once
// config/config_types.h — immutable config snapshot + character profiles.
// V1: loaded once at startup, restart-only reload.  No runtime mutation.
#include <cstdint>
#include <string>
#include <unordered_map>

enum class MotionMode {
  Off = 0,
  Synthetic = 1,
  AmplifyNative = 2
};

enum class Axis {
  X = 0,
  Y = 1,
  Z = 2
};

// Gait enum: -1 none, 0 idle, 1 walk, 2 run, 3 sprint, 4 zipline.
enum Gait {
  GaitNone = -1,
  GaitIdle = 0,
  GaitWalk = 1,
  GaitRun = 2,
  GaitSprint = 3,
  GaitZipline = 4
};

struct GaitParam {
  float amplitudeDeg = 0.0f;      // up (+direction) amplitude
  float amplitudeDownDeg = 0.0f;  // down (-direction); 0 = symmetric (=up)
  float frequencyHz = 1.0f;
};

struct BoneConfig {
  std::string rightName;   // explicit override; empty = use candidates
  std::string leftName;
  bool allowFallbackCandidates = true;
};

struct AxisConfig {
  Axis axis = Axis::Z;
  float sign = 1.0f;       // +1 or -1
};

struct EnvelopeConfig {
  float amplitudeAttackTauSec = 0.15f;
  float frequencyTauSec = 0.20f;
  float toIdleReleaseTauSec = 0.015f;
};

struct JumpConfig {
  bool enabled = false;
  std::string mode = "off";      // V1: off | landing_damped
  float amplitudeDeg = 10.0f;    // landing_damped params (baseline values)
  float dampingTauSec = 0.3f;
  float frequencyHz = 1.5f;
  float maxDurationSec = 1.2f;
};

struct NativeAmplifyConfig {
  float factor = 2.0f;           // K exponent (baseline g_ampK)
};

// axisExplicit=true  -> use AxisConfig as-is (preset overrides family)
// axisExplicit=false -> auto: axis/sign come from the bone-family default
//                       discovered at runtime (girl/lady->Z, xiong->Y)
struct CharacterProfile {
  bool enabled = false;
  MotionMode motionMode = MotionMode::Off;

  BoneConfig bones;
  AxisConfig axis;
  bool axisExplicit = false;   // set by ConfigLoader when "axis" key present
  float amplitudeScale = 1.0f; // EXTRA scale on top of the bone-family
                               // default (xiong family already includes 0.4;
                               // leave at 1.0 unless fine-tuning a character)

  GaitParam idle;    // amplitudeDeg 0.0  / 1.2 Hz
  GaitParam walk;    // 3.6 / 1.5
  GaitParam run;     // 8.5 / 1.7
  GaitParam sprint;  // 12.0 / 2.0
  GaitParam zipline{8.5f, 0.0f, 1.7f};  // zipline slide (interact_zipline_*)
                                         // defaults = run-family values

  EnvelopeConfig envelope;
  JumpConfig jump;
  NativeAmplifyConfig nativeAmplify;
};

struct PartyCompensationConfig {
  bool enabled = true;
  std::string strategy = "legacy_four_stack";
  float fourMemberFactor = 0.25f;   // legacy V1 factor (kept for compat)
  float transitionTauSec = 0.15f;   // smoothing into/out of stack state
  float alpha = 1.0f;               // V2 formula: factor = 1/(1+a*(N-1))
};

struct GlobalConfig {
  PartyCompensationConfig partyCompensation;
  uint32_t gaitSampleIntervalMs = 50;    // 20 Hz
  uint32_t entityRefreshIntervalMs = 500;
  uint32_t replayVerifyWindowMs = 150;
  // Marker compatibility: V2 default OFF (config-driven).  When true, the
  // spring_test.txt / amplify_test.txt markers gate writes (V1 behavior).
  bool legacyMarkerMode = false;
};

struct ConfigSnapshot {
  std::string presetName;
  uint32_t schemaVersion = 1;
  int revision = 0;  // runtime/config.json revision (hot-apply ACK)
  // Global plugin switch: config.json "enabled"=false -> NO WRITE for any
  // character (clean native game), config still loads for instant re-enable.
  bool pluginEnabled = true;

  GlobalConfig global;
  std::unordered_map<std::string, CharacterProfile> characters;

  uint64_t contentHash = 0;
  bool valid = false;
};

// Known bone-family defaults (architecture spec §28):
//   breast_*    -> Z, scale 1.0
//   R/L_breast_* -> Z, scale 1.0
//   xiong_*     -> Y, scale 0.4
struct BoneFamilyDefaults {
  Axis axis;
  float scale;
};

static BoneFamilyDefaults BoneFamilyDefaultByName(const char *boneName) {
  if (boneName && strstr(boneName, "xiong"))
    return {Axis::Y, 0.4f};
  return {Axis::Z, 1.0f};
}
