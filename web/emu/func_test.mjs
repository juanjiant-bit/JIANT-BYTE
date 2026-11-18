// SPDX-License-Identifier: GPL-3.0-only
// Same boot/program/note/render sequence as tests/funcbeat_web_test.c.
import fs from 'node:fs';import assert from 'node:assert/strict';
const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]||'build/emu/felucca.wasm'),{});
const ex=instance.exports;ex._initialize();ex.web_nor_erase();ex.web_seed(123);ex.web_boot();
function render(n,save=false){const out=new Float32Array(save?n*2:0);let at=0;while(n){const m=Math.min(128,n);ex.web_render(m);if(save){const l=new Float32Array(ex.memory.buffer,ex.web_out_l(),m),r=new Float32Array(ex.memory.buffer,ex.web_out_r(),m);for(let i=0;i<m;i++){out[at++]=l[i];out[at++]=r[i];}}n-=m;}return out;}
function text(p){const b=new Uint8Array(ex.memory.buffer,p,256);return new TextDecoder().decode(b.subarray(0,b.indexOf(0)));}
function apply(s){const b=new Uint8Array(ex.memory.buffer,ex.web_func_buffer(),256);b.fill(0);b.set(new TextEncoder().encode(s));return ex.web_func_apply(0);}
render(97020);const source=text(ex.web_func_get(0));assert(source.startsWith('func'));assert.equal(apply(source),0);render(4410);ex.web_midi(0x90,60,110);
const out=render(88200,true);assert(out.every(Number.isFinite));const rms=Math.sqrt(out.reduce((a,b)=>a+b*b,0)/out.length);assert(rms>.01&&rms<.5);
if(process.argv[3])assert.equal(Buffer.compare(Buffer.from(out.buffer),fs.readFileSync(process.argv[3])),0,'native/WASM FUNC sample mismatch');
assert.notEqual(apply('func{return nope;}'),0);assert.equal(text(ex.web_func_get(0)),source);
fs.writeFileSync('build/func-demo.f32',Buffer.from(out.buffer));
console.log('FUNC WASM: real note audio, finite level, native88200-frame parity and invalid-program transaction PASS; RMS',rms.toFixed(5));
