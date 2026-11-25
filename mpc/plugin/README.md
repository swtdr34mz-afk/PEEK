# PEEK for the MPC

The PEEK synth as a plugin for Akai MPC standalone hardware: the same engine as the PEEK VST3, built with Akai's
MPC Plugin SDK. A polysynth in the manner of the Novation Peak: three oscillators per voice (sine, triangle, saw,
pulse with shape, VSync, saw density, 60 wavetables), linear FM, ring mod, noise, the state-variable filter (LP, BP,
HP; 12 or 24 dB) between overdrive and post drive, three looping envelopes, four LFOs, ANIMATE, a 16-slot
modulation matrix, the arp, distortion, chorus, delay and reverb, 8 or 16 voices, and the 32 factory patches.

Unofficial: not made, endorsed or supported by Novation or Focusrite.

## Installieren (kurz auf Deutsch)

1. MPC OS **3.10.1 oder neuer**. Auf der MPC: **Preferences → Enhancements (Beta) → Enable Enhancements** und
   **External Plugin Support** einschalten.
2. Den Ordner **`swtdr34mz - VST - PEEK`** (aus dem Download `PEEK-MPC.zip`) auf einen USB-Stick, eine SD-Karte
   oder das interne Laufwerk der MPC kopieren, in einen Ordner namens **`VST2 Plugins`** ganz oben auf dem Laufwerk:
   `VST2 Plugins/swtdr34mz - VST - PEEK/`.
3. Die MPC neu starten. PEEK steht dann im Plugin-Browser unter **External Plugins** (als Plugin-Programm laden).

Die MPC XL, Live III (64 Bit) und die 32-Bit-Geräte (X, X SE, Live, Live II, One, One+, Key 61, Key 37) nehmen
denselben Ordner; jedes Gerät lädt seine eigene Datei darin.

## Die Seiten

Sechs Funktionstasten mit je zwei Seiten: **einmal drücken die erste, nochmal drücken die zweite** (unten links steht,
welche gerade zu sehen ist).

| Taste | Seite 1 | Seite 2 |
| --- | --- | --- |
| **OSC** | Oszillator 1 und 2 (Range, Wave, Coarse, Fine, Pitch-Mod, Shape, WaveMore, Bend, Fixed Note) | Oszillator 3; VSync, SawDense, DenseDet aller drei |
| **FILTER** | Mixer (Pegel, VCA Gain, Noise-Filter, Post Drive); Filter (Frequenz, Resonanz, Overdrive, Hüllkurven, LFO 1, Osc 3, Shape, Slope) | Voice (Modus, Unison, Spread, Pan, Patch Level, Glide); Diverge, Drift, Key Sync; **8/16 Stimmen, Qualität** |
| **ENV** | Amp-Hüllkurve, Mod-Hüllkurve 1 (mit Delay, Hold, Velocity, Repeats) | Mod-Hüllkurve 2; ANIMATE 1 und 2 (gedrückt halten), HOLD |
| **LFO** | LFO 1 und 2 | LFO 3 und 4; Arpeggiator |
| **MOD** | Mod-Matrix Slot 1-8 (Quelle A, Quelle B, Ziel, Tiefe) | Slot 9-16 |
| **FX** | Distortion, Chorus, Delay | Reverb; Wet/Dry, Routing, Bypass |

Ein Knopf wird mit dem Finger hoch und runter gezogen oder mit seinem Q-Link gedreht; zweimal tippen öffnet den
großen Regler der MPC (bei einer Auswahl die Liste). Ein Taster schaltet um, eine LED-Liste wählt aus. Die Q-Links
liegen auf jeder Seite auf den Reglern mit Rahmen (die oberen zwei Reihen der Q-Links = obere Hälfte der Seite).

Im **Program-Modus** (ohne Plugin-Seite) machen die Q-Links: 13-16 Frequenz, Resonanz, Overdrive, Filter-Hüllkurve;
9-12 Mod-Hüllkurve 1; 5-8 Amp-Hüllkurve; 1-4 Chorus-, Delay-, Reverb-Pegel, Lautstärke.

**QUALITY**: ECO (2x) ist voreingestellt und reicht auf der MPC; HIGH (4x) klingt in den Höhen noch etwas sauberer,
braucht aber fast doppelt so viel Rechenzeit. Voreingestellt sind 8 Stimmen; 16 gehen, kosten aber das Doppelte.

Die Programme: die 32 Werks-Patches der VST3 (Programm-Wechsel auf der MPC). Das Tempo kommt von der MPC.

## Licence

MPL-2.0. Fonts: Fira Sans and Anonymous Pro, SIL Open Font License.
