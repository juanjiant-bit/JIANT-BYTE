# FUNC in JIANT /B1T3 v0.10

Compose functions inside **MATH → WAVE: FUNC**, or select the **FUNC INIT** preset. A/B/C/D are live musical controls and modulation-matrix destinations. In the MATH engine page, **OCT+** opens the formula editor; **OCT+** applies and **OCT−** cancels. The browser emulator also has a **FUNC** text editor below the instrument: choose a track, read it, edit, and apply.

This is BYTE's bounded function language, compiled to the same native VM in hardware and WebAssembly. It is **not JavaScript**, and arbitrary Dollchan Funcbeat programs cannot be pasted unchanged.

```c
func{
 state q;
 fn tone(z)=sin(z)+q*.4*sin(z*2);
 q=q+(A-q)*.002;
 let l=sin(ph(.2+C*4));
 return tone(w+B*.1*l)*(.6-D*.3);
}
```

Here A controls a smoothed second harmonic, B the phase-modulation depth, C its rate, and D the output attenuation. Those meanings come from the program; A/B/C/D have no hidden synthesis behavior in FUNC. `sin()` uses turns, so `sin(w)` is one cycle per played note.

- `let zz=...;` creates a local, evaluated each sample. Assign it later with `zz=...;`.
- `state q;` creates persistent fixed-point memory, initially zero. Assignments run in sample order. State is isolated per voice, resets on each new note and on applying new code, and is not saved as live audio history.
- `fn tone(z)=...;` defines a one-argument helper. Arguments run exactly once, including stateful calls. Helpers can read previously declared locals/state and call earlier helpers. There is no recursion.
- `return ...;` produces a signed waveform, normally −1..1. Existing formula operators/functions remain available, including `sin`, `tri`, `sqr`, `ph`, `lp`, comparisons and ternaries.

| Input | Meaning |
| --- | --- |
| `A B C D` | Controls normalized to 0..1 |
| `a b c d` | Integer controls, 0..254 |
| `w` | Played-note oscillator phase, in turns |
| `T` | Seconds since this voice/program started |
| `t` | Integer sample counter since this voice/program started |
| `p` | Song/transport beat position (looping timeline) |
| `s` | Current sequencer step |
| `freq` | Played-note frequency, Hz |
| `sr` | Internal sample rate, integer |
| `n v` | MIDI note and velocity |

Use `ph(rate)` for a slow independent clock: its internal phase has higher precision than adding `rate/sr` to a Q16.16 state. Q16.16 is deterministic fixed-point arithmetic, not JavaScript floating point; very small per-sample increments can round to zero. For a note oscillator, `w` is the most direct clock. `T` wraps with the underlying fixed-point time representation; it is intended for short voice-time routines, not an unlimited wall clock.

A beat-driven harmonic phrase, without additional state:

```c
func{let h=(int(p)%4)+1;return sin(w)+A*.5*sin(w*h);}
```

A stateful plucked-style tone using the built-in filter memory:

```c
func{let z=sin(w)+A*.4*sin(w*3);return lp(z,.02+B*.4);}
```

Limits: **255 ASCII characters per track**, six locals, two state words shared between explicit `state` and stateful helpers (`ph`, `lp`...), two helper definitions, four nested helper calls, 64 VM instructions and bounded expression depth. Names have up to seven characters; built-in names such as `x`, `a`, `n` and `A` are reserved. No loops, recursion, allocation or network execution. Compilation happens outside the audio callback. An invalid program leaves the previous program intact.

**Save the project** to retain code, controls and sections. The short USER sound-slot format cannot hold FUNC code: its save command tells you to save the project instead. Switching to MATH USER preserves the FUNC text for when you return to FUNC, while USER runs its SEED/MUTA generator.

FUNC participates in LIFE. Donor analysis copies its state, so inspecting a formula never advances the live voice's memory. Actual playback uses the complete program, sample by sample.
