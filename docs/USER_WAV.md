# WaveOne v0.18 user WAV support (experimental)

WaveOne reads up to six user WAV files **when the plugin instance is created**.

## Locations

The Open-MPC engine receives a `data_dir` path from the wrapper. Place files in these folders **relative to that directory**:

- `user_wavetables/osc1.wav`, `osc2.wav`, `osc3.wav`
- `user_granular/osc1.wav`, `osc2.wav`, `osc3.wav`

On the MPC the exact data directory depends on installation and Open-MPC wrapper configuration. Do not assume a fixed /sdcard path. Verify the actual engine directory before copying files. Reopen the plugin after changing WAVs.

## WAV formats

RIFF/WAVE, uncompressed PCM16, PCM24 or IEEE float32, mono or stereo (first channel used). Max 16 MiB data chunk, minimum 16 frames. The loader resamples the **entire file** to a fixed 2048-sample buffer, then peak-normalizes it. For a wavetable, supply a single-cycle waveform; for granular, longer samples are currently reduced to 2048 samples, so this is a first-stage loader, **not** full-length sample playback.

Choose `USER WAV` on the OSC WT page or the OSC Granular page. If a file is missing/invalid, the engine falls back to its internal waveform.

## Limitations

- No in-plugin file browser, automatic folder creation or live reload.
- The wavetable and granular engines do not yet expose a truthful source waveform display.
- Existing project presets store the USER WAV selection, but **not the WAV file bytes**; projects depend on these external files being present.
- Do not load or replace WAV files during active playback. Loading happens only in create(), not render().
- The MPC host's data_dir and file permissions must be verified on hardware.
