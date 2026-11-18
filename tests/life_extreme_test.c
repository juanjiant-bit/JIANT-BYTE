/* SPDX-License-Identifier: GPL-3.0-only */
#define LIFE_TEST_LIBRARY 1
#include "life_byte_test.c"
#include <sys/stat.h>
#define LONGNS (CTL*2757u)
static int32_t audio[LONGNS];
static void scene(unsigned level,unsigned hit,unsigned kind){
 config(level,100,100,100);trk_live=3;host_preset(&trk[0],ENGI_WAVE,1);host_preset(&trk[1],ENGI_LOFI,12);trk[1].p[P_E0]=4;trk[1].p[P_E7]=0;host_drums(&trk[2]);memset(dx_form,0,sizeof dx_form);dx_kit_load(kind?0:DX_NKIT);
 const char*s="func{state q;q=q+(A-q)*.002;return sin(w)+q*.4*sin(w*3);}";
 compile(&src,s);*lofi_slot(1)=src.p;lofi_prg_id[1]=0x400;math_ver[1]=0;
 for(unsigned k=0;k<3;k++){trk[k].p[P_DIST]=trk[k].p[P_CHOR]=trk[k].p[P_DLY]=trk[k].p[P_REV]=0;trk[k].p[P_SUS]=127;trk[k].p[P_LEVEL]=100;trk[k].p[P_VOICE]=V_POLY;trk[k].p[P_DRIFT]=127;}
 trk_note_on(&trk[0],48,110);
 int32_t b[CTL];for(unsigned o=0;o<LONGNS;o+=CTL){
  if(o==CTL*344u)trk_note_on(&trk[1],55,110);
  if(o==CTL*689u)trk_note_on(&trk[0],60,100);
  if(hit&&o>=CTL*1033u&&((o/CTL-1033u)%344u)==0)trk_note_on(&trk[2],(hit==1?36:hit==2?38:39),115);
  life_formula_block(0);for(unsigned k=0;k<3;k++){track_render(&trk[k],b,CTL);if(!k)memcpy(audio+o,b,sizeof b);}
 }
 char path[160];snprintf(path,sizeof path,"build/life-extreme/%u-%u-%u.i32",level,hit,kind);FILE*f=fopen(path,"wb");fwrite(audio,sizeof audio,1,f);fclose(f);
}
int main(void){mkdir("build/life-extreme",0777);for(unsigned level=0;level<=100;level+=25)scene(level,1,0);for(unsigned hit=0;hit<=3;hit++)for(unsigned kind=0;kind<2;kind++)scene(100,hit,kind);puts("life_extreme: WAVE receiver + FUNC overlap + formula/classic kick/snare/clap rendered");return fails!=0;}
