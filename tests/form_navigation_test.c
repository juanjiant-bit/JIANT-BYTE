/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
static void form_label_capture(int32_t,int32_t,int32_t,int32_t,const char*,uint32_t);
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) form_label_capture(x0,y0,x1,y1,s,flags)
#define GFX_HOOK_BLIT(x,y,r0) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
static int inspect,clipped;
static void form_label_capture(int32_t x0,int32_t y0,int32_t x1,int32_t y1,const char*s,uint32_t flags){if(inspect&&(flags&2u||x0<0||y0<0||x1>240||y1>140)){printf("FORM label clipped: %s (%d,%d)..(%d,%d)\n",s,x0,y0,x1,y1);clipped++;}}
static uint16_t view[2][240*140];
static void capture(unsigned side){cv_begin(240,140,T_BG);inspect=1;graph_form_lz(TSEL);inspect=0;memcpy(view[side],cv_px,sizeof view[side]);}
int main(void){
 ui_power_on();load_depth++;fm6_init();power_on_parts();load_depth--;song.sel=0;assert(trk_eng(TSEL)==ENGI_FORM&&TSEL->preset==0&&eng_vis(0)==ENGI_FORM);assert(ENGINE_PC_ORDER[0]==ENGI_LOFI);
 uint32_t total,current=preset_pos(&total);assert(current==0);uint8_t notes[sizeof TSEL->step];memcpy(notes,TSEL->step,sizeof notes);
 for(unsigned i=0;i<total;i++){uint32_t k,e=preset_at(i,&k);assert(TSEL->eng_req==e&&TSEL->preset==k);turn(EN_PRESET,1);assert(ui.home&&!memcmp(notes,TSEL->step,sizeof notes));}
 assert(trk_eng(TSEL)==ENGI_FORM&&TSEL->preset==0);turn(EN_PRESET,-1);assert(trk_eng(TSEL)==ENGI_LOFI);turn(EN_PRESET,1);assert(trk_eng(TSEL)==ENGI_FORM);
 for(unsigned i=0;i<NENG_SHOWN;i++){preset_engine_jump(1);assert(trk_eng(TSEL)==eng_vis((i+1)%NENG_SHOWN));assert(TSEL->preset==preset_nth(trk_engp(TSEL),0));}
 static const unsigned pages[]={P_E0,P_E4,P_LRTRG,P_ROOT,P_CAOS};
 static const unsigned ids[5][4]={{P_E0,P_E1,P_E2,P_E3},{P_E4,P_E5,P_E6,P_E7},{P_LRTRG,P_SDIV,P_SLEN,P_SSWING},{P_ROOT,P_SCALE,P_CHRD,P_CVOI},{P_CAOS,P_KEY,P_DETUNE,P_DLY}};
 static const int lo[5][4]={{0,1,0,0},{0,0,0,0},{0,0,4,0},{0,0,0,0},{0,0,0,0}};
 static const int hi[5][4]={{21,21,127,127},{2,127,11,127},{1,3,64,100},{11,8,9,2},{4,127,127,127}};
 for(unsigned page=0;page<5;page++){
  ui.home=0;for(unsigned p=0;p<NPAGES;p++)if(PAGES[p].fam==FAM_EDIT&&PAGES[p].id[0]==pages[page]){ui.page=p;break;}
  for(unsigned knob=0;knob<4;knob++){apply_preset_to(TSEL,0);TSEL->p[ids[page][knob]]=lo[page][knob];capture(0);TSEL->p[ids[page][knob]]=hi[page][knob];capture(1);assert(memcmp(view[0],view[1],sizeof view[0]));}
  for(unsigned state=0;state<22;state++){apply_preset_to(TSEL,state);capture(0);}
 }
 /* Stored common values outside FORM's domain range must never index its labels. */
 TSEL->p[P_CAOS]=127;capture(0);TSEL->p[P_CAOS]=-1;capture(1);assert(!clipped);
 puts("FORM navigation: all factory sounds contiguous, boundaries reversible, EDIT jumps, 20 visual controls, all state labels in bounds PASS");return 0;
}
