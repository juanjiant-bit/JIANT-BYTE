/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * FORM 3: one morphing sequence of funcbeat. Two formulas of FORM's bank, A and B, each a whole sound in time (a pluck,
 * a pad, a bass, a beat; a bytebeat last): MORPH from one to the other, DEFORM A and DEFORM B their A inside; under them
 * a PROGRESSION of the song's scale (SCL) in roman numerals, its CHORD RATE, the NOTES the formulas get from each chord.
 * The formulas' variables (fvm.c):
 *   w  the note's phase (turns)          w3 w5  the chord's third and fifth     wd  the note a little sharp (detune)
 *   T  the step's age (0 .. 1 a 16th)    s  the step (16ths)    c  its place in the bar (0 .. 15)
 *   P  the beats                         t  the beats x 4096 (bytebeat's time, ~8 kHz at 120 BPM)
 *   D  the chord's age (0 .. 1)          A  DEFORM              n  the note    b  the chord's degree (0 ..)
 * One formula a sample while MORPH is at an end, both between; at half the rate (voice.c FORM_HALF: .half 2). */
static int32_t scale_snap(const track_t*,int32_t);      /* seq.c: the song's scale */
static uint32_t scale_mask(const track_t *t);           /* seq.c */
#include "eng_form1.c"                                  /* FORM 1's 36 states: FORM A / B's first */
/* ---- the bank: the synths first, then the drums, the bytebeats last (each in 32 instructions: FORMULA B's slot) ---- */
static const char *const FORM_BANK[]={"sin(w)"};    /* (FORM 3's formulas: out for now, Juan: "too simple"; the deeper ones to come) */
/* FORM 1's states' own DRIVE WOBBLE TONE CHORD VOICING (its presets', FORM 2) */
static const uint8_t F1_XTRA[36][5]={{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{20,2,64,4,6},{0,2,64,7,4},{0,2,64,1,0},{0,2,64,4,6},{30,4,90,1,0},{0,2,64,4,6},{10,2,64,4,0},{20,4,64,1,0},{0,2,64,7,7},{0,2,64,4,6},{10,2,64,1,0},{60,5,70,1,0},{70,6,60,1,0},{30,4,100,1,0}};
static const char *const FORM_NAMES[]={"01 SCORE","02 TIME RULE","03 COUNTER","04 FIVE PART","05 TUNED MEM","06 SWITCH","07 BASS ECHO","08 DETUNE","09 RATIOS","10 NESTED FM","11 BIT MORPH","12 CRUSH CHD","13 FILTER CHD","14 SWEEPS","15 BIT GROOVE","16 MASK SONG","17 NONLIN CHD","18 GENERATIVE","19 SHAPED ARP","20 BUILD UP","21 BIT PAIRS","22 NESTED EQ","23 STRUM 332","24 HOUSE M9","25 CHIRP","26 DRIFT","27 BITPACK","28 BUILD","29 RISER","30 FILLS","31 SUITE","32 DEGREES","33 NIBBLE","34 GROWL","35 WOBBLE","36 ACID","SINE",0};
#define FORM_N (sizeof FORM_BANK/sizeof FORM_BANK[0])
_Static_assert(F1_N+FORM_N==sizeof FORM_NAMES/sizeof FORM_NAMES[0]-1u,"FORM: a name a formula");
#define FORM_NT F1_N                                    /* FORM A / B: FORM 1's 36 states (the bank's formulas out of the
                                                         * list: Juan, "too simple"; the path stays for deeper ones) */
/* ---- the harmony: progressions as degrees of the scale (a 7-note one: these; another: its own degrees) ---- */
static const int8_t FORM_PROGS[][8]={
 {0,5,2,6,-1},{0,5,2,6,-1},{0,3,-1},{0,6,5,4,-1},{1,4,0,0,-1},{0,4,5,3,-1},{0,3,4,0,-1},{0,5,3,4,-1},{0,6,-1},{0,1,-1},
 {0,3,0,4,-1},{0,3,6,2,5,1,4,-1},{0,2,5,1,-1},{0,-1}};
#define FORM_NPROG (sizeof FORM_PROGS/sizeof FORM_PROGS[0])
static const char *const FORM_PROGN[]={"STATE","AXIS","VAMP IV","ANDALUZ","II-V-I","POP","CADENCE","50S","VAMP VII","PHRYGIAN",
 "PLAGAL","CIRCLE","MEDIANT","PEDAL",0};
static const char *const FORM_CRATE[]={"1/4","1/2","1","2","4",0};      /* a chord's length in bars (P_ARATE) */
static const char *const FORM_NOTES[]={"ROOT","UP","DOWN","PEND","RANDOM","WALK","BASS",0};   /* what w is (P_AORDER) */
static const char *const FORM_OCT[]={"-1","0","+1","+2",0};             /* (OCT A, OCT B: P_E6, P_E7) */
static const char *const FORM_RATE[]={"AUTO","1/16","1/8","1/4","1/2","1 BAR","2 BAR","4 BAR",0};   /* (BYTE 0.16) a step's
                                                         * length: AUTO the state's own, then absolute, to FORM 1's slowest */
static const char *const FORM_RUN[]={"TRIG","PLAY","FREE",0};
typedef struct{int8_t d[12];int32_t n;} form_scale_t;
static void form_scale(const track_t*t,form_scale_t*sc)  /* the part's scale as degrees (chromatic or few: natural minor) */
{
 uint32_t m=scale_mask(t)&0xFFFu;
 if(m==0xFFFu||__builtin_popcount(m)<5)m=0x5ADu;
 sc->n=0;for(uint32_t k=0;k<12u;k++)if((m>>k)&1u)sc->d[sc->n++]=(int8_t)k;
}
static int32_t form_deg(const form_scale_t*sc,int32_t d){int32_t o=(d>=0?d:d-sc->n+1)/sc->n;return sc->d[d-o*sc->n]+12*o;}   /* degree -> semitones */
static uint32_t form_plen(uint32_t p){uint32_t k=0;while(k<8u&&FORM_PROGS[p%FORM_NPROG][k]>=0)k++;return k?k:1u;}
/* a degree's numeral in this scale: I .. VII, lower case minor, "o" diminished (b: 8 bytes) */
static void form_roman(const form_scale_t*sc,int32_t d,char*b)
{
 static const char *const R[7]={"I","II","III","IV","V","VI","VII"};
 int32_t r=form_deg(sc,d),th=form_deg(sc,d+2)-r,fi=form_deg(sc,d+4)-r;
 uint32_t i=(uint32_t)(((d%sc->n)+sc->n)%sc->n)%7u,k;
 str_cpy(b,R[i],8);
 if(th<4)for(k=0;b[k];k++)b[k]=(char)(b[k]-'A'+'a');
 if(fi<7)str_cpy(b+str_len(b),"o",8-str_len(b));
}
/* ---- the engine's pages: FORM (FORMULA A, FORMULA B, MORPH, DEFORM A), DEFORM (DEFORM B, RATE, SWING, RUN), HARMONY ---- */
static int form_owned_param(uint32_t id){return id==P_MORPH||id==P_CSTR||id==P_DXIMG||id==P_AGATE||id==P_ARATE||id==P_AORDER||id==P_LRTRG||id==P_FRA||id==P_FRB||id==P_KEY||id==P_DETUNE||id==P_CHRD||id==P_CVOI||id==P_CAOS||id==P_ASWING||id==P_APROB;}
/* MOTION: an equation a side moving its DEF (DEPTH how far, SPEED its time x1/2 .. x4), drawn as written */
static const char *const FORM_MOTION[]={"OFF","sin(P/4)","tri(P/2)","P/4%1","1-T","(t>>11)&7","(t>>9^t>>12)&15","rnd(s)","sin(P)*sin(P/3)","abs(sin(P*3))","sqr(P/2)","s*5%7",0};
#define FORM_NMOT 12u
static const char *const FORM_STYLE[]={"FUNC","WOBBLE","REESE","GROWL","RING","SCREECH","ACID",0};   /* (funcbeat alone) */
static const uint8_t FORM_STYLE_DOM[7]={0,5,6,7,8,9,10};
static const char *const FORM_KIT[]={"808","909","FLOAT","BIT",0},*const FORM_GROOVE[]={"STATE","FOUR","BREAK","SPARSE",0};   /* (BYTE 0.16) DRUMS */
static const char *const FORM_SPEED[]={"1/2","1","2","4",0};
/* a MOTION's value (Q15, -1 .. 1) at the beats pq (Q16), the step s, its age T (Q16) */
static int32_t form_motion(uint32_t k,uint32_t pq,uint32_t s,uint32_t T)
{
 int32_t v;uint32_t t=pq>>4;
 switch(k){
 case 1:return sine_i(pq<<14);                                   /* sin(P/4): a turn in 4 beats */
 case 2:v=(int32_t)((pq<<15)>>16);return v<32768?v*2-32767:98303-v*2;   /* tri(P/2) */
 case 3:return (int32_t)((pq<<14)>>17)-32767;                   /* P/4%1: a ramp a bar */
 case 4:return 32767-(int32_t)(T>>1)*2;                          /* 1-T: down over each step */
 case 5:return (int32_t)((t>>11)&7u)*9362-32767;                 /* (t>>11)&7: eight steps a 4 beats */
 case 6:return (int32_t)(((t>>9)^(t>>12))&15u)*4369-32767;       /* bits */
 case 7:return (int32_t)(fvm_hash(s*2654435761u)>>16)-32768;    /* rnd(s): held a step */
 case 8:return mulq15(sine_i(pq<<16),sine_i((uint32_t)(((uint64_t)pq<<16)/3u)));   /* two against three */
 case 9:{int32_t x=sine_i((uint32_t)(pq*3u)<<16);return (x<0?-x:x)*2-32767;}      /* abs(sin(P*3)) */
 case 10:return (pq>>16)&1u?32767:-32767;                        /* sqr(P/2) */
 case 11:return (int32_t)((s*5u)%7u)*10922-32767;                /* s*5%7 */
 }
 return 0;
}
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t prog={"PROG",F_ENUM,0,FORM_NPROG-1,0,FORM_PROGN},crate={"LENGTH",F_ENUM,0,4,2,FORM_CRATE},
  notes={"NOTES",F_ENUM,0,6,0,FORM_NOTES},run={"RUN",F_ENUM,0,2,1,FORM_RUN},ra={"RATE A",F_ENUM,0,7,0,FORM_RATE},rb={"RATE B",F_ENUM,0,7,0,FORM_RATE},
  hits={"HITS",F_INT,1,16,16,0},rot={"ROTATE",F_INT,0,15,0,0},voi={"VOICING",F_ENUM,0,7,0,F1_VOICE};
 static const param_desc_t ma={"MOD A",F_ENUM,0,FORM_NMOT-1,1,FORM_MOTION},mb={"MOD B",F_ENUM,0,FORM_NMOT-1,0,FORM_MOTION},dep={"DEPTH",F_PCT,0,127,24,0},
  spd={"SPEED",F_ENUM,0,3,1,FORM_SPEED},swg={"LEVEL",F_PCT,0,127,0,0},fus={"FUSION",F_PCT,0,127,0,0},kit={"KIT",F_ENUM,0,3,0,FORM_KIT},grv={"GROOVE",F_ENUM,0,3,0,FORM_GROOVE};
 return id==P_AGATE?&prog:id==P_ARATE?&crate:id==P_LRTRG?&run:id==P_FRA?&ra:id==P_FRB?&rb:id==P_KEY?&hits:id==P_DETUNE?&swg:id==P_MORPH?&fus:id==P_CSTR?&kit:id==P_DXIMG?&grv:id==P_CVOI?&voi:
        id==P_CAOS?&ma:id==P_ASWING?&mb:id==P_APROB?&dep:id==P_AORDER?&spd:0;
}
/* the presets: A B MORPH SWING, DEF A DEF B OCT A OCT B; the PROG, LENGTH, NOTES; RATE A RATE B HITS ROTATE (FP: the
 * rates x1, every step; FQ: its own) */
#define FQ(label,tag,a,b,m,da,db,sw,prog,cr,nt,oa,ob,ra,rb,hi,ro) {label,{(a)+F1_N,(b)+F1_N,m,sw,da,db,oa,ob},{0,0,127,24},0,TAG(tag),FX(0,0,30,30),0,\
  MODS(P_AGATE,(prog)+1,P_ARATE,cr,P_AORDER,nt,P_LRTRG,1,P_FRA,ra,P_FRB,rb,P_KEY,hi,P_DETUNE,ro)}
/* FORM 1's: its state to the next, its sends, its own progression (STATE) */
#define FSTATE(label,a,b,echo,space,ch,vo) {label,{a,b,0,0,64,64,1,1},{0,0,127,24},0,TAG(FX),FX(0,24,echo,space),0,\
  MODS(P_AGATE,0,P_ARATE,2,P_AORDER,1,P_LRTRG,1,P_FRA,0,P_FRB,0,P_KEY,16,P_DETUNE,0,P_MORPH,0,P_CSTR,0,P_DXIMG,0,P_CHRD,ch,P_CVOI,vo,P_CAOS,1,P_ASWING,0,P_APROB,24)}
#define FP(label,tag,a,b,m,da,db,sw,prog,cr,nt,oc) FQ(label,tag,a,b,m,da,db,sw,prog,cr,nt,oc,oc,0,0,16,0)
static const preset_t FORM_PRESETS[]={
 FSTATE("01 SCORE",0,1,10,20,1,0), FSTATE("02 TIME RULE",1,2,0,5,1,0), FSTATE("03 COUNTER",2,3,72,45,1,0),
 FSTATE("04 FIVE PART",3,4,40,25,1,0), FSTATE("05 TUNED MEM",4,5,100,60,1,0), FSTATE("06 SWITCH",5,6,0,8,1,0),
 FSTATE("07 BASS ECHO",6,7,96,20,1,0), FSTATE("08 DETUNE",7,8,112,60,1,0), FSTATE("09 RATIOS",8,9,18,15,1,0),
 FSTATE("10 NESTED FM",9,10,32,40,1,0), FSTATE("11 BIT MORPH",10,11,14,5,1,0), FSTATE("12 CRUSH CHD",11,12,110,25,1,0),
 FSTATE("13 FILTER CHD",12,13,100,55,1,0), FSTATE("14 SWEEPS",13,14,5,5,1,0), FSTATE("15 BIT GROOVE",14,15,0,8,1,0),
 FSTATE("16 MASK SONG",15,16,24,12,1,0), FSTATE("17 NONLIN CHD",16,17,58,48,1,0), FSTATE("18 GENERATIVE",17,18,127,90,1,0),
 FSTATE("19 SHAPED ARP",18,19,50,35,1,0), FSTATE("20 BUILD UP",19,20,30,22,1,0), FSTATE("21 BIT PAIRS",20,21,52,30,1,0),
 FSTATE("22 NESTED EQ",21,0,28,28,1,0), FSTATE("23 STRUM 332",22,23,60,40,4,6), FSTATE("24 HOUSE M9",23,30,40,30,7,4),
 FSTATE("25 CHIRP",24,31,30,40,1,0), FSTATE("26 DRIFT",25,24,70,80,4,6), FSTATE("27 BITPACK",26,35,10,10,1,0),
 FSTATE("28 BUILD",27,22,30,30,4,6), FSTATE("29 RISER",28,27,20,20,4,0), FSTATE("30 FILLS",29,26,10,10,1,0),
 FSTATE("31 SUITE",30,23,45,50,7,7), FSTATE("32 DEGREES",31,22,50,60,4,6), FSTATE("33 NIBBLE",32,28,30,30,1,0),
 FSTATE("34 GROWL",33,34,10,30,1,0), FSTATE("35 WOBBLE",34,33,10,30,1,0), FSTATE("36 ACID",35,26,30,20,1,0)
};
#undef FP
#undef FQ
#undef FSTATE
/* ---- the dice (ui_input.c: SELECT the whole of it; SEQ + SELECT the harmony alone): a pitched A, B by chance a pitched or
 * a drum, MORPH most of the time at an end; the harmony anything ---- */
static const uint8_t FORM_DICE_COMP[8]={P_AGATE,P_ARATE,P_CHRD,P_CVOI,P_KEY,P_CAOS,P_ASWING,P_APROB};
static int16_t form_rnd(uint32_t n){return (int16_t)(n?rng()%n:0u);}
static void form_dice(track_t*t,uint32_t comp_only)
{
 int16_t*p=t->p;
 if(!comp_only){
  p[P_E0]=form_rnd(F1_N);p[P_E1]=form_rnd(3u)?form_rnd(F1_N):p[P_E0];
  p[P_E3]=form_rnd(2u)?0:(int16_t)(1+form_rnd(6u));p[P_DETUNE]=form_rnd(3u)?0:form_rnd(50u);p[P_E6]=(int16_t)(form_rnd(3u));p[P_E7]=(int16_t)(form_rnd(3u));
  p[P_FRA]=form_rnd(3u)?0:(int16_t)form_rnd(8u);p[P_FRB]=form_rnd(3u)?0:(int16_t)form_rnd(8u);p[P_LRTRG]=1;
 }
 p[P_AGATE]=form_rnd(FORM_NPROG);p[P_ARATE]=(int16_t)(1+form_rnd(4u));p[P_AORDER]=form_rnd(7u);
 p[P_KEY]=form_rnd(2u)?16:(int16_t)(3+form_rnd(11u));p[P_CAOS]=form_rnd(FORM_NMOT);p[P_ASWING]=form_rnd(FORM_NMOT);p[P_APROB]=form_rnd(80u);p[P_AORDER]=form_rnd(4u);p[P_CHRD]=(int16_t)(1+form_rnd(11u));p[P_CVOI]=form_rnd(8u);
 p[P_E2]=form_rnd(3u)?0:form_rnd(128u);p[P_E4]=(int16_t)(20+form_rnd(90u));p[P_E5]=(int16_t)(20+form_rnd(90u));
}
static uint32_t form_cost(const track_t*t){uint32_t two=t->p[P_E2]>0&&t->p[P_E2]<127&&t->p[P_E0]!=t->p[P_E1];return (t->p[P_E0]<(int)F1_N||t->p[P_E1]<(int)F1_N?7u:6u)+(two?5u:0u);}
static f1_vs_t f1_st[NTRK] __attribute__((section(".pool")));   /* (FORM 1's state a part: its sides') */
static void form_note_on(track_t*t,voice_t*v){
 f1_vs_t*q=&f1_st[(uint32_t)(t-trk)%NTRK];
 memset(v->s,0,sizeof v->s);memset(v->ph,0,sizeof v->ph);memset(q,0,sizeof *q);q->note=v->note;
}
/* FORM 3's controls as FORM 1's (its view f1_p): state a (to b by morph), DEF the COLOR, SWING, OCT the melody's, RATE
 * its DIV, LENGTH its CYCLE, PROG its family (STATE: its own), the state's own DRIVE WOBBLE TONE CHORD VOICING */
static void f1_view(const track_t*t,int16_t*q,uint32_t a,uint32_t b,uint32_t morph,uint32_t def,uint32_t oct,uint32_t rate)
{
 static const uint8_t PR[]={0,1,3,4,2,7,2,6,3,0,0,5,8,0},CY[5]={8,16,32,64,64},CX[8]={1,1,1,2,4,8,16,32};
 const uint8_t*x=F1_XTRA[a%36u];
 uint32_t pr=(uint32_t)clamp(t->p[P_AGATE],0,FORM_NPROG-1);
 memcpy(q,t->p,P_COUNT*sizeof *q);
 q[P_E0]=(int16_t)a;q[P_E1]=(int16_t)b;q[P_E2]=(int16_t)morph;q[P_E3]=(int16_t)def;q[P_E4]=0;q[P_E5]=100;q[P_E6]=0;q[P_E7]=64;
 q[P_CAOS]=FORM_STYLE_DOM[clamp(t->p[P_E3],0,6)];q[P_ASWING]=x[0];q[P_ARATE]=x[1];q[P_APROB]=x[2];q[P_KEY]=(int16_t)clamp(t->p[P_DETUNE],0,127);q[P_DETUNE]=24;f1_dx[MC_CPU()&1u]=(uint8_t)clamp(t->p[P_MORPH],0,127);f1_kit[MC_CPU()&1u]=(uint8_t)clamp(t->p[P_CSTR],0,3);f1_grv[MC_CPU()&1u]=(uint8_t)clamp(t->p[P_DXIMG],0,3);   /* (BYTE 0.16)
                                                         * DRUMS: their level, and the formula in them */   /* (CHORD, VOICING: the part's own, P_CHRD P_CVOI) */
 q[P_AORDER]=pr==FORM_NPROG-1u?5:0;q[P_AGATE]=PR[pr<sizeof PR?pr:0];q[P_AOCT]=(int16_t)oct;q[P_SDIV]=(int16_t)(rate&7u?9u+(rate&7u):2u);
 q[P_SLEN]=(int16_t)clamp(CY[clamp(t->p[P_ARATE],0,4)]*CX[rate&7u],8,64);q[P_SSWING]=0;   /* (slow: the cycle as long, to 64 beats) */
}
/* a formula's program: A in the part's own slot (lofi_slot: MATH's), B in its second (WAVE's B); compiled when it changes */
static const uint8_t FORM_CRB[5]={1,2,4,8,16};          /* CHORD RATE in beats */
static const uint16_t FORM_SEMI[13]={4096,4340,4598,4871,5161,5468,5793,6137,6502,6889,7298,7732,8192};   /* 2^(k/12), Q12 */
/* the step's note and chord: the progression's degree at beat bq (Q16), the NOTES on step s */
typedef struct{int32_t note,third,fifth,deg;uint32_t chord;} form_note_t;
static void form_harmony(const track_t*t,uint32_t bq,uint32_t s,form_note_t*o)
{
 form_scale_t sc;form_scale(t,&sc);
 uint32_t pr=(uint32_t)clamp(t->p[P_AGATE],0,FORM_NPROG-1),len=form_plen(pr),cb=FORM_CRB[clamp(t->p[P_ARATE],0,4)];
 uint32_t ci=((bq>>16)/cb)%len;
 int32_t d=FORM_PROGS[pr][ci],k,off=0,base=36+t->p[P_ROOT];
 static const int8_t TONE[4]={0,2,4,7};
 switch(clamp(t->p[P_AORDER],0,6)){
 case 1:off=TONE[s&3u];break;                                     /* UP */
 case 2:off=TONE[3u-(s&3u)];break;                                /* DOWN */
 case 3:{static const int8_t PD[6]={0,2,4,7,4,2};off=PD[s%6u];break;}   /* PEND */
 case 4:off=TONE[(fvm_hash(s*2654435761u+ci)>>13)&3u];break;      /* RANDOM: the chord's, by chance */
 case 5:{uint32_t h=s&15u;off=0;for(k=0;k<(int32_t)h;k++)off+=(fvm_hash((s&~15u)+(uint32_t)k)>>30)==0?-1:(fvm_hash((s&~15u)+(uint32_t)k)>>31)?1:0;off=clamp(off,-3,7);break;}   /* WALK */
 case 6:{static const int8_t BS[8]={0,0,7,0,0,4,7,0};off=BS[s&7u];break;}   /* BASS: the root, its fifth, its octave */
 default:break;
 }
 o->deg=d;o->chord=ci;
 o->note=clamp(scale_snap(t,base+form_deg(&sc,d+off)),12,108);
 o->third=form_deg(&sc,d+off+2)-form_deg(&sc,d+off);o->fifth=form_deg(&sc,d+off+4)-form_deg(&sc,d+off);
 o->third=clamp(o->third,0,12);o->fifth=clamp(o->fifth,0,12);
}
/* (FORM 3) the panel's scopes: the last block of A and of B (half-rate samples, Q15) of the part form_scope_tr */
static int16_t form_scope[2][64];
static volatile uint8_t form_scope_tr=0xFF;
static volatile uint8_t form_mot[2];                   /* (the panel: each side's DEF moved, now) */
static uint8_t form_sc_i=64,form_sc_c;                 /* (the scopes' place, its countdown) */
/* one side's time: its RATE r over the song's beats pb[] (Q16), SWING's pair split; into its t, T, P and its step */
static void form_side_clock(const uint32_t*pb,uint32_t n,uint32_t r,uint32_t split,uint32_t r1,uint32_t r2,uint32_t*TT,uint32_t*TS,uint32_t*PP,uint32_t*s0)
{
 for(uint32_t i=0;i<n;i++){
  uint32_t p=(pb[i]<<r)>>1,q=p<<2,pair=q&0x1FFFFu,u=pair<split?(uint32_t)(((uint64_t)pair*r1)>>15):65536u+(uint32_t)(((uint64_t)(pair-split)*r2)>>15);
  TS[i]=u&0xFFFFu;PP[i]=p&0x7FFFFFFFu;TT[i]=p>>4;
  if(!i)*s0=((q>>17)<<1)+(u>>16);
 }
}
static __attribute__((noinline)) void form_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 uint32_t hs=1u<<VH_SH(),tr=(uint32_t)(t-trk)%NTRK,i,a=(uint32_t)clamp(t->p[P_E0],0,FORM_NT-1),b=(uint32_t)clamp(t->p[P_E1],0,FORM_NT-1);
 uint32_t morph=(uint32_t)clamp(t->p[P_E2],0,127),bl=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300),pos;
 int32_t YA[CTL],YB[CTL];
 if(n>CTL)n=CTL;if(!n)return;
 /* the song's step (16ths, SWING's pair split: the gate's, the panel's) */
 pos=func_bar(t)?func_clock_begin:(uint32_t)(((uint64_t)v->ph[0]<<16)/bl);
 uint32_t q0=pos<<2,pair=q0&0x1FFFFu,split=65536u,sb=((q0>>17)<<1)+(pair<split?0u:1u);
 /* the states: one (MORPH at an end, or A = B) or both, by FORM 1's own MORPH, the one that composes; DEF its COLOR, OCT
  * its melody's octave, RATE its DIV (B's at MORPH 127) */
 {
  uint32_t one=morph==0u||a==b?1u:morph==127u?2u:0u,f=one==2u?b:a,sd=one==2u;
  int16_t q[P_COUNT];const int16_t*keep=f1_p[MC_CPU()&1u];
  static const uint8_t MSH[8]={0,0,0,0,1,2,3,4};          /* (a slow RATE: MOTION as slow) */
  uint32_t pm=(pos<<(uint32_t)clamp(t->p[P_AORDER],0,3))>>(1u+MSH[clamp(t->p[sd?P_FRB:P_FRA],0,7)]),T=(pair<split?pair*65536u/split:(pair-split)*65536u/(131072u-split))&0xFFFFu,dep=(uint32_t)clamp(t->p[P_APROB],0,127);
  int32_t da=clamp(t->p[P_E4]+(int32_t)((form_motion((uint32_t)clamp(t->p[P_CAOS],0,FORM_NMOT-1),pm,sb,T)*(int32_t)dep)>>15),0,127);
  int32_t db=clamp(t->p[P_E5]+(int32_t)((form_motion((uint32_t)clamp(t->p[P_ASWING],0,FORM_NMOT-1),pm,sb,T)*(int32_t)dep)>>15),0,127);
  form_mot[0]=(uint8_t)da;form_mot[1]=(uint8_t)db;          /* (the panel's) */
  f1_view(t,q,f,one?f:b,one?0u:morph,(uint32_t)(one==2u?db:one?da:da+((db-da)*(int32_t)morph)/127),(uint32_t)clamp(t->p[sd?P_E7:P_E6],0,3),(uint32_t)clamp(t->p[sd?P_FRB:P_FRA],0,7));
  f1_p[MC_CPU()&1u]=q;f1_st[tr].note=v->note;f1_render(t,&f1_st[tr],YA,n,m);f1_p[MC_CPU()&1u]=keep;
  for(i=0;i<n;i++)YB[i]=YA[i];
 }
 uint32_t inc=pitch_inc((uint32_t)clamp(v->note,24,96)*16u)*hs,p0=v->ph[1];   /* (the scopes' trigger: the note's turn) */
 v->ph[1]=p0+inc*n;
 /* the EUCLID gate: HITS of the bar's 16 steps (16 all), ROTATE; an off step fades out, the next hit fades in */
 uint32_t hits=(uint32_t)clamp(t->p[P_KEY],1,16),kk=sb&15u;
 int32_t gt=hits>=16u||((kk+1u)*hits>>4)!=(kk*hits>>4)?32767:0,g=v->s[5];
 if(tr==form_scope_tr){                                  /* the scopes: triggered on the note's turn, ~2.5 turns in 64 points */
  uint32_t dec=clamp((int32_t)((0xFFFFFFFFu/(inc|1u))/26u),1,64),ph=p0;
  for(i=0;i<n;i++,ph+=inc){
   if(form_sc_i<64u){if(!form_sc_c--){form_sc_c=(uint8_t)(dec-1u);form_scope[0][form_sc_i]=(int16_t)YA[i];form_scope[1][form_sc_i]=(int16_t)YB[i];form_sc_i++;}}
   else if(ph+inc<ph){form_sc_i=0;form_sc_c=0;}
  }
 }
 for(i=0;i<n;i++){
  int32_t y=YA[i];
  g+=(gt-g)>>(6u-VH_SH());y=mulq15(y,g);
  y=soft_knee(y,26000);
  v->s[4]+=(y-v->s[4])/(int32_t)(512u/hs);y-=v->s[4];      /* (the DC away) */
  out[i]+=voice_amp(clamp(y,-32767,32767),m,i);
 }
 v->s[5]=g;v->ph[0]+=n*hs;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"FORM","DEFORM"},
 .edit={{"FORM A",F_ENUM,0,FORM_NT-1,0,FORM_NAMES},{"FORM B",F_ENUM,0,FORM_NT-1,1,FORM_NAMES},{"MORPH",F_PCT,0,127,0,0},{"STYLE",F_ENUM,0,6,0,FORM_STYLE},
        {"DEF A",F_PCT,0,127,64,0},{"DEF B",F_PCT,0,127,64,0},{"OCT A",F_ENUM,0,3,1,FORM_OCT},{"OCT B",F_ENUM,0,3,1,FORM_OCT}},
 .presets=FORM_PRESETS,.npresets=NELEM(FORM_PRESETS),.knob={P_E0,P_E1,P_E2,P_E4},.poly=1,.half=2,.note_on=form_note_on,.render=form_render};
