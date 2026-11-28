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
 for(int k=0;k<16;k++)dw_poll();
 trk[0].p[P_E6]=fold;trk[0].p[P_E5]=env;trk[0].p[P_DIST]=0;
 trk_note_on(&trk[0],DRUM_LANE_NOTE[0],110);
 for(unsigned n=0;n<BN;n+=CTL)track_render(&trk[0],y+n,CTL);
}
static void distortion(unsigned ty,int tone,int noise,int32_t*y){
 ui_power_on();memset(dist_w,0,sizeof dist_w);memset(deq_id,0,sizeof deq_id);dist_rnd=12345;
 track_t*t=&trk[0];t->p[P_DTYPE]=ty;t->p[P_DIST]=75;t->p[P_DTONE]=tone;dnz_v[0]=noise;
 for(unsigned n=0;n<BN;n+=CTL){for(unsigned j=0;j<CTL;j++)y[n+j]=(sine_i((n+j)*98765432u)+sine_i((n+j)*12345678u))/3;track_dist(t,y+n,CTL);fvm_host_serve();}   /* (c1fault: the main loop between blocks compiles its formula) */
}
int main(void){
 drum(0,0,a);drum(100,0,b);drum(0,100,c);
 printf("Drum formula FOLD delta %.3f MOD ENV delta %.3f\n",delta(a,b),delta(a,c));
 OK(delta(a,b)>.15,"DRUM-W FOLD changes the audio");OK(delta(a,c)>.15,"DRUM-W MOD ENV changes the audio");
 drum(0,0,b);OK(!memcmp(a,b,sizeof a),"zero FOLD/MOD ENV remains deterministic and dry");
 for(unsigned ty=0;ty<N_DTYPE_N;ty++)if(!enum_skip(&TP[P_DTYPE],ty)){
  distortion(ty,-64,0,a);distortion(ty,63,0,b);distortion(ty,-64,80,c);
  printf("DIST %s tone %.3f noise %.3f\n",PN_AT(N_DTYPE, ty),delta(a,b),delta(a,c));
  OK(delta(a,b)>.02,"each selectable distortion has an audible TONE");OK(delta(a,c)>.001,"each selectable distortion has an audible NOISE");
 }
 OK(enum_skip(&TP[P_DTYPE],12)&&enum_skip(&TP[P_DTYPE],13)&&enum_skip(&TP[P_DTYPE],14),"filters are skipped by the DIST selector");
 for(unsigned ty=12;ty<=14;ty++){distortion(ty,-64,0,a);distortion(ty,63,0,b);distortion(ty,-64,80,c);OK(delta(a,b)>.02&&delta(a,c)>.001,"stored filter effects retain working TONE/NOISE");}
 ui_power_on();go_title("DIST");int16_t*amount=0;page_desc(cur_page(),3,&amount);OK(amount==&trk[song.sel].p[P_DIST],"DIST knob four directly controls its amount");song.playing=0;static project_t before,after;project_capture(&before);
 for(unsigned k=0;k<10;k++){
  perf_latch_on=1;perf_press(PF_OCTD+k,1);pfx_block(CTL,perf_held&PF_NOTES);
  OK(pt_ops==(1u<<k)&&trk[0].pfx_pit==(k==0?-192:k==1?192:0)&&!trk[0].pfx,"punch-in reaches its time transform (OCT -/+ the pitch), the sequencer untouched");   /* (BYTE 0.16: the white keys transform t) */
  project_capture(&after);OK(!memcmp(&before,&after,sizeof before),"SAVE while held sees exactly the unchanged patch/project");
  pfx_end();perf_press(PF_OCTD+k,0);pfx_block(CTL,perf_held&PF_NOTES);OK(!pt_ops&&!formula_punch&&!perf_latched,"release clears formula effect even with FX LATCH enabled");pfx_end();
 }
 seq_stop();song_poll();
 perf_latch_on=0;
 perf_press(PF_COMB,1);pfx_block(CTL,perf_held&PF_NOTES);
 OK(!formula_punch && perf_pick(perf_held)==PF_COMB,"G5 plays the COMB (a buffer effect), not the exchange");
 perf_press(PF_COMB,0);pfx_block(CTL,perf_held&PF_NOTES);
 OK(!formula_punch,"COMB let go: no exchange left");
 printf("byte_controls: %s\n",bad?"FAIL":"all ok");return !!bad;
}
