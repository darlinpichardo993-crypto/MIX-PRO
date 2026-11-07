# Test report (core suite, JUCE-free)

Executed locally with g++ 13.3 (Ubuntu 24.04), -O2 -Wall, 2026-11-07. Plugin-level validation (init, state restore, sample rates/buffers in the real VST3) is done by pluginval in CI and has NOT been run yet.

```
[test] EQ: peak filter gain at centre frequency
[test] Filters: highpass / lowpass attenuation
[test] Compression: static gain reduction
[test] Saturation: harmonics added, output bounded
[test] Transient shaping: attack boosts peaks
[test] Delay: echo position and level
[test] Reverb: tail present, finite, mix=0 transparent
[test] Stereo: width 0 collapses to mono; mono-low keeps lows centred
[test] Loudness: BS.1770 reference level
[test] Spectral analysis: centroid and flatness
[test] Fingerprint JSON round-trip
[test] Presets: save / load / apply / state restoration
[test] Genre library: edit, save, load
[test] Bypass preserves original signal exactly
[test] Parameters change the audio
[test] Master engine: loudness target and ceiling
[test] Reference matching moves spectrum toward the reference
   spectral distance to reference: before 1.97 dB, after 0.98 dB
[test] Stability: NaN/Inf, all modes/roles, sample rates and block sizes

74 checks passed, 0 failed
```
