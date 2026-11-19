/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void render(unsigned n,FILE*f){while(n){unsigned m=n>128?128:n;web_render(m);if(f)for(unsigned i=0;i<m;i++){float x[2]={out_l[i],out_r[i]};fwrite(x,sizeof x,1,f);}n-=m;}}
int main(int argc,char**argv){assert(argc==2);web_nor_erase();web_seed(123);web_boot();render(110250,0);assert(trk_eng(TSEL)==ENGI_FORM && TSEL->preset==0);
 web_enc(EN_PRESET,-1);render(2205,0);
 assert(trk_eng(TSEL)==eng_vis(NENG_SHOWN-1));
 web_buttons(1u<<B_EDIT);render(2205,0);
 web_enc(EN_PRESET,1);render(2205,0);
 web_buttons(0);render(2205,0);
 assert(trk_eng(TSEL)==ENGI_FORM && TSEL->preset==0);
 FILE*f=fopen(argv[1],"wb");assert(f);
 for(unsigned state=0;state<FORM_N;state++){if(state){web_enc(EN_PRESET,1);render(2205,0);}assert(TSEL->preset==state);web_buttons(1u<<B_PLAY);render(2205,0);web_buttons(0);render(88200,f);web_buttons(1u<<B_PLAY);render(2205,0);web_buttons(0);render(FS,0);assert(!song.playing);}web_enc(EN_PRESET,1);render(2205,0);assert(trk_eng(TSEL)==ENGI_DRUM);
 web_enc(EN_PRESET,-1);render(2205,0);
 assert(trk_eng(TSEL)==ENGI_FORM && TSEL->preset==21);
 fclose(f);puts("FORM 22 panel presets native audio ready");return 0;}
