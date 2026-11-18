// SPDX-License-Identifier: GPL-3.0-only
// Actual cold-boot panel route, matching tests/funcsong_web_test.c.
import fs from 'node:fs';import assert from 'node:assert/strict';
const{instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{}),ex=instance.exports;
ex._initialize();ex.web_nor_erase();ex.web_seed(123);ex.web_boot();
function render(n,save=false){const out=new Float32Array(save?n*2:0);let at=0;while(n){const m=Math.min(128,n);ex.web_render(m);if(save){const l=new Float32Array(ex.memory.buffer,ex.web_out_l(),m),r=new Float32Array(ex.memory.buffer,ex.web_out_r(),m);for(let i=0;i<m;i++){out[at++]=l[i];out[at++]=r[i];}}n-=m;}return out;}
render(110250);
for(let j=0;j<2;j++){ex.web_buttons(1<<4);render(2205);ex.web_enc(2,1);render(2205);ex.web_buttons(0);render(2205);}
for(let j=0;j<(process.argv[4]==='beat'?13:12);j++){ex.web_enc(2,1);render(2205);}
assert.equal(ex.web_func_mode(0,-1),1);
ex.web_buttons(1<<10);render(2205);ex.web_buttons(0);const out=render(529200,true);
assert.equal(Buffer.compare(Buffer.from(out.buffer),fs.readFileSync(process.argv[3])),0,'panel native/WASM audio mismatch');
ex.web_buttons(1<<10);render(2205);ex.web_buttons(0);render(ex.web_sample_rate());assert.equal(ex.web_playing(),0);
console.log('FUNC panel',process.argv[4]||'song',': 529200 frames native/WASM bit-exact, PLAY/STOP PASS');
