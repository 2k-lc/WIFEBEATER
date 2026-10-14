# WIFEBEATER

A harsh-noise instrument + effect plugin (VST3 / Standalone, AU on macOS), built with JUCE 8.

**Signal flow**

```
 Noise ─┐
 Chaos ─┤
 Osc   ─┼─► amp env ─► (+ audio in) ─► Spectral mangler ─► [feedback loop]
 Reson ─┤                                  ─► Dist 1 ► Dist 2 ► Dist 3 ─► Filter ─► width/gain ─► Safe limiter
 Sample ┘
```

Two fully independent channels (L/R) run the whole chain, so noise is naturally wide. Play it with MIDI, or turn on **DRONE** to sound continuously. It also processes incoming audio when the host provides an input.

## Sound sources
- **Noise**: white, pink, brown, blue, violet, crackle, impulse, plus a tilt "color" control
- **Chaos**: logistic map, Lorenz, Henon, feedback-FM (all key-tracked)
- **Oscillator**: saw/square/sine/tri with a second oscillator for hard sync, FM, ring mod
- **Resonator**: Karplus-Strong with noise excitation (self-oscillating at high decay)
- **Sample engine** (drop a file on the window or LOAD SAMPLE):
  - Loop: start/length/pitch/reverse
  - Granular: size, density, position + pitch scatter, reverse grains
  - Glitch: clocked repeats, reverse, tape-stop
  - Wavetable: the sample is the table; position, warp, FM, ring

## Spectral mangler
Freeze, Shuffle, Smear, Scramble, Gate, Shift/Stretch, Robot, Comb (1024-pt FFT, 75% overlap, about 21 ms latency while active).

## Distortion
Three serial slots, each with 17 algorithms: Tube, Tape, Diode, Hard Clip, Fuzz, Bitcrush, Rate Crush, Wavefold, Overflow (integer wraparound), Ring Mod, Comb, FM, Chaos Map, Rectify, Chebyshev, **Drawn Curve** (draw your own waveshaper), Slew. A global **feedback loop** (time, tone, amount up to self-oscillation) wraps the chain, followed by an SVF filter.

## Modulation
8-slot matrix. Sources: 2 LFOs (sine/tri/saw/square/S&H/smooth random), amp env, mod env, chaos, random, output follower, velocity, mod wheel, key. 48 destinations. **MANGLE** randomizes the patch (amount slider sets how far from the current patch).

## Safety
**SAFE LIMITER** (on by default): DC blocker and a -0.5 dBFS brickwall limiter on the output. Turn it off for fully raw output at your own risk. Output is always protected against NaN/Inf.

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```
The VST3 lands in `build/WIFEBEATER_artefacts/Release/VST3/`. Linux needs the usual JUCE deps (ALSA, freetype, fontconfig, GL, X11 dev packages). Run the offline DSP test with `build/wb_test_artefacts/Release/wb_test` (needs a display, e.g. `xvfb-run`). The GitHub Actions workflow builds Windows, macOS and Linux binaries as artifacts.
