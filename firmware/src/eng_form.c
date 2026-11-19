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
static const char *const FORM_NAMES[]={"PLUCK","FM BELL","FM BASS","PAD","SAW CHORD","SUPERSAW","PWM","ACID","REESE","WOBBLE","GROWL","FORMANT","FOLD","SYNC","KEYS","SUB","STRUM","STAB","PUMP PAD","HARMONICS","CHIRP","RISER","CRUSH CHD","NESTED FM","DUB","VOX","ORGAN","RING","NIBBLE","KICK","KICK BASS","HATS","SNARE","BEAT","BREAK","PERC","CLAP","RUMBLE","ROLLS","TOMS","BYTE","BITS","XOR","GLITCH","BITGROOVE",0};
#define FORM_NSYN 29u                     /* the pitched ones first: the dice's A */
#define FORM_N (sizeof FORM_BANK/sizeof FORM_BANK[0])
_Static_assert(FORM_N==sizeof FORM_NAMES/sizeof FORM_NAMES[0]-1u,"FORM: a name a formula");
/* ---- the harmony: progressions as degrees of the scale (a 7-note one: these; another: its own degrees) ---- */
static const int8_t FORM_PROGS[][8]={
 {0,5,2,6,-1},{0,3,-1},{0,6,5,4,-1},{1,4,0,0,-1},{0,4,5,3,-1},{0,3,4,0,-1},{0,5,3,4,-1},{0,6,-1},{0,1,-1},
 {0,3,0,4,-1},{0,3,6,2,5,1,4,-1},{0,2,5,1,-1},{0,-1}};
#define FORM_NPROG (sizeof FORM_PROGS/sizeof FORM_PROGS[0])
static const char *const FORM_PROGN[]={"AXIS","VAMP IV","ANDALUZ","II-V-I","POP","CADENCE","50S","VAMP VII","PHRYGIAN",
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
static int form_owned_param(uint32_t id){return id==P_AGATE||id==P_ARATE||id==P_AORDER||id==P_LRTRG||id==P_SDIV||id==P_SLEN||id==P_CHRD||id==P_CVOI;}
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t prog={"PROG",F_ENUM,0,FORM_NPROG-1,0,FORM_PROGN},crate={"LENGTH",F_ENUM,0,4,2,FORM_CRATE},
  notes={"NOTES",F_ENUM,0,6,0,FORM_NOTES},run={"RUN",F_ENUM,0,1,1,FORM_RUN},ra={"RATE A",F_ENUM,0,3,1,FORM_RATE},rb={"RATE B",F_ENUM,0,3,1,FORM_RATE},
  hits={"HITS",F_INT,1,16,16,0},rot={"ROTATE",F_INT,0,15,0,0};
 return id==P_AGATE?&prog:id==P_ARATE?&crate:id==P_AORDER?&notes:id==P_LRTRG?&run:id==P_SDIV?&ra:id==P_SLEN?&rb:id==P_CHRD?&hits:id==P_CVOI?&rot:0;
}
/* the presets: A B MORPH SWING, DEF A DEF B OCT A OCT B; the PROG, LENGTH, NOTES; RATE A RATE B HITS ROTATE (FP: the
 * rates x1, every step; FQ: its own) */
#define FQ(label,tag,a,b,m,da,db,sw,prog,cr,nt,oa,ob,ra,rb,hi,ro) {label,{a,b,m,sw,da,db,oa,ob},{0,0,127,24},0,TAG(tag),FX(0,0,30,30),0,\
  MODS(P_AGATE,prog,P_ARATE,cr,P_AORDER,nt,P_LRTRG,1,P_SDIV,ra,P_SLEN,rb,P_CHRD,hi,P_CVOI,ro)}
#define FP(label,tag,a,b,m,da,db,sw,prog,cr,nt,oc) FQ(label,tag,a,b,m,da,db,sw,prog,cr,nt,oc,oc,1,1,16,0)
static const preset_t FORM_PRESETS[]={
 FP("PLUCK ARP",PLUCK,0,14,0,40,50,0,0,2,1,1),     FP("BELL DRIFT",BELL,1,3,40,30,60,0,4,3,3,1),
 FP("FM BASS",BASS,2,7,0,40,50,10,0,2,6,1),         FP("PAD AXIS",PAD,3,4,30,60,40,0,0,3,0,1),
 FP("SAW CHORDS",PAD,4,5,0,50,50,0,2,2,0,1),         FP("SUPERSAW",LEAD,5,13,0,60,50,0,4,2,1,1),
 FP("PWM KEYS",KEYS,6,14,0,60,60,0,6,2,3,1),         FP("ACID LINE",BASS,7,8,0,60,60,20,8,2,4,1),
 FP("REESE",BASS,8,9,0,40,60,0,1,3,6,1),            FP("WOBBLE",BASS,9,10,0,40,60,0,7,2,0,1),
 FP("GROWL",BASS,10,9,50,60,50,0,8,1,6,1),          FP("FORMANT",LEAD,11,12,0,60,60,0,2,2,5,1),
 FP("FOLD",LEAD,12,13,0,40,60,0,10,1,1,1),          FP("SYNC LEAD",LEAD,13,0,0,50,40,0,0,2,3,2),
 FP("KEYS",KEYS,14,3,0,50,60,0,3,2,0,1),            FP("SUB",BASS,15,30,0,30,40,0,0,2,6,1),
 FP("KICK",DRUM,29,33,0,50,50,0,12,2,0,1),          FP("KICK BASS",DRUM,30,7,0,50,50,0,0,2,6,1),
 FP("HATS",DRUM,31,34,0,40,60,30,12,2,0,1),         FP("SNARE",DRUM,32,33,0,50,50,0,12,2,0,1),
 FP("BEAT",DRUM,33,34,0,60,60,20,12,2,0,1),         FP("BREAK",DRUM,34,33,0,50,60,0,12,2,0,1),
 FP("PERC",DRUM,35,1,0,60,50,20,5,2,4,1),           FP("BYTE",FX,40,41,0,60,60,0,0,2,0,1),
 FP("BITS",FX,41,42,0,60,60,0,9,2,4,1),             FP("XOR",FX,42,40,0,60,60,0,11,2,5,1),
 FQ("STRUM 332",PLUCK,16,3,0,40,50,0,0,2,0,1,1,1,1,10,0),
 FQ("HOUSE STAB",KEYS,17,18,0,50,50,20,4,3,0,1,1,1,1,16,0),
 FQ("PUMP PAD",PAD,18,3,0,50,60,0,0,3,0,1,1,1,1,16,0),
 FQ("HARMONICS",BELL,19,1,0,50,50,0,12,2,0,1,2,1,2,16,0),
 FQ("CHIRP",FX,20,13,0,70,50,0,10,2,1,1,1,1,1,5,2),
 FQ("RISER",FX,21,3,0,80,60,0,0,4,0,1,1,1,1,16,0),
 FQ("CRUSH CHD",PAD,22,4,0,40,60,0,2,2,0,1,1,1,1,16,0),
 FQ("NESTED FM",BELL,23,1,0,60,60,0,11,2,3,1,2,1,1,7,0),
 FQ("DUB",BASS,24,10,0,60,60,0,7,2,0,1,1,1,1,16,0),
 FQ("VOX",PAD,25,11,0,50,60,0,4,3,0,1,1,1,1,16,0),
 FQ("ORGAN",KEYS,26,14,0,40,50,0,5,2,0,1,1,1,1,6,1),
 FQ("RING",LEAD,27,12,0,50,50,0,9,2,4,1,2,1,1,16,0),
 FQ("NIBBLE",LEAD,28,0,0,50,50,0,0,2,1,1,2,1,1,16,0),
 FQ("CLAP",DRUM,36,33,0,50,50,0,12,2,0,1,1,1,1,16,0),
 FQ("RUMBLE",DRUM,37,29,0,50,50,0,12,2,0,1,1,1,1,16,0),
 FQ("ROLLS",DRUM,38,31,0,60,50,0,12,2,0,1,1,1,1,16,0),
 FQ("TOMS",DRUM,39,35,0,50,50,0,1,2,0,1,1,1,1,16,0),
 FQ("GLITCH",FX,43,41,0,60,60,0,9,2,4,1,1,2,1,11,3),
 FQ("BITGROOVE",FX,44,40,0,60,60,0,0,2,0,1,1,1,1,16,0),
};
#undef FP
#undef FQ
/* ---- the dice (ui_input.c: SELECT the whole of it; SEQ + SELECT the harmony alone): a pitched A, B by chance a pitched or
 * a drum, MORPH most of the time at an end; the harmony anything ---- */
static const uint8_t FORM_DICE_COMP[8]={P_AGATE,P_ARATE,P_AORDER,P_CHRD,P_CVOI,P_E2,P_E4,P_E5};
static int16_t form_rnd(uint32_t n){return (int16_t)(n?rng()%n:0u);}
static void form_dice(track_t*t,uint32_t comp_only)
{
 int16_t*p=t->p;
 if(!comp_only){
  p[P_E0]=form_rnd(FORM_NSYN);p[P_E1]=form_rnd(3u)?form_rnd(FORM_NSYN):(int16_t)(FORM_NSYN+form_rnd(FORM_N-FORM_NSYN));
  p[P_E3]=form_rnd(3u)?0:form_rnd(50u);p[P_E6]=(int16_t)(form_rnd(3u));p[P_E7]=(int16_t)(form_rnd(3u));
  p[P_SDIV]=form_rnd(4u)?1:form_rnd(4u);p[P_SLEN]=form_rnd(4u)?1:form_rnd(4u);p[P_LRTRG]=1;
 }
 p[P_AGATE]=form_rnd(FORM_NPROG);p[P_ARATE]=(int16_t)(1+form_rnd(4u));p[P_AORDER]=form_rnd(7u);
 p[P_CHRD]=form_rnd(2u)?16:(int16_t)(3+form_rnd(11u));p[P_CVOI]=form_rnd(16u);
 p[P_E2]=form_rnd(3u)?0:form_rnd(128u);p[P_E4]=(int16_t)(20+form_rnd(90u));p[P_E5]=(int16_t)(20+form_rnd(90u));
}
static uint32_t form_cost(const track_t*t){return t->p[P_E2]>0&&t->p[P_E2]<127&&t->p[P_E0]!=t->p[P_E1]?10u:6u;}
static void form_note_on(track_t*t,voice_t*v){(void)t;memset(v->s,0,sizeof v->s);memset(v->ph,0,sizeof v->ph);}
/* a formula's program: A in the part's own slot (lofi_slot: MATH's), B in its second (WAVE's B); compiled when it changes */
static const fvm_prog_t *form_prog(uint32_t tr,uint32_t side,uint32_t f,lofi_work_t*w)
{
 fvm_prog_t *pg=side?&wave_prg_b[tr]:lofi_slot(tr);
 uint16_t *id=side?&wave_id_b[tr]:&lofi_prg_id[tr],want=(uint16_t)(0x8000u|f);
 if(side&&!pg->ins)fvm_prog_init(pg,wave_ins_b[tr],32u);
 if(*id!=want){
  const char*s=FORM_BANK[f%FORM_N];uint32_t n=0;
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
 uint32_t hs=1u<<VH_SH(),tr=(uint32_t)(t-trk)%NTRK,i,a=(uint32_t)clamp(t->p[P_E0],0,FORM_N-1),b=(uint32_t)clamp(t->p[P_E1],0,FORM_N-1);
 uint32_t morph=(uint32_t)clamp(t->p[P_E2],0,127),bl=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300);
 uint32_t PB[CTL],W[CTL],X3[CTL],X5[CTL],XD[CTL],TT[CTL],TS[CTL],PP[CTL],YA[CTL],YB[CTL],pos,dpos,rem,carry=0;
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
 for(uint32_t side=s0i;side<s1i;side++){
  uint32_t k=side?kb:ka,r=(uint32_t)clamp(t->p[side?P_SLEN:P_SDIV],0,3),s,q0=p0,q3=p3,q5=p5,qd=pd,f=side?b:a,df=(uint32_t)clamp(t->p[side?P_E5:P_E4],0,127);
  for(i=0;i<n;i++){W[i]=(q0<<k)>>16;X3[i]=(q3<<k)>>16;X5[i]=(q5<<k)>>16;XD[i]=(qd<<k)>>16;q0+=inc;q3+=i3;q5+=i5;qd+=id;}
  form_side_clock(PB,n,r,split,r1,r2,TT,TS,PP,&s);
  fvm_env_t e={{TT,W,TS,XD,X3},{0},&v->s[side?2:0]};
  e.arr[FA_BEAT]=PP;e.arr[FA_IN]=X5;
  e.u[FU_NOTE]=(uint32_t)clamp(fn.note+((int32_t)(side?ob:oa)-1)*12,0,127);e.u[FU_STEP]=s;e.u[FU_C]=s&15u;e.u[FU_D]=s>>4;e.u[FU_B]=(uint32_t)fn.deg;
  e.u[FU_DF]=(uint32_t)((PB[0]%cb)/(cb>>16));e.u[FU_BPS]=bl;e.u[FU_AF]=df*65536u/127u;e.u[FU_A]=df*2u;
  lofi_run(form_prog(tr,side,f,wk),&e,wk,n,side?YB:YA);
  if(s1i-s0i==1u)for(i=0;i<n;i++)(side?YA:YB)[i]=(side?YB:YA)[i];   /* (one side: both buffers it) */
 }
 v->ph[1]=p0+inc*n;v->s[6]=(int32_t)(p3+i3*n);v->s[7]=(int32_t)(p5+i5*n);v->ph[2]=pd+id*n;
 /* the EUCLID gate: HITS of the bar's 16 steps (16 all), ROTATE; an off step fades out, the next hit fades in */
 uint32_t hits=(uint32_t)clamp(t->p[P_CHRD],1,16),kk=(sb+(uint32_t)clamp(t->p[P_CVOI],0,15))&15u;
 int32_t gt=hits>=16u||((kk+1u)*hits>>4)!=(kk*hits>>4)?32767:0,g=v->s[5];
 if(tr==form_scope_tr){                                  /* the scopes: triggered on the note's turn, ~2.5 turns in 64 points */
  uint32_t dec=clamp((int32_t)((0xFFFFFFFFu/(inc|1u))/26u),1,64),ph=p0;
  for(i=0;i<n;i++,ph+=inc){
   if(form_sc_i<64u){if(!form_sc_c--){form_sc_c=(uint8_t)(dec-1u);form_scope[0][form_sc_i]=(int16_t)clamp(fvm_sample(FT_FIX,YA[i]),-32767,32767);form_scope[1][form_sc_i]=(int16_t)clamp(fvm_sample(FT_FIX,YB[i]),-32767,32767);form_sc_i++;}}
   else if(ph+inc<ph){form_sc_i=0;form_sc_c=0;}
  }
 }
 for(i=0;i<n;i++){
  int32_t y=fvm_sample(FT_FIX,YA[i]);
  if(s1i-s0i==2u)y+=((fvm_sample(FT_FIX,YB[i])-y)*(int32_t)morph)>>7;
  g+=(gt-g)>>(6u-VH_SH());y=mulq15(y,g);
  y=soft_knee(y,26000);
  v->s[4]+=(y-v->s[4])/(int32_t)(512u/hs);y-=v->s[4];      /* (the DC away) */
  out[i]+=voice_amp(clamp(y,-32767,32767),m,i);
 }
 v->s[5]=g;v->ph[0]+=n*hs;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"FORM","DEFORM"},
 .edit={{"FORM A",F_ENUM,0,FORM_N-1,0,FORM_NAMES},{"FORM B",F_ENUM,0,FORM_N-1,1,FORM_NAMES},{"MORPH",F_PCT,0,127,0,0},{"SWING",F_PCT,0,100,0,0},
        {"DEF A",F_PCT,0,127,64,0},{"DEF B",F_PCT,0,127,64,0},{"OCT A",F_ENUM,0,3,1,FORM_OCT},{"OCT B",F_ENUM,0,3,1,FORM_OCT}},
 .presets=FORM_PRESETS,.npresets=NELEM(FORM_PRESETS),.knob={P_E0,P_E1,P_E2,P_E4},.poly=1,.half=2,.note_on=form_note_on,.render=form_render};
