// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Copyright (C) 2026 Charles Vestal (X0X)
// Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// Felucca in an AudioWorklet: felucca.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/felucca_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the songs
// 0x97000..0xDFFFF; the autosave 0xE5000..0xE6FFF (1.2); the settings and the second FM6 copy
// 0xFC000..0xFFFFF), only the sectors that hold something (an erased sector is all 0xFF). {offset: bytes}
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE7000], [0xFC000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  /* (studio) the editor's replies and pushes waiting in the device (editor-api.h), each a whole F0..F7 message */
function sysexOut(ex) {
  const out = [];
  for (let n, k = 0; k < 64 && (n = ex.web_sysex_out()); k++) out.push(Array.from(new Uint8Array(ex.memory.buffer, ex.web_sysex_msg(), n)));
  return out;
}
class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, {});
          const ex = instance.exports;
          ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          if (ex.web_seed) ex.web_seed((Math.random() * 4294967296) >>> 0);   // (JIANT 0.4) this session's macros
          ex.web_boot();
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate() });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "func-get" || m.type === "func-apply") {
        if (!ex.web_func_get) { this.port.postMessage({type:"func-result",error:"This engine does not support FUNC."}); return; }
        const track=Math.max(0,Math.min(5,m.track|0));let rc=0;
        if(m.type==="func-apply") {
          const text=String(m.text||"").replace(/[\r\n\t]/g," ");
          if(text.length>255 || /[^\x20-\x7e]/.test(text)){this.port.postMessage({type:"func-result",error:"Use up to 255 ASCII characters."});return;}
          const b=new Uint8Array(ex.memory.buffer,ex.web_func_buffer(),256);b.fill(0);for(let i=0;i<text.length;i++)b[i]=text.charCodeAt(i);
          rc=ex.web_func_apply(track);
          if(!rc&&ex.web_func_mode)ex.web_func_mode(track,m.run?1:0);
        }
        const b=new Uint8Array(ex.memory.buffer,ex.web_func_get(track),256);let text="";for(let i=0;i<255&&b[i];i++)text+=String.fromCharCode(b[i]);
        const names=["","Syntax","Unknown/reserved name","Arguments","Too complex","Too deep","Too complex","Table full","State budget (2), or nonzero initialization","Empty"];
        this.port.postMessage({type:"func-result",track,text,run:ex.web_func_mode?ex.web_func_mode(track,-1):0,applied:m.type==="func-apply"&&!rc,error:rc?(names[rc]||"Compile error")+" at "+(ex.web_func_error_pos()+1):""});return;
      }
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "tap") {                 // (studio) the stems and MIDI OUT to JIANT Studio's recorder
        const on = !!m.on && !!ex.web_stems;
        if (ex.web_stems) ex.web_stems(on ? 1 : 0);
        if (ex.web_midi_tap) ex.web_midi_tap(on ? 1 : 0);
        this.tap = on ? { planes: Array.from({ length: 8 }, () => new Float32Array(4096)), n: 0, at: this.pos, midi: [] } : null;
        this.port.postMessage({ type: "tap", on, stems: !!ex.web_stems });
      }
      else if (m.type === "sysex") {               // (studio) the web editor's frame (editor-api.h): its replies at once
        if (!ex.web_sysex || !Array.isArray(m.data) || m.data.length > 640) return;
        new Uint8Array(ex.memory.buffer, ex.web_sysex_buf(), m.data.length).set(m.data.map((v) => v & 255));
        ex.web_sysex(m.data.length);
        const out = sysexOut(ex);
        if (out.length) this.port.postMessage({ type: "sysex", msgs: out });
      }
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (ex.web_editor_poll) {                       // (studio) the editor's live pushes (WATCH), in the same message
        ex.web_editor_poll();
        const out = sysexOut(ex);
        if (out.length) msg.sysex = out;
      }
      if (msg.leds || msg.fb || msg.sectors || msg.sysex || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    /* (studio) a block's stems (web_stem_out: T1..T6, FX L, FX R) and the MIDI OUT packets it sent (each at its sample),
       posted about every 93 ms: { at: the first sample's number, planes, midi: [{ at, data }] } */
    tapBlock(ex, n) {
      const T = this.tap;
      for (let c = 0; c < 8; c++) T.planes[c].set(new Float32Array(ex.memory.buffer, ex.web_stem_out(c), n), T.n);
      for (let p; ex.web_midi_take && (p = ex.web_midi_take()) !== 0;) {
        const cin = p & 15, len = cin === 0xC || cin === 0xD ? 2 : cin >= 0x8 && cin <= 0xE ? 3 : cin === 0xF ? 1 : 0;
        if (len) T.midi.push({ at: this.pos, data: [(p >>> 8) & 255, (p >>> 16) & 255, (p >>> 24) & 255].slice(0, len) });
      }
      T.n += n;
      if (T.n + 128 > 4096) {
        const planes = T.planes.map((pl) => pl.slice(0, T.n));
        this.port.postMessage({ type: "tap-chunk", at: T.at, planes, midi: T.midi }, planes.map((pl) => pl.buffer));
        T.n = 0; T.at = this.pos + n; T.midi = [];
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      if (this.tap) this.tapBlock(ex, n);
      this.pos = (this.pos || 0) + n;
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("felucca", Felucca);
}
