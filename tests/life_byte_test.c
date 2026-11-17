/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
static int fails;
#define CHECK(c,s) do{int q=!!(c);printf("lifeb: %s %s\n",s,q?"ok":"FAIL");fails+=!q;}while(0)
#define NS 32768u
static int32_t dry[NS],lwet[NS],other[NS];
static const char *target_text="sin(w*2.0)";
static fvm_full_t recv,src;static fvm_cc_t cc;static fvm_scratch_t scratch;
static void compile(fvm_full_t *p,const char*s){CHECK(!fvm_compile(&cc,FVM_FULL(p),s,strlen(s),0),s);}
static void config(int amt,int numbers,int fragments,int link){
 memset(trk,0,sizeof trk);host_tracks_init();life_reset();memset(&MOD,0,sizeof MOD);rng_state=1234;trk_live=4;voice_limits_default();
 life_defaults();life_v[LIFE_AMT]=amt;life_v[LIFE_NUMBERS]=numbers;life_v[LIFE_FRAGMENTS]=fragments;life_v[LIFE_LINK]=link;
 for(unsigned k=0;k<NTRK;k++){trk[k].p[P_MUTE]=0;trk[k].p[P_LEVEL]=127;}
}
static double delta(const int32_t*a,const int32_t*b){double d=0,e=1;for(unsigned i=0;i<NS;i++){double x=(double)a[i]-b[i];d+=x*x;e+=(double)a[i]*a[i];}return sqrt(d/e);}
/* Execute two actual compiled formulas, snapshots from the previous control block.
 * Both orders are tested; source samples are deliberately zeroed in fragment-only
 * tests, so a waveform crossfade cannot pass the instruction-graft test. */
static void vm(const char*donor,int amt,int number,int frag,int link,int reverse,int quiet,int32_t*out){
 config(amt,number,frag,link);compile(&recv,target_text);compile(&src,donor);
 fvm_ins_t saved[FVM_MAXINS];memcpy(saved,recv.ins,sizeof saved);
 for(unsigned k=0;k<2;k++){trk[k].v[0].active=1;trk[k].v[0].stage=2;trk[k].v[0].env_out=32767;trk[k].v[0].note=60+k*7;}
 uint32_t T[FVM_BLK],W[FVM_BLK],values[FVM_BLK];int32_t state[8]={0};
 fvm_env_t e={{T,W,0},{0},state};e.u[FU_AF]=32768;
 for(unsigned o=0;o<NS;o+=FVM_BLK){
  if(quiet==1)trk[1].p[P_MUTE]=1;if(quiet==2||(quiet==5&&o>=NS/2))trk[1].v[0].active=0;if(quiet==4)life_v[LIFE_N]=0;
  life_formula_block(quiet==3?2:0);
  if(!link)memset(fl.sum,0,sizeof fl.sum);
  for(unsigned a=0;a<2;a++){
   unsigned k=reverse?1-a:a;for(unsigned j=0;j<FVM_BLK;j++){T[j]=(o+j)*3;W[j]=((o+j)*(k?725:389))&65535;}
   fl_bind(&trk[k],&trk[k].v[0],fl.depth[k]);fl_run(k?&src.p:&recv.p,&e,&scratch,FVM_BLK,values);fl_unbind();
   if(!k)for(unsigned j=0;j<FVM_BLK;j++)out[o+j]=fvm_sample(recv.p.ty,values[j]);
  }
 }
 CHECK(!memcmp(saved,recv.ins,sizeof saved),"receiving program is unchanged");
}
static void engine(unsigned source,int amt,int same,int reverse,int32_t*out,uint32_t*clock){
 config(amt,65,60,40);memset(drum_kit,0,sizeof drum_kit);
 host_preset(&trk[0],ENGI_WAVE,1);trk[0].p[P_E6]=0;trk[0].p[P_VOICE]=V_POLY;trk[0].p[P_SUS]=127;trk[0].p[P_DIST]=0;
 if(source==ENGI_DRUM){host_drums(&trk[1]);dx_kit_load(DX_NKIT);}else host_preset(&trk[1],source,9);
 for(unsigned k=0;k<2;k++){trk[k].p[P_DIST]=trk[k].p[P_CHOR]=trk[k].p[P_DLY]=trk[k].p[P_REV]=0;}
 trk_note_on(&trk[0],60,110);trk_note_on(&trk[same?0:1],source==ENGI_DRUM?36:67,110);
 int32_t b[CTL];for(unsigned o=0;o<NS;o+=CTL){life_formula_block(0);for(unsigned a=0;a<2;a++){unsigned k=reverse?1-a:a;track_render(&trk[k],b,CTL);if(!k)memcpy(out+o,b,sizeof b);}}
 *clock=trk[0].v[0].ph[0];
}
int main(void){
 int monotonic=1;for(int i=1;i<=100;i++)monotonic&=life_curve(i)>=life_curve(i-1);
 CHECK(monotonic&&life_curve(0)==0&&life_curve(50)==48&&life_curve(75)==126&&life_curve(100)==256,"master audio taper preserved");
 vm("sin(w*7.0)",0,100,100,100,0,0,dry);
 vm("sin(w*7.0)",100,0,0,0,0,0,lwet);CHECK(!memcmp(dry,lwet,sizeof dry),"zero controls exact bypass");
 for(unsigned k=0;k<3;k++){vm("sin(w*7.0)",100,k==0?100:0,k==1?100:0,k==2?100:0,0,0,lwet);printf("control %u relative difference %.4f\n",k,delta(dry,lwet));CHECK(delta(dry,lwet)>.1,"each control changes actual receiving samples");}
 vm("sin(w*7.0)",100,0,100,0,0,0,lwet);vm("sin(w*3.0)",100,0,100,0,0,0,other);CHECK(delta(lwet,other)>.1,"changing donor instructions changes destination without donor audio");
 vm("t*5",100,0,100,0,0,0,lwet);CHECK(delta(dry,lwet)>.1,"integer fragment modulates FIX destination");
 target_text="t*2";vm("sin(w*7.0)",0,0,0,0,0,0,dry);vm("sin(w*7.0)",100,0,100,0,0,0,lwet);CHECK(delta(dry,lwet)>.1,"FIX fragment modulates INT destination");
 target_text="sin(w*2.0)";vm("sin(w*7.0)",0,0,0,0,0,0,dry);
 vm("sin(w*7.0)",100,65,60,40,0,0,lwet);vm("sin(w*7.0)",100,65,60,40,1,0,other);CHECK(!memcmp(lwet,other,sizeof lwet),"render order independence");
 for(int k=1;k<=4;k++){vm("sin(w*7.0)",100,100,100,100,0,k,lwet);CHECK(!memcmp(dry,lwet,sizeof dry),"mute / inactive / performance hush / per-track zero bypass");}
 vm("sin(w*7.0)",100,65,60,40,0,5,lwet);CHECK(!memcmp(dry+NS/2,lwet+NS/2,sizeof dry/2),"ending a previously active donor leaves no stale interaction");
 compile(&src,"ph(2.0)");CHECK(src.p.life_fn==1,"stateful opcode excluded from fragment");
 compile(&src,"\"123\"[t]");CHECK(src.p.life_fn<=1,"table opcode excluded from fragment");
 for(unsigned i=0;i<3;i++){unsigned eng=i==0?ENGI_LOFI:i==1?ENGI_DRUM:ENGI_WAVE;uint32_t c0,c1;
  engine(eng,0,i==2,0,dry,&c0);engine(eng,100,i==2,0,lwet,&c1);
  printf("engine %u relative difference %.4f\n",eng,delta(dry,lwet));CHECK(delta(dry,lwet)>.02,"overlapping MATH / DRUM-X / same-engine notes modulate WAVE");CHECK(c0==c1,"underlying oscillator clock preserved");
 }
 puts(fails?"lifeb: FAILED":"lifeb: all ok");return !!fails;
}
