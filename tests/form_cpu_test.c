/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <time.h>
static void bench(unsigned state,unsigned count,unsigned pair,unsigned life,FILE*f){
 ui_power_on();song.master_q12=4096;trk_live=6;song.sel=0;int32_t out[CTL*2];
 for(unsigned k=0;k<NTRK;k++){if(k<count){set_engine_of(&trk[k],ENGI_FORM);apply_preset_to(&trk[k],state);trk[k].p[P_E1]=(state+9)%22;trk[k].p[P_E2]=pair?64:0;trk[k].p[P_MUTE]=0;}else trk[k].p[P_MUTE]=1;}
 life_v[LIFE_AMT]=life;life_v[LIFE_NUMBERS]=life_v[LIFE_FRAGMENTS]=life_v[LIFE_LINK]=100;
 for(unsigned k=0;k<100;k++)mix_block(out,CTL);transport_req=1;mix_block(out,CTL);
 clock_t start=clock();unsigned max=0;double en=0;
 for(unsigned j=0;j<FS*2u/CTL;j++){mix_block(out,CTL);unsigned active=0;for(unsigned k=0;k<count;k++)for(unsigned v=0;v<NVOICE;v++)active+=trk[k].v[v].active;assert(active<=count);if(active>max)max=active;for(unsigned i=0;i<CTL*2;i++)en+=(double)out[i]*out[i];}
 double ratio=(double)(clock()-start)/CLOCKS_PER_SEC/2.;printf("state%u tracks%u pair%u life%u actual%u energy%.1f\n",state,count,pair,life,max,en);fflush(stdout);assert(max==count&&en>1e8);fprintf(f,"%u,%u,%u,%u,%u,%.6f\n",state+1,count,pair?2:1,life,max,ratio);
}
int main(void){FILE*f=fopen("build/form/cpu.csv","w");assert(f);fprintf(f,"state,tracks,active_states_per_track,life,max_engine_voices,host_realtime_ratio\n");for(unsigned s=0;s<22;s++){bench(s,1,0,0,f);bench(s,1,1,0,f);}unsigned n[]={2,4,6};for(unsigned i=0;i<3;i++)for(unsigned l=0;l<=100;l+=50)bench(7,n[i],1,l,f);fclose(f);puts("FORM CPU: 44 single/pair states + 9 dense/LIFE scenarios; bounded one engine voice per track PASS (host only)");return 0;}
