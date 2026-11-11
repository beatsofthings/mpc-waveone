# MPC WaveOne

Experimental, lightweight wavetable synthesizer intended for the Akai MPC standalone ecosystem.

**Status: engine proof of concept only. Not yet an MPC plugin.**

## v0.1 engine
- 8-voice polyphony, MIDI note on/off methods
- Three single-cycle waveforms (sine, saw, square), continuously morphable
- ADSR amplitude envelope and output gain
- Stereo float output (mono duplicated); no heap allocations in the audio render function

## Local or GitHub Actions test
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The GitHub Actions `WaveOne Engine CI` workflow performs the same smoke test in the cloud; no local tools required to view results.

## MPC plugin integration (not yet implemented)
The [Open MPC toolchain](https://github.com/open-mpc/mpc-vst-plugins) supports VST2-compatible MPC standalone plugins with native UI pages. The next milestone is to implement its documented module/port interface and Gen2 (aarch64) build and packaging. A successful engine test **does not** prove MPC host compatibility.

The prototype waveforms are not band-limited and can alias at high pitches. Polyphony uses oldest-voice stealing, with no smoothing yet. These are engineering limitations of this initial version.

## Licensing
Code provided under MIT; see LICENSE. Independent community development, not affiliated with Akai or inMusic.
