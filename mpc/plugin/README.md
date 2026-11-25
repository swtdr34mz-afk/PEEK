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

Vier Funktionstasten, vier Seiten, jede ein Teil des Peak-Panels (wie im VST):

| Taste | Seite |
| --- | --- |
| **OSC** | Oszillator 1 bis 3 und der Mixer |
| **FILTER** | Filter, Distortion, Chorus, Delay, Effects (Bypass), Reverb |
| **ENV** | Arp, LFO 1 und 2, Glide, Amp-Hüllkurve, Mod-Hüllkurven (Select 1/2) |
| **SCREEN** | Master, Animate, Patch und das Menü mit dem Display |

Ein Knopf oder Fader wird mit dem Finger gezogen oder mit seinem Q-Link gedreht; zweimal tippen öffnet den großen
Regler der MPC (bei einer Auswahl die Liste). Die Tasten unter den LEDs schalten weiter wie am Peak, die LEDs selbst
kann man auch direkt antippen. Wie im VST zeigt ein Knopf den gewählten Parameter: SHAPE AMOUNT die Quelle (Manual,
Mod Env 1, LFO 1), ENV DEPTH Amp Env oder Mod Env 1, die LFO-RATE bei SYNC die Teilung, die Delay-TIME bei SYNC die
Teilung, die MOD ENVELOPES die mit SELECT gewählte.

**SCREEN**: Das Display sitzt groß im MENU-Feld. Die Menütasten (Patch, Osc, Env, LFO, Arp/Clock, Mod, Voice, FX,
Settings) öffnen dieselben Display-Seiten wie am VST; nochmal drücken zeigt die nächste Seite des Menüs (wie am Peak).
Jeder Wert im Display ist selbst ein Regler: antippen und ziehen, zweimal tippen öffnet den großen Regler (bei einer
Auswahl die Liste). Der Patch-Knopf und PATCH − / + wechseln den Werks-Patch, INITIALISE setzt den Init-Patch,
AUDITION spielt ein C, solange man drückt. (COMPARE und SAVE haben auf der MPC keine Aufgabe: die MPC speichert ihre
Programme selbst. Die Active-Voices-LEDs bleiben aus.)

Unten auf jeder Seite: **VOICES** (8 oder 16 Stimmen) und **QUALITY** (ECO oder HIGH), je eine Taste und zwei LEDs.

Die Q-Links: jede Reihe der 16 Q-Links liegt auf vier Reglern der Seite (mit Rahmen). OSC: je ein Oszillator (Coarse,
Fine, Mod Env 2, LFO 2), unten der Mixer; FILTER: Filter, Filter-Modulation, Distortion/Chorus/Delay, Delay/Reverb;
ENV: Amp-Hüllkurve, Mod-Hüllkurve 1, die LFOs, Arp Gate und Glide.

Im **Program-Modus** (ohne Plugin-Seite) machen die Q-Links: 13-16 Frequenz, Resonanz, Overdrive, Filter-Hüllkurve;
9-12 Mod-Hüllkurve 1; 5-8 Amp-Hüllkurve; 1-4 Chorus-, Delay-, Reverb-Pegel, Lautstärke.

**QUALITY**: ECO (2x) ist voreingestellt und reicht auf der MPC; HIGH (4x) klingt in den Höhen noch etwas sauberer,
braucht aber fast doppelt so viel Rechenzeit. Voreingestellt sind 8 Stimmen; 16 gehen, kosten aber das Doppelte.

Die Programme: die 32 Werks-Patches der VST3 (Programm-Wechsel auf der MPC). Das Tempo kommt von der MPC.

## Licence

MPL-2.0. Fonts: Fira Sans and Anonymous Pro, SIL Open Font License.
