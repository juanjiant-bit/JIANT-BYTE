/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
int main(void){
 const unsigned durations[]={8192,24576,32768,65536,262144};
 for(unsigned k=0;k<5;k++){
  unsigned d=durations[k];int32_t previous=32767;
  for(unsigned a=0;a<d;a++){
   unsigned up=a<1024?a*32:32767;
   int32_t expected=(int32_t)((uint64_t)up*((uint64_t)(d-a)*32767/d)/32768);
   int32_t actual=fvm_music_env(a,d);
   assert(actual>=0&&actual<=32767&&abs(actual-expected)<=1);
   if(a>=1024){assert(actual<=previous);previous=actual;}
  }
  assert(!fvm_music_env(-1,d)&&!fvm_music_env(d,d));
 }
 /* Harmonic accumulation must agree with a wide reference at full COLOR,
  * including negative partials: no signed multiplication overflow. */
 for(unsigned color=0;color<=65536;color+=8192){
  uint32_t inc=pitch_inc(69*16u);
  for(unsigned tick=0;tick<FS;tick++){
   uint32_t ph=inc*tick;int32_t a=sine_i(ph),b=sine_i(ph*2u),c=sine_i(ph*3u);
   int32_t expected=(a*3+(int32_t)(((int64_t)(b+c/2)*color)>>16))/5;
   assert(abs(fvm_music_tone(69,tick,color)-expected)<=1);
  }
 }
 puts("func_music: complete chord decay, bounded envelopes and full-color harmonic arithmetic PASS");
 return 0;
}
