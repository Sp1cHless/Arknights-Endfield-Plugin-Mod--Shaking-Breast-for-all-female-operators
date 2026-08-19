# Secondary Motion Manager for Arknights: Endfield

> An unofficial open-source runtime secondary-motion tool and character-profile manager for **Arknights: Endfield**.

This project adds configurable runtime secondary motion to supported characters while preserving the game's normal locomotion animation. It includes a lightweight Windows manager for per-character settings, presets, live configuration updates, native-motion amplification, and developer tools for adding newly supported characters.

The project grew out of a reverse-engineering investigation of Endfield's Unity/IL2CPP animation pipeline. The final approach does **not** replace locomotion clips. It identifies the actively controlled character, resolves a verified chest-bone pair, observes gait state from the running Animator, and applies controlled quaternion deltas at verified points in the animation update chain.

> **Unofficial project.** Not affiliated with Hypergryph or GRYPHLINE.

---

## Features

- Per-character enable/disable
- Three motion modes:
  - **Original** — no custom transform writes
  - **Synthetic** — configurable gait-driven secondary motion
  - **Amplify Native** — amplifies the character's existing native motion
- Independent Walk / Run / Sprint /Zipline amplitude and frequency
- Smooth gait transitions and configurable return-to-idle behavior
- Character-specific axis, direction, and amplitude scaling
- Preset management
- Runtime configuration apply/reload
- Automatic active-character detection
- Fail-closed behavior for unsupported characters
- Developer tools for character detection, bone inspection, axis testing, tuning, validation, and profile creation
- Lightweight external Windows UI; no in-game overlay required

The current product scope focuses on the **actively controlled character**. Background party members are not a visual target of the project.

---

## Reverse-Engineering Overview

The project began by asking a simple question: why could different model modifications show dramatically different visible secondary motion even when the underlying body animation appeared similar?

That led to several stages of static analysis, IL2CPP inspection, runtime hooks, transform recording, same-rig animation tests, and controlled write experiments. A number of early animation hypotheses were rejected as stronger runtime evidence became available.

The main findings are summarized below.

### 1. Mesh/bone mapping was an important clue

Static inspection of existing XXMI/EFMI model modifications showed that changing breast-related palette/bone mappings could produce a large visible difference without redesigning the whole rig.

This was useful for locating relevant transform families and motivated direct runtime bone investigation.

One important correction from the research: per-draw-call palette indices are **not global bone IDs**. Earlier labels such as `b21` / `b22` / `b23` / `b24` must be interpreted inside the component/draw call where they were observed.

### 2. The locomotion clips did not contain the expected chest curves

The original plan explored direct animation-clip reuse.

Runtime single-object tests were then performed on shared locomotion clips, including same-rig tests. The tested girl/lady locomotion clips produced no meaningful chest-bone rotation curves when evaluated directly.

That closed the original “copy the locomotion chest curves” route and strongly supported a later runtime secondary process rather than reusable chest curves embedded directly in those locomotion clips.

### 3. Direct runtime Transform control works

A major milestone was proving that writing the relevant chest-bone `Transform.localRotation` at runtime produces visible motion.

Native-motion amplification was also verified. The corrected amplification path operates on a quaternion delta relative to a base/reference orientation:

```text
delta  = inverse(base) * current
output = base * delta^K
```

With the corrected quaternion math, `K = 2` produces a true 2x amplification of the rotational delta.

### 4. Update order was the difficult part

Writing the correct rotation once was not sufficient. Later animation/runtime jobs could overwrite it.

The investigation therefore focused on **when** a value must be written so that it survives until rendering.

The proven runtime design keeps the primary synthetic-motion update around `AnimatorMono.PreLateTick`, with later replay/write points in the Animator synchronization chain used as reinforcement.

The key reverse-engineering lesson was:

> The problem was not only “what rotation should be written?” but also “where in the game's animation pipeline can that rotation survive?”

### 5. Gait can be inferred from the active Animator clips

The runtime samples the controlled character's Animator and classifies the active locomotion clip into states such as:

```text
Idle
Walk
Run
Sprint
```

This lets the secondary-motion system react to the existing locomotion state machine without replacing it.

### 6. Smooth motion required separating gait state from output phase

Synthetic motion uses a continuous oscillator:

```text
angle(t) = A(t) * sin(phi(t))
```

where:

- `A(t)` is a smoothed amplitude envelope
- `phi(t)` is a continuously integrated phase
- target frequency is also smoothed

The phase is not reset when changing gait. This avoids hard cuts between walk, run, and sprint.

A separate fast to-idle release handles the end of locomotion.

### 7. Character profiles are necessary

Different characters use different bone-name families, axes, directions, and visual lever arms.

Examples found during research include:

```text
breast_R_01_jnt / breast_L_01_jnt
R_breast_01_jnt / L_breast_01_jnt
xiong_R_0_skin_jnt / xiong_L_0_skin_jnt
```

The same numeric angle can look different on different rigs/models, so the project uses **per-character profiles** instead of one universal global setting.

Unsupported characters fail closed and remain on native animation until a verified profile is added.

### 8. Multiple Animator callbacks exposed a stacking issue

Four-character testing showed that multiple Animator callbacks could participate in the controlled character's update path.

Repeated application of a rotational delta could make visible amplitude much larger than the single-character case. The runtime retains a compatibility compensation mechanism for this callback-stack behavior.

This is treated as an implementation workaround, not as a claim about how multiplayer animation should physically behave.

---

## Runtime Architecture

High-level flow:

```text
Game / IL2CPP Runtime
        |
        v
Active-character detection
        |
        +--> Character ID
        +--> Character profile
        +--> Verified bone pair
        |
        v
Animator gait sampling
        |
        v
Motion Engine
   |            |
   |            +--> Amplify Native
   |
   +--> Synthetic oscillator
          |
          +--> amplitude envelope
          +--> frequency smoothing
          +--> continuous phase
          +--> callback-stack compensation
        |
        v
Quaternion target
        |
        v
Transform.localRotation
```

The injected runtime is `sbm.dll`.

Core technologies:

- C++
- IL2CPP metadata/reflection
- MinHook
- GameAssembly runtime method hooks
- Unity `Transform.localRotation`
- quaternion composition
- JSON configuration
- separate Windows manager UI

The release design avoids relying on old fixed GameAssembly RVAs/field offsets where runtime metadata/reflection resolution is available.

---

## Motion Modes

### Original

Stops custom transform writes for the selected character and leaves the game to use its native behavior.

### Synthetic

Adds a configurable gait-driven oscillator on top of the current bone orientation.

Typical controls:

- Walk / Run / Sprint / Zipline amplitude
- Walk / Run / Sprint / Zipline frequency
- amplitude response smoothing
- frequency response smoothing
- return-to-idle smoothing
- axis
- direction
- character amplitude scale

### Amplify Native

Amplifies motion already produced by the game.

This is useful when the native motion pattern is already desirable and only needs a stronger response.

---

## Character Profiles

Technical character data is separated from normal user preferences.

A character profile can contain:

- canonical character ID
- display name
- left/right bone names
- rotation axis
- direction/sign
- bone amplitude scale
- supported motion modes
- default gait parameters

This lets corrected character mappings be distributed through the default character database without requiring users to rebuild their personal presets.

---

## Developer Mode

Developer Mode exists mainly for maintaining the supported-character database.

Intended workflow:

```text
Detect active character
        ->
Scan skeleton
        ->
Choose/verify left and right bones
        ->
Add/update characters.default.json
```

The goal is to make support for a new character a short profiling task instead of a new reverse-engineering project.

Normal users do not need Developer Mode.

---

## Safety / Fail-Closed Behavior

The runtime favors **no write** over guessing.

Custom writes are disabled when, for example:

- the character is unsupported
- the profile is disabled
- required bones cannot be found
- required runtime symbols cannot be resolved
- a required hook is unavailable
- configuration is invalid
- the active character is switching

The runtime does not intentionally reuse a previous character's transform target for a new character.

---

## Manager

The external Manager provides four main pages:

- **Main**
- **Characters**
- **Presets**
- **Developer**

The Manager edits configuration outside the game and receives runtime status/application acknowledgement.

The UI is deliberately separated from the motion runtime so layout/styling can be changed without rewriting the hook or motion code.

---

## Current Scope / Limitations

- Windows x64
- Focused on the actively controlled character
- Background party-member visuals are not a project requirement
- Jump-specific synthetic motion is currently outside the main supported scope
- New game versions may require a runtime compatibility update
- Unsupported characters remain native until a verified profile is added
- Visual tuning is character-dependent

---

## Technical Stack

### Runtime

```text
C++
IL2CPP reflection / metadata resolution
MinHook
Unity Transform access
JSON configuration
```

### Manager

```text
C#
.NET
WPF
WPF UI / Fluent-style controls
XAML
```

Release builds are intended to be self-contained so normal users do not need the .NET SDK.

---

## Research Methodology

The project was developed through many small runtime experiments rather than one large rewrite.

Investigation tools and methods included:

- XXMI / EFMI model inspection
- Blender mesh/bone visualization
- IL2CPP dumps and metadata inspection
- runtime reflection
- animation clip enumeration
- same-object / same-rig animation tests
- transform recording
- hook call-rate diagnostics
- quaternion read/write verification
- controlled synthetic oscillators
- native-motion amplification experiments

A recurring rule was:

> One experiment should answer one question and have an explicit PASS / FAIL criterion.

That was especially important because multiple early animation hypotheses were later rejected by stronger runtime evidence.

---

## Credits / Related Work

The project builds on and learns from the Endfield modding and reverse-engineering ecosystem, including:

- XXMI / EFMI
- MinHook
- Better-Endfield
- EIEM
- EF-Start-Change
- community IL2CPP tooling and dumps

Please keep upstream licenses and credits intact when redistributing derived code.

---

## Disclaimer

This is an unofficial community project intended for experimentation and modding.

Use it at your own risk. Game updates may change runtime structures or behavior and can temporarily break compatibility.
