/* SPDX-License-Identifier: GPL-3.0-only */
/* Audible control paths, formula-only momentaries, and dry/save invariants. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
#define OK(c,s) do{bad+=check(s,c);}while(0)
#define BN 8192
static int32_t a[BN],b[BN],c[BN];
static double delta(const int32_t*x,const int32_t*y){double d=0,e=0;for(unsigned i=0;i<BN;i++){double z=x[i]-y[i];d+=z*z;e+=(double)x[i]*x[i];}return sqrt(d/(e+1));}
static void drum(int fold,int env,int32_t*y){
 ui_power_on();memset(drum_kit,0,sizeof drum_kit);host_drums(&trk[0]);dx_kit_load(DX_NKIT);
 str_cpy(dx_text[0],"sin(w)",DX_TEXT);dx_form[0]=DX_USER;dx_fid[0]=0;
 trk[0].p[P_E6]=fold;trk[0].p[P_E5]=env;trk[0].p[P_DIST]=0;
 trk_note_on(&trk[0],DRUM_LANE_NOTE[0],110);
 for(unsigned n=0;n<BN;n+=CTL)track_render(&trk[0],y+n,CTL);
}
static void distortion(unsigned ty,int tone,int noise,int32_t*y){
 ui_power_on();memset(dist_w,0,sizeof dist_w);memset(deq_id,0,sizeof deq_id);dist_rnd=12345;
 track_t*t=&trk[0];t->p[P_DTYPE]=ty;t->p[P_DIST]=75;t->p[P_DTONE]=tone;dnz_v[0]=noise;
 for(unsigned n=0;n<BN;n+=CTL){for(unsigned j=0;j<CTL;j++)y[n+j]=(sine_i((n+j)*98765432u)+sine_i((n+j)*12345678u))/3;track_dist(t,y+n,CTL);}
}
static void engine_punch(unsigned engine,unsigned mask,int32_t*y,uint32_t *clock){
 ui_power_on();memset(drum_kit,0,sizeof drum_kit);
 if(engine==ENGI_DRUM){host_drums(&trk[0]);dx_kit_load(DX_NKIT);}
 else host_preset(&trk[0],engine,1);
 trk[0].p[P_DIST]=0;formula_punch=mask;
 trk_note_on(&trk[0],engine==ENGI_DRUM?36:60,110);
 for(unsigned n=0;n<BN;n+=CTL)track_render(&trk[0],y+n,CTL);
 *clock=engine==ENGI_DRUM?drum_kit[0][0].x.ph:trk[0].v[0].ph[0];formula_punch=0;
}
int main(void){
 drum(0,0,a);drum(100,0,b);drum(0,100,c);
 printf("Drum formula FOLD delta %.3f MOD ENV delta %.3f\n",delta(a,b),delta(a,c));
 OK(delta(a,b)>.15,"DrumX FOLD changes the actual formula audio");OK(delta(a,c)>.15,"DrumX MOD ENV changes the actual formula audio");
 drum(0,0,b);OK(!memcmp(a,b,sizeof a),"zero FOLD/MOD ENV remains deterministic and dry");
 for(unsigned ty=0;ty<NELEM(N_DTYPE);ty++)if(!enum_skip(&TP[P_DTYPE],ty)){
  distortion(ty,-64,0,a);distortion(ty,63,0,b);distortion(ty,-64,80,c);
  printf("DIST %s tone %.3f noise %.3f\n",N_DTYPE[ty],delta(a,b),delta(a,c));
  OK(delta(a,b)>.02,"each selectable distortion has an audible TONE");OK(delta(a,c)>.001,"each selectable distortion has an audible NOISE");
 }
 OK(enum_skip(&TP[P_DTYPE],12)&&enum_skip(&TP[P_DTYPE],13)&&enum_skip(&TP[P_DTYPE],14),"filters are skipped by the DIST selector");
 for(unsigned ty=12;ty<=14;ty++){distortion(ty,-64,0,a);distortion(ty,63,0,b);distortion(ty,-64,80,c);OK(delta(a,b)>.02&&delta(a,c)>.001,"stored filter effects retain working TONE/NOISE");}
 ui_power_on();go_title("DIST");int16_t*amount=0;page_desc(cur_page(),3,&amount);OK(amount==&trk[song.sel].p[P_DIST],"DIST knob four directly controls its amount");song.playing=0;static project_t before,after;project_capture(&before);
 for(unsigned k=0;k<10;k++){
  perf_latch_on=1;perf_press(PF_OCTD+k,1);pfx_block(CTL,perf_held&PF_NOTES);
  OK(formula_punch==(1u<<k)&&!trk[0].pfx_pit&&!trk[0].pfx,"punch-in reaches formula operator without changing sequencer or pitch state");
  project_capture(&after);OK(!memcmp(&before,&after,sizeof before),"SAVE while held sees exactly the unchanged patch/project");
  pfx_end();perf_press(PF_OCTD+k,0);pfx_block(CTL,perf_held&PF_NOTES);OK(!formula_punch&&!perf_latched,"release clears formula effect even with FX LATCH enabled");pfx_end();
 }
 seq_stop();song_poll();memset(pfx_lane,0,sizeof pfx_lane);pfx_lane_put(0,10);pfx_lph=0;song.playing=1;pfx_rec_held=0;
 pfx_block(CTL,0);OK(formula_punch==512u,"recorded punch lane applies the same formula operator");pfx_end();
 song.playing=0;pfx_block(CTL,0);OK(!formula_punch,"stopping playback clears recorded formula operator");pfx_end();
 for(unsigned k=0;k<3;k++) {
  unsigned engine=k==0?ENGI_WAVE:k==1?ENGI_LOFI:ENGI_DRUM;uint32_t ca,cb;
  engine_punch(engine,0,a,&ca);engine_punch(engine,128,b,&cb);
  OK(delta(a,b)>.05,"punch modifies actual WAVE/MATH/DRUM-X rendered audio");
  OK(ca==cb,"punch leaves the underlying engine phase advancing normally");
 }
 for(unsigned type=0;type<2;type++){
  fvm_full_t prog;fvm_cc_t comp;fvm_scratch_t scratch;int32_t state[2]={0};
  const char*text=type?"sin(w+sin(w*2)*A)+sin(T*7)/5":"(t*(A+3))^(t>>5)";
  int err=fvm_compile(&comp,FVM_FULL(&prog),text,str_len(text),0);OK(!err,"punch reference formula compiles");
  uint32_t T[FVM_BLK],W[FVM_BLK],S[FVM_BLK],base[FVM_BLK],wet[FVM_BLK];
  for(unsigned j=0;j<FVM_BLK;j++){T[j]=0x3210+j*3;W[j]=1234+j*1917;S[j]=40000+j*113;}
  fvm_env_t e={{T,W,S,0,0},{0},state};e.u[FU_A]=5;e.u[FU_AF]=20000;
  formula_punch=0;formula_run(&prog.p,&e,&scratch,FVM_BLK,base);
  for(unsigned k=0;k<10;k++){formula_punch=1u<<k;formula_run(&prog.p,&e,&scratch,FVM_BLK,wet);OK(memcmp(base,wet,sizeof base)!=0,"each formula operator changes generated samples");}
  formula_punch=0;formula_run(&prog.p,&e,&scratch,FVM_BLK,wet);OK(!memcmp(base,wet,sizeof base),"release restores original samples at the continuing time");
 }
 printf("byte_controls: %s\n",bad?"FAIL":"all ok");return !!bad;
}
