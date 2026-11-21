/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * FORM (BYTE 0.17, the owner: "a monophonic entity, sound plasticine: a drone the modulators turn into every stage of
 * the beat; silence by the frequency taken to the bottom; sudden automations of the timbre"). One voice, one drone that
 * never stops: SHAPE (sine .. triangle .. saw .. pulse), FM, FOLD, CRUSH. Four modulators, each its own euclid (HITS of
 * STEPS from a table, or FUSE: two drawn from the seed and joined; its rotation the seed's), each hit an envelope of its
 * DECAY, by its AMOUNT, to its DEST:
 *   UP     the pitch thrown up and falling back (fast: a kick's sweep; slow: a slide)
 *   DIVE   the pitch taken down to nothing (the silence: the drone under hearing) and back
 *   NOISE  the drone turned to bit noise (a hat, a snare)
 *   FM FOLD CRUSH SHAPE  the timbre's own, opened
 *   CHORD  its third and fifth joined in (the same voice, a chord)
 *   JUMP   the pitch stepped to another degree and held (a melody: octaves, fifths, by the hits)
 * where they meet they compound. RATE the step (1/4 .. 1/32, the song's), SEED the rotations and the fusions, ROOT the
 * key's. At half the rate, all in C (no formula machine): one voice's light cost. */
static const char *const FORM_RUN[]={"TRIG","PLAY","FREE",0};
static const char *const FORM_DEST[]={"OFF","UP","DIVE","NOISE","FM","FOLD","CRUSH","SHAPE","CHORD","JUMP",0};
enum{FD_OFF,FD_UP,FD_DIVE,FD_NOISE,FD_FM,FD_FOLD,FD_CRUSH,FD_SHAPE,FD_CHORD,FD_JUMP,FD_N};
static const char *const FORM_EU[]={"FUSE","1/4","2/4","3/4","1/8","2/8","3/8","5/8","7/8","2/5","3/5","3/7","4/7","2/9","4/9","5/9",
 "5/12","7/12","4/11","5/13","3/16","5/16","7/16","9/16",0};
static const uint8_t FORM_EUK[24][2]={{0,0},{1,4},{2,4},{3,4},{1,8},{2,8},{3,8},{5,8},{7,8},{2,5},{3,5},{3,7},{4,7},{2,9},{4,9},{5,9},
 {5,12},{7,12},{4,11},{5,13},{3,16},{5,16},{7,16},{9,16}};
#define FORM_NEU 24u
static const char *const FORM_RATE[]={"1/4","1/8","1/16","1/32",0};
/* (the owner: "the drone by default under audio rate: a rhythm, an LFO locked to a resolution; from there up stretched to
 * every timbre") FREQ: a cycle every 4 BAR .. 1/128, its phase the transport's, then the audio's: semitones on the note */
static const char *const FORM_FREQ[]={"4BAR","2BAR","1BAR","1/2","1/4","1/8","1/16","1/32","1/64","1/128",
 "-24","-19","-12","-7","-5","0","+5","+7","+12","+19","+24","+36",0};
static const int8_t FORM_FOFS[12]={-24,-19,-12,-7,-5,0,5,7,12,19,24,36};
#define FORM_NF 22u
/* the modulators' four fields each: DEST EUCLID AMOUNT DECAY (MOD 1's DEST EUCLID in EDIT's E6 E7, the rest FORM's slots) */
static const uint8_t FORM_MF[4][4]={{P_E6,P_E7,P_CAOS,P_ASWING},{P_APROB,P_ARATE,P_AORDER,P_AGATE},{P_AOCT,P_DETUNE,P_CVOI,P_CSTR},{P_DXIMG,P_MORPH,P_FRB,P_KEY}};
static int form_owned_param(uint32_t id){return id==P_LRTRG||id==P_ROOT||id==P_CAOS||id==P_ASWING||id==P_APROB||id==P_ARATE||id==P_AORDER||id==P_AGATE||
 id==P_AOCT||id==P_DETUNE||id==P_CVOI||id==P_CSTR||id==P_DXIMG||id==P_MORPH||id==P_FRB||id==P_KEY;}
static const param_desc_t FORM_MD[4]={{"DEST",F_ENUM,0,FD_N-1,0,FORM_DEST},{"EUCLID",F_ENUM,0,FORM_NEU-1,0,FORM_EU},{"AMOUNT",F_PCT,0,127,0,0},{"DECAY",F_PCT,0,127,40,0}};
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t run={"RUN",F_ENUM,0,2,1,FORM_RUN},root={"FREQ",F_ENUM,0,FORM_NF-1,6,FORM_FREQ};
 if(id==P_LRTRG)return &run;
 if(id==P_ROOT)return &root;
 for(uint32_t m=0;m<4u;m++)for(uint32_t f=0;f<4u;f++)if(FORM_MF[m][f]==id)return &FORM_MD[f];
 return 0;
}
/* the states (presets): RATE SEED SHAPE FM FOLD CRUSH, then each modulator's DEST EUCLID AMOUNT DECAY */
#define FP(label,fr,ra,se,sh,fm,fo,cr,d1,e1,a1,k1,d2,e2,a2,k2,d3,e3,a3,k3,d4,e4,a4,k4) {label,{ra,se,sh,fm,fo,cr,d1,e1},{0,0,127,24},0,TAG(FX),FX(0,10,30,30),0,\
  MODS(P_LRTRG,1,P_ROOT,fr,P_CAOS,a1,P_ASWING,k1,P_APROB,d2,P_ARATE,e2,P_AORDER,a2,P_AGATE,k2,P_AOCT,d3,P_DETUNE,e3,P_CVOI,a3,P_CSTR,k3,P_DXIMG,d4,P_MORPH,e4,P_FRB,a4,P_KEY,k4)}
static const preset_t FORM_PRESETS[]={
 /*           FREQ RATE SEED SHAPE FM  FOLD CRUSH   MOD 1: DEST EU AMT DEC      MOD 2               MOD 3               MOD 4 */
 FP("01 PULSE",  6,    2,  7,  30, 20,  10,  0,     FD_UP,1,110,12,       FD_NOISE,11,70,8,    FD_JUMP,9,60,90,      FD_FM,13,60,40),
 FP("02 CRITTER",  7,  2, 11,  70, 50,  40, 30,     FD_UP,7,90,10,        FD_NOISE,12,80,6,    FD_FOLD,16,100,30,    FD_JUMP,15,80,70),
 FP("03 DRONE", 10,    2,  3,  20, 30,  20,  0,     FD_DIVE,4,60,110,     FD_FM,9,60,90,       FD_CHORD,13,90,120,   FD_SHAPE,18,70,100),
 FP("04 BITSWARM",  8, 3, 21, 110, 20,  20, 90,     FD_NOISE,22,90,6,     FD_UP,7,100,8,       FD_CRUSH,0,100,20,    FD_JUMP,21,90,40),
 FP("05 WOBBLE",  5,   2,  5,  80, 70,  60, 20,     FD_UP,1,100,14,       FD_FOLD,5,110,60,    FD_DIVE,14,70,50,     FD_FM,0,90,80),
 FP("06 GLASS", 15,    2, 17,   0, 70,   0,  0,     FD_FM,11,110,50,      FD_JUMP,15,90,100,   FD_CHORD,9,80,90,     FD_NOISE,23,40,4),
 FP("07 BREAK",  6,    2,  9,  60, 40,  70, 50,     FD_UP,7,120,10,       FD_NOISE,12,110,10,  FD_DIVE,22,90,20,     FD_CRUSH,16,90,30),
 FP("08 AMBIENT", 12,  1, 13,  15, 25,  10,  0,     FD_CHORD,9,100,127,   FD_JUMP,13,80,120,   FD_FM,18,60,110,      FD_DIVE,4,40,127),
 FP("09 SILENCE",  4,  2, 29,  50, 40,  30, 10,     FD_DIVE,0,127,60,     FD_UP,7,110,9,       FD_NOISE,11,90,7,     FD_FOLD,13,100,40),
 FP("10 MACHINE",  7,  2, 31,  90, 60,  50, 60,     FD_UP,3,127,8,        FD_NOISE,20,100,5,   FD_SHAPE,12,127,30,   FD_JUMP,0,100,60),
};
#undef FP
#define FORM_N ((uint32_t)NELEM(FORM_PRESETS))
/* ---- the dice (ui_input.c: SELECT the whole of it; SEQ + SELECT and the generator its modulators alone) ---- */
static const uint8_t FORM_DICE_COMP[8]={P_CAOS,P_ASWING,P_AORDER,P_AGATE,P_CVOI,P_CSTR,P_FRB,P_KEY};
static int16_t form_rnd(uint32_t n){return (int16_t)(n?rng()%n:0u);}
static void form_dice(track_t*t,uint32_t comp_only)
{
 int16_t*p=t->p;
 if(!comp_only){p[P_E1]=form_rnd(128u);p[P_E2]=form_rnd(128u);p[P_E3]=form_rnd(100u);p[P_E4]=form_rnd(90u);p[P_E5]=form_rnd(3u)?0:form_rnd(100u);}
 for(uint32_t m=0;m<4u;m++){
  if(!comp_only){p[FORM_MF[m][0]]=(int16_t)(1+form_rnd(FD_N-1u));p[FORM_MF[m][1]]=form_rnd(FORM_NEU);}
  p[FORM_MF[m][2]]=(int16_t)(40+form_rnd(88u));p[FORM_MF[m][3]]=form_rnd(128u);
 }
}
static uint32_t form_cost(const track_t*t){(void)t;return 2u;}
static void form_note_on(track_t*t,voice_t*v){(void)t;memset(v->s,0,sizeof v->s);v->ph[0]=v->ph[1]=v->ph[2]=0;}
/* a part's modulators: their envelopes (Q15), the step they last read, the jump's interval; the crush and DC states */
typedef struct{int32_t e[4];int32_t last;int32_t jump;uint32_t nz;int32_t hold,hc,dcx,dcy;} form_st_t;
static form_st_t form_st[NTRK];
static int16_t form_scope[64];
static volatile uint8_t form_scope_tr=0xFF;
static uint8_t form_sc_i;
static uint8_t form_lit[NTRK];                           /* (the panel: the modulators that struck this step, bit m) */
/* modulator m's hit at step s */
static int form_hit(const int16_t*p,uint32_t m,uint32_t s,uint32_t seed)
{
 uint32_t e=(uint32_t)clamp(p[FORM_MF[m][1]],0,FORM_NEU-1);
 if(!e)return (int)fvm_eu(s,seed+m*37u);
 uint32_t k=FORM_EUK[e][0],n=FORM_EUK[e][1],r=fvm_hash(seed*0x9E3779B1u+m*0x85EBCA77u)%n,q=(s+r)%n;
 return ((q+1u)*k/n)!=(q*k/n);
}
static int32_t form_fold(int32_t x){uint32_t q=((uint32_t)(x+32768)<<15)>>16;int32_t d=(int32_t)q-32768;return 32767-2*(d<0?-d:d);}
static __attribute__((noinline)) void form_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 uint32_t tr=(uint32_t)(t-trk)%NTRK,i,k,sh=VH_SH(),seed=(uint32_t)clamp(t->p[P_E1],0,127)*977u+1u;
 const int16_t*p=t->p;form_st_t*st=&form_st[tr];
 if(!n)return;
 /* the clock: the transport's (not FREE), else the voice's own; the step at RATE */
 uint32_t q=(uint32_t)v->s[2],bps=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300),end;
 if(func_bar(t))end=func_clock_end;
 else end=(uint32_t)(((uint64_t)(q+(n<<sh))<<16)/bps);
 uint32_t rs=(uint32_t)clamp(p[P_E0],0,3),s1=end>>(16-rs);
 /* the modulators: a hit at a new step sets its envelope; each falls by its DECAY a block */
 int32_t amt[4],dst[4];
 for(k=0;k<4u;k++){dst[k]=clamp(p[FORM_MF[k][0]],0,FD_N-1);amt[k]=clamp(p[FORM_MF[k][2]],0,127);}
 if((int32_t)s1!=st->last){
  uint32_t lit=0;st->last=(int32_t)s1;
  for(k=0;k<4u;k++)if(dst[k]&&form_hit(p,k,s1,seed)){st->e[k]=32767;lit|=1u<<k;
   if(dst[k]==FD_JUMP){static const int8_t J[8]={0,12,7,19,5,24,-12,3};st->jump=J[fvm_hash(s1*0x27D4EB2Fu^seed)&7u]*amt[k]/127;}}
  form_lit[tr]=(uint8_t)lit;
 }
 int32_t ev[FD_N]={0};
 for(k=0;k<4u;k++){
  int32_t d=clamp(p[FORM_MF[k][3]],0,127),rate=4+(127-d)*(127-d)/40;   /* the fall a block: ~3 ms (DECAY 0) .. ~3 s (127) */
  int32_t x=st->e[k]*amt[k]>>7;if(dst[k])ev[dst[k]]+=x;
  st->e[k]-=(int32_t)(((int64_t)st->e[k]*rate)>>10);if(st->e[k]<16)st->e[k]=0;
 }
 for(k=1;k<FD_N;k++)if(ev[k]>32767)ev[k]=32767;
 /* the frequency: FREQ's (a tempo's cycle, its phase the transport's while nothing moves it; or the note's, shifted); UP
 * thrown up four octaves and falling, JUMP held, both as octaves on it; DIVE down eight (the silence) */
 uint32_t fq=(uint32_t)clamp(p[P_ROOT],0,FORM_NF-1),inc,locked=0;
 int32_t up16=(ev[FD_JUMP]?st->jump*16:0)+(int32_t)((int64_t)ev[FD_UP]*768>>15);
 if(!ev[FD_JUMP])st->jump=0;
 if(fq<10u){uint32_t sb=(bps>>sh)?(bps>>sh):1u,i1=0xFFFFFFFFu/sb;inc=fq>=4u?i1<<(fq-4u):i1>>(4u-fq);locked=!up16&&!ev[FD_DIVE]&&func_bar(t);}
 else inc=pitch_inc((uint32_t)clamp(m->pitch16+FORM_FOFS[fq-10u]*16,0,2047));
 if(up16){int32_t o=up16/192,f=up16-o*192;if(f<0){f+=192;o--;}
  uint32_t r=(pitch_inc((uint32_t)f)*16u)/((pitch_inc(0)>>12)?(pitch_inc(0)>>12):1u);inc=(uint32_t)(((uint64_t)inc*r)>>16);
  if(o>=0){while(o-->0&&inc<0x20000000u)inc<<=1;}else inc>>=(uint32_t)(-o>31?31:-o);}
 {uint32_t dv=(uint32_t)ev[FD_DIVE]*8u,oc=dv>>15,fr=(dv&32767u)>>3;inc=(inc>>oc)-(uint32_t)(((uint64_t)(inc>>oc)*fr)>>13);}   /* DIVE: down to 8 octaves under the note (no floor: the silence) */
 uint32_t ph=locked?func_clock_begin<<(12u+fq):v->ph[0],mph=v->ph[1],cph=v->ph[2],c5=(uint32_t)v->s[3];   /* (locked: the cycle on the beat) */
 /* the timbre now: its knobs and the modulators' */
 int32_t shp=clamp(p[P_E2]*258+(ev[FD_SHAPE]>>1),0,32767),fm=clamp(p[P_E3]*258+ev[FD_FM],0,32767),fo=clamp(p[P_E4]*258+ev[FD_FOLD],0,32767);
 int32_t cr=clamp(p[P_E5]*258+ev[FD_CRUSH],0,32767),nzv=ev[FD_NOISE],chv=ev[FD_CHORD];
 uint32_t cs=(uint32_t)(cr*12>>15),hn=1u+(uint32_t)((cr>>11)*(cr>>11)>>4),c3=(seed>>3)&1u?20649u:21845u;   /* (the chord's third: minor or major, x Q14) */
 int32_t hold=st->hold,hc=st->hc,dx=st->dcx,dy=st->dcy;uint32_t nz=st->nz;
 int sc=form_scope_tr==tr&&v==&t->v[0];
 for(i=0;i<n;i++){
  ph+=inc;mph+=inc<<1;
  uint32_t pp=ph+(uint32_t)((int64_t)sine_i(mph)*fm<<3);      /* FM: the modulator an octave up, up to ~2 turns */
  int32_t sn=sine_i(pp),u=(int32_t)(pp>>16)-32768,tri=32767-2*(u<0?-u:u),saw=u,pul=u<0?-32767:32767,y;
  if(shp<10923)y=sn+(int32_t)(((int64_t)(tri-sn)*(shp*3))>>15);
  else if(shp<21846)y=tri+(int32_t)(((int64_t)(saw-tri)*((shp-10923)*3))>>15);
  else y=saw+(int32_t)(((int64_t)(pul-saw)*((shp-21846)*3))>>15);
  if(chv){cph+=(uint32_t)(((uint64_t)inc*c3)>>14);c5+=inc+(inc>>1);   /* CHORD: the third and the fifth (x1.5) */
   y+=((sine_i(cph)+sine_i(c5))>>1)*chv>>15;}
  if(fo>256)y+=(int32_t)((int64_t)(form_fold(clamp(y*(256+(fo>>7)*5)>>8,-262144,262143))-y)*fo>>15);
  if(nzv){uint32_t tt=q+(i<<sh);nz=nz*1664525u+1013904223u;int32_t w=(int32_t)(((tt*((tt>>5)|(tt>>8)))&255u)<<8)-32768;
   w=(((int32_t)nz>>16)*3+w)>>2;y+=(int32_t)((int64_t)(w-y)*nzv>>15);}   /* (white, the bits' grain in it: bright) */
  if(cs){if(++hc>=(int32_t)hn){hc=0;hold=(y>>cs)<<cs;}y=hold;}
  int32_t o=y-dx+(dy*253>>8);dx=y;dy=o;                         /* (under ~40 Hz out: the dive's bottom silent) */
  if(sc&&form_sc_i<64u)form_scope[form_sc_i++]=(int16_t)clamp(o,-32767,32767);
  out[i]+=voice_amp(soft_knee(o,20000),m,i);
 }
 if(sc&&form_sc_i>=64u)form_sc_i=0;
 st->hold=hold;st->hc=hc;st->dcx=dx;st->dcy=dy;st->nz=nz;
 v->s[2]=(int32_t)(q+(n<<sh));v->ph[0]=ph;v->ph[1]=mph;v->ph[2]=cph;v->s[3]=(int32_t)c5;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"FORM","DRONE"},
 .edit={{"RATE",F_ENUM,0,3,2,FORM_RATE},{"SEED",F_INT,0,127,0,0},{"SHAPE",F_PCT,0,127,30,0},{"FM",F_PCT,0,127,20,0},
        {"FOLD",F_PCT,0,127,10,0},{"CRUSH",F_PCT,0,127,0,0},{"DEST",F_ENUM,0,FD_N-1,1,FORM_DEST},{"EUCLID",F_ENUM,0,FORM_NEU-1,7,FORM_EU}},
 .presets=FORM_PRESETS,.npresets=NELEM(FORM_PRESETS),.knob={P_E2,P_E3,P_E4,P_E5},.poly=1,.half=1,.note_on=form_note_on,.render=form_render};
