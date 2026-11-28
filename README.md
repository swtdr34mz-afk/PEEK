# PEEK

An 8-voice (or 16-voice) polysynth in the manner of the **Novation Peak**, as a **VST3** for Windows and the Mac, an
**AU** for the Mac, a **standalone app** and a plugin for **Akai MPC** hardware. By **swtdr34mz**. Its sound is its
own, built from the Peak's architecture: three oscillators per voice, the state-variable filter between two drives,
the distortion after the VCAs, and the Peak's effects and modulation matrix.

Unofficial: not made, endorsed or supported by Novation or Focusrite. The wavetables and the factory patches are
PEEK's own.

## Download

[Actions](https://github.com/swtdr34mz-afk/PEEK/actions) → the newest green run → **Artifacts** at the bottom:
`PEEK-Windows`, `PEEK-macOS` or `PEEK-MPC` (a GitHub sign-in is needed for artifacts).

## Installieren (kurz auf Deutsch)

**Windows (Cubase):**
1. Zip entpacken.
2. Den Ordner `PEEK.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
3. Cubase starten (oder *Studio → VST-PlugIn-Manager → Aktualisieren*). PEEK steht unter den Instrumenten,
   Hersteller swtdr34mz.
4. `PEEK.exe` ist die Standalone-App. Audio- und MIDI-Gerät stellt man unter *Options* ein.

Windows SmartScreen kann beim ersten Start der App warnen (sie ist nicht signiert): *Weitere Informationen →
Trotzdem ausführen*.

**Mac (Ableton, Logic, ...):** Am besten die `.tar.gz` nehmen (Doppelklick entpackt sie). `PEEK.vst3` nach
`~/Library/Audio/Plug-Ins/VST3/`, `PEEK.component` nach `~/Library/Audio/Plug-Ins/Components/`. Dann einmal im
Terminal:
```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/PEEK.vst3 ~/Library/Audio/Plug-Ins/Components/PEEK.component
```
Die Plugins laufen auf Apple Silicon und Intel-Macs (eine Datei für beide).

**MPC:** siehe [mpc/plugin/README.md](mpc/plugin/README.md).

**Linux (Bitwig, Reaper, Ardour, ...):** Die `.tar.gz` entpacken. `PEEK.vst3` nach `~/.vst3/` kopieren, `PEEK.lv2` nach `~/.lv2/`. Die App `PEEK` startet auch ohne DAW. 64 Bit, gebaut auf Ubuntu 22.04.

## What it has

- **8 or 16 voices** (VOICES, in the panel's bottom strip); POLY, POLY2, MONO, MONOLG, MONO2; UNISON of 1, 2, 3, 4
  or 8 voices with its detune and stereo spread (four spread modes), PAN POSITION, glide and PRE-GLIDE.
- Per voice: **three oscillators**: sine, triangle, sawtooth, square/pulse and **60 wavetables** of five waves each
  (MORE); SHAPE by hand, by Mod Env 1 and by LFO 1 (wavefolding, the pulse's width, the table's position);
  **VSYNC** (a virtual master), **SAWDENSE** and its detune, DIVERGE and DRIFT, FIXED NOTE, a bend range each; band-limited
  at 2x or 4x the host's rate, so they stay clean while they frequency-modulate each other.
- **Linear FM** (O1>O2, O2>O3, O3>O1, noise>O1) from the matrix, **RING 1*2**, noise with its low- and high-pass.
- **OVERDRIVE** before the filter (fuller), the **state-variable filter** (LP, BP, HP; 12 or 24 dB; it sings on its
  own from about 85 % resonance and tracks the keys), Osc 3 into its frequency at audio rate, FILTER DIVERGE, the
  **POST DRIVE** after it (brighter, grittier), the VCA, then the analogue-style **DISTORTION** on the sum.
- **Three DAHDSR envelopes** with velocity, mono legato or re-trigger and REPEATS (looping); **four LFOs** (1 and 2 per
  voice: fade in/out, phase, slew, repeats, common; 3 and 4 global), **ANIMATE 1 and 2** with HOLD and their own
  attack and release.
- **Modulation matrix**: 16 slots of two sources each, 24 sources, 48 destinations (the Peak's 2.0 lists); the
  **FX matrix**: 4 slots, 24 destinations.
- **Effects**: CHORUS (three programs; chorus, flanger, "phlanger"), DELAY (to 1.4 s, sync, L/R ratios, damping,
  slew, width, lo-fi time modes), REVERB (a modulated 8-line network: size, time, pre-delay, damping, low and high
  pass), in parallel or in series in any order, WET and DRY.
- **ARP**: up, down, up-down 1 and 2, played, random, chord, chord 2; 33 rhythms, 1 to 6 octaves, swing, gate,
  chance, key latch, key sync.
- **HOST SYNC**: the clock, synced LFOs and the delay follow the DAW's tempo and position.
- 32 factory patches; SAVE keeps your own (Documents/PEEK/Patches).
- **QUALITY** (Settings menu): HIGH runs the voices at 4x the sample rate, ECO at 2x (less CPU).

## The panel

The panel is the Peak's (no keys: play it from a MIDI keyboard or the DAW). As on the Peak: the display shows what
was touched last for a moment; the menu buttons (Osc, Env, LFO, Arp/Clock, Mod, Voice, FX, Settings) open their
pages, **Page/Select** steps through them, the three buttons left of the display pick the row the big knob and
**Value - / +** change. With the mouse: the wheel over the display turns the big knob, a click on a row picks from
its list. On a matrix page the first row's button (or a click on the left or right half) picks source A or B.
SHAPE AMOUNT edits the source its SOURCE button shows, ENV DEPTH the envelope its SOURCE button shows, the mod
envelope sliders the envelope SELECT shows.

MIDI: CC 1 mod wheel, CC 11 expression (pedal 1), CC 2 and 4 pedal 2, CC 3 the CV input (64 the middle), CC 64
sustain, CC 80 / 81 ANIMATE 1 / 2, channel and poly pressure.

## Building

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE 8 comes from GitHub. The engine's tests: `cmake -S tests -B build-tests && cmake --build build-tests && build-tests/engine_tests`.

## Licence

MPL-2.0 (LICENSE). Fonts: Fira Sans and Anonymous Pro, SIL Open Font License (fonts/).
