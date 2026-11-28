// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (v1.1b2) The generator's whole state and its regenerations, in the browser build (web/emu/felucca_web.c):
//   node web/emu/gen_state_test.mjs build/emu/felucca.wasm
// web_gen_state / web_gen_set_state: a piece made again on a fresh device from the 19 numbers is the same piece (its
// sections byte for byte, its tracks' steps and sounds, its tempo), a GEN OCT+ piece (a seed thrown) too; web_gen_cmd 5:
// one track made again, every other track and the tempo as they were; web_gen_cmd 6: one section made again, the other
// sections byte for byte; a salt given twice, the same result. (v1.1b4) The state's regeneration log (web_gen_state_len()
// 44: the 19, [19] the count, a triple each): a piece with track and section regenerations restored on a fresh device
// byte for byte; a 19-number state still taken (no replay); a bad log entry refused; a new piece clears the log.
import fs from "fs";

const wasmPath = process.argv[2] || "build/emu/felucca.wasm";
const bytes = fs.readFileSync(wasmPath);
let fails = 0;
const check = (what, ok) => { console.log(`gen_state: ${what.padEnd(84)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

async function device() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_master(1023); ex.web_seed(5); ex.web_boot();
  ex.web_render(4096);
  const mem = () => ex.memory.buffer;
  const d = {
    ex,
    render(n = 10) { for (let k = 0; k < n; k++) ex.web_render(1024); },
    len: ex.web_gen_state_len ? ex.web_gen_state_len() : 19,
    state() { return Array.from(new Int32Array(mem(), ex.web_gen_state(), d.len)); },
    setState(s, regen) {                               // (as worklet.js: a 19-number state padded with zeros, no log)
      const b = new Int32Array(d.len); b.set(s);
      new Int32Array(mem(), ex.web_gen_state(), d.len).set(b); return ex.web_gen_set_state(regen ? 1 : 0);
    },
    slot(k) { const n = ex.web_bk_len(40 + k); return n ? Array.from(new Uint8Array(mem(), ex.web_bk_buf(), n)) : []; },
    slots() { return [0, 1, 2, 3].map((k) => d.slot(k)); },
    req(cmd, args) {                                   // an editor request (editor-api.h): its reply's payload
      const f = [0xF0, 0x7D, 0x46, 0x4C, cmd, ...args, 0xF7];
      new Uint8Array(mem(), ex.web_sysex_buf(), f.length).set(f);
      ex.web_sysex(f.length);
      let out = null;
      for (let n; (n = ex.web_sysex_out());) {
        const m = Array.from(new Uint8Array(mem(), ex.web_sysex_msg(), n));
        if (m[4] === cmd && !out) out = m.slice(5, -1);
      }
      return out;
    },
    track(t) {                                         // a track as the editor reads it: its sound, then its 64 steps
      const r = [...d.req(29, [t])];
      for (let i = 0; i < 64; i++) r.push(...d.req(30, [t, i]));
      return r;
    },
    bpm() { return d.state()[17]; },
  };
  return d;
}

{ /* a SONG piece, made again from its state on a fresh device */
  const a = await device();
  a.ex.web_gen(6, 1234, 70, 2);                        // TECHNO, seed 1234, ENERGY 70, SONG
  a.render();
  const st = a.state(), sl = a.slots(), tr = [0, 1, 2, 3].map((t) => a.track(t));
  check(`web_gen_state: ${st.length} numbers (19 + the log), the piece's (genre 6 seed 1234 energy 70 form 2, ${st[17]} BPM)`,
        st.length === 44 && a.ex.web_gen_state_len() === 44 && st[19] === 0 && st[0] === 1 && st[1] === 6 && st[2] === 1234 && st[3] === 70 && st[4] === 2 && st[14] >= 4);
  check("the piece has its sections (A..D stored)", sl.filter((s) => s.length).length >= 3);
  const b = await device();
  b.ex.web_gen(2, 77, 20, 0);                          // (another piece there first)
  b.render(4);
  check("web_gen_set_state(1) takes them", b.setState(st, 1) === 0);
  b.render();
  const sl2 = b.slots();
  check("  the sections again byte for byte", sl.every((s, k) => same(s, sl2[k])));
  check("  every track's sound and steps the same, the tempo too",
        [0, 1, 2, 3].every((t) => same(tr[t], b.track(t))) && b.bpm() === st[17]);
  check("  its state reads the same", same(b.state(), st));
  const bad = st.slice(); bad[2] = 10000;
  check("a seed out of its range: refused (1), nothing changed", b.setState(bad, 1) === 1 && same(b.state(), st));
  const bad2 = st.slice(); bad2[14] = 5;
  check("tracks 5: refused", b.setState(bad2, 0) === 1);

  /* cmd 5: track 1 made again (salt 7), the rest as it was */
  const before = [0, 1, 2, 3].map((t) => b.track(t)), bpm = b.bpm();
  b.ex.web_gen_cmd(5, 1 | 7 << 4);
  b.render();
  const after = [0, 1, 2, 3].map((t) => b.track(t));
  const info = Array.from(new Int32Array(b.ex.memory.buffer, b.ex.web_gen_info(), 9));
  check("web_gen_cmd 5 track 1: track 1 new", !same(before[1], after[1]));
  check("  tracks 0, 2, 3 as they were, the tempo too", [0, 2, 3].every((t) => same(before[t], after[t])) && b.bpm() === bpm);
  check("  web_gen_info()[8]: the salt used (7)", info[8] === 7);
  const c = await device();
  c.setState(st, 1); c.render();
  c.ex.web_gen_cmd(5, 1 | 7 << 4); c.render();
  check("  the same salt on another device: the same track", same(c.track(1), after[1]) && same(c.slot(0), b.slot(0)));
  c.ex.web_gen_cmd(5, 1); c.render();
  const n1 = Array.from(new Int32Array(c.ex.memory.buffer, c.ex.web_gen_info(), 9))[8];
  check("  salt 0: a new one each time (not 0, not 7)", n1 !== 0 && n1 !== 7);

  /* cmd 6: section C made again, A B D byte for byte */
  const s0 = b.slots(), tA = [0, 1, 2, 3].map((t) => b.track(t));
  b.ex.web_gen_cmd(6, 2 | 9 << 4);
  b.render();
  const s1 = b.slots();
  check("web_gen_cmd 6 section C: C new", s0[2].length && !same(s0[2], s1[2]));
  check("  A, B and D byte for byte, A live as it was, the tempo too",
        [0, 1, 3].every((k) => same(s0[k], s1[k])) && [0, 1, 2, 3].every((t) => same(tA[t], b.track(t))) && b.bpm() === bpm);
  b.ex.web_gen_cmd(6, 0 | 9 << 4);
  b.render();
  const s2 = b.slots();
  check("web_gen_cmd 6 section A: the whole piece again (every stored section new), the tempo kept",
        [0, 1, 2, 3].filter((k) => s1[k].length).every((k) => !same(s1[k], s2[k])) && b.bpm() === bpm);
}

{ /* a GEN OCT+ piece (a seed thrown on the device), and a LOOP on six tracks, made again from their state */
  const a = await device();
  a.ex.web_gen(3, 5, 50, 0);
  a.ex.web_gen_cmd(1, 0);                              // OCT+ again: a new seed thrown
  a.render();
  const st = a.state(), sl = a.slot(0);
  const b = await device();
  b.setState(st, 1); b.render();
  check(`an OCT+ piece (seed ${st[2]}) made again: the same`, same(sl, b.slot(0)) && same(b.state(), st));
  const six = st.slice(); six[14] = 6; six[4] = 0;
  a.setState(six, 1); a.render();
  const st6 = a.state(), sl6 = a.slot(0);
  const c = await device();
  c.setState(st6, 1); c.render();
  check("six tracks: made again the same", st6[14] === 6 && same(sl6, c.slot(0)) && same(c.track(5), a.track(5)));
}

{ /* (v1.1b4) the regenerations replayed: a piece, a track made again twice, a section, restored on a fresh device */
  const a = await device();
  a.ex.web_gen(6, 1234, 70, 2); a.render(4);
  a.ex.web_gen_cmd(5, 1 | 7 << 4); a.render(2);
  a.ex.web_gen_cmd(5, 2); a.render(2);               // (a salt of its own: logged)
  a.ex.web_gen_cmd(6, 2 | 9 << 4); a.render(2);
  a.ex.web_gen_cmd(5, 1 | 11 << 4); a.render();
  const st = a.state(), sl = a.slots(), tr = [0, 1, 2, 3].map((t) => a.track(t));
  check(`the log: 4 regenerations, in order (5 1 7, 5 2 ${st[25]}, 6 2 9, 5 1 11)`,
        st[19] === 4 && same(st.slice(20, 23), [5, 1, 7]) && st[23] === 5 && st[24] === 2 && st[25] > 0 &&
        same(st.slice(26, 29), [6, 2, 9]) && same(st.slice(29, 32), [5, 1, 11]));
  const b = await device();
  b.ex.web_gen(2, 77, 20, 0); b.render(4);
  check("web_gen_set_state(1) with the log: 0", b.setState(st, 1) === 0);
  b.render();
  check("  the sections byte for byte, every track's sound and steps, the tempo", same(sl.flat(), b.slots().flat()) &&
        sl.every((s, k) => same(s, b.slot(k))) && [0, 1, 2, 3].every((t) => same(tr[t], b.track(t))) && b.bpm() === st[17]);
  check("  its state reads the same (the log replayed into it)", same(b.state(), st));
  const c = await device();
  c.setState(st.slice(0, 19), 1); c.render();
  check("the first 19 alone: the piece without its regenerations (not the same tracks)",
        c.state()[19] === 0 && ![0, 1, 2, 3].every((t) => same(tr[t], c.track(t))));
  const bad = st.slice(); bad[21] = 9;                // (a track past the tracks live)
  check("a log entry out of range: refused (1), nothing changed", c.setState(bad, 1) === 1);
  const bad2 = st.slice(); bad2[19] = 9;
  check("a count past 8: refused (1)", c.setState(bad2, 1) === 1);
  a.ex.web_gen_cmd(1, 0); a.render(2);                // OCT+: a new piece
  check("a new piece: the log empty", a.state()[19] === 0);
  for (let k = 0; k < 9; k++) a.ex.web_gen_cmd(5, 0 | (k + 1) << 4);
  const full = a.state();
  check("nine regenerations: [19] -1, the first eight kept", full[19] === -1 && full[20] === 5 && full[22] === 1 && full[43] === 8);
  const d = await device();
  check("  restored: the piece made, none replayed (rc 2)", d.setState(full, 1) === 2 && d.state()[19] === 0);
}

console.log(fails ? `gen_state: ${fails} FAILED` : "gen_state: all ok");
process.exit(fails ? 1 : 0);
