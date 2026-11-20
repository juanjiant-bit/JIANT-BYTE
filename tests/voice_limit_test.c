/* SPDX-License-Identifier: GPL-3.0-only */
/* Per-track polyphony actually limits allocation and retires excess DSP voices. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
#define OK(c,s) do{bad+=check(s,c);}while(0)
static unsigned busy(track_t*t){unsigned n=0;for(unsigned i=0;i<NVOICE;i++)n+=t->v[i].active;return n;}
static void block(track_t*t,unsigned n){int32_t b[CTL];while(n--)track_render(t,b,CTL);}
static void init(unsigned e){ui_power_on();sys_load=0;host_preset(&trk[0],e,0);trk[0].p[P_VOICE]=V_POLY;voice_limit[0]=8;trk[0].p[P_SUS]=127;trk[0].p[P_DIST]=trk[0].p[P_CHOR]=trk[0].p[P_DLY]=trk[0].p[P_REV]=0;}
int main(void){
 for(unsigned e=0;e<2;e++){
  init(e?ENGI_LOFI:ENGI_WAVE);track_t*t=&trk[0];
  for(unsigned n=0;n<8;n++)trk_note_on(t,48+n*2,100);
  block(t,8);OK(busy(t)==8,"maximum permits eight simultaneous synth voices");
  voice_limit[0]=1;block(t,1);unsigned fading=0;for(unsigned i=1;i<NVOICE;i++)fading+=t->v[i].stage==4;
  OK(fading==7,"lowering limit retires excess slots through the existing fade");block(t,16);
  OK(busy(t)==1,"after short fade only one voice reaches synthesis");
  for(unsigned n=0;n<16;n++){trk_note_on(t,50+n,110);block(t,1);}
  OK(busy(t)==1 && trk_nvoice(t)==1,"new notes steal within a one-voice budget");
  voice_limit[0]=3;t->p[P_VOICE]=V_UNISON;trk_note_on(t,60,100);block(t,16);
  OK(busy(t)<=3,"UNISON obeys the same per-track limit");
 }
 init(ENGI_DRUM);voice_limit[0]=2;
 for(unsigned n=0;n<8;n++)trk_note_on(&trk[0],DRUM_LANE_NOTE[n],110);
 block(&trk[0],1);OK(busy(&trk[0])<=2,"DRUM-X simultaneous hits obey the voice budget");
 static project_t a,b;static project_store_t st;project_capture(&a);
 OK(proj_pack(&st,&a)&&proj_import(&b,st.raw,sizeof st.raw)&&b.voice_limit[0]==2,"voice limit persists with the project");
 init(ENGI_WAVE);song.sel=0;go_title("MORE");int16_t*v=0;const param_desc_t*d=page_desc(cur_page(),0,&v);   /* (BYTE 0.16: MORE's knob one) */
 OK(d && v==&voice_limit[0] && d->min==1 && d->max==8,"MORE exposes the real 1–8 voice limit on knob one");
 return !!bad;
}
