/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * FORM (BYTE 0.17, the owner: "sound plasticine: one sound morphing with every modulator; start from something simple
 * and add layers of different kinds of modulation"). ONE voice, ONE sound: a drone (SHAPE FM FOLD CRUSH, its PALET
 * SOFT or PD) and eight parameters every modulator only ADDS to (nothing replaces anything: where they meet they mix):
 *   PITCH FM FOLD CRUSH NOISE (the drone's own phase dirtied) SHAPE LEVEL RATIO (the FM's)
 * Four modulators (MOD 1..4; KNOB 1 of the MOD page picks the one the others edit), each a SHAPE out of five tables
 * (TABLES, HITS, GESTURES, NOTES, FM: the keys' pages, OCT -2..+2), its DEST, its EUCLID ("--": only the keys), AMOUNT
 * and DECAY. A HIT (kick, snare, hat ..) is a bundle of moves of the same sound (the kick: an octave down, to sine,
 * louder); a NOTE lifts the drone to the root and steps it (arps, chords strummed at 1/64 as the old chips did).
 * RUN PLAY / FREE: the keys are gestures, not notes (eng_form.c form_key from seq.c key_on): the whites the shapes of
 * the page into the chosen MOD (velocity its amount, aftertouch more), the blacks MOD 1..4 (tapped: chosen; held: the
 * whites into it), LATCH x2 /2 INV; C#5..G5 the root (the scale's degrees). At half the rate, all in C. */
static const char *const FORM_RUN[]={"TRIG","PLAY","FREE",0};
static const char *const FORM_DEST[]={"PITCH","FM","FOLD","CRUSH","NOISE","SHAPE","LEVEL","RATIO",0};
enum{FD_PITCH,FD_FM,FD_FOLD,FD_CRUSH,FD_NOISE,FD_SHAPE,FD_LEVEL,FD_RATIO,FD_N};
static const char *const FORM_SHAPE[]={
 "SINE","S&H","DRIFT","SPRING","STAIRS","BOUNCE","EXP UP","LOG DN","CHAOS","SWEEP","RND GATE","TRIPLET",
 "KICK","SNARE","HAT","OPEN HAT","CLAP","RIM","TOM","ZAP","BITHAT","NOISE DN","SWELL","CRASH",
 "RAMP UP","RAMP DN","TRI","GATE","ENV UP","ENV DN","RISER","STUTTER","STUT UP","DOWN","UP","REST",
 "ARP UP","ARP DN","ARP UPDN","ARP RND","CHORD m","CHORD M","SUS4","m7","CLUSTER","+OCT","+5TH","GLIDE DN",
 "FM x1/2","FM x1","FM x1.5","FM x2","FM x3","FM x5","FM x7",0};
#define FORM_NSH 55u
enum{FPG_TAB,FPG_HIT,FPG_GES,FPG_NOTE,FPG_FM};
enum{S_SINE=0,S_KICK=12,S_SNARE,S_HAT,S_OHAT,S_CLAP,S_RIM,S_TOM,S_ZAP,S_BITHAT,S_NOISEDN,S_SWELL,S_CRASH,
 S_RAMPUP=24,S_RAMPDN,S_TRI,S_GATE,S_ENVUP,S_ENVDN,S_RISER,S_STUT,S_STUTUP,S_DOWN,S_UP,S_REST,
 S_ARPUP=36,S_ARPDN,S_ARPUD,S_ARPRND,S_CHm,S_CHM,S_SUS4,S_m7,S_CLUSTER,S_OCT,S_FIFTH,S_GLIDE};
#define FORM_DS(d,s) ((d)+8*(s))                         /* a modulator's DEST, SHAPE and RATE, one slot: */
#define FORM_DSMAX (512*7-1)                             /* DEST + 8 x SHAPE + 512 x RATE (0: FORM's RATE) */
static const char *const FORM_MRATE[]={"FORM","1/1","1/2","1/4","1/8","1/16","1/32",0};
/* a modulator's step: 2^e a beat (e -2 .. 3: 1/1 .. 1/32), its RATE's or FORM's */
static int32_t form_mexpo(const int16_t*p,int32_t ds){int32_t r=ds>>9;return r?r-3:clamp(p[P_E0],0,3);}
static const char *const FORM_EU[]={"--","FUSE","1/4","2/4","3/4","1/8","2/8","3/8","5/8","7/8","2/5","3/5","3/7","4/7","2/9","4/9","5/9",
 "5/12","7/12","4/11","5/13","3/16","5/16","7/16","9/16",0};
static const uint8_t FORM_EUK[25][2]={{0,0},{0,0},{1,4},{2,4},{3,4},{1,8},{2,8},{3,8},{5,8},{7,8},{2,5},{3,5},{3,7},{4,7},{2,9},{4,9},{5,9},
 {5,12},{7,12},{4,11},{5,13},{3,16},{5,16},{7,16},{9,16}};
#define FORM_NEU 25u
static const char *const FORM_RATE[]={"1/4","1/8","1/16","1/32",0};
/* FREQ: the drone a cycle every 4 BAR .. 1/128 on the transport's phase (a rhythm: under hearing until a modulator
 * stretches it), then the audio's semitones on the note */
static const char *const FORM_FREQ[]={"4BAR","2BAR","1BAR","1/2","1/4","1/8","1/16","1/32","1/64","1/128",
 "-24","-19","-12","-7","-5","0","+5","+7","+12","+19","+24","+36",0};
static const int8_t FORM_FOFS[12]={-24,-19,-12,-7,-5,0,5,7,12,19,24,36};
#define FORM_NF 22u
static const char *const FORM_PAL[]={"SOFT","PD",0};
/* the modulators' slots: DEST+SHAPE (FORM_DS), EUCLID, AMOUNT, DECAY; PALET in P_FRA */
static const uint8_t FORM_MF[4][4]={{P_E6,P_E7,P_CAOS,P_ASWING},{P_APROB,P_ARATE,P_AORDER,P_AGATE},{P_AOCT,P_DETUNE,P_CVOI,P_CSTR},{P_DXIMG,P_MORPH,P_FRB,P_KEY}};
static int form_owned_param(uint32_t id){return id==P_LRTRG||id==P_ROOT||id==P_FRA||id==P_CAOS||id==P_ASWING||id==P_APROB||id==P_ARATE||id==P_AORDER||id==P_AGATE||
 id==P_AOCT||id==P_DETUNE||id==P_CVOI||id==P_CSTR||id==P_DXIMG||id==P_MORPH||id==P_FRB||id==P_KEY;}
static const param_desc_t FORM_MD[4]={{"DS",F_INT,0,FORM_DSMAX,0,0},{"EUCLID",F_ENUM,0,FORM_NEU-1,0,FORM_EU},{"AMOUNT",F_PCT,0,127,100,0},{"DECAY",F_PCT,0,127,14,0}};
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t run={"RUN",F_ENUM,0,2,1,FORM_RUN},root={"FREQ",F_ENUM,0,FORM_NF-1,6,FORM_FREQ},pal={"PALET",F_ENUM,0,1,0,FORM_PAL};
 if(id==P_LRTRG)return &run;
 if(id==P_ROOT)return &root;
 if(id==P_FRA)return &pal;
 for(uint32_t m=0;m<4u;m++)for(uint32_t f=0;f<4u;f++)if(FORM_MF[m][f]==id)return &FORM_MD[f];
 return 0;
}
/* the MOD page's views (params.c page_desc, ui_input.c): KNOB 1 the modulator, 2 its DEST, 3 its SHAPE (one slot) */
#define ID_FV0 0xDCu
_Static_assert(P_COUNT<ID_FV0,"FORM's view ids over the parameters");
static const char *const FORM_MODN[]={"1","2","3","4",0};
static const param_desc_t FORM_VD[4]={{"MOD",F_ENUM,0,3,0,FORM_MODN},{"DEST",F_ENUM,0,FD_N-1,0,FORM_DEST},{"SHAPE",F_ENUM,0,FORM_NSH-1,0,FORM_SHAPE},{"RATE",F_ENUM,0,6,0,FORM_MRATE}};
static int16_t form_view[4];
static const param_desc_t *form_view_desc(const track_t*t,uint32_t f,int16_t**valp){
 uint32_t sel=(uint32_t)form_view[0]&3u;int32_t ds=clamp(t->p[FORM_MF[sel][0]],0,FORM_DSMAX);
 form_view[0]=(int16_t)sel;form_view[1]=(int16_t)(ds&7);form_view[2]=(int16_t)((ds&511)>>3);form_view[3]=(int16_t)(ds>>9);*valp=&form_view[f];return &FORM_VD[f];
}
static void form_view_put(track_t*t,uint32_t f,int32_t v){
 uint32_t sel=(uint32_t)form_view[0]&3u;int32_t ds=clamp(t->p[FORM_MF[sel][0]],0,FORM_DSMAX);
 if(f==0){form_view[0]=(int16_t)(v&3);return;}
 if(f==1)ds=(ds&~7)|clamp(v,0,FD_N-1);else if(f==2)ds=(ds&~0x1F8)|(clamp(v,0,FORM_NSH-1)<<3);else ds=(ds&511)|(clamp(v,0,6)<<9);
 t->p[FORM_MF[sel][0]]=(int16_t)ds;
}
/* a FORM page's slot: MOD 1's fields stand for the chosen modulator's */
static uint32_t form_page_id(uint32_t id){
 for(uint32_t f=1;f<4u;f++)if(id==FORM_MF[0][f])return FORM_MF[form_view[0]&3][f];
 return id;
}
/* the states: FREQ RATE SEED SHAPE FM FOLD CRUSH, then each modulator's DEST+SHAPE EUCLID AMOUNT DECAY */
#define FP(label,fr,ra,se,sh,fm,fo,cr,d1,e1,a1,k1,d2,e2,a2,k2,d3,e3,a3,k3,d4,e4,a4,k4) {label,{ra,se,sh,fm,fo,cr,d1,e1},{0,0,127,24},0,TAG(FX),FX(0,10,30,30),0,\
  MODS(P_LRTRG,1,P_ROOT,fr,P_FRA,0,P_CAOS,a1,P_ASWING,k1,P_APROB,d2,P_ARATE,e2,P_AORDER,a2,P_AGATE,k2,P_AOCT,d3,P_DETUNE,e3,P_CVOI,a3,P_CSTR,k3,P_DXIMG,d4,P_MORPH,e4,P_FRB,a4,P_KEY,k4)}
#define FORM_PLIST(X) \
 X("01 DRONE",     12,  2,   7,  30,  5,  0,  0,  FORM_DS(FD_PITCH,S_KICK),0,110,10,    FORM_DS(FD_PITCH,S_ARPUP),0,90,14,    FORM_DS(FD_FOLD,S_TRI),0,80,30,      FORM_DS(FD_NOISE,S_HAT),0,80,10) \
 X("02 PLASTICINE", 6,  2,  17,  30,  5,  0,  0,  FORM_DS(FD_PITCH,S_ENVDN),1,100,14,   FORM_DS(FD_FM,S_STUT),7,64,6,         FORM_DS(FD_FOLD,S_TRI),8,76,30,      FORM_DS(FD_PITCH,S_UP),0,100,14) \
 X("03 BEAT",       6,  2,  11,  30,  5,  0,  0,  FORM_DS(FD_PITCH,S_KICK),1,115,8,     FORM_DS(FD_PITCH,S_SNARE),21,90,8,    FORM_DS(FD_PITCH,S_HAT),22,76,8,     FORM_DS(FD_PITCH,S_ARPUP),7,90,14) \
 X("04 MELODY",    12,  2,   5,  20, 10,  0,  0,  FORM_DS(FD_PITCH,S_ARPUD),23,100,14,  FORM_DS(FD_FM,S_ENVDN),7,80,20,       FORM_DS(FD_SHAPE,S_TRI),0,70,30,     FORM_DS(FD_PITCH,3),0,60,20) \
 X("05 SILENCE",    6,  2,  29,  40,  5,  0,  0,  FORM_DS(FD_PITCH,S_REST),10,127,40,  FORM_DS(FD_PITCH,S_UP),7,110,10,      FORM_DS(FD_NOISE,S_STUT),12,80,6,    FORM_DS(FD_FOLD,S_SINE),0,80,40) \
 X("06 MACHINE",    7,  2,  31,  50, 10,  10, 0,  FORM_DS(FD_PITCH,S_KICK),4,120,8,     FORM_DS(FD_PITCH,S_CLAP),21,90,8,     FORM_DS(FD_FM,S_RAMPUP),0,70,20,     FORM_DS(FD_PITCH,S_ZAP),13,80,10)
/* (a preset's DEST+SHAPE+RATE over int8's 127: set in full after it, form_preset_ds; the preset's own slots 0) */
#define FPX(label,fr,ra,se,sh,fm,fo,cr,d1,e1,a1,k1,d2,e2,a2,k2,d3,e3,a3,k3,d4,e4,a4,k4) FP(label,fr,ra,se,sh,fm,fo,cr,0,e1,a1,k1,0,e2,a2,k2,0,e3,a3,k3,0,e4,a4,k4),
static const preset_t FORM_PRESETS[]={FORM_PLIST(FPX)};
#undef FPX
#define FPD(label,fr,ra,se,sh,fm,fo,cr,d1,e1,a1,k1,d2,e2,a2,k2,d3,e3,a3,k3,d4,e4,a4,k4) {d1,d2,d3,d4},
static const int16_t FORM_PDS[][4]={FORM_PLIST(FPD)};
#undef FPD
#undef FP
static void form_preset_ds(track_t*t,uint32_t pi){if(pi<NELEM(FORM_PDS))for(uint32_t m=0;m<4u;m++)t->p[FORM_MF[m][0]]=FORM_PDS[pi][m];}

#define FORM_N ((uint32_t)NELEM(FORM_PRESETS))
/* ---- the dice (ui_input.c: SELECT the whole of it; SEQ + SELECT and the generator its modulators alone) ---- */
static const uint8_t FORM_DICE_COMP[8]={P_CAOS,P_ASWING,P_AORDER,P_AGATE,P_CVOI,P_CSTR,P_FRB,P_KEY};
static int16_t form_rnd(uint32_t n){return (int16_t)(n?rng()%n:0u);}
static void form_dice(track_t*t,uint32_t comp_only)
{
 int16_t*p=t->p;
 if(!comp_only){p[P_E1]=form_rnd(128u);p[P_E2]=form_rnd(70u);p[P_E3]=form_rnd(40u);p[P_E4]=form_rnd(60u);p[P_E5]=0;}
 for(uint32_t m=0;m<4u;m++){
  if(!comp_only){p[FORM_MF[m][0]]=(int16_t)FORM_DS(form_rnd(FD_N),12+form_rnd(36u));p[FORM_MF[m][1]]=form_rnd(FORM_NEU);}
  p[FORM_MF[m][2]]=(int16_t)(40+form_rnd(88u));p[FORM_MF[m][3]]=form_rnd(128u);
 }
}
static uint32_t form_cost(const track_t*t){(void)t;return 2u;}
static void form_note_on(track_t*t,voice_t*v){(void)t;memset(v->s,0,sizeof v->s);v->ph[0]=v->ph[1]=0;}
/* ---- the gestures: a key's or a euclid's, alive until their envelope is out ---- */
typedef struct{uint8_t sh,mod,fl,vel,key,spd;uint16_t len;uint32_t t0,rel;} form_g_t;   /* fl: 1 held 2 latched 4 inverted 8 alive */
#define FORM_G 6u
typedef struct{int32_t last,lastm[4];int16_t dv[8];uint32_t clk,nz,hdown;int32_t nzl,lpz,hold,hc,dcx,dcy;int8_t hmod;   /* (hmod: the black held, MOD n+1; 0 none) */uint8_t modes;form_g_t g[FORM_G];} form_st_t;
static form_st_t form_st[NTRK] __attribute__((section(".pool")));   /* (out of the bss: its room) */
static int16_t form_scope[64];
static volatile uint8_t form_scope_tr=0xFF;
static uint8_t form_sc_i;
static uint8_t form_lit[NTRK];                           /* (the panel: the modulators that struck this step, bit m) */
/* modulator m's hit at step s ("--": none, FUSE: two drawn from the seed and joined) */
static int form_hit(const int16_t*p,uint32_t m,uint32_t s,uint32_t seed)
{
 uint32_t e=(uint32_t)clamp(p[FORM_MF[m][1]],0,FORM_NEU-1);
 if(!e)return 0;
 if(e==1u)return (int)fvm_eu(s,seed+m*37u);
 uint32_t k=FORM_EUK[e][0],n=FORM_EUK[e][1],r=fvm_hash(seed*0x9E3779B1u+m*0x85EBCA77u)%n,q=(s+r)%n;
 return ((q+1u)*k/n)!=(q*k/n);
}
static int32_t form_fold(int32_t x){uint32_t q=((uint32_t)(x+32768)<<15)>>16;int32_t d=(int32_t)q-32768;return 32767-2*(d<0?-d:d);}
/* 2^(-x/256), Q15 */
static int32_t form_fexp(uint32_t x){if(x>=15u*256u)return 0;uint32_t u=x&255u;int32_t m=32768-(int32_t)((u*(21512u-((u*5128u)>>8)))>>8);m>>=x>>8;return m>32767?32767:m;}
/* x * 2^(e/4096) */
static uint32_t form_mexp(uint32_t x,int32_t e){int32_t o=e>>12;uint32_t f=(uint32_t)e&4095u,m=65536u+((f*(43024u+((f*22512u)>>12)))>>12);uint64_t r=((uint64_t)x*m)>>16;
 if(o>0){while(o-->0&&r<0x40000000u)r<<=1;}else if(o<0)r>>=(uint32_t)(-o>31?31:-o);return r>0x30000000u?0x30000000u:(uint32_t)r;}
/* log2(x) * 4096 */
static int32_t form_log2(uint32_t x){if(!x)return 0;int32_t b=31-__builtin_clz(x);uint32_t f=(b>=12?x>>(b-12):x<<(12-b))&4095u;return b*4096+(int32_t)f+(int32_t)((((f*(4096u-f))>>12)*1420u)>>12);}
static uint32_t form_isqrt(uint32_t x){uint32_t r=0,b=1u<<30;while(b>x)b>>=2;while(b){if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
static int form_oneshot(uint32_t sh){return sh/12u==FPG_HIT||sh==S_ENVDN||sh==3u||sh==5u;}
static void form_spawn(form_st_t*st,uint32_t sh,uint32_t mod,uint32_t vel,uint32_t fl,uint32_t len,uint32_t key)
{
 uint32_t i,b=0;
 for(i=0;i<FORM_G;i++){form_g_t*g=&st->g[i];if(!(g->fl&8u)){b=i;break;}
  if(!(g->fl&3u)&&(st->g[b].fl&3u||g->t0<st->g[b].t0))b=i;}
 form_g_t*g=&st->g[b];
 g->sh=(uint8_t)sh;g->mod=(uint8_t)mod;g->vel=(uint8_t)vel;g->key=(uint8_t)key;g->len=(uint16_t)len;
 g->fl=(uint8_t)(fl|8u|((st->modes&8u)?4u:0u));g->spd=(uint8_t)(st->modes&2u?1u:st->modes&4u?2u:0u);g->t0=st->clk;g->rel=0;
}
/* the keys (seq.c key_on / key_off, RUN PLAY or FREE): physical key k (0 F3 .. 26 G5) down (vel) or up; a root key's
 * note returned (the caller's func_pitch), else 0 */
static const uint8_t FORM_KMAP[27]={0,16,1,17,2,18,3,4,19,5,20,6,7,21,8,22,9,23,10,11,32,33,34,35,36,37,38};   /* white n, 16+black n, 32+root */
static const uint8_t FORM_SCALE[7]={0,2,3,5,7,8,10};
static int form_keys(const track_t*t){return t->eng_req==ENGI_FORM&&t->p[P_LRTRG]!=0;}
static uint32_t form_key(track_t*t,uint32_t k,uint32_t vel,int on)
{
 uint32_t tr=(uint32_t)(t-trk)%NTRK,c=FORM_KMAP[k%27u],i;form_st_t*st=&form_st[tr];
 if(c>=32u)return on?48u+FORM_SCALE[c-32u]:0u;
 if(c>=16u){uint32_t b=c-16u;
  if(b<4u){if(on){st->hmod=(int8_t)(b+1u);st->hdown=st->clk;}else{if(st->clk-st->hdown<(3u<<15))form_view[0]=(int16_t)b;st->hmod=0;}return 0;}
  uint8_t bit=(uint8_t)(1u<<(b-4u));if(on)st->modes|=bit;else st->modes&=(uint8_t)~bit;return 0;}
 if(on){
  int32_t pg=clamp(song.octave,-2,2)+2;uint32_t sh=(uint32_t)pg*12u+c;
  if(sh>=FORM_NSH)return 0;
  for(i=0;i<FORM_G;i++)if((st->g[i].fl&10u)==10u&&st->g[i].key==k+1u){st->g[i].fl&=(uint8_t)~3u;return 0;}   /* (a latched one: let go) */
  form_spawn(st,sh,(uint32_t)(st->hmod>0?st->hmod-1:form_view[0]&3),vel,1u|((st->modes&1u)?2u:0u),0u,k+1u);
 }else for(i=0;i<FORM_G;i++)if((st->g[i].fl&8u)&&st->g[i].key==k+1u&&!(st->g[i].fl&2u))st->g[i].fl&=(uint8_t)~1u;
 return 0;
}
/* a shape's value at tau (steps, Q8), Q15 signed: the TABLES' and the GESTURES' */
static int32_t form_shape(uint32_t sh,uint32_t tau,uint32_t seed)
{
 uint32_t g=sh%12u;
 if(sh/12u==FPG_GES)switch(g){
  case 0:return (int32_t)((tau&1023u)<<5);
  case 1:return 32767-(int32_t)((tau&1023u)<<5);
  case 2:{int32_t x=(int32_t)((tau&1023u)<<6);return x<32768?x:65535-x;}
  case 3:return (tau&511u)<256u?32767:0;
  case 4:return tau>=1024u?32767:(int32_t)(tau<<5);
  case 5:return form_fexp((tau*332u)>>8);
  case 6:{int32_t r=tau>=4096u?32767:(int32_t)(tau<<3);return r*r>>15;}
  case 7:return (tau&127u)<64u?32767:0;
  case 8:{uint32_t q=(tau*tau)>>13;int32_t a=9830+(int32_t)(tau<<4);return (q&127u)<64u?(a>32767?32767:a):0;}
  case 9:return -32767;
  default:return 32767;
 }
 switch(g){
  case 0:return sine_i(tau<<22);
  case 1:return (int32_t)(fvm_hash(seed+(tau>>8))&65535u)-32768;
  case 2:{uint32_t i=tau>>10,f=tau&1023u;int32_t a=(int32_t)(fvm_hash(seed+i)&65535u)-32768,b=(int32_t)(fvm_hash(seed+i+1u)&65535u)-32768;return a+(b-a)*(int32_t)f/1024;}
  case 3:return form_fexp((tau*185u)>>8)*sine_i(tau*(3u<<22)+0x40000000u)>>15;
  case 4:return (int32_t)((tau>>10)&3u)*10922;
  case 5:{int32_t s=sine_i((tau*tau)<<9);return (s<0?-s:s)*form_fexp((tau*92u)>>8)>>15;}
  case 6:{int32_t r=tau>=2340u?32767:(int32_t)(tau*14u);return (r*r>>15)*r>>15;}
  case 7:{uint32_t s=tau>=3276u?32767u:tau*10u;return 32767-(int32_t)form_isqrt(s<<15);}
  case 8:return (int32_t)(fvm_hash(seed*7u+(tau>>7))&32767u);
  case 9:return (int32_t)((tau&16383u)<<1);
  case 10:return (fvm_hash(seed+(tau>>7))&3u)?0:32767;
  default:return ((tau*3u)&1023u)<512u?32767:0;
 }
}
/* a HIT's envelope (Q15) and its moves: lift, octaves (Q12), level, shape, fm, ratio (Q12 oct), noise, crush */
static int32_t form_henv(uint32_t g,uint32_t tau)
{
 switch(g){
  case 0:case 3:return form_fexp((tau*591u)>>8);
  case 1:{int32_t a=tau>=7u?32767:(int32_t)(tau*5120u);return form_fexp((tau*1477u)>>8)*a>>15;}
  case 2:{int32_t a=tau>=5u?32767:(int32_t)(tau*7680u);return form_fexp((tau*5170u)>>8)*a>>15;}
  case 4:return tau<77u?(((tau*10u)>>8)&1u?6554:32767):form_fexp(((tau-77u)*1108u)>>8);
  case 5:return form_fexp((tau*4800u)>>8);
  case 6:case 7:return form_fexp((tau*923u)>>8);
  case 8:return form_fexp((tau*2954u)>>8);
  case 9:return form_fexp((tau*258u)>>8);
  case 10:{int32_t r=tau>=2048u?32767:(int32_t)(tau<<4);return r*r>>15;}
  default:return form_fexp((tau*166u)>>8);
 }
}
static const int16_t FORM_HIT[12][8]={
 /* lift   oct    lvl    shape   fm    ratio  noise  crush */
 {32767,-6144,13107,-32767,   0,    0,    0,    0},   /* KICK (its octaves falling: form_eval) */
 {32767, 1229, 9830,-21845,3120, 1434,29491,    0},   /* SNARE */
 {32767,19661,    0,-10923,6760, 3645, 6554,    0},   /* HAT */
 {32767,19661,    0,-10923,6760, 3645, 6554,    0},   /* OPEN HAT */
 {32767,10650, 6554,-10923,1040,    0,32767,    0},   /* CLAP */
 {32767, 9830, 3277,-21845,2080,    0,    0,    0},   /* RIM */
 {32767,-2458,13107,-32767,   0,    0,    0,    0},   /* TOM */
 {32767,-4096, 9830,-10923,   0,-1638,    0,    0},   /* ZAP */
 {32767,15565, 3277,     0,   0,    0,    0,18022},   /* BITHAT */
 {    0,    0,    0,     0,   0,    0,29491,    0},   /* NOISE DN */
 {    0,    0,    0,     0,   0,    0,29491,    0},   /* SWELL */
 {32767,18842,    0,-10923,7800, 3645,13107,    0}};  /* CRASH */
static const int8_t FORM_ARP[3][6]={{0,3,7,12,0,0},{12,7,3,0,0,0},{0,3,7,12,7,3}};
static const int8_t FORM_CH[5][4]={{0,3,7,0},{0,4,7,0},{0,5,7,0},{0,3,7,10},{0,1,2,3}};
static const int16_t FORM_RAT[7]={-8192,-4096,-1700,0,2396,5415,7403};
enum{FV_OCT,FV_FM,FV_FOLD,FV_CRUSH,FV_NOISE,FV_SHAPE,FV_LEVEL,FV_RATIO,FV_LIFT,FV_SEM,FV_ESUM,FV_N};
/* every gesture alive, summed into the eight (and the lift, the notes' semitones) */
static void form_eval(form_st_t*st,const int16_t*p,int32_t*D,uint32_t at)
{
 uint32_t i;
 for(i=0;i<FORM_G;i++){
  form_g_t*g=&st->g[i];if(!(g->fl&8u))continue;
  uint32_t m=g->mod&3u,raw=(st->clk-g->t0)>>8,sh=g->sh,pg=sh/12u,k=sh%12u;
  int32_t ds=clamp(p[FORM_MF[m][0]],0,FORM_DSMAX),dst=ds&7,amt=clamp(p[FORM_MF[m][2]],0,127),dec=clamp(p[FORM_MF[m][3]],0,127),e,xe=form_mexpo(p,ds)-2;
  uint32_t tau=xe>=0?raw<<xe:raw>>(-xe);                 /* (its time in its own RATE's steps) */
  tau=g->spd==1u?tau<<1:g->spd==2u?tau>>1:tau;
  if(form_oneshot(sh)){if(tau>=2048u){g->fl=0;continue;}e=32767;}
  else if((g->fl&3u)||(g->len&&tau<g->len))e=32767;
  else{if(!g->rel)g->rel=st->clk|1u;uint32_t rt=(st->clk-g->rel)>>8,T=64u+(uint32_t)dec*32u;e=form_fexp(rt*369u/T);if(e<200){g->fl=0;continue;}}
  int32_t gn=amt*g->vel*2;if(g->fl&1u)gn+=gn*(int32_t)at/127;
  int32_t a=(int32_t)((int64_t)e*gn>>15);
  if(pg==FPG_HIT){
   const int16_t*h=FORM_HIT[k];int32_t A=form_henv(k,tau)*gn>>15,oct=h[1],fm=h[4];
   if(k==0)oct=-6144+(9011*form_fexp((tau*3324u)>>8)>>15);
   else if(k==1)oct=1229+(3277*form_fexp((tau*5170u)>>8)>>15);
   else if(k==6)oct=-2458+(4915*form_fexp((tau*1847u)>>8)>>15);
   else if(k==7){int32_t x=form_fexp((tau*1477u)>>8);oct=-4096+(24576*x>>15);fm=5200*x>>15;}
   D[FV_LIFT]+=h[0]*A>>15;D[FV_OCT]+=oct*A>>15;D[FV_LEVEL]+=h[2]*A>>15;D[FV_SHAPE]+=h[3]*A>>15;
   D[FV_FM]+=fm*A>>15;D[FV_RATIO]+=h[5]*A>>15;D[FV_NOISE]+=h[6]*A>>15;D[FV_CRUSH]+=h[7]*A>>15;
   continue;}
  if(pg==FPG_NOTE){
   int32_t sem,pl=0;
   if(k<3u){sem=FORM_ARP[k][(tau>>8)%(k==2u?6u:4u)];pl=1;}
   else if(k==3u){static const int8_t R[7]={0,3,5,7,10,12,15};sem=R[fvm_hash(g->t0+(tau>>8))%7u];pl=1;}
   else if(k<9u){const int8_t*c=FORM_CH[k-4u];sem=c[(tau>>6)%(k==7u||k==8u?4u:3u)];}
   else if(k==9u)sem=12;else if(k==10u)sem=7;else sem=-(int32_t)(tau>=512u?12:tau*12u/512u);
   D[FV_LIFT]+=a;D[FV_SEM]+=(int32_t)((int64_t)sem*341*a>>15);D[FV_ESUM]+=a;
   if(pl){int32_t q=32767-form_fexp(((tau&255u)*923u)>>8);D[FV_LEVEL]-=(27852*q>>15)*a>>15;}
   continue;}
  if(pg==FPG_FM){if(k<7u){D[FV_RATIO]+=FORM_RAT[k]*e>>15;D[FV_FM]+=5200*a>>15;}continue;}
  if(sh==S_REST){D[FV_OCT]-=(int32_t)((int64_t)8*4096*a>>15);continue;}
  int32_t v=form_shape(sh,tau,g->t0);if(g->fl&4u)v=-v;
  int32_t va=v*a>>15;
  switch(dst){
   case FD_PITCH:D[FV_OCT]+=va>>2;if(va>0)D[FV_LIFT]+=va;break;
   case FD_FM:D[FV_FM]+=va*15600>>15;break;
   case FD_NOISE:D[FV_NOISE]+=va+(va>>1);break;
   case FD_RATIO:D[FV_RATIO]+=va>>3;break;
   case FD_FOLD:D[FV_FOLD]+=va;break;
   case FD_CRUSH:D[FV_CRUSH]+=va;break;
   case FD_SHAPE:D[FV_SHAPE]+=va;break;
   default:D[FV_LEVEL]+=va;break;
  }
 }
}
static __attribute__((noinline)) void form_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 uint32_t tr=(uint32_t)(t-trk)%NTRK,i,k,sh=VH_SH(),seed=(uint32_t)clamp(t->p[P_E1],0,127)*977u+1u;
 const int16_t*p=t->p;form_st_t*st=&form_st[tr];
 if(!n)return;
 /* the clock: the transport's (not FREE), else the voice's own; the euclids' step at RATE; the gestures' own time */
 uint32_t q=(uint32_t)v->s[2],bps=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300),end;
 if(func_bar(t))end=func_clock_end;
 else end=(uint32_t)(((uint64_t)(q+(n<<sh))<<16)/bps);
 st->clk+=(uint32_t)((((n<<sh))<<18)/(bps?bps:1u));
 st->last=(int32_t)(end>>(16-clamp(p[P_E0],0,3)));
 for(k=0;k<4u;k++){                                     /* each modulator on its own RATE's steps */
  int32_t ds=clamp(p[FORM_MF[k][0]],0,FORM_DSMAX),dec=clamp(p[FORM_MF[k][3]],0,127);uint32_t s1=end>>(16-form_mexpo(p,ds));
  if((int32_t)s1==st->lastm[k])continue;
  st->lastm[k]=(int32_t)s1;form_lit[tr]=(uint8_t)(form_lit[tr]&~(1u<<k));
  if(form_hit(p,k,s1,seed)){form_spawn(st,(uint32_t)((ds&511)>>3),k,(s1&3u)?90u:127u,0u,64u+(uint32_t)dec*32u,0u);form_lit[tr]|=(uint8_t)(1u<<k);}
 }
 int32_t D[FV_N]={0};
 form_eval(st,p,D,t->at);
 {static const int16_t SC[8]={8192,6760,32767,32767,32767,32767,32767,4096};   /* (the panel: the eight, -1 .. 1) */
  static const uint8_t IX[8]={FV_OCT,FV_FM,FV_FOLD,FV_CRUSH,FV_NOISE,FV_SHAPE,FV_LEVEL,FV_RATIO};
  for(k=0;k<8u;k++){int32_t x=D[IX[k]]*1024/SC[k];if(k==0u)x+=D[FV_LIFT]>>5;st->dv[k]=(int16_t)clamp(x,-1024,1024);}}
 /* the frequency: FREQ's (a tempo's cycle, locked to the transport while nothing moves it; or the note's shifted), lifted
  * toward the root (+ the notes' semitones) and moved by octaves */
 uint32_t fq=(uint32_t)clamp(p[P_ROOT],0,FORM_NF-1),base,locked=0;
 int32_t lift=clamp(D[FV_LIFT],0,32767),oct=clamp(D[FV_OCT],-9*4096,6*4096),sem=D[FV_ESUM]>0?D[FV_SEM]*32767/(D[FV_ESUM]>32767?D[FV_ESUM]:32767):0;
 if(fq<10u){uint32_t sb=(bps>>sh)?(bps>>sh):1u,i1=0xFFFFFFFFu/sb;base=fq>=4u?i1<<(fq-4u):i1>>(4u-fq);locked=!lift&&!oct&&func_bar(t);}
 else base=pitch_inc((uint32_t)clamp(m->pitch16+FORM_FOFS[fq-10u]*16,0,2047));
 uint32_t tgt=pitch_inc((uint32_t)clamp(m->pitch16+(sem*192>>12),0,2047));
 int32_t lg=oct+(int32_t)((int64_t)(form_log2(tgt)-form_log2(base))*lift>>15);
 uint32_t inc=lg?form_mexp(base,lg):base,rinc=form_mexp(inc,4096+clamp(D[FV_RATIO],-3*4096,3*4096));
 uint32_t ph=locked?func_clock_begin<<(12u+fq):v->ph[0],mph=v->ph[1];
 /* the timbre now: the drone's knobs and the modulators' */
 int32_t shp=clamp(p[P_E2]*258+D[FV_SHAPE],0,32767),fm=clamp(p[P_E3]*258+D[FV_FM],0,32767),fo=clamp(p[P_E4]*258+D[FV_FOLD],0,32767);
 int32_t cr=clamp(p[P_E5]*258+D[FV_CRUSH],0,32767),nzv=clamp(D[FV_NOISE],0,52428),lvl=clamp(32768+D[FV_LEVEL],0,52428);
 uint32_t cs=(uint32_t)(cr*12>>15),hn=1u+(uint32_t)((cr>>11)*(cr>>11)>>4),pal=(uint32_t)clamp(p[P_FRA],0,1);
 uint32_t sr=(uint32_t)FS>>sh,fc=(inc>>16)*3u*(sr>>4)>>12;if(fc<4200u)fc=4200u;if(fc>20000u)fc=20000u;   /* (the low-pass: 4.2 kHz, or 3 x the pitch) */
 int32_t lpk=(int32_t)(fc*205887u/(sr?sr:1u));if(lpk>32767)lpk=32767;
 uint32_t pd=32768u-(uint32_t)(shp*15400>>15),ra=(32768u<<16)/pd,rb=(32768u<<16)/(65536u-pd);
 int32_t hold=st->hold,hc=st->hc,dx=st->dcx,dy=st->dcy,nzl=st->nzl,lpz=st->lpz;uint32_t nz=st->nz;
 int sc=form_scope_tr==tr&&v==&t->v[0];
 for(i=0;i<n;i++){
  ph+=inc;mph+=rinc;
  uint32_t pp=ph+(uint32_t)((int64_t)sine_i(mph)*fm<<3);      /* FM: up to ~2 turns, at RATIO (an octave up) */
  if(nzv){nz=nz*1664525u+1013904223u;nzl+=(((int32_t)nz>>16)-nzl)*15>>5;pp+=(uint32_t)(nzl*(nzv>>2))<<4;}   /* NOISE: the phase dirtied (~2.6 kHz) */
  int32_t y;
  if(pal){uint32_t u,qq=pp>>16;u=qq<pd?(qq*ra)>>16:32768u+(((qq-pd)*rb)>>16);y=-sine_i((u<<16)+0x40000000u);}   /* PD: sine .. a saw, its phase bent */
  else{int32_t sn=sine_i(pp),u=(int32_t)(pp>>16)-32768,tri=32767-2*(u<0?-u:u),saw=u,pul=u<0?-32767:32767;
   if(shp<10923)y=sn+(int32_t)(((int64_t)(tri-sn)*(shp*3))>>15);
   else if(shp<21846)y=tri+(int32_t)(((int64_t)(saw-tri)*((shp-10923)*3))>>15);
   else y=saw+(int32_t)(((int64_t)(pul-saw)*((shp-21846)*3))>>15);}
  if(fo>256)y+=(int32_t)((int64_t)(form_fold(clamp(y*(256+(fo>>7)*5)>>8,-262144,262143))-y)*fo>>15);
  if(cs){if(++hc>=(int32_t)hn){hc=0;hold=(y>>cs)<<cs;}y=hold;}
  y=(int32_t)((int64_t)y*lvl>>15);
  lpz+=(int32_t)((int64_t)(y-lpz)*lpk>>15);
  int32_t o=lpz-dx+(dy*253>>8);dx=lpz;dy=o;                     /* (under ~40 Hz out: REST's bottom silent) */
  if(sc&&form_sc_i<64u)form_scope[form_sc_i++]=(int16_t)clamp(o,-32767,32767);
  out[i]+=voice_amp(soft_knee(o,20000),m,i);
 }
 if(sc&&form_sc_i>=64u)form_sc_i=0;
 st->hold=hold;st->hc=hc;st->dcx=dx;st->dcy=dy;st->nz=nz;st->nzl=nzl;st->lpz=lpz;
 v->s[2]=(int32_t)(q+(n<<sh));v->ph[0]=ph;v->ph[1]=mph;
}
/* (seq.c kb_map) PLAY / FREE: every key sounds (a gesture: no scale's QUANT silences one) */
static int32_t form_kmap(const track_t*t,uint32_t k){return form_keys(t)?(int32_t)(53u+k):-1;}
static const engine_t ENG_FORM={.name="FORM",.page_title={"FORM","DRONE"},
 .edit={{"RATE",F_ENUM,0,3,2,FORM_RATE},{"SEED",F_INT,0,127,0,0},{"SHAPE",F_PCT,0,127,30,0},{"FM",F_PCT,0,127,5,0},
        {"FOLD",F_PCT,0,127,0,0},{"CRUSH",F_PCT,0,127,0,0},{"DS",F_INT,0,FORM_DSMAX,0,0},{"EUCLID",F_ENUM,0,FORM_NEU-1,0,FORM_EU}},
 .presets=FORM_PRESETS,.npresets=NELEM(FORM_PRESETS),.knob={P_E2,P_E3,P_E4,P_E5},.poly=1,.half=1,.note_on=form_note_on,.render=form_render,.keys=form_kmap};
