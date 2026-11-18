/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <sys/stat.h>
static void setup(unsigned preset){
 ui_power_on();song.sel=0;select_engine(ENGI_LOFI);frame();
 for(unsigned k=1;k<NTRK;k++)trk[k].p[P_MUTE]=1;
 apply_preset_to(TSEL,preset);math_poll();frame();song.master_q12=4096;
 int32_t b[CTL*2];for(unsigned j=0;j<100;j++)mix_block(b,CTL);
 assert(func_song(TSEL)&&lofi_prg[0].func&&lofi_prg[0].n&&math_music_cost(TSEL)>=6);
}
static unsigned song_voices(void){unsigned k=0;for(unsigned i=0;i<NVOICE;i++)k+=TSEL->v[i].active&&TSEL->v[i].gate;return k;}
static void example(unsigned preset,int knob,int value){
 setup(preset);if(knob>=0)TSEL->p[P_E2+knob]=value;
 char path[128];snprintf(path,sizeof path,"build/funcsong/p%u-k%d-v%d.i32",preset,knob,value);
 FILE*f=fopen(path,"wb");assert(f);int32_t out[CTL*2];transport_req=1;
 for(unsigned j=0;j<FS*12u/CTL;j++){mix_block(out,CTL);fwrite(out,sizeof out,1,f);}
 fclose(f);transport_req=2;for(unsigned j=0;j<FS/CTL;j++)mix_block(out,CTL);
}
int main(void){
 mkdir("build/funcsong",0777);setup(12);printf("FUNC score %u instructions, cost %u\n",lofi_prg[0].n,math_music_cost(TSEL));
 assert(!song.playing&&!song_voices());transport_req=1;
 FILE*f=fopen("build/funcsong/song.i32","wb");int32_t b[CTL*2];double energy=0;unsigned maxv=0;
 for(unsigned j=0;j<FS*33u/CTL;j++){mix_block(b,CTL);unsigned v=song_voices();if(v>maxv)maxv=v;for(unsigned i=0;i<CTL*2;i++)energy+=(double)b[i]*b[i];fwrite(b,sizeof b,1,f);}
 fclose(f);printf("FUNC song clock=%u sampleclock=%d maxvoices=%u rms=%.2f\n",func_clock_end>>16,TSEL->v[0].s[2],maxv,sqrt(energy/(FS*33u*2)));
 assert(func_clock_end>64u*65536u&&maxv==1&&energy>1e8);
 assert(TSEL->v[0].s[2]>FS*30u); /* no sequencer-step retrigger */
 unsigned held=TSEL->v[0].s[2];trk_note_on(TSEL,72,120);assert(song_voices()==1&&TSEL->v[0].s[2]==held);
 transport_req=2;for(unsigned j=0;j<FS/CTL;j++)mix_block(b,CTL);assert(!song.playing&&!song_voices());
 const char*custom="func{return sin(w)*A;}";lyr_set(0,custom);TSEL->p[P_E7]=0;math_poll();assert(!strcmp(math_func_text(TSEL,lyr_get(0)),custom));
 apply_preset_to(TSEL,13);math_poll();assert(!strcmp(lyr_get(0),custom)&&!strcmp(math_func_text(TSEL,custom),MATH_FUNC_BEAT));
 TSEL->p[P_E7]=0;math_poll();assert(!strcmp(math_func_text(TSEL,lyr_get(0)),custom));
 /* Tempo alters the score clock, not the pitched sample-clock oscillators. */
 setup(12);transport_req=1;for(unsigned j=0;j<FS/CTL;j++)mix_block(b,CTL);
 uint32_t begin=func_clock_end;song.g[G_BPM]=90;
 for(unsigned j=0;j<FS*2u/CTL;j++)mix_block(b,CTL);
 uint32_t elapsed=func_clock_end-begin;assert(elapsed>2u*65536u&&elapsed<4u*65536u);
 transport_req=2;for(unsigned j=0;j<FS/CTL;j++)mix_block(b,CTL);transport_req=1;mix_block(b,CTL);assert(func_clock_end<65536u);
 transport_req=2;mix_block(b,CTL);
 static project_t captured,restored;static project_store_t stored;lyr_set(0,custom);TSEL->p[P_E7]=0;
 project_capture(&captured);assert(proj_pack(&stored,&captured)&&proj_import(&restored,stored.raw,sizeof stored.raw));assert(restored.t[0].p[P_E0]==5&&!strcmp(restored.lyr[0],custom));
 static fvm_cc_t cc;static fvm_full_t pg;const char*over="func{return chord(48,P,A)+chord(60,P,B)+note(72,P,C);}";
 assert(fvm_compile(&cc,FVM_FULL(&pg),over,strlen(over),0)==FE_LONG);
 example(12,-1,0);example(13,-1,0);
 for(int k=0;k<4;k++){example(12,k,0);example(12,k,127);}
 puts("funcsong: transport, global clock, single voice, keyboard isolation, custom-source preservation PASS");return 0;
}
