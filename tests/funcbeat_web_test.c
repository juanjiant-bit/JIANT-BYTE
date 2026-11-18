/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void frames(unsigned n,FILE*f){while(n){unsigned m=n>128?128:n;web_render(m);if(f)for(unsigned i=0;i<m;i++){float v[2]={out_l[i],out_r[i]};fwrite(v,sizeof v,1,f);}n-=m;}}
int main(int argc,char**argv){if(argc<2)return 2;int song_mode=argc>2;web_nor_erase();web_seed(123);web_boot();frames(97020,0);str_cpy(web_func_buffer(),song_mode?MATH_FUNC_DEFAULT:"func{state q;fn tone(z)=sin(z)+q*.4*sin(z*2);q=q+(A-q)*.002;let l=sin(ph(.2+C*4));return tone(w+B*.1*l)*(.6-D*.3);}",256);assert(!web_func_apply(0));frames(4410,0);if(song_mode){web_func_mode(0,1);web_play(1);}else web_midi(0x90,60,110);FILE*f=fopen(argv[1],"wb");assert(f);frames(song_mode?529200:88200,f);fclose(f);return 0;}
