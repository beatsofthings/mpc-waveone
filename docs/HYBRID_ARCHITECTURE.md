# WaveOne Hybrid – architecture and implementation plan

Status: design specification; not a claim that these engines are implemented.

## Three independent engine slots
Each slot has its own enable, type, source, pitch, gain, pan, envelope and modulation inputs.
Engine type (stable enum values): 0=Wavetable, 1=Granular, 2=Virtual Analog, 3=FM6.
Existing v0.7 project parameters and VST indices must not be reordered; append new keys.

### Wavetable
Factory banks; per-slot user WAV table import; interpolation, warp, scan, unison.
Never load/decode WAV on the audio thread. Store normalized relative source identifiers and missing-file fallback.

### Granular
Independent sample source and per-slot size, density, position, spray, pitch, reverse, window, stereo spread, jitter and mix.
Use preallocated voices/grains; no filesystem or memory allocations in render.

### Virtual Analog
Two oscillators within one slot: saw/pulse/triangle/sine, PWM, oscillator sync, cross-mod, drift, noise, detune, filter routing.
Jupiter-inspired architecture, not proprietary Roland code.

### FM6
Six operators, 32 DX7 algorithm topologies, ratio/fixed frequency, feedback, operator envelopes, key scaling, velocity.
DX7 SysEx import must validate checksums, packed voice format, and algorithm/parameter mapping.
Do not advertise verified DX7 compatibility before regression tests against known patches.

## Modulation matrix
Minimum 16 slots. Each routing is source, destination, signed amount, optional curve, polarity and smoothing.
Sources: LFO1, LFO2, LFO3, Amp Env, Filter Env, Mod Env, Velocity, Keytrack, Mod Wheel, Aftertouch, Random, Macros 1–4.
Destinations: each OSC pitch/level/pan/wavetable position/grain position/grain density, FM index, VA PWM, filters cutoff/resonance, FX mix.
Apply global/per-voice sources at correct rates; smooth discontinuous parameter changes.

## UI
OSC overview plus independent OSC1/OSC2/OSC3 edit tabs; per-engine conditional panels or separate subpages.
Keep controls within MPC skin safe bounds and use skin preview checks.
3D wavetable animation is parameter-stepped artwork, not a continuous 3D renderer.

## Milestones / release gates
1. Stabilize existing v0.7 layout and independent granular settings; test on device.
2. User WAV load and factory banks, safe missing-file handling, project restore.
3. VA engine with unit tests and CPU budget.
4. FM6 and DX7 patch importer with comparison fixtures.
5. 16-slot matrix with parameter automation tests.
6. Preset migration, stress/CPU test, complete skin preview and hardware smoke test.

Do not replace a working MPC install until ARM64 CI, packaging, preview and hardware smoke test pass.
