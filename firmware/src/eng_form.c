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
static const char *const FORM_BANK[]={
 "sat(saw(w*2)*(1-T)*(1-T)*(1+A*4))*.6",                                            /* PLUCK: a saw struck, A its drive */
 "sin(w*4+sin(w*14)*(1-T)*(.5+A*6))*(1-T)*.7",                                      /* FM BELL: 3.5 : 1, A the index */
 "sin(w+sin(w*2)*(1-T)*(1+A*4))*.8",                                                /* FM BASS */
 "(sin(w*2)+sin(w3*2)+sin(w5*2)+sin(w)*A)*.25",                                     /* PAD: the chord, A its root below */
 "lp(saw(w*2)+saw(w3*2)+saw(w5*2),.04+A*.4)*.45",                                   /* SAW CHORD: A the cutoff */
 "(saw(w*2)+saw(wd*2)+saw(w3*2)*A)*.3",                                             /* SUPERSAW: detuned, A the third */
 "(sqr(w*2)-sqr(w*2+.5+tri(P/4)*.4*A))*.35",                                        /* PWM: its width moving, A how far */
 "sat(lp(saw(w),.02+(1-T)*(1-T)*(.05+A*.6))*3)*.5",                                 /* ACID: a filter struck, A its sweep */
 "lp(saw(w)+saw(wd),.04+A*.3)*.6",                                                  /* REESE: two saws beating, A the cutoff */
 "lp(saw(w)+sqr(w),.01+abs(sin(P*(1+floor(A*4))))*.3)*.6",                          /* WOBBLE: A its rate, 1 .. 5 a beat */
 "sin(w*(1+floor(A*5))+sin(w)*3*abs(sin(P*2)))*.6",                                 /* GROWL: FM breathing, A the partial */
 "sin(w*2)*sin(w*(4+floor((1+tri(P/4))*A*6)))*.8",                                  /* FORMANT: a vowel moving, A how far */
 "sin(sin(w*2)*(1+A*6+(1-T)*3))*.6",                                                /* FOLD: A the folds */
 "tri(w*(2+A*7+(1-T)*4))*.6",                                                       /* SYNC: the hard sync struck, A its ratio */
 "(sin(w*4)+sin(w*8)*.3*A+sin(w3*4)*.5+sin(w5*4)*.5)*(1-T)*.4",                     /* KEYS: the chord struck, A its overtone */
 "(sin(w)+tri(w*2)*A*.5)*.9",                                                       /* SUB: A its edge */
 "(sin(w*2)+sin(w3*2)*(T>.06+A*.1)+sin(w5*2)*(T>.12+A*.2))*(1-T)*.4",               /* STRUM: the chord strummed, A slower (ref 1) */
 "lp(saw(w*2)+saw(w3*2)+saw(w5*2),.06+A*.4)*(1-T)*(1-T)*(c%4==2)*.6",               /* STAB: the offbeat chord (ref 2) */
 "lp(saw(w*2)+saw(w3*2)+saw(w5*2),.08+A*.2)*.4*(1-(1-P%1)*(1-P%1))",                /* PUMP PAD: ducked each beat (ref 2) */
 "sin(w*(2+s%4))*(1-T)*(.6+A*.4)",                                                  /* HARMONICS: the harmonic series stepped (ref 3) */
 "sin(w*2*(1+(1-T)*(1-T)*A*4))*(1-T)*.7",                                           /* CHIRP: the pitch falling (ref 14) */
 "(rnd(t)*D*A+sin(w*2*(1+D)))*D*.5",                                                /* RISER: up over the chord (ref 7) */
 "floor((sin(w*2)+sin(w3*2)+sin(w5*2))*(2+A*6))/(2+A*6)*.3",                        /* CRUSH CHD: the chord in few levels */
 "sin(w*2+sin(w*3+sin(w*5)*A*2)*2)*(1-T)*.6",                                       /* NESTED FM: FM in FM */
 "sat(lp(saw(w)+saw(wd)+sin(w),.01+abs(sin(P*3))*(.15+A*.4))*2)*.5",                /* DUB: the triplet wobble (ref 4) */
 "(sin(w*2)*sin(w*(6+floor(A*4)))+sin(w3*2)*sin(w3*(5+floor(A*4)))*.6)*.5",         /* VOX: two formants (ref 12) */
 "(sin(w*2)+sin(w*4)*.5+sin(w*6)*.3*A+sin(w3*2)*.4+sin(w5*2)*.4)*.25",              /* ORGAN */
 "sin(w*2)*sin(w*(7+floor(A*9)))*(1-T*.5)*.8",                                      /* RING: ring modulated */
 "lp(sqr(w*2),.1+A*.4)*((s*3)%8<5)*(1-T)*.5",                                       /* NIBBLE: an arpeggio with rests (ref 11) */
 "sin(T*10+(1-T)*(1-T)*(2+A*6))*(1-T)*(c%4==0)",                                    /* KICK: four on the floor, A its sweep */
 "sin(T*10+(1-T)*(1-T)*6)*(1-T)*(c%4==0)+saw(w)*(1-T)*(c%4==2)*(.3+A*.5)",          /* KICK BASS: the bass between */
 "hp(rnd(t),.5)*(1-T)*(1-T)*(.15+A*.4)",                                            /* HATS: every 16th, A louder */
 "(rnd(t)*.7+sin(T*30)*.3)*(1-T)*(1-T)*(c%8==4)",                                   /* SNARE: on 2 and 4 */
 "(sin(T*10+(1-T)*4)*(c%4==0)+rnd(t)*((c%8==4)*.5+(c%2)*A*.25))*(1-T)",             /* BEAT */
 "(sin(T*10+(1-T)*4)*(2313>>c&1)+rnd(t)*((4112>>c&1)*.45+A*.15))*(1-T)",            /* BREAK */
 "sin(w*4*(1+(1-T)*2))*(1-T)*(1-T)*((s*5+A*8)%8<3)",                                /* PERC: tuned, A its rhythm */
 "rnd(t)*((T<.08)+(T>.12)*(T<.2)+(T>.25)*(1-T))*(c%8==4)*.7",                       /* CLAP: three hands */
 "lp(sin(T*10+(1-T)*(1-T)*4)*(c%4==0)+rnd(t)*.3,.03+A*.1)*(1-T)*2",                 /* RUMBLE: the kick's low tail */
 "hp(rnd(t),.6)*(1-(T*(1+floor(A*3)))%1)*.35",                                      /* ROLLS: hats in 2, 3, 4 a step (ref 5) */
 "sin(w*2*(1+(1-T)*.5))*(1-T)*(1-T)*(1170>>c&1)",                                   /* TOMS: the chord's root as toms */
 "lp(fix(t*(t>>9|t>>12)&255)/128-1,.15+A*.6)",                                      /* BYTE: the classic, A the cutoff */
 "lp(fix((int(w*256)^(t>>7))&255)/128-1,.15+A*.7)",                                 /* BITS: the phase's byte xor time */
 "lp(fix((t*(b+1)&t>>5|t>>8)&255)/128-1,.2+A*.6)",                                  /* XOR: the chord's degree in it */
 "lp(fix((t*(s%7+1))>>(c%4)&255)/128-1,.15+A*.6)",                                  /* GLITCH: its shift by the bar */
 "lp(fix(t*((t>>11)%4+1)&t>>5&255)/128-1,.2+A*.5)",                                 /* BITGROOVE: (ref 15) */
};
/* FORM 1's states' own DRIVE WOBBLE TONE CHORD VOICING (its presets', FORM 2) */
static const uint8_t F1_XTRA[36][5]={{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{0,2,127,1,0},{20,2,64,4,6},{0,2,64,7,4},{0,2,64,1,0},{0,2,64,4,6},{30,4,90,1,0},{0,2,64,4,6},{10,2,64,4,0},{20,4,64,1,0},{0,2,64,7,7},{0,2,64,4,6},{10,2,64,1,0},{60,5,70,1,0},{70,6,60,1,0},{30,4,100,1,0}};
static const char *const FORM_NAMES[]={"01 SCORE","02 TIME RULE","03 COUNTER","04 FIVE PART","05 TUNED MEM","06 SWITCH","07 BASS ECHO","08 DETUNE","09 RATIOS","10 NESTED FM","11 BIT MORPH","12 CRUSH CHD","13 FILTER CHD","14 SWEEPS","15 BIT GROOVE","16 MASK SONG","17 NONLIN CHD","18 GENERATIVE","19 SHAPED ARP","20 BUILD UP","21 BIT PAIRS","22 NESTED EQ","23 STRUM 332","24 HOUSE M9","25 CHIRP","26 DRIFT","27 BITPACK","28 BUILD","29 RISER","30 FILLS","31 SUITE","32 DEGREES","33 NIBBLE","34 GROWL","35 WOBBLE","36 ACID",
 "PLUCK","FM BELL","FM BASS","PAD","SAW CHORD","SUPERSAW","PWM","ACID","REESE","WOBBLE","GROWL","FORMANT","FOLD","SYNC","KEYS","SUB","STRUM","STAB","PUMP PAD","HARMONICS","CHIRP","RISER","CRUSH CHD","NESTED FM","DUB","VOX","ORGAN","RING","NIBBLE","KICK","KICK BASS","HATS","SNARE","BEAT","BREAK","PERC","CLAP","RUMBLE","ROLLS","TOMS","BYTE","BITS","XOR","GLITCH","BITGROOVE",0};
#define FORM_NSYN 29u                     /* the pitched ones first: the dice's A */
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
static const char *const FORM_RATE[]={"1/2","1","2","4",0};             /* the formulas' time (P_E5) */
static const char *const FORM_RUN[]={"NOTE","SONG",0};
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
static int form_owned_param(uint32_t id){return id==P_AGATE||id==P_ARATE||id==P_AORDER||id==P_LRTRG||id==P_SDIV||id==P_SLEN||id==P_KEY||id==P_DETUNE||id==P_CHRD||id==P_CVOI;}
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t prog={"PROG",F_ENUM,0,FORM_NPROG-1,0,FORM_PROGN},crate={"LENGTH",F_ENUM,0,4,2,FORM_CRATE},
  notes={"NOTES",F_ENUM,0,6,0,FORM_NOTES},run={"RUN",F_ENUM,0,1,1,FORM_RUN},ra={"RATE A",F_ENUM,0,3,1,FORM_RATE},rb={"RATE B",F_ENUM,0,3,1,FORM_RATE},
  hits={"HITS",F_INT,1,16,16,0},rot={"ROTATE",F_INT,0,15,0,0},voi={"VOICING",F_ENUM,0,7,0,F1_VOICE};
 return id==P_AGATE?&prog:id==P_ARATE?&crate:id==P_AORDER?&notes:id==P_LRTRG?&run:id==P_SDIV?&ra:id==P_SLEN?&rb:id==P_KEY?&hits:id==P_DETUNE?&rot:id==P_CVOI?&voi:0;
}
/* the presets: A B MORPH SWING, DEF A DEF B OCT A OCT B; the PROG, LENGTH, NOTES; RATE A RATE B HITS ROTATE (FP: the
 * rates x1, every step; FQ: its own) */
#define FQ(label,tag,a,b,m,da,db,sw,prog,cr,nt,oa,ob,ra,rb,hi,ro) {label,{(a)+F1_N,(b)+F1_N,m,sw,da,db,oa,ob},{0,0,127,24},0,TAG(tag),FX(0,0,30,30),0,\
  MODS(P_AGATE,(prog)+1,P_ARATE,cr,P_AORDER,nt,P_LRTRG,1,P_SDIV,ra,P_SLEN,rb,P_KEY,hi,P_DETUNE,ro)}
/* FORM 1's: its state to the next, its sends, its own progression (STATE) */
#define FSTATE(label,a,b,echo,space,ch,vo) {label,{a,b,0,0,64,64,1,1},{0,0,127,24},0,TAG(FX),FX(0,24,echo,space),0,\
  MODS(P_AGATE,0,P_ARATE,2,P_AORDER,0,P_LRTRG,1,P_SDIV,1,P_SLEN,1,P_KEY,16,P_DETUNE,0,P_CHRD,ch,P_CVOI,vo)}
#define FP(label,tag,a,b,m,da,db,sw,prog,cr,nt,oc) FQ(label,tag,a,b,m,da,db,sw,prog,cr,nt,oc,oc,1,1,16,0)
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
static const uint8_t FORM_DICE_COMP[8]={P_AGATE,P_ARATE,P_CHRD,P_CVOI,P_KEY,P_DETUNE,P_E2,P_E4};
static int16_t form_rnd(uint32_t n){return (int16_t)(n?rng()%n:0u);}
static void form_dice(track_t*t,uint32_t comp_only)
{
 int16_t*p=t->p;
 if(!comp_only){
  p[P_E0]=form_rnd(F1_N);p[P_E1]=form_rnd(3u)?form_rnd(F1_N):p[P_E0];
  p[P_E3]=form_rnd(3u)?0:form_rnd(50u);p[P_E6]=(int16_t)(form_rnd(3u));p[P_E7]=(int16_t)(form_rnd(3u));
  p[P_SDIV]=form_rnd(4u)?1:form_rnd(4u);p[P_SLEN]=form_rnd(4u)?1:form_rnd(4u);p[P_LRTRG]=1;
 }
 p[P_AGATE]=form_rnd(FORM_NPROG);p[P_ARATE]=(int16_t)(1+form_rnd(4u));p[P_AORDER]=form_rnd(7u);
 p[P_KEY]=form_rnd(2u)?16:(int16_t)(3+form_rnd(11u));p[P_DETUNE]=form_rnd(16u);p[P_CHRD]=(int16_t)(1+form_rnd(11u));p[P_CVOI]=form_rnd(8u);
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
 static const uint8_t PR[]={0,1,3,4,2,7,2,6,3,0,0,5,8,0},DV[4]={1,2,3,3},CY[5]={8,16,32,64,64};
 const uint8_t*x=F1_XTRA[a%36u];
 uint32_t pr=(uint32_t)clamp(t->p[P_AGATE],0,FORM_NPROG-1);
 memcpy(q,t->p,P_COUNT*sizeof *q);
 q[P_E0]=(int16_t)a;q[P_E1]=(int16_t)b;q[P_E2]=(int16_t)morph;q[P_E3]=(int16_t)def;q[P_E4]=0;q[P_E5]=100;q[P_E6]=0;q[P_E7]=64;
 q[P_CAOS]=0;q[P_ASWING]=x[0];q[P_ARATE]=x[1];q[P_APROB]=x[2];q[P_KEY]=100;q[P_DETUNE]=24;   /* (CHORD, VOICING: the part's own, P_CHRD P_CVOI) */
 q[P_AORDER]=pr==FORM_NPROG-1u?5:0;q[P_AGATE]=PR[pr<sizeof PR?pr:0];q[P_AOCT]=(int16_t)oct;q[P_SDIV]=DV[rate&3u];
 q[P_SLEN]=CY[clamp(t->p[P_ARATE],0,4)];q[P_SSWING]=t->p[P_E3];
}
/* a formula's program: A in the part's own slot (lofi_slot: MATH's), B in its second (WAVE's B); compiled when it changes */
static const fvm_prog_t *form_prog(uint32_t tr,uint32_t side,uint32_t f,lofi_work_t*w)
{
 fvm_prog_t *pg=side?&wave_prg_b[tr]:lofi_slot(tr);
 uint16_t *id=side?&wave_id_b[tr]:&lofi_prg_id[tr],want=(uint16_t)(0x8000u|f);
 if(side&&!pg->ins)fvm_prog_init(pg,wave_ins_b[tr],32u);
 if(*id!=want){
  const char*s=FORM_BANK[(f-F1_N)%FORM_N];uint32_t n=0;
  while(s[n])n++;
  if(fvm_compile(&w->c,pg,s,n,0)||pg->nstate>2u)pg->n=0;   /* (silent) */
  *id=want;
 }
 return pg;
}
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
 uint32_t morph=(uint32_t)clamp(t->p[P_E2],0,127),bl=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300);
 uint32_t PB[CTL],W[CTL],X3[CTL],X5[CTL],XD[CTL],TT[CTL],TS[CTL],PP[CTL],YU[CTL],pos,dpos,rem,carry=0;
 int32_t YA[CTL],YB[CTL];
 lofi_work_t*wk=&lofi_work[MC_CPU()&1u];
 if(n>CTL)n=CTL;if(!n)return;
 uint32_t s0i=morph==127u&&a!=b?1u:0u,s1i=morph&&morph<127u&&a!=b?2u:s0i+1u;   /* the sides played: A, B, or both */
 /* the song's beats (Q16) at each sample: its clock (RUN SONG: seq.c's) or the note's own */
 if(func_song(t)){pos=func_clock_begin;dpos=func_clock_end-func_clock_begin;}
 else{uint32_t tk=v->ph[0];pos=(uint32_t)(((uint64_t)tk<<16)/bl);dpos=(uint32_t)((((uint64_t)(tk+n*hs))<<16)/bl)-pos;}
 rem=dpos%n;dpos/=n;
 for(i=0;i<n;i++){PB[i]=pos;pos+=dpos;carry+=rem;if(carry>=n){carry-=n;pos++;}}
 /* SWING: each pair of 16ths, the second late (up to a triplet's) */
 uint32_t sw=(uint32_t)clamp(t->p[P_E3],0,100)*218u,split=65536u+sw,r1=(uint32_t)(0x100000000ull/split)>>1,r2=(uint32_t)(0x100000000ull/(131072u-split))>>1;
 uint32_t sb;form_side_clock(PB,1u,1u,split,r1,r2,TT,TS,PP,&sb);   /* (the song's step: the harmony's, the gate's) */
 form_note_t fn;form_harmony(t,PB[0],sb,&fn);
 /* the phases at the lower OCTAVE of the two, the other's an exact octave up (a shift) */
 uint32_t oa=(uint32_t)clamp(t->p[P_E6],0,3),ob=(uint32_t)clamp(t->p[P_E7],0,3),om=s0i?ob:s1i==2u&&ob<oa?ob:oa,ka=oa>om?oa-om:0u,kb=ob>om?ob-om:0u;
 uint32_t inc=pitch_inc((uint32_t)clamp(fn.note+((int32_t)om-1)*12,12,108)*16u)*hs,i3=(uint32_t)(((uint64_t)inc*FORM_SEMI[fn.third])>>12),i5=(uint32_t)(((uint64_t)inc*FORM_SEMI[fn.fifth])>>12),id=inc+(inc>>7);
 uint32_t p0=v->ph[1],p3=(uint32_t)v->s[6],p5=(uint32_t)v->s[7],pd=v->ph[2],cb=FORM_CRB[clamp(t->p[P_ARATE],0,4)]<<16;
 uint32_t f1both=s1i-s0i==2u&&a<F1_N&&b<F1_N;           /* (FORM 1 on both sides: its own MORPH, the one that composes) */
 for(uint32_t side=s0i;side<s1i;side++){
  uint32_t k=side?kb:ka,r=(uint32_t)clamp(t->p[side?P_SLEN:P_SDIV],0,3),s,q0=p0,q3=p3,q5=p5,qd=pd,f=side?b:a,df=(uint32_t)clamp(t->p[side?P_E5:P_E4],0,127);
  int32_t*y=side?YB:YA;
  if(f<F1_N){                                           /* FORM 1: its state, its whole composition */
   int16_t q[P_COUNT];f1_vs_t*st=&f1_st[tr];
   f1_view(t,q,f,f1both?b:f,f1both?morph:0u,df,side?ob:oa,r);f1_p[MC_CPU()&1u]=q;
   st->note=v->note;f1_render(t,st,y,n,m);
   if(f1both){for(i=0;i<n;i++)YB[i]=YA[i];s1i=s0i+1u;break;}
   continue;
  }
  for(i=0;i<n;i++){W[i]=(q0<<k)>>16;X3[i]=(q3<<k)>>16;X5[i]=(q5<<k)>>16;XD[i]=(qd<<k)>>16;q0+=inc;q3+=i3;q5+=i5;qd+=id;}
  form_side_clock(PB,n,r,split,r1,r2,TT,TS,PP,&s);
  fvm_env_t e={{TT,W,TS,XD,X3},{0},&v->s[side?2:0]};
  e.arr[FA_BEAT]=PP;e.arr[FA_IN]=X5;
  e.u[FU_NOTE]=(uint32_t)clamp(fn.note+((int32_t)(side?ob:oa)-1)*12,0,127);e.u[FU_STEP]=s;e.u[FU_C]=s&15u;e.u[FU_D]=s>>4;e.u[FU_B]=(uint32_t)fn.deg;
  e.u[FU_DF]=(uint32_t)((PB[0]%cb)/(cb>>16));e.u[FU_BPS]=bl;e.u[FU_AF]=df*65536u/127u;e.u[FU_A]=df*2u;
  lofi_run(form_prog(tr,side,f,wk),&e,wk,n,YU);
  for(i=0;i<n;i++)y[i]=clamp(fvm_sample(FT_FIX,YU[i]),-32767,32767);
 }
 if(s1i-s0i==1u)for(i=0;i<n;i++)(s0i?YA:YB)[i]=(s0i?YB:YA)[i];   /* (one side: both buffers it) */
 v->ph[1]=p0+inc*n;v->s[6]=(int32_t)(p3+i3*n);v->s[7]=(int32_t)(p5+i5*n);v->ph[2]=pd+id*n;
 /* the EUCLID gate: HITS of the bar's 16 steps (16 all), ROTATE; an off step fades out, the next hit fades in */
 uint32_t hits=(uint32_t)clamp(t->p[P_KEY],1,16),kk=(sb+(uint32_t)clamp(t->p[P_DETUNE],0,15))&15u;
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
  if(s1i-s0i==2u)y+=((YB[i]-y)*(int32_t)morph)>>7;
  g+=(gt-g)>>(6u-VH_SH());y=mulq15(y,g);
  y=soft_knee(y,26000);
  v->s[4]+=(y-v->s[4])/(int32_t)(512u/hs);y-=v->s[4];      /* (the DC away) */
  out[i]+=voice_amp(clamp(y,-32767,32767),m,i);
 }
 v->s[5]=g;v->ph[0]+=n*hs;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"FORM","DEFORM"},
 .edit={{"FORM A",F_ENUM,0,FORM_NT-1,0,FORM_NAMES},{"FORM B",F_ENUM,0,FORM_NT-1,1,FORM_NAMES},{"MORPH",F_PCT,0,127,0,0},{"SWING",F_PCT,0,100,0,0},
        {"DEF A",F_PCT,0,127,64,0},{"DEF B",F_PCT,0,127,64,0},{"OCT A",F_ENUM,0,3,1,FORM_OCT},{"OCT B",F_ENUM,0,3,1,FORM_OCT}},
 .presets=FORM_PRESETS,.npresets=NELEM(FORM_PRESETS),.knob={P_E0,P_E1,P_E2,P_E4},.poly=1,.half=2,.note_on=form_note_on,.render=form_render};
