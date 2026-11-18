/* SPDX-License-Identifier: GPL-3.0-only
 * Reproducible host CPU/voice audit; not a device CPU measurement.
 * Usage: audit_byte_cpu tracks per_track_limit life user_automation global12
 */
#include <time.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static double ticks(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static int cmp(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}
static int guards(void){
 ui_power_on();sys_load=LOAD_FULL;voice_headroom=0;
 if(voice_cap0()!=8)return 10;voice_headroom=1;if(voice_cap0()!=12)return 11;
 sys_load=LOAD_ECO;if(voice_cap0()!=6)return 12;sys_load=LOAD_FULL;voice_headroom=0;
 load_tight=guard_cap=guard_run=0;load_avg_q8=load_all_q8=0;
 for(unsigned i=0;i<300;i++)load_half(70,70,0);
 load_half(95,95,0);if(load_tight)return 13;
 for(unsigned i=0;i<3;i++)load_half(99,99,0);
 if(!load_tight||voice_cap()>8||voice_cap()<4)return 14;
 for(unsigned i=0;i<2000;i++)load_half(40,40,0);
 if(load_tight||voice_cap()!=8)return 15;
 host_preset(&trk[0],ENGI_DRUM,5);host_preset(&trk[1],ENGI_WAVE,1);
 for(unsigned i=0;i<NPART;i++)voice_limit[i]=8;
 trk_note_on(&trk[0],36,110);trk_note_on(&trk[0],38,110);trk_note_on(&trk[0],39,110);
 for(unsigned i=0;i<16;i++)trk_note_on(&trk[1],48+i,100);
 unsigned kept=0;for(unsigned i=0;i<NVOICE;i++)kept+=trk[0].v[i].active&&trk[0].v[i].stage!=4&&voice_core_drum(&trk[0],&trk[0].v[i]);
 if(kept!=3)return 16;
 puts("guard audit: budgets8/12/ECO6, isolated spike, sustained overload, recovery and core drum protection PASS");return 0;
}
int main(int argc,char**argv){
 if(argc==2&&!strcmp(argv[1],"--guards"))return guards();
 if(argc!=6)return 2;
 unsigned tracks=atoi(argv[1]),limit=atoi(argv[2]),life=atoi(argv[3]),user=atoi(argv[4]),extra=atoi(argv[5]);
 if(!tracks||tracks>NPART||!limit||limit>NVOICE)return 2;
 ui_power_on();song.master_q12=4096;sys_load=LOAD_FULL;voice_headroom=extra!=0;
 life_v[0]=life;life_v[1]=65;life_v[2]=60;life_v[3]=40;
 for(unsigned p=0;p<NPART;p++){
  unsigned e=!p?ENGI_DRUM:p&1&&!user?ENGI_WAVE:ENGI_LOFI;
  host_preset(&trk[p],e,!p?5:e==ENGI_WAVE?1:3);voice_limit[p]=limit;
  trk[p].p[P_MUTE]=p>=tracks;trk[p].p[P_VOICE]=V_POLY;
  trk[p].p[P_SUS]=100;trk[p].p[P_REL]=55;trk[p].p[P_REV]=20;trk[p].p[P_DLY]=10;
  if(user&&p){
   trk[p].p[P_E1]=RW_USER;trk[p].p[P_E2]=37+p;trk[p].p[P_E3]=20;
   trk[p].p[P_CAOS]=0;lyr_txt[p][0]=0;
   trk[p].p[P_M1SRC]=MS_LFO;trk[p].p[P_M1DST]=MD_E1+2;trk[p].p[P_M1AMT]=30;
   trk[p].p[P_M2SRC]=MS_ENV;trk[p].p[P_M2DST]=MD_E1+3;trk[p].p[P_M2AMT]=25;
  }
 }
 math_poll();unsigned old[NPART];for(unsigned p=0;p<NPART;p++)old[p]=math_ver[p];
 int32_t out[2*CTL];double audio=0,main=0,cost[5000],energy=0;unsigned maxactive=0,maxfade=0,maxown=0,sum=0,rebuilds=0,count=0,phase=0;
 unsigned total=3*FS/CTL,interval=FS/4/CTL;
 for(unsigned b=0;b<total;b++){
  if(!(b%interval)){
   for(unsigned p=0;p<tracks;p++){
    if(!p){trk_note_on(&trk[p],36,110);trk_note_on(&trk[p],phase&1?39:38,105);trk_note_on(&trk[p],42,80);}
    else{for(unsigned j=0;j<limit;j++)trk_note_off(&trk[p],48+p*2+j*3+((phase-1)&1));
     for(unsigned j=0;j<limit;j++)trk_note_on(&trk[p],48+p*2+j*3+(phase&1),90);}
   }phase++;
  }
  double start=ticks();mix_block(out,CTL);double elapsed=ticks()-start;audio+=elapsed;cost[count++]=elapsed;
  for(unsigned j=0;j<2*CTL;j++)energy+=(double)out[j]*out[j];
  unsigned active=0,fade=0;
  for(unsigned p=0;p<NPART;p++){unsigned own=0;for(unsigned j=0;j<NVOICE;j++)if(trk[p].v[j].active){own++;active++;fade+=trk[p].v[j].stage==4;}
   if(own>maxown)maxown=own;
  }sum+=active;if(active>maxactive)maxactive=active;if(fade>maxfade)maxfade=fade;
  fm1_ms=(b+1)*CTL*1000u/FS;
  start=ticks();math_poll();main+=ticks()-start;
  for(unsigned p=0;p<NPART;p++)if(old[p]!=math_ver[p]){rebuilds++;old[p]=math_ver[p];}
 }
 qsort(cost,count,sizeof cost[0],cmp);double seconds=(double)total*CTL/FS;
 printf("%u,%u,%u,%u,%u,%.3f,%.3f,%.3f,%u,%u,%u,%.2f,%u,%.4f\n",tracks,limit,life,user,voice_cap0(),audio*1000/seconds,main*1000/seconds,cost[count*95/100]*1e6,maxactive,maxfade,maxown,(double)sum/count,rebuilds,sqrt(energy/(2*total*CTL))/32768.0);
 return maxown>limit||!energy;
}
