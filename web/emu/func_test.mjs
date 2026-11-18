// SPDX-License-Identifier: GPL-3.0-only
// Same boot/program/note/render sequence as tests/funcbeat_web_test.c.
import fs from 'node:fs';import assert from 'node:assert/strict';
const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]||'build/emu/felucca.wasm'),{});
const ex=instance.exports;ex._initialize();ex.web_nor_erase();ex.web_seed(123);ex.web_boot();
function render(n,save=false){const out=new Float32Array(save?n*2:0);let at=0;while(n){const m=Math.min(128,n);ex.web_render(m);if(save){const l=new Float32Array(ex.memory.buffer,ex.web_out_l(),m),r=new Float32Array(ex.memory.buffer,ex.web_out_r(),m);for(let i=0;i<m;i++){out[at++]=l[i];out[at++]=r[i];}}n-=m;}return out;}
function text(p){const b=new Uint8Array(ex.memory.buffer,p,256);return new TextDecoder().decode(b.subarray(0,b.indexOf(0)));}
function apply(s){const b=new Uint8Array(ex.memory.buffer,ex.web_func_buffer(),256);b.fill(0);b.set(new TextEncoder().encode(s));return ex.web_func_apply(0);}
const songMode=process.argv[4]==='song';
render(97020);const source=songMode?text(ex.web_func_get(0)):"func{state q;fn tone(z)=sin(z)+q*.4*sin(z*2);q=q+(A-q)*.002;let l=sin(ph(.2+C*4));return tone(w+B*.1*l)*(.6-D*.3);}";assert(source.startsWith('func'));assert.equal(apply(source),0);render(4410);if(songMode){ex.web_func_mode(0,1);ex.web_play(1);}else ex.web_midi(0x90,60,110);
const started=performance.now();const out=render(songMode?529200:88200,true);const renderMs=performance.now()-started;assert(out.every(Number.isFinite));const rms=Math.sqrt(out.reduce((a,b)=>a+b*b,0)/out.length);assert(rms>.002&&rms<.5);
if(process.argv[3])assert.equal(Buffer.compare(Buffer.from(out.buffer),fs.readFileSync(process.argv[3])),0,'native/WASM FUNC sample mismatch');
assert.notEqual(apply('func{return nope;}'),0);assert.equal(text(ex.web_func_get(0)),source);
assert.equal(apply('  func{return sin(w)*A;}'),0);assert.equal(text(ex.web_func_get(0)),'  func{return sin(w)*A;}');
fs.writeFileSync(songMode?'build/func-song-demo.f32':'build/func-demo.f32',Buffer.from(out.buffer));
console.log('FUNC WASM:',songMode?'SONG529200frames':'NOTE88200frames','audio/parity/transaction PASS; RMS',rms.toFixed(5),'host render ms/s',(renderMs/(songMode?12:2)).toFixed(2));
