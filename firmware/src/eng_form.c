/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * FORM (BYTE 0.17, the owner: "MATH's FUNC and FORM one engine, whole"): two formulas of funcbeat, A and B (the recipes
 * of docs/FORM-REFERENCIAS.md, or the part's own text: CUSTOM), MORPH from one to the other at equal power; their
 * A B C D the four macros, each a recipe's own below its middle and, above it, a stage of the timbre chain after the
 * formulas, in C: A folds, B rings at a ratio of the note, C an acid filter closing on the note, D crushes (D is the
 * recipes' seed too). MOTION a curve of the song's (SINE TRI S&H RAMP, 1/4 bar .. 8 bars) moving MORPH and the four
 * stages, each at its quarter; PULSE the drums, native (a kick, a snare, hats on euclids fused from D), the song's
 * 16ths. RUN TRIG (played as a synth), PLAY (the transport's: keys and steps its pitch), FREE; ROOT transposes.
 * The formulas run by blocks (fvm_func_block: no state), at half the rate; one while MORPH is at an end, both between. */
static const char *const FORM_RUN[]={"TRIG","PLAY","FREE",0};
static const char *const FORM_NAMES[]={"CUSTOM","SONG","BEAT","GROWL","HALFTIME","ACID","REESE","HOUSE","STRUM","JUST","PLUCK","NIBBLE","BITS","DRUMS","SCREECH",0};
#define FORM_NT 15u
static const char *const FORM_MOD[]={"SINE","TRI","S&H","RAMP",0};
static const char *const FORM_SPD[]={"1/4","1/2","1 BAR","2 BAR","4 BAR","8 BAR",0};
static const char *const FORM_KIT[]={"OFF","KIT","BIT",0};
static const uint8_t FORM_SPB[6]={1,2,4,8,16,32};         /* MOTION's period in beats */
/* a recipe's text: CUSTOM the part's own (a func, else SONG's) */
static const char *form_recipe(uint32_t i,const char*custom){
 static const char *const R[4]={MATH_FUNC_DEFAULT,MATH_FUNC_BEAT,MATH_FUNC_GROWL,MATH_FUNC_HALF};
 return i>=1u&&i<=4u?R[i-1u]:i>=5u&&i<5u+NELEM(MATH_FUNC_R)?MATH_FUNC_R[i-5u]:math_is_func(custom)?custom:MATH_FUNC_DEFAULT;}
/* the slots FORM takes from the common pages (the others' again when the part leaves it: ui.c) */
static int form_owned_param(uint32_t id){return id==P_LRTRG||id==P_CAOS||id==P_ASWING||id==P_APROB||id==P_CSTR||id==P_DETUNE||id==P_DXIMG;}
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t run={"RUN",F_ENUM,0,2,1,FORM_RUN},mod={"MOD",F_ENUM,0,3,0,FORM_MOD},spd={"SPEED",F_ENUM,0,5,3,FORM_SPD},
  dep={"DEPTH",F_PCT,0,127,0,0},kit={"DRUMS",F_ENUM,0,2,0,FORM_KIT},lev={"LEVEL",F_PCT,0,127,90,0},den={"DENSITY",F_PCT,0,127,64,0};
 return id==P_LRTRG?&run:id==P_CAOS?&mod:id==P_ASWING?&spd:id==P_APROB?&dep:id==P_CSTR?&kit:id==P_DETUNE?&lev:id==P_DXIMG?&den:0;
}
#define FP(label,a,b,mo,ka,kb,kc,kd,md,sp,dp,kit,lv,dn) {label,{a,b,mo,0,ka,kb,kc,kd},{0,0,127,24},0,TAG(FX),FX(0,10,30,30),0,\
  MODS(P_LRTRG,1,P_CAOS,md,P_ASWING,sp,P_APROB,dp,P_CSTR,kit,P_DETUNE,lv,P_DXIMG,dn)}
static const preset_t FORM_PRESETS[]={
 FP("01 SONG",1,2,0,64,32,16,0,0,3,0,0,90,64),        FP("02 GROWL",3,6,0,70,50,40,17,0,2,40,1,90,50),
 FP("03 HALFTIME",4,3,0,60,60,50,9,0,3,30,0,90,64),   FP("04 ACID",5,5,0,90,40,100,33,1,2,60,1,80,60),
 FP("05 REESE",6,3,40,60,64,40,5,0,4,50,1,80,40),     FP("06 HOUSE",7,8,0,64,40,30,12,0,3,20,1,90,80),
 FP("07 STRUM",8,9,0,60,30,20,3,0,4,30,0,90,64),      FP("08 JUST",9,10,30,50,40,30,21,1,3,40,1,70,40),
 FP("09 PLUCK",10,9,0,60,70,60,5,2,1,50,1,80,60),     FP("10 NIBBLE",11,12,0,60,50,40,44,0,3,30,2,80,64),
 FP("11 BITS",12,11,0,50,60,30,90,2,2,40,2,90,90),    FP("12 DRUMS",13,12,0,60,70,60,9,0,3,0,0,0,64),
 FP("13 SCREECH",14,12,0,50,100,30,90,1,1,50,1,90,80),FP("14 FUSION",3,6,64,80,70,90,40,2,2,90,1,90,70),
};
#undef FP
/* ---- the dice (ui_input.c: SELECT the whole of it; SEQ + SELECT, and the generator, its macros and motion alone) ---- */
static const uint8_t FORM_DICE_COMP[8]={P_E4,P_E5,P_E6,P_E7,P_CAOS,P_ASWING,P_APROB,P_DXIMG};
static int16_t form_rnd(uint32_t n){return (int16_t)(n?rng()%n:0u);}
static void form_dice(track_t*t,uint32_t comp_only)
{
 int16_t*p=t->p;
 if(!comp_only){p[P_E0]=(int16_t)(3+form_rnd(FORM_NT-3u));p[P_E1]=form_rnd(3u)?(int16_t)(3+form_rnd(FORM_NT-3u)):p[P_E0];
  p[P_E2]=form_rnd(3u)?0:form_rnd(128u);p[P_CSTR]=form_rnd(3u);}
 for(uint32_t k=0;k<4u;k++)p[P_E4+k]=(int16_t)(20+form_rnd(100u));
 p[P_CAOS]=form_rnd(4u);p[P_ASWING]=(int16_t)(1+form_rnd(5u));p[P_APROB]=form_rnd(2u)?form_rnd(90u):0;p[P_DXIMG]=(int16_t)(30+form_rnd(90u));
}
static uint32_t form_cost(const track_t*t){return t->p[P_E2]>0&&t->p[P_E2]<127&&t->p[P_E0]!=t->p[P_E1]?5u:3u;}
static uint8_t form_ok[NTRK];                            /* bit 0 A's program published, bit 1 B's (engines.c math_poll_ws) */
static int16_t form_scope[2][64];                        /* (the panel: the sound playing, half-rate samples) */
static volatile uint8_t form_scope_tr=0xFF;
static uint8_t form_sc_i;
static void form_note_on(track_t*t,voice_t*v){(void)t;memset(v->s,0,sizeof v->s);v->ph[0]=v->ph[1]=0;}
/* the drums: a part's, by its first voice (one a part in PLAY and FREE; TRIG's while a key sounds) */
typedef struct{uint32_t kph,sph;int32_t ka,kp,sa,ha,hp,last;uint32_t nz;} form_dr_t;
static form_dr_t form_dr[NTRK];
static const uint16_t FORM_RAT[8]={2048,4096,6144,8192,12288,16384,20480,28672};   /* B's ring: 1/2 .. 7 the note, Q12 */
static int32_t form_fold(int32_t x){uint32_t q=((uint32_t)(x+32768)<<15)>>16;int32_t d=(int32_t)q-32768;return 32767-2*(d<0?-d:d);}
static __attribute__((noinline)) void form_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 uint32_t tr=(uint32_t)(t-trk)%NTRK,i,k,rate,sh=VH_SH();
 const int16_t*p=t->p;
 if(!(form_ok[tr]&1u)||lofi_prg_id[tr]!=0x800u||!lofi_prg[tr].n)return;
 if(n>LOFI_N)n=LOFI_N;
 if(!n)return;
 uint32_t T[LOFI_N],W[LOFI_N],S[LOFI_N],P[LOFI_N],Y[LOFI_N],Z[LOFI_N],q=(uint32_t)v->s[2],ph=v->ph[0];
 uint32_t inc=pitch_inc((uint32_t)clamp(m->pitch16+clamp(p[P_E3],-24,24)*16,0,2047));
 const fvm_prog_t*pa=lofi_slot(tr),*pb=&wave_prg_b[tr];
 fvm_env_t en={{T,W,S,0,0},{0},&v->s[6]};
 en.u[FU_NOTE]=(uint32_t)clamp((int32_t)v->note+p[P_E3],0,127);en.u[FU_VEL]=v->vel;en.u[FU_HZ]=(uint32_t)(((uint64_t)inc*FS)>>(16+sh));
 en.u[FU_BEAT]=lofi_sync(&rate)*8u;en.u[FU_STEP]=t->seq_idx;
 en.u[FU_BPS]=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300);en.arr[FA_BEAT]=P;
 for(k=0;k<4;k++){uint32_t a=(uint32_t)clamp(p[P_E4+k],0,127);en.u[FU_A+k]=a*2u;en.u[FU_AF+k]=a*65536u/127u;}
 /* the clocks: the transport's while it plays (not FREE), else the voice's own (formula_run's exact steps) */
 uint32_t pos,step,rem,carry=0,den;
 if(func_bar(t)){pos=func_clock_begin;den=n;step=(func_clock_end-pos)/den;rem=(func_clock_end-pos)%den;}
 else {uint64_t ticks=(uint64_t)q<<16;den=en.u[FU_BPS];pos=(uint32_t)(ticks/den);carry=(uint32_t)(ticks%den);step=(65536u<<sh)/den;rem=(65536u<<sh)%den;}
 uint64_t s0=(uint64_t)q*65536u;uint32_t sd=(uint32_t)FS,sv=(uint32_t)(s0/sd),sr=(uint32_t)(s0%sd),ss=(65536u<<sh)/sd,srm=(65536u<<sh)%sd;
 for(i=0;i<n;i++){T[i]=q+(i<<sh);W[i]=ph>>16;S[i]=sv;sv+=ss;sr+=srm;if(sr>=sd){sr-=sd;sv++;}ph+=inc;P[i]=pos&0x7fffffffu;pos+=step;carry+=rem;while(carry>=den){carry-=den;pos++;}}
 /* MOTION: one curve, five places on it (the four stages, MORPH) */
 int32_t dep=clamp(p[P_APROB],0,127),lf[5]={0,0,0,0,0};
 if(dep){uint32_t per=(uint32_t)FORM_SPB[clamp(p[P_ASWING],0,5)]<<16,cyc=P[0]/per,x=(uint32_t)(((uint64_t)(P[0]%per)<<32)/per),sh2=(uint32_t)clamp(p[P_CAOS],0,3);
  for(k=0;k<5u;k++){uint32_t y=x+k*0x40000000u;int32_t l;
   if(sh2==0u)l=sine_i(y);else if(sh2==1u){int32_t d=(int32_t)(y>>16)-32768;l=32767-2*(d<0?-d:d);}
   else if(sh2==2u)l=(int32_t)(fvm_hash(cyc*977u+k*40503u+(uint32_t)p[P_E7]*131u+(y<x?1u:0u))>>16)-32768;
   else l=(int32_t)(y>>16)-32768;
   lf[k]=l*dep>>15;}}
 int32_t mo=clamp(p[P_E2]+lf[4],0,127),am[4];
 for(k=0;k<4u;k++)am[k]=clamp((p[P_E4+k]>64?(p[P_E4+k]-64)*2:0)+(lf[k]>>1),0,127);
 /* the formulas: one at an end, both between, at equal power ((128-x)(128+x), x(256-x)) */
 uint32_t useb=mo>0&&(form_ok[tr]&2u)&&wave_id_b[tr]==0x800u&&pb->n&&!pb->nstate,usea=mo<127||!useb;
 int32_t ga=usea?(useb?(128-mo)*(128+mo)>>7:128):0,gb=useb?(usea?mo*(256-mo)>>7:128):0;
 if(usea)lofi_run(pa,&en,&lofi_work[MC_CPU()&1u],n,Y);
 if(useb){int32_t*st=en.state;en.state=0;lofi_run(pb,&en,&lofi_work[MC_CPU()&1u],n,Z);en.state=st;}
 /* the chain's coefficients a block */
 uint32_t rinc=(uint32_t)(((uint64_t)inc*FORM_RAT[am[1]>>4])>>12),rph=v->ph[1];
 int32_t fg=256+am[0]*14,lp=v->s[0],bp=v->s[1],hold=v->s[3],hc=v->s[5],G=0,a1=0,ga1=0,cs=0,hn=1;
 if(am[2]){int32_t c=127-am[2];uint64_t ic=(uint64_t)inc*(uint64_t)(64+c*c*40/127)>>6;if(ic>681000000u)ic=681000000u;
  int32_t w=(int32_t)((ic*102944u)>>32);G=w+(w*w>>15)*w/98304;
  int32_t kq=62259-57344*am[2]/127;a1=(1<<30)/(32768+(G*(G+kq)>>15));ga1=(int32_t)(((int64_t)G*a1)>>15);}
 if(am[3]){cs=am[3]*13/127;hn=1+am[3]*am[3]*14/16129;}
 /* the drums' place: the part's first voice sounding */
 form_dr_t*dr=&form_dr[tr];uint32_t kit=(uint32_t)clamp(p[P_CSTR],0,2),drums=0,seed=(uint32_t)clamp(p[P_E7],0,127),dens=(uint32_t)clamp(p[P_DXIMG],0,127);
 int32_t lev=clamp(p[P_DETUNE],0,127)*258;
 if(kit){for(k=0;k<NVOICE;k++)if(t->v[k].active){drums=&t->v[k]==v;break;}}
 int sc=form_scope_tr==tr&&v==&t->v[0];
 for(i=0;i<n;i++){
  int32_t x=(usea?fvm_sample(FT_FIX,Y[i])*ga:0)+(useb?fvm_sample(FT_FIX,Z[i])*gb:0);x>>=7;
  if(am[0])x+=(form_fold(clamp(x*fg>>8,-262144,262143))-x)*am[0]>>7;                   /* A: folded */
  if(am[1]){rph+=rinc;x+=((mulq15(x,sine_i(rph))<<1)-x)*am[1]>>7;}                     /* B: a ring at a ratio of the note */
  if(am[2]){int32_t v3=x-lp,v1=(int32_t)(((int64_t)a1*bp+(int64_t)ga1*v3)>>15),v2=lp+(int32_t)(((int64_t)G*v1)>>15);
   bp=clamp(2*v1-bp,-(1<<20),1<<20);lp=clamp(2*v2-lp,-(1<<20),1<<20);x+=(v2*45/32-x)*am[2]>>7;}   /* C: the acid filter */
  if(cs){if(++hc>=hn){hc=0;hold=(x>>cs)<<cs;}x=hold;}                                   /* D: crushed */
  if(drums){uint32_t st=P[i]>>14;int32_t d=0;
   if((int32_t)st!=dr->last){dr->last=(int32_t)st;
    if(fvm_eu(st,seed)&&(!(st&3u)||dens>64u)){dr->ka=32767;dr->kp=32767;dr->kph=0;}
    if(fvm_eu(st+4u,seed+5u)&&(st&7u)==4u)dr->sa=26000;
    if(fvm_eu(st*3u,seed+9u)||dens>90u)dr->ha=20000;}
   if(dr->ka>64){dr->kph+=8765235u+(uint32_t)dr->kp*1070u;int32_t s=sine_i(dr->kph);
    if(kit==2u)s=form_fold(s*3);d+=mulq15(s,dr->ka);dr->ka-=dr->ka>>12;dr->kp-=dr->kp>>10;}
   dr->nz=dr->nz*1664525u+1013904223u;int32_t w=(int32_t)dr->nz>>16;
   if(dr->sa>64){d+=mulq15(mulq15(w,22938)+(sine_i(dr->sph+=37008000u)>>1),dr->sa);dr->sa-=dr->sa>>10;}
   if(dr->ha>64){int32_t h=kit==2u?(w<0?-16384:16384):w-dr->hp;dr->hp=w;d+=mulq15(h,dr->ha)>>1;dr->ha-=dr->ha>>8;}
   x=(x*26214>>15)+mulq15(d,lev>>1);}
  if(sc&&form_sc_i<64u)form_scope[0][form_sc_i++]=(int16_t)clamp(x,-32767,32767);
  out[i]+=voice_amp(soft_knee(x,16000)<<1,m,i);
 }
 if(sc&&form_sc_i>=64u)form_sc_i=0;
 v->s[0]=lp;v->s[1]=bp;v->s[2]=(int32_t)(q+(n<<sh));v->s[3]=hold;v->s[5]=hc;v->ph[0]=ph;v->ph[1]=rph;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"FORM","SHAPE"},
 .edit={{"FORM A",F_ENUM,0,FORM_NT-1,3,FORM_NAMES},{"FORM B",F_ENUM,0,FORM_NT-1,6,FORM_NAMES},{"MORPH",F_PCT,0,127,0,0},{"ROOT",F_INT,-24,24,0,0},
        {"A",F_PCT,0,127,64,0},{"B",F_PCT,0,127,32,0},{"C",F_PCT,0,127,16,0},{"D",F_PCT,0,127,0,0}},
 .presets=FORM_PRESETS,.npresets=NELEM(FORM_PRESETS),.knob={P_E0,P_E2,P_E4,P_E5},.poly=1,.half=1,.note_on=form_note_on,.render=form_render};
