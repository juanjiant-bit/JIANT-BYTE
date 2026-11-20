/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * FORM 1's sound inside FORM 3 (eng_form.c): its 36 states (the 22 references, the 14 of FORM 2) as they were, each a
 * whole composition (melody, roots, drums, its equation's timbre, its arrangement), FORM A / FORM B's first 36. Its
 * parameters read from the view f1_p (FORM 3's controls mapped onto FORM 1's: eng_form.c f1_view), its state its own
 * (f1_vs_t, a part's: not the voice's, FORM 3's) */
typedef struct{int32_t s[8];uint32_t ph[3];int32_t lp;int16_t kenv,senv;uint8_t note;} f1_vs_t;   /* (lp: SOFT's low-pass; kenv senv:
                                                         * the kick's and the snare's hit, FUSION's, a block late) */
static const int16_t *f1_p[2];                           /* (a core's view of the part's parameters, its render's) */
#define F1P(x) (f1_p[MC_CPU()&1u][x])
/* STYLE FUNC: every state as funcbeat (its character bass its own); a character chosen: that one */
static uint32_t f1_dom(uint32_t d,uint32_t own){return d?d:own>=5u?own:3u;}   /* (FORM: funcbeat alone, Juan: no BYTE HYBRID FLOAT; a character state its own) */
/* SPDX-License-Identifier: GPL-3.0-only
 * FORM: bounded native adaptations of the 22 user-supplied reference equations.
 * Source/credits: docs/references/bytebeat-floatbeat-funcbeat.txt.
 * Only selected A/B programs run. No recorded loops, JS heap or per-voice delay allocation. */
#define F1_N 36u                       /* (FORM 2: 22 of the first references, 14 of Juan's: docs/FORM-REFERENCIAS.md) */
#define F1_N1 22u                      /* the first 22: as they were */
static const char *const F1_NAMES[]={"01 SCORE","02 TIME RULE","03 COUNTER","04 FIVE PART","05 TUNED MEM","06 SWITCH","07 BASS ECHO","08 DETUNE","09 RATIOS","10 NESTED FM","11 BIT MORPH","12 CRUSH CHD","13 FILTER CHD","14 SWEEPS","15 BIT GROOVE","16 MASK SONG","17 NONLIN CHD","18 GENERATIVE","19 SHAPED ARP","20 BUILD UP","21 BIT PAIRS","22 NESTED EQ",
 "23 STRUM 332","24 HOUSE M9","25 CHIRP","26 DRIFT","27 BITPACK","28 BUILD","29 RISER","30 FILLS","31 SUITE","32 DEGREES","33 NIBBLE","34 GROWL","35 WOBBLE","36 ACID"};
static const char *const F1_GROOVE[]={"ORIGINAL","EUCLID","BREAKS","POLY","REVERSE","HALF","DOUBLE","ROTATE"};
static const char *const F1_DOMAIN[]={"AUTO","BYTE","FLOAT","FUNC","HYBRID","WOBBLE","REESE","GROWL","RING","SCREECH","ACID"};
/* (FORM 2) the GENES page: HARMONY (P_AORDER: 0 the state's own), DRIVE (P_ASWING), WOBBLE (P_ARATE), TONE (P_APROB); FORM's
 * ARP slots (no arpeggio on FORM: its song plays itself). Their defaults (0, 0, 2, 127) are the classic sound */
static const char *const F1_HARM[]={"AUTO","CLASSIC","DEGREES","FAMILY","PEDAL","BITS"};
static const char *const F1_WOB[]={"1/2","1/4","1/8","1/8T","1/16","STEPS","JUMPS","BAR"};
static const char *const F1_PROG[]={"AUTO","AXIS","II-V-I","VAMP","DESCEND","FOURTHS","EPIC","POP","CIRCLE"};   /* F1_FAM, P_AGATE */
static const char *const F1_OCT[]={"-1","0","+1","+2"};                     /* the melody's octave, P_AOCT */
static const char *const F1_VOICE[]={"CLOSE","LEAD","OPEN","+BASS","DROP","QUARTAL","NEAR","WIDE"};
enum{F1D_WOBBLE=5,F1D_REESE,F1D_GROWL,F1D_RING,F1D_SCREECH,F1D_ACID};   /* DOMAIN 5..10: the character bass (f1_cbass) */
static const char *const F1_RUN[]={"NOTE","SONG"};
typedef struct {const char*melody,*roots;uint16_t kick,snare,hat,gate;uint8_t domain,rate,voices,drums,echo,space,structure,color;} f1_state_t;
/* Each row keeps a different motif/clock, layer topology and mathematical voice;
 * state number also selects the corresponding equation topology in f1_wave. */
static const f1_state_t F1_STATE[F1_N]={
 {"0356797553207530","0537",0x1111,0x1010,0xffff,0xffff,3,2,3,100,10,20,1,52},
 {"007570370a735730","0573",0x1491,0x1040,0xaaaa,0xb6db,4,4,2,110,0,5,2,96},
 {"037a753037a7c753","0375",0x1111,0x1010,0xaaaa,0xf7df,3,2,3,80,72,45,3,55},
 {"0000000000000000","0000",0,0,0,0xffff,2,1,1,0,40,25,4,88},
 {"14bgfb6b34bgiknr","0573",0x1111,0x1010,0xaaaa,0xffef,3,2,3,88,100,60,5,60},
 {"000355730005a753","0053",0x5111,0x1040,0xfafa,0xff7f,1,3,2,110,0,8,6,95},
 {"00c70a7503c70a75","0357",0x1111,0x1010,0xffff,0xeeee,4,2,3,100,96,20,7,72},
 {"037a037c057c057a","0357",0x0101,0x1010,0x8888,0xffff,2,2,4,45,112,60,8,56},
 {"0c75357c0c75357c","0357",0x0101,0x1010,0xaaaa,0xffff,4,3,3,75,18,15,9,84},
 {"0000000000000000","0000",0,0,0,0xffff,2,1,1,0,32,40,10,85},
 {"003300350033553a","0035",0x1111,0x1010,0xaaaa,0xffff,1,4,2,76,14,5,11,100},
 {"037a05ac037a05ac","0055",0x1911,0x1010,0xeeee,0xdf7b,4,2,4,110,110,25,12,82},
 {"037a05ac037a05ac","0537",0x1111,0x1010,0xaaaa,0xffdf,4,2,4,115,100,55,13,42},
 {"0037003500370032","0000",0x1551,0x1010,0xeeee,0xffff,4,4,2,110,5,5,14,110},
 {"007757a7007757a7","0573",0x5111,0x1040,0xffee,0xefff,1,4,2,105,0,8,15,112},
 {"057a7530057a7530","0055",0x1491,0x1010,0xbaba,0xf7df,1,4,3,115,24,12,16,102},
 {"07c70a0f07c7ha75","7530",0x1111,0x1010,0xeaaa,0xc737,4,3,4,100,58,48,17,92},
 {"0579b753079b7530","9753",0x1491,0x1040,0xaeee,0xefff,3,3,4,88,127,90,18,70},
 {"037cf7c7037cf7c7","0753",0x1111,0x1010,0xaaaa,0xafaf,4,4,3,104,50,35,19,105},
 {"0085008500850085","0085",0x0441,0x1010,0xd7d7,0xfffe,1,4,3,120,30,22,20,115},
 {"057a3757057a3757","0573",0x5a91,0x1040,0xfafa,0xeeee,1,4,2,120,52,30,21,118},
 {"03570a7503570a75","0530",0x1111,0x1010,0xaeee,0xbdbd,4,3,2,110,28,28,22,100},
 /* (FORM 2) Juan's references (docs/FORM-REFERENCIAS.md): their genes in F1_GENE */
 {"0247024702470247","0537",0x4949,0x1010,0xaaaa,0x4949,0,4,4,90,60,40,23,70},     /* 23 STRUM 332 (ref 1) */
 {"7a7c7a7c4a7c7a9c","0526",0x1111,0x1010,0x4444,0x2222,0,4,4,110,40,30,24,80},    /* 24 HOUSE M9 (ref 2) */
 {"0024002100240021","0000",0x0101,0x1010,0x2222,0xffff,0,4,1,80,30,40,25,60},     /* 25 CHIRP (ref 3) */
 {"0000000000000000","0000",0x0011,0x0100,0x2222,0xb6db,0,2,2,70,70,80,26,50},     /* 26 DRIFT (ref 4) */
 {"0000000000000000","0000",0x1111,0x1010,0xffff,0xffff,1,4,1,110,10,10,27,96},    /* 27 BITPACK (ref 5) */
 {"0247042702470429","0362",0x0441,0x1010,0xaaaa,0xeeee,0,4,4,110,30,30,28,64},    /* 28 BUILD (ref 6) */
 {"05a705a7059a05a7","0000",0x1111,0x1010,0xaaaa,0xffff,1,4,2,110,20,20,29,90},    /* 29 RISER (ref 7) */
 {"0000000000000000","0000",0x1111,0x1010,0xffff,0xffff,0,4,2,115,10,10,30,84},    /* 30 FILLS (ref 8) */
 {"0427042704270429","0000",0x1111,0x1010,0x2222,0xa5a5,0,4,4,100,45,50,31,58},    /* 31 SUITE (ref 9) */
 {"0242024202420247","0000",0x0101,0x1010,0xaaaa,0xffff,0,4,4,90,50,60,32,52},     /* 32 DEGREES (ref 10) */
 {"0000000000000000","0000",0x1111,0x1010,0xaaaa,0xffff,0,4,2,100,30,30,33,76},    /* 33 NIBBLE (ref 11) */
 {"0000000000000000","0000",0x0401,0x0100,0x5555,0xffff,F1D_GROWL,4,1,120,10,30,34,90},   /* 34 GROWL (ref 12) */
 {"0000000000000000","0000",0x0401,0x0100,0x5555,0xffff,F1D_WOBBLE,4,1,120,10,30,35,90},  /* 35 WOBBLE (ref 4) */
 {"0000000000000000","0000",0x1111,0x1010,0x4444,0xffff,F1D_ACID,4,1,110,30,20,36,90}     /* 36 ACID (ref 5) */
};
/* (FORM 2) each state's genes: 0 = as the first 22 were (their rows above, untouched). harm: the progression
 * (F1H_*); fam: its family (F1_FAM, SEED moves it); mel: the melody (F1M_*: the row's string, an arpeggio with rests,
 * a packed sequence, bits, just ratios); form: F1F_* (sections, BUILD, RISER, FILL); mix: F1X_SIDE the kick's
 * sidechain, F1X_RATCH the drums' ratchets; pack: 8 nibbles (degrees; 15 a rest) for F1M_PACK and the bass jumps */
enum{F1H_CLASSIC,F1H_DEGREES,F1H_FAMILY,F1H_PEDAL,F1H_BITS};
enum{F1M_STR,F1M_ARP,F1M_PACK,F1M_BITS,F1M_JUST};
enum{F1F_SECT,F1F_BUILD,F1F_RISER,F1F_FILL};
#define F1X_SIDE 1u
#define F1X_RATCH 2u
typedef struct{uint8_t harm,fam,mel,form,mix;uint32_t pack;} f1_gene_t;
static const f1_gene_t F1_GENE[F1_N]={
 [22]={F1H_FAMILY,0,F1M_STR,F1F_SECT,0,0},
 [23]={F1H_FAMILY,0,F1M_STR,F1F_SECT,F1X_SIDE,0},
 [24]={F1H_DEGREES,6,F1M_JUST,F1F_SECT,0,0},
 [25]={F1H_BITS,3,F1M_BITS,F1F_SECT,0,0},
 [26]={F1H_PEDAL,2,F1M_PACK,F1F_SECT,F1X_RATCH,0x40F2F40Fu},
 [27]={F1H_FAMILY,4,F1M_STR,F1F_BUILD,F1X_RATCH,0},
 [28]={F1H_DEGREES,5,F1M_STR,F1F_RISER,0,0},
 [29]={F1H_FAMILY,5,F1M_ARP,F1F_FILL,0,0},
 [30]={F1H_FAMILY,6,F1M_STR,F1F_SECT,0,0},
 [31]={F1H_BITS,1,F1M_ARP,F1F_SECT,0,0},
 [32]={F1H_FAMILY,0,F1M_ARP,F1F_SECT,0,0},
 [33]={F1H_PEDAL,2,F1M_PACK,F1F_SECT,F1X_SIDE,0x00FF5FF0u},
 [34]={F1H_PEDAL,0,F1M_PACK,F1F_SECT,F1X_SIDE,0x0F7F0F00u},
 [35]={F1H_FAMILY,2,F1M_PACK,F1F_SECT,F1X_RATCH,0x7F2F04F0u},
};
static int32_t f1_mix(int32_t a,int32_t b,uint32_t w){return a+(b-a)*(int32_t)w/127;}
static uint32_t f1_char(const char*s,uint32_t i){uint32_t c=(uint8_t)s[i%str_len(s)];return c>='a'?c-'a'+10u:c-'0';}
static int32_t f1_sin(uint32_t tick,uint32_t inc,uint32_t num,uint32_t den){return sine_i(tick*(inc/den*num));}
static int32_t f1_tri(uint32_t ph){int32_t x=(int32_t)(ph>>16);return x<32768?x*2-32767:98303-x*2;}
/* Equation topologies, indexed by the reference number. Integer and nonlinear
 * paths are deliberate timbres; singular divisions use a finite denominator. */
static __attribute__((noinline)) int32_t f1_wave(uint32_t state,uint32_t tick,uint32_t inc,uint32_t age,int32_t color,uint32_t seed)
{
 uint32_t ph=tick*inc,bt=ph>>22,c=(uint32_t)clamp(color,0,127),slow=tick>>11;
 int32_t s=0,q=0,n=0,y=0;   /* (byte-cpu-light) the sine, triangle and noise only for the states that read them */
 if(state==11u||state==18u||state==21u)s=sine_i(ph);
 if(state==1u||state==9u||state==16u||state==18u||state==21u)q=f1_tri(ph);
 if(state==21u)n=(int32_t)(fvm_hash(tick+seed*977u)>>16)-32768;
 switch(state){
 case 0:y=sine_i(ph+(uint32_t)(f1_sin(tick,inc,2,1)*(int32_t)(c+16)*160));break;
 case 1:y=(q+(int32_t)(((bt^(bt>>3))&255u)*256u)-32768)/2;break;
 case 2:y=(sine_i(ph+(uint32_t)(f1_sin(tick,inc,3,2)*(int32_t)c*96))+f1_sin(tick,inc,1003,1000))/2;break;
 case 3:for(uint32_t k=1;k<=9;k+=2){int32_t e=32767-(int32_t)((slow/k)&2047u)*16;y+=mulq15(f1_sin(tick,inc,k,1),e)*(int32_t)(k==1?127u:16u+c)/381;}y=soft_knee(y,24000);break;
 case 4:y=(osc_saw(ph,inc)+osc_pulse(tick*(inc/1000u*1003u),inc,0x40000000u+c*0x800000u))/2;break;
 case 5:y=(int32_t)((((bt*(3u+(slow>>4&3u)))^(bt>>(1u+(slow&3u))))&255u)*256u)-32768;break;
 case 6:y=(osc_saw(ph,inc)+osc_saw(tick*(inc/10000u*(9999u-c)),inc)+f1_sin(tick,inc,1,2))/3;break;
 case 7:y=(osc_saw(ph,inc)+osc_saw(tick*(inc/10000u*(10001u+c)),inc)+osc_saw(tick*(inc/10000u*(9999u-c)),inc)+f1_sin(tick,inc,3,2))/4;break;
 case 8:{static const uint8_t nr[]={1,4,3,1,1},dr[]={1,5,4,2,4};uint32_t k=(slow>>5)%8u;if(k>4u)k=8u-k;y=f1_mix(f1_sin(tick,inc,nr[k],dr[k]),f1_tri(tick*(inc/5u*4u)),c);break;}
 case 9:{int32_t l=sine_i(tick*110349u);y=sine_i(ph+(uint32_t)(mulq15(q+(f1_sin(tick,inc,8,1)/5),16384+l/2)*(int32_t)(c+4)*180));break;}
 case 10:y=(int32_t)((((bt&(63u+c))^(bt*(1u+(slow&3u))))&255u)*256u)-32768;break;
 case 11:y=softclip(s*(2+(int32_t)c/16));y=(int32_t)((uint32_t)y&~((1u<<(1u+(slow>>4&3u)))-1u));break;
 case 12:y=(osc_pulse(ph,inc,0x20000000u+c*0xa00000u)+f1_sin(tick,inc,1,2))/2;break;
 case 13:y=sine_i(ph+(0x60000000u/(1u+(age>>7)))*(256u+c*24u));break;
 case 14:y=(int32_t)((((bt*(1u+c/16u+(slow>>3&7u)))>>(1u+(slow&3u)))|(bt>>2))&255u)*256-32768;break;
 case 15:y=(int32_t)(((bt*(1u+c/16u+(slow>>6&2u))*(1u+(slow>>2&3u)))^(bt>>(slow&7u)))&255u)*256-32768;break;
 case 16:{int32_t a=mulq15(q,q),b=mulq15(a,a);y=sine_i((uint32_t)mulq15(b,q)*(c+16u)*600u);break;}
 case 17:y=softclip((f1_tri(ph)+f1_tri(tick*(inc/3u*2u)))*(1+(int32_t)c/32));break;
 case 18:{int32_t a=mulq15(s,s);y=softclip(mulq15(a,a)*(2+(int32_t)c/12)-8192+q/3);break;}
 case 19:y=(int32_t)((((bt*(1u+(slow&3u)))^(bt>>2))&255u)*256u)-32768;y=(y+f1_tri(tick*(inc/10000u*(10001u+c))))/2;break;
 case 20:y=(int32_t)((((bt*(bt>>3|(129u+c/2u)))&128u)^((bt>>2)*(bt>>5|53u)&191u))*256u)-32768;break;
 /* (FORM 2) the references' timbres (docs/FORM-REFERENCIAS.md): a sample's work each about one of the above */
 case 22:y=softclip(sine_i(ph)*(1+(int32_t)((tick>>13)&3u))+(int32_t)c*64);break;                /* ref 1: tanh, its drive breathing */
 case 23:{uint32_t w=((tick>>9)&31u)|(c>>3);y=(int32_t)(((ph>>27)&w)<<11)-32768;y=(y*3+sine_i(ph))/4;break;}   /* ref 2: the saw's bits masked, the mask sweeping */
 case 24:{uint32_t u=ph>>16,q2=(u*(131072u-u))>>16;y=sine_i((q2*(3u+(c>>5)))<<16);break;}           /* ref 3: the phase's root (a chirp in the cycle) */
 case 25:y=sine_i(ph+(uint32_t)(sine_i(ph*2u+(uint32_t)(sine_i(tick*3u)<<12))*((int32_t)c+8)*64));break;   /* ref 4: FM fed back, drifting */
 case 26:{uint32_t u=ph>>21,a=slow&7u;y=(int32_t)((((u*(u>>9|1932555120u>>a))&128u)^(((u>>2)*(u>>11|58132275u>>a))&191u))*256u)-32768;break;}   /* ref 5: 8 bits */
 case 27:y=(f1_tri(ph)+f1_tri(tick*(inc+(inc>>13))))/2;break;                                  /* ref 6: two triangles phasing */
 case 28:y=(int32_t)((((bt*((slow&1u)?7u:5u))>>((slow>>1)&3u))&255u)*256u)-32768;y=(y+sine_i(ph))/2;break;   /* ref 7: the formula by section */
 case 29:y=(sine_i(ph)+sine_i(ph*3u)/3+sine_i(ph*((tick>>12&1u)?5u:7u))/(5-(int32_t)(c>>6)))*2/3;break;      /* ref 8: 5ths and 7ths */
 case 30:{int32_t x=(int32_t)(ph>>16)-32768,x2=mulq15(x,x),x4=mulq15(x2,x2),x8=mulq15(x4,x4),x9=mulq15(x8,x);   /* ref 9: the pluck (M%2-1)^9 */
  y=sine_i(((uint32_t)x9<<16)+(uint32_t)(sine_i(ph)<<15)+(uint32_t)c*0x1000000u);break;}
 case 31:y=f1_mix(f1_tri(ph),f1_tri(ph*2u),c/2u);break;                                       /* ref 10: triangles, an octave */
 case 32:{int32_t a=sine_i(ph),b=a<0?-a:a,b2=mulq15(b,b),b4=mulq15(b2,b2);uint32_t k=(slow>>4)&3u;   /* ref 11: the pulse breathing */
  int32_t w=k==0?b:k==1?b2:k==2?b4:mulq15(b4,b4);y=w*2-32767+(int32_t)(((tick*((inc>>3)*(80u+(c>>3))/80u))>>16)&8191u)-4096;break;}
 case 33:case 34:case 35:y=(osc_saw(ph,inc)+sine_i(ph*2u))/2;break;                                 /* (the bass is the character: f1_cbass) */
 default:{int32_t a=sine_i(ph*3u),b=mulq15(mulq15(s,s),s);y=softclip(mulq15(a,b)*(2+(int32_t)c/8)+q/3+n/16);break;}
 }
 return clamp(y,-32767,32767);
}
/* Bass phrases for references 19..22. beat is Q16 quarter notes.
 * Keep the fundamental below the equation layer so its gates can leave space. */
static __attribute__((noinline)) int32_t f1_drop_bass(uint32_t id, uint32_t beat, uint32_t phase, int32_t color)
{
 static const char pair_div[]="11111111111144222211111122223389";
 static const char build_div[]="11222212";
 uint32_t step=beat>>14, bar=beat>>18;
 uint32_t div=id==20?(uint32_t)(pair_div[(beat>>15)&31]-'0'):
              id==19?(uint32_t)(build_div[(beat>>15)&7]-'0'):
              id==18?1u+((bar^step)&3u):1u+((step^(step>>3|step>>6))&3u);
 uint32_t age=(beat*div)&65535u;
 int32_t env=32767-(int32_t)(age>>1);
 int32_t sub=sine_i(phase), mod=sine_i(phase*(2u+((step>>2)&1u)));
 int32_t growl=sine_i(phase+(uint32_t)mulq15(mod,env)*(uint32_t)(color+24)*768u);
 if(id==20){
  uint32_t t=phase>>21,shift=(step>>1)&7u;
  uint32_t bits=(t*(t>>9|1932555120u>>shift)&128u)^((t>>2)*(t>>11|58132275u>>shift)&191u);
  growl=(growl+(int32_t)bits*256-32768)/2;
 }else if(id==19){
  uint32_t t=phase>>21;
  int32_t bits=(int32_t)((t*((t>>2)^(t>>9))&255u)*256u)-32768;
  growl=f1_mix(growl,bits,(bar&3u)*30u);
 }else if(id==18){
  int32_t square=mulq15(growl,growl);
  growl=mulq15(growl,mulq15(square,square));
 }else{
  int32_t carrier=sine_i(phase*(1u+((step^(step>>3|step>>6))&3u)));
  growl=softclip(growl+mulq15(carrier,sine_i(phase*8u))*3);
 }
 return sub/2+mulq15(growl,env)/2;
}
/* Four percussion equation families share a bounded event age, but not a fixed
 * recorded hit or a single common drum timbre. Morph evaluates only A/B. */
/* (BYTE 0.16, the owner: "the drums not always the same; DRUMS an amount whose formula meets the funcbeat's, each drum
 * taking a little of it, coherent") a state its own family and tuning (not its domain's four), and src, the funcbeat's
 * sample now, in each drum by dx (Q15): the kick's phase bent by it, the snare's and the hat's noise the formula with
 * random signs (its loudness, the noise's brightness) */
static uint8_t f1_dx[2],f1_kit[2],f1_grv[2];            /* (DRUMS page: FUSION Q7, KIT, GROOVE; a core's; FORM 3's f1_view) */
/* (BYTE 0.16) KIT BIT: the first drums, 8-bit, a state's family and tuning */
static __attribute__((noinline)) int32_t f1_drum_bit(uint32_t state,uint32_t lane,uint32_t age,uint32_t tick,uint32_t samples,int32_t color,uint32_t seed,int32_t src,int32_t dx){
 uint32_t dom=F1_STATE[state].domain,family=((dom==1u?3u:dom==2u?2u:dom==3u?0u:1u)+state*3u)&3u,tun=(state*5u)%7u;
 uint32_t duration=lane==0?20480u:lane==1?24576u:4096u+(uint32_t)color*48u;
 int32_t env=fvm_music_env((int32_t)age,duration);if(!env)return 0;
 uint32_t ns=(uint32_t)(umul64(age,samples)>>16),phase;
 int32_t noise=(int32_t)(fvm_hash(tick+seed*977u+state*1297u)>>16)-32768,y,fx=mulq15(src,dx);
 noise+=mulq15((noise<0?-fx:fx)-noise,dx);               /* (the noise: the formula's, its signs at random) */
 if(lane==0){
  /* 12-turn reciprocal sweep: 32-bit division, bounded to <1/16384 turn
   * quantization, replacing the costly 64-bit division in a generic helper. */
  uint32_t sweep=(ns*49152u/(ns+900u))<<18;
  phase=ns*pitch_inc((uint32_t)(29+(int32_t)tun+color/32)*16u)+sweep+(uint32_t)(fx<<14);   /* (its phase bent by the formula) */
  if(family==0)y=sine_i(phase);
  else if(family==1)y=sine_i(phase+(uint32_t)(sine_i(phase*3u)*(color+8)*96));
  else if(family==2)y=(sine_i(phase)*3+f1_tri(phase))/4;
  else y=softclip(f1_tri(phase)*(1+color/32));
 }else if(lane==1){
  phase=ns*pitch_inc((uint32_t)(47+(int32_t)tun*2+color/16)*16u);
  if(family==0)y=noise*3/5+sine_i(phase)/3;
  else if(family==1){int32_t clap=age<8192u?32767-(int32_t)(age&2047u)*16:32767-(int32_t)(age-8192u);y=mulq15(noise,clap)*3/4+sine_i(phase)/5;}
  else if(family==2)y=(mulq15(sine_i(phase*7u),sine_i(phase*11u))+noise)/2;
  else{uint32_t z=ns*(13u+(uint32_t)color/8u);y=((int32_t)((z^(z>>3)^(z>>7))&255u)-128)*192+noise/4;}
 }else{
  if(family==0)y=noise/2;
  else if(family==1)y=(noise+mulq15(sine_i(tick*193827485u),sine_i(tick*317829103u)))/3;
  else if(family==2)y=(sine_i(tick*193827485u)+sine_i(tick*317829103u)+sine_i(tick*231847011u))/4;
  else y=((int32_t)(((ns*31u)^(ns>>2)^(ns*17u))&255u)-128)*128;
  y+=mulq15(noise-y,dx>>1);                              /* (the formula's noise: its signs at random, bright) */
 }
 return mulq15(clamp(y,-32767,32767),env);
}
/* (BYTE 0.16, the owner: "the drums lack power, very 8-bit; floatbeat can do much more") the kits 808 909 FLOAT, full
 * resolution: the kick a sine falling (a reciprocal sweep, 32-bit) to its sub, saturated, 909's click; the snare two
 * sines and the noise (FLOAT: a clap, three bursts and the tail); the hats six squares (the metal) and the noise. Their
 * time from the hit in samples, each envelope (T/(ns+T))^2: no division but one a drum. The formula in them by dx
 * (FUSION): the kick's phase bent by it, the noise made of it. A state its tuning (tun) */
typedef struct{uint16_t fe,q,tau,td,dr;} f1_kick_t;   /* end Hz, sweep turns Q14, its tau, decay T, drive Q4 */
static const f1_kick_t F1_KICK[3]={{46,53000,1300,2600,22},{55,40000,500,1500,30},{41,62000,1500,3500,40}};   /* (BYTE 0.16: decay T a quarter, its envelope ^4: over by the next step, ~250 ms 808, 150 909) */
static __attribute__((noinline)) int32_t f1_drum(uint32_t kit,uint32_t state,uint32_t lane,uint32_t age,uint32_t tick,uint32_t samples,int32_t color,uint32_t seed,int32_t src,int32_t dx){
 if(kit>=3u)return f1_drum_bit(state,lane,age,tick,samples,color,seed,src,dx);
 uint32_t ns=(uint32_t)(umul64(age,samples)>>16),tun=(state*5u)%7u;
 int32_t noise=(int32_t)(fvm_hash(tick+seed*977u+state*1297u)>>16)-32768,y,fx=mulq15(src,dx),e;
 noise+=mulq15((noise<0?-fx:fx)-noise,dx);               /* (the noise: the formula's, its signs at random) */
 if(ns>65535u)return 0;
 if(lane==0){
  const f1_kick_t*K=&F1_KICK[kit];
  uint32_t T=K->td+(uint32_t)color*8u;if(ns>T*6u)return 0;
  uint32_t r=(T<<15)/(ns+T);e=(int32_t)((r*r)>>15);e=(int32_t)(((int64_t)e*e)>>15);
  uint32_t ph=ns*((uint32_t)(K->fe+tun*2u)*97391u)+((ns*K->q/(ns+K->tau))<<18)+(uint32_t)(fx<<14);
  y=sine_i(ph);
  if(kit==1u&&ns<120u)y+=(int32_t)(120u-ns)*noise/160;   /* (909: the click) */
  y=softclip((y*K->dr*45/32)>>4);   /* (BYTE 0.16: +3 dB into its drive, the shorter kick's punch) */
 }else if(lane==1){
  uint32_t T=kit==1u?2600u:kit==2u?4200u:3400u,r=(T<<15)/(ns+T),rb=(1100u<<15)/(ns+1100u);
  if(ns>T*10u)return 0;
  e=(int32_t)((r*r)>>15);int32_t eb=(int32_t)((rb*rb)>>15);
  uint32_t f0=(kit==1u?196u:180u)+tun*6u,ph=ns*(f0*97391u)+((ns*6000u/(ns+400u))<<18);
  int32_t body=(sine_i(ph)+sine_i(ph*11u/6u)/2)*2/3;
  if(kit==2u){                                            /* (FLOAT: the clap: three bursts, ~10 ms apart, then the tail) */
   uint32_t b=ns<1350u?ns%450u:ns-1350u;int32_t ce=ns<1350u?32767-(int32_t)b*60:(int32_t)((((3000u<<15)/(b+3000u))*((3000u<<15)/(b+3000u)))>>15);
   y=mulq15(noise,clamp(ce,0,32767))+mulq15(body,eb)/4;e=32767;
  }else y=mulq15(body,eb)*(kit==1u?2:3)/4+mulq15(noise,e)*(kit==1u?3:2)/4,e=32767;
  y=softclip((y*24)>>4);
 }else{
  uint32_t T=kit==2u?2400u+(uint32_t)color*40u:900u+(uint32_t)color*12u,r=(T<<15)/(ns+T);
  if(ns>T*10u)return 0;
  e=(int32_t)((r*r)>>15);
  static const uint16_t HF[6]={2053,3044,3696,5227,5400,8000};   /* (the metal: 808's six, x10 Hz) */
  int32_t m=0;for(uint32_t k=0;k<6u;k++)m+=((int32_t)(ns*((uint32_t)HF[k]*9739u))>>31)|1;
  y=m*3000+noise*(kit==1u?2:1)/3;
  y=y*2/3;
 }
 return mulq15(clamp(y,-32767,32767),e);
}

static uint32_t f1_rot(uint32_t mask,uint32_t r){r&=15u;return ((mask<<r)|(mask>>(16u-r)))&65535u;}
static uint32_t f1_pattern(uint32_t mask,uint32_t groove,uint32_t seed,uint32_t cycle){
 switch(groove){case 1:mask=0x4925u;break;case 2:mask^=0x0444u;break;case 3:mask|=0x1249u;break;case 4:{uint32_t r=0;for(uint32_t k=0;k<16;k++)r=(r<<1)|((mask>>k)&1u);mask=r;break;}case 5:mask&=0x5555u;break;case 6:mask|=f1_rot(mask,1);break;case 7:mask=f1_rot(mask,cycle&7u);break;}
 return f1_rot(mask,seed&15u);
}
/* (BYTE 0.16) a state its own groove: the common four-on-the-floor's kick a push of its own, the snare a ghost by chance,
 * the hats one of eight figures (the downbeat kept) */
static uint32_t f1_vary(uint32_t mask,uint32_t lane,uint32_t state){
 static const uint16_t KADD[8]={0,0x0080u,0x0400u,0x2000u,0x0020u,0x8000u,0x0200u,0x0008u},HAT[8]={0,0xAAAAu,0xFFFFu,0xEEEEu,0xB6DBu,0x5555u,0xDDDDu,0xA5A5u};
 uint32_t h=fvm_hash(state*7919u+lane*31u+1u)>>8;
 if(!mask)return 0;
 if(lane==1u)return mask|KADD[h&7u];
 if(lane==2u)return mask|((h>>3)&1u?1u<<(((h>>4)&3u)*4u+3u):0u);
 return HAT[(h>>6)&7u]?HAT[(h>>6)&7u]:mask;
}
/* (BYTE 0.16) GROOVE: the state's own (0), FOUR, BREAK, SPARSE: kick, snare, hats (a bit a step, the downbeat bit 0) */
static const uint16_t F1_GRV[3][3]={{0x1111u,0x1010u,0xAAAAu},{0x0405u,0x9090u,0xFFFFu},{0x0101u,0x1000u,0x4444u}};
/* Return the age and continuously weighted onset of the latest event in a bar.
 * A/B masks coexist at intermediate weights; density adds/removes events with
 * a short continuous amplitude margin, not random toggles per audio block. */
static int32_t f1_event(uint32_t maskA,uint32_t maskB,uint32_t step,uint32_t frac,uint32_t morph,uint32_t density,uint32_t seed,int32_t*level){
 for(uint32_t back=0;back<16u;back++){
  uint32_t at=(step-back)&15u,aa=(maskA>>at)&1u,bb=(maskB>>at)&1u;
  int32_t amp=f1_mix(aa?32767:0,bb?32767:0,morph);
  uint32_t rank=(fvm_hash(at+seed*131u)>>24)&127u;
  int32_t gate=density?clamp((int32_t)density-(int32_t)rank+16,0,32)*1024:0;
  amp=mulq15(amp,gate>32767?32767:gate);
  if(amp>0){*level=amp;return (int32_t)((back<<16)+frac);}
 }
 *level=0;return 0;
}
static int32_t f1_section(uint32_t state,uint32_t phase,uint32_t layer,uint32_t evolve){
 /* phase is 0..65535 over CYCLE. Distinct arrangements follow the references. */
 uint32_t part=phase>>13,on=1;
 switch(state){
 case 0:on=layer==2?part>=1&&part<6:1;break;
 case 2:on=layer==1?(part==2||part==3||part==5):layer==2?part>=1&&part<6:1;break;
 case 3:case 9:on=layer==0;break;
 case 4:on=layer==1?part>=2:1;break;
 case 5:on=layer==0||part%3u!=2;break;
 case 7:on=layer!=2||part>=4;break;
 case 11:on=layer!=1||(part&1u)==0;break;
 case 12:on=layer!=0||part!=6;break;
 case 15:on=layer==0||part>=2u*layer;break;
 case 17:on=layer==0||part>=layer+1u;break;
 case 18:on=layer!=0||(part&3u)!=3;break;
 case 19:on=layer==2?1:part>=2u+layer*2u;break;
 case 20:on=layer!=2||part>=2;break;
 case 27:on=layer==0||part>=2u*layer;break;            /* (FORM 2, ref 6) BUILD: the chords from section 2, the drums from 4 */
 default:on=state>=F1_N1||layer!=1||part<6;break;
 }
 return f1_mix(32767,on?32767:0,evolve);
}
static uint32_t f1_pitch(const track_t*t,int32_t note){return pitch_inc((uint32_t)clamp(scale_snap(t,note),24,96)*16u);}

/* ---- (FORM 2) the harmony by degrees, the melodies' genes, the character bass (docs/FORM-REFERENCIAS.md) ---- */
typedef struct{int8_t d[12];int32_t n;} f1_scale_t;
static void f1_scale(const track_t*t,f1_scale_t*sc)  /* the part's scale as degrees (chromatic or few: natural minor) */
{
 uint32_t m=scale_mask(t)&0xFFFu;
 if(m==0xFFFu||__builtin_popcount(m)<5)m=0x5ADu;
 sc->n=0;for(uint32_t k=0;k<12u;k++)if((m>>k)&1u)sc->d[sc->n++]=(int8_t)k;
}
static int32_t f1_deg(const f1_scale_t*sc,int32_t d){int32_t o=(d>=0?d:d-sc->n+1)/sc->n;return sc->d[d-o*sc->n]+12*o;}   /* degree -> semitones */
static int32_t f1_degof(const f1_scale_t*sc,int32_t semi){int32_t o=(semi>=0?semi:semi-11)/12,r=semi-o*12,k=0;while(k+1<sc->n&&sc->d[k+1]<=r)k++;return k+o*sc->n;}
/* the progressions (degrees of a 7-note scale; another scale: its own degrees): i-VI-III-VII (ref 2), ii-V-I, i-iv vamp,
 * the bass down, by fourths, i-VI-iv-V, I-V-vi-IV, i-III-VI-ii */
static const int8_t F1_FAM[8][4]={{0,5,2,6},{1,4,0,0},{0,3,0,3},{0,6,5,4},{0,3,6,2},{0,5,3,4},{0,4,5,3},{0,2,5,1}};
static const int8_t F1_ARP[8]={0,2,4,7,-1,9,-1,4};      /* ref 11: the arpeggio with its rests */
static const int8_t F1_JUST[8]={0,0,2,4,0,0,2,1};       /* ref 3: 1 1 1.2 1.5 .. 1.1 as degrees */
static const int8_t F1_JUMP[8]={0,0,12,0,7,0,12,19};    /* ref 12: the bass's octaves and fifths */
typedef struct{uint32_t mi,bi,ci[4],cb;int16_t mn,cn[4];uint8_t rest,acc,slide;} f1_step_t;   /* (mn, cn: the notes, the screen's) */
/* a step's notes: S the state the melody comes from, R the one the progression comes from (the composing MORPH picks
 * them), pp the progression's place (a section pair), ix the melody's. Classic (the first 22 as they were): the rows'
 * strings in semitones; the other harmonies: chords stacked in the scale's degrees, the melody on the chord */
static __attribute__((noinline)) void f1_step(const track_t*t,uint32_t sid,uint32_t rid,uint32_t step,uint32_t sectionn,uint32_t cyc,uint32_t seed,uint32_t mutation,int32_t root,f1_step_t*o)
{
 const f1_state_t*S=&F1_STATE[sid],*R=&F1_STATE[rid];const f1_gene_t*G=&F1_GENE[sid],*H=&F1_GENE[rid];
 f1_scale_t sc;f1_scale(t,&sc);
 uint32_t hm=F1P(P_AORDER)?(uint32_t)clamp(F1P(P_AORDER)-1,0,4):F1P(P_AGATE)>0&&H->harm==F1H_CLASSIC?F1H_FAMILY:H->harm,   /* (a PROGRESS chosen
                                                          * on HARMONY AUTO: a classic state played by its family) */
cv=(uint32_t)clamp(F1P(P_CVOI),0,7),ix=step+seed+mutation,pp=sectionn/2u;
 int32_t md=0,mel,bass,sk[4],chord=clamp(F1P(P_CHRD),0,11);o->rest=0;
 switch(G->mel){                                          /* the melody: semitones (classic string) or degrees */
 case F1M_ARP:{int32_t a=F1_ARP[(step+(((step>>2)&3u)^((step>>3)&1u))+seed)&7u];if(a<0)o->rest=1;else md=a;break;}
 case F1M_PACK:{uint32_t nb=(G->pack>>(((step+seed+mutation)&7u)*4u))&15u;if(nb==15u)o->rest=1;else md=(int32_t)nb;break;}
 case F1M_BITS:{uint32_t X=step+cyc*16u+seed;md=(int32_t)((((X*((X&7u)+1u))^(X>>3|X>>6)))%8u);break;}
 case F1M_JUST:md=F1_JUST[(step+seed)&7u];break;
 default:md=(int32_t)f1_char(S->melody,ix);if(hm!=F1H_CLASSIC)md=f1_degof(&sc,md);break;
 }
 if(hm==F1H_CLASSIC){                                      /* the first 22's way: the rows' strings, intervals in semitones */
  int32_t rr=(int32_t)f1_char(R->roots,pp),third=chord==4?2:chord==5||chord==11?5:chord==6||chord==8||chord==9?4:3,top=chord==2||chord==8||chord==10?10:chord==9?11:chord==3?14:12;
  int32_t ints[4]={0,third,7,top};if(cv==5u){ints[1]=5;ints[2]=10;ints[3]=15;}else if(cv==7u){ints[1]=7;ints[2]=14;ints[3]=16;}
  mel=root+12+(G->mel==F1M_STR?md:f1_deg(&sc,md));bass=root+rr-12;
  for(uint32_t k=0;k<4u;k++)sk[k]=root+rr+ints[k];
 }else{
  int32_t rd,bd;uint32_t f=F1P(P_AGATE)>0?(uint32_t)clamp(F1P(P_AGATE)-1,0,7):(H->fam+seed/16u)&7u;   /* PROGRESS, else the state's and SEED's */
  if(hm==F1H_DEGREES)rd=f1_degof(&sc,(int32_t)f1_char(R->roots,pp));
  else if(hm==F1H_BITS){uint32_t b=cyc*4u+pp;rd=(int32_t)(((b^(b>>3))+3u+seed)%(uint32_t)sc.n);}   /* ref 10 */
  else rd=F1_FAM[f][pp&3u];
  if(mutation&4u)rd+=2;else if((mutation&2u)&&(pp&1u))rd+=5;   /* EVOLVE: a substitution (the relative, a fourth on) */
  bd=hm==F1H_PEDAL?0:rd;
  static const int8_t EXT[4][4]={{0,2,4,7},{0,2,4,6},{2,4,6,8},{0,3,4,7}};   /* triad, 7th, 9th (rootless), sus */
  const int8_t*e=EXT[chord<=2?0:chord<=5?1:chord<=8?2:3];
  int8_t q[4]={e[0],e[1],e[2],e[3]};if(e==EXT[0])q[3]=(int8_t)sc.n;
  if(cv==5u){q[0]=0;q[1]=3;q[2]=6;q[3]=9;}else if(cv==7u){q[0]=0;q[1]=4;q[2]=8;q[3]=11;}   /* QUARTAL; WIDE (ref 9) */
  mel=root+12+f1_deg(&sc,rd+md);bass=root+f1_deg(&sc,bd)-12;
  for(uint32_t k=0;k<4u;k++)sk[k]=root+f1_deg(&sc,rd+q[k]);
 }
 mel+=(clamp(F1P(P_AOCT),0,3)-1)*12;                    /* OCTAVE: the melody's */
 for(uint32_t k=0;k<4u;k++){                              /* the voicing: 0..3 as ever; DROP; NEAR: every tone to the
                                                           * octave above the root (the next chord's common tones stay) */
  int32_t sh=cv==2u?(k&1u?12:0):cv==3u&&k==0?-12:cv==1u&&(sectionn&1u)&&k==3?-12:cv==4u?(k==0?-12:k==2?12:0):0;
  if(cv==6u){int32_t d=sk[k]-root-1;d=((d%12)+12)%12;sk[k]=root+1+d;}
  o->cn[k]=(int16_t)clamp(scale_snap(t,sk[k]+sh),24,96);o->ci[k]=f1_pitch(t,sk[k]+sh);
 }
 o->mn=(int16_t)clamp(scale_snap(t,mel),24,96);o->mi=f1_pitch(t,mel);o->bi=f1_pitch(t,bass);
 {uint32_t h=fvm_hash(step*2654435761u^cyc*977u^seed);   /* the character bass: ACID the melody two octaves down, its
                                                           * accents and slides (ref 5); the others the jumps (ref 12) */
  o->acc=(uint8_t)((h>>8)&1u);o->slide=(uint8_t)(((h>>9)&3u)==0u);
  o->cb=f1_pitch(t,F1_STATE[sid].domain==F1D_ACID||(uint32_t)clamp(F1P(P_CAOS),0,10)==F1D_ACID?mel-24:bass+F1_JUMP[(step+seed)&7u]-12);}
}
/* the character bass (DOMAIN 5..10), one a voice: its own phase (ph[1]) and pitch (ph[2]: a slide glides), a filter's two
 * states packed in s[7]. lph the wobble's phase (WOBBLE: its rate), tone TONE, env the step's, acc its accent */
static __attribute__((noinline)) int32_t f1_cbass(f1_vs_t*v,uint32_t dom,uint32_t tick,uint32_t lph,uint32_t step,int32_t env,uint32_t tone,uint32_t acc,uint32_t hs)
{
 uint32_t inc=v->ph[2],u=v->ph[1]+=inc*hs;
 int32_t l=sine_i(lph);l=clamp(l*3/2,-32767,32767);int32_t lu=(l+32767)>>1,y=0,sub=sine_i(u);   /* (the wobble: trapezoid, ref 12) */
 int32_t f1=(int16_t)(v->s[7]&0xFFFF),f2=(int16_t)(v->s[7]>>16);
 switch(dom){
 case F1D_WOBBLE:{int32_t c=clamp((300+mulq15(lu,2500+(int32_t)tone*90))*(int32_t)hs,0,30000);   /* a saw, two poles opening */
  f1+=mulq15(osc_saw(u,inc)-f1,c);f2+=mulq15(f1-f2,c);y=f2*2+sub/2;break;}
 case F1D_REESE:{uint32_t d=(inc>>9)*(1u+tone/16u);int32_t c=clamp((1200+mulq15(lu,3000))*(int32_t)hs,0,30000);   /* detuned saws, phasing */
  int32_t x=(osc_saw(u,inc)+osc_saw(u+tick*d,inc+d)+osc_saw(u-tick*d,inc-d))/3;f1+=mulq15(x-f1,c);y=f1*2+sub/2;break;}
 case F1D_GROWL:{int32_t l2=sine_i(lph+0x40000000u);l2=(clamp(l2*3/2,-32767,32767)+32767)>>1;   /* ref 12: two formants synced */
  uint32_t r1=384u+(uint32_t)((lu*(256+(int32_t)tone*8))>>15),r2=2048u+(uint32_t)((l2*(1024+(int32_t)tone*48))>>15);
  int32_t a=sine_i((uint32_t)(((uint64_t)u*r1)>>8)),b=sine_i((uint32_t)(((uint64_t)u*r2)>>8)),w=32767-(int32_t)(u>>17);
  y=mulq15(a+b/2,w)+sub/2;break;}
 case F1D_RING:{int32_t l2=mulq15(l,l),l5=mulq15(mulq15(l2,l2),l);uint32_t k=1u+((step^(step>>3|step>>7))&7u);   /* ref 4 */
  int32_t c=sine_i(u*(6u+tone/16u)),c3=mulq15(mulq15(c,c),c);y=mulq15(mulq15(l5,sine_i(u*k)),c3)*4+sub/2;break;}
 case F1D_SCREECH:{int32_t ix=4000+mulq15(lu,30000+(int32_t)tone*200);y=sine_i(u+(uint32_t)(sine_i(u*3u)*ix))/2+sub/2;break;}   /* FM, high */
 default:{int32_t f=clamp((1500+mulq15(env,9000+(int32_t)tone*100)+(int32_t)acc*3000)*(int32_t)hs,0,20000);   /* ACID (ref 5): a
   * resonant state variable on a saw, its envelope a step's, the accent; the squelch a soft clip of the band */
  int32_t damp=6000+(127-(int32_t)tone)*150,x=osc_saw(u,inc)/2;f1+=mulq15(f,f2);int32_t hp=x-f1-mulq15(damp,f2);f2+=mulq15(f,hp);
  f1=clamp(f1,-32767,32767);f2=clamp(f2,-32767,32767);y=f1+mulq15(softclip(f2*(2+(int32_t)acc*2)),12000);break;}
 }
 v->s[7]=(int32_t)((uint32_t)(uint16_t)f1|((uint32_t)(uint16_t)f2<<16));
 return clamp(y,-32767,32767);
}
static __attribute__((noinline)) void f1_life(track_t*t,f1_vs_t*v,uint32_t a,uint32_t b,uint32_t morph,int32_t color,uint32_t inc,uint32_t tick,int32_t*y,uint32_t n,uint32_t hs){
 if(!fl.on)return;
 uint32_t id=fl_ctx[MC_CPU()&1u].id;if(!id)return;
 fl_node_t*node=&fl.node[fl.rd^1u][id-1u];
 if(!node->valid){int32_t cycle[FVM_BLK];for(uint32_t i=0;i<FVM_BLK;i++)cycle[i]=f1_mix(f1_wave(a,i,0x100000000ull/FVM_BLK,0,color,0),f1_wave(b,i,0x100000000ull/FVM_BLK,0,color,0),morph);node->valid=1;node->note=v->note;node->envelope=32767;node->has_number=1;node->number=(uint16_t)(color*64);node->n=1;fl_project(node,cycle);}
 static fvm_ins_t in={FO_PUSH,FM_ARR,0,FA_XF};static const fvm_prog_t p={.ins=&in,.cap=1,.n=1,.depth=1,.ty=FT_FIX};
 for(uint32_t off=0;off<n;off+=FVM_BLK){uint32_t count=n-off<FVM_BLK?n-off:FVM_BLK,W[FVM_BLK],X[FVM_BLK],O[FVM_BLK];for(uint32_t i=0;i<count;i++){W[i]=((tick+(off+i)*hs)*inc)>>16;X[i]=(uint32_t)(y[off+i]*2);}fvm_env_t e={{0},{0},0};e.arr[FA_W]=W;e.arr[FA_XF]=X;fl_run(&p,&e,&lofi_work[MC_CPU()&1u].s,count,O);for(uint32_t i=0;i<count;i++)y[off+i]=fvm_sample(FT_FIX,O[i]);}
 (void)t;
}
static volatile uint8_t load_tight;              /* (voice.c: the overload guard tripped) */
/* (BYTE 0.16, the owner: "a few formulas still harsh") the measured harsh states' low-pass (256 - its coefficient, Q8;
 * 0 none): 27 BITPACK, 06 SWITCH, 16 MASK SONG (a third of their energy over 4 kHz), 11 BIT MORPH, 15 BIT GROOVE */
static const uint8_t F1_SOFT[F1_N]={[5]=150,[10]=90,[14]=90,[15]=150,[26]=150};
static __attribute__((noinline)) void f1_render(track_t*t,f1_vs_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 if(n>CTL)n=CTL;if(!n)return;
 /* (byte-cpu-light) at half the rate (voice.c voice_half, .half 2): n is CTL / 2 (or / 4) and each sample is hs on.
  * FORM's time is its tick (phases tick*inc, the song's clock, the ages): the tick steps hs, the one-poles' coefs hs times */
 uint32_t hs=1u<<VH_SH();
 uint32_t a=(uint32_t)clamp(F1P(P_E0),0,F1_N-1),b=(uint32_t)clamp(F1P(P_E1),0,F1_N-1),seed=(uint32_t)clamp(F1P(P_E6),0,127),groove=(uint32_t)clamp(F1P(P_E4),0,7),density=(uint32_t)clamp(F1P(P_E5),0,127),evolve=(uint32_t)clamp(F1P(P_E7),0,127),domain=(uint32_t)clamp(F1P(P_CAOS),0,10);
 const f1_state_t*A=&F1_STATE[a],*B=&F1_STATE[b];
 uint32_t samples=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300),tick=v->ph[0],pos,end;
 if(func_song(t)){pos=func_clock_begin;end=func_clock_end;}else{pos=(uint32_t)(((uint64_t)tick<<16)/samples);end=(uint32_t)(((uint64_t)(tick+n*hs)<<16)/samples);}
 uint32_t dpos=(end-pos)/n,rem=(end-pos)%n,carry=0;
 static const uint8_t RATE_N[]={1,2,4,8,3,6,1,1,1,1},RATE_D[]={4,4,4,4,4,4,8,16,32,64};uint32_t rate=(uint32_t)clamp(F1P(P_SDIV),0,16),cycle=(uint32_t)clamp(F1P(P_SLEN),1,64),last=0xffffffffu,mi=0,bi=0,ci[4]={0};
 int32_t tonal[CTL],drums[CTL];uint32_t lastinc=pitch_inc((uint32_t)clamp(v->note,24,96)*16u);
 v->s[5]+=(F1P(P_E2)*256-v->s[5])/4;v->s[6]+=(F1P(P_E3)*256-v->s[6])/4;
 uint32_t morph=(uint32_t)clamp(v->s[5]>>8,0,127);int32_t color=clamp((v->s[6]>>8)+(m->shape>>8)-64,0,127);
 uint32_t am=a,bm=b;if(morph==127)am=b;if(morph==0)bm=a;
 /* (byte-cpu-light) The overload guard tripped (voice.c load_tight): one state, the nearer, not both. A SONG's FORM is
  * one held voice the guard never sheds, and A to B is twice its work: a jump to the nearer state, not a reset. */
 if(load_tight){if(morph<64u)bm=a;else am=b;}
 uint32_t masks[2][4],cached_cycle=~0u,rateq=(morph<64u?A->rate:B->rate)*127u;   /* (BYTE 0.16, the owner: "FORM out of time")
                                                         * the nearer state's rate, whole: A at 4 and B at 1 made 2.5 steps a
                                                         * beat at the middle, off the grid */
 if(rate>=10u){static const uint16_t RQ[7]={8128,4064,2032,1016,508,254,127};rateq=RQ[rate-10u];rate=7;}   /* (BYTE 0.16) RATE
                                                         * absolute: 4 .. 1/16 steps a beat (127 x 16 a step a beat at 1/16) */
 int32_t dxq=(int32_t)f1_dx[MC_CPU()&1u]*192,fus=(int32_t)f1_dx[MC_CPU()&1u];   /* (FUSION: the formula's share in the drums, to
                                                         * ~75 %; the drums' in the formula) */
 uint32_t kit=f1_kit[MC_CPU()&1u]&3u,grv=f1_grv[MC_CPU()&1u]&3u,kq=0;
 int32_t wm=(int32_t)(morph*(127u-morph))>>5,gq=0;            /* (the MORPH's depth, most at the middle) */
 int32_t fk=0,fc=fus?(v->senv*fus)>>16:0;                      /* (FUSION, a block's: the kick's push, the snare's COLOR) */
 if(fus){uint32_t k0=lastinc?0x7FFFFFFFu/((lastinc>>1)|1u):0u;fk=(int32_t)(((int64_t)v->kenv*fus*(int32_t)k0)>>24);}
 int32_t soft=(int32_t)(F1_SOFT[am]*(127u-morph)+F1_SOFT[bm]*morph)/127;   /* (SOFT: the harsh states' low-pass, 0 none) */
 uint32_t event_step=~0u,drum_step=~0u;int32_t event_age=0,event_level=0,drum_age[3]={0},drum_level[3]={0};
 int32_t pitchroot=m->pitch16/16+F1P(P_ROOT)-12;
 int32_t swing=clamp(F1P(P_SSWING),0,100)*160;uint32_t split=32768u+(uint32_t)swing;
 uint32_t recip1=(uint32_t)(0x80000000ull/split),recip2=(uint32_t)(0x80000000ull/(65536u-split));
 uint32_t mcyc=~0u,msec=~0u,mutation=0;int32_t sec0=0,sec1=0,sec2=0;uint32_t cd[4]={0};
 /* (byte-cpu-light) no division by a variable a sample (the FM-1's are dear): the cycle's position kept on (pm, cyc),
  * the section by a reciprocal (exact: pm < 2^22), RATE_D a shift, the age's /rateq and the chord's /count by
  * reciprocals, the drums' level a block's */
 static const uint8_t RATE_S[]={2,2,2,2,2,2,3,4,5,6};static const int16_t RCOUNT[5]={0,16384,8192,5461,4096};
 uint32_t cl=cycle<<16,pm=pos%cl,cyq=pos/cl,rcy=(uint32_t)(0xFFFFFFFFu/cycle)+1u,rag=(127u<<16)/(rateq?rateq:1u);
 int32_t dmq=f1_mix(A->drums,B->drums,morph)*clamp(F1P(P_KEY),0,127)/127*258;   /* (x/127 as x*258>>15) */
 /* (FORM 2) the genes a block: the mix (sidechain, ratchets) and the form (FILL, RISER) of the states sounding; the
  * character bass's domain (the nearer state's with DOMAIN AUTO); the GENES page (DRIVE, WOBBLE, TONE) */
 static const char RATCH[]="11111111111144222211111122224488";   /* (ref 5: a hit in 2, 4, 8) */
 uint32_t mixf=F1_GENE[am].mix|(am!=bm?F1_GENE[bm].mix:0u),fill=F1_GENE[am].form==F1F_FILL||F1_GENE[bm].form==F1F_FILL,riser=F1_GENE[am].form==F1F_RISER||F1_GENE[bm].form==F1F_RISER;
 uint32_t cdom=f1_dom(domain,F1_STATE[morph<64u?a:b].domain),tone=(uint32_t)clamp(F1P(P_APROB),0,127),wob=(uint32_t)clamp(F1P(P_ARATE),0,7),drive=(uint32_t)clamp(F1P(P_ASWING),0,100);
 uint32_t mrest=0,cacc=0,cslide=0,cbt=0,clr=32;
 /* (BYTE 0.16, CPU) a side's constants out of the sample loop; its bass and chords every other sample (their next
  * value a sample ahead, the one between the two's mean: a linear half rate, no state across blocks), the lead full */
 uint32_t sdom[2],svo[2],scnt[2];int32_t slc[2],bpe[2]={0},bne[2]={0},cpe[2]={0},cne[2]={0};
 for(uint32_t side=0;side<2u;side++){uint32_t id=side?bm:am;sdom[side]=f1_dom(domain,F1_STATE[id].domain);slc[side]=clamp(color+fc+(int32_t)F1_STATE[id].color-64,0,127);
  svo[side]=F1_STATE[id].voices;scnt[side]=F1P(P_CHRD)?(uint32_t)clamp((int32_t)svo[side],1,4):1u;}
 for(uint32_t i=0;i<n;i++,tick+=hs){
  uint32_t beat=(pm*RATE_N[rate])>>RATE_S[rate],section=(uint32_t)(((uint64_t)pm*rcy)>>32);
  uint32_t cyc=cyq,swingpos=beat&65535u;
  if(cached_cycle!=cyc){for(uint32_t side=0;side<2;side++){const f1_state_t*st=side?B:A;uint32_t raw[4]={st->gate,st->kick,st->snare,st->hat};for(uint32_t k=0;k<4;k++)masks[side][k]=f1_pattern(!k?raw[k]:grv?F1_GRV[grv-1u][k-1u]:f1_vary(raw[k],k,side?bm:am),grv?0u:groove,grv?0u:seed,cyc);}cached_cycle=cyc;event_step=drum_step=~0u;}
  if(swingpos<split)swingpos=(swingpos*recip1)>>16;else swingpos=32768u+(((swingpos-split)*recip2)>>16);beat=(beat&0xffff0000u)|swingpos;
  uint32_t clock=(uint32_t)((beat*rateq)/127u),step=clock>>16,frac=clock&65535u,sectionn=section>>13;
  if(cyc!=mcyc||sectionn!=msec){   /* (byte-cpu-light) the mutation and the three layers' sections: once a section */
   mutation=(fvm_hash(seed+cyc*37u+sectionn*7u)>>(28u-evolve/43u))&7u;mutation=mutation*evolve/127u;
   sec0=f1_mix(f1_section(a,section,0,evolve),f1_section(b,section,0,evolve),morph);sec1=f1_mix(f1_section(a,section,1,evolve),f1_section(b,section,1,evolve),morph);
   sec2=f1_mix(f1_section(a,section,2,evolve),f1_section(b,section,2,evolve),morph);mcyc=cyc;msec=sectionn;
  }
  if(step!=last){
   /* (FORM 2) the MORPH composes: the step's melody from A or B, the section's progression from A or B, by chance
    * (MORPH the odds of B; 0 and 127 the one state, as ever) */
   uint32_t hm=fvm_hash(step*2654435761u^cyc*977u^seed)>>25,hp=fvm_hash(sectionn*40503u^cyc*131u^seed)>>25;
   uint32_t sid=morph==127u||(morph&&hm<morph)?b:a,rid=morph==127u||(morph&&hp<morph)?b:a;
   f1_step_t fs;f1_step(t,sid,rid,step,sectionn,cyc,seed,mutation,pitchroot,&fs);
   mi=fs.mi;bi=fs.bi;kq=mi?0x7FFFFFFFu/((mi>>1)|1u):0u;gq=(int32_t)(((int64_t)wm*(int32_t)kq)>>12);   /* (the morph's depth, a step's) */for(uint32_t k=0;k<4u;k++)ci[k]=fs.ci[k];
   for(uint32_t k=0;k<4u;k++)cd[k]=ci[k]+(ci[k]>>15)*(uint32_t)(k*F1P(P_DETUNE)/8);   /* (byte-cpu-light: once a step) */
   mrest=fs.rest;cacc=fs.acc;cslide=fs.slide;cbt=fs.cb;if(!cslide||!v->ph[2])v->ph[2]=cbt;
   clr=wob==0?8u:wob==1?16u:wob==2?32u:wob==3?48u:wob==4?64u:wob==5?(uint32_t)"\x10\x20\x40\x30"[(step^(step>>3|step>>6))&3u]:
       wob==6?(uint32_t)"\x10\x20\x30\x40\x60\x80"[fvm_hash(step+seed)%6u]:4u;   /* the wobble's rate, Q4 a quarter (STEPS, JUMPS: ref 4) */
   last=step;lastinc=mi;
  }
  uint32_t dstep=beat>>14,dfrac=(beat&16383u)<<2;
  if(step!=event_step){event_age=f1_event(masks[0][0],masks[1][0],step,0,morph,density,seed,&event_level);event_step=step;}
  int32_t age=(int32_t)(((uint64_t)((uint32_t)event_age+frac)*rag)>>16),env=fvm_music_env(age,65536u);env=mrest?0:mulq15(env,event_level);
  int32_t cb=0;
  if(cdom>=F1D_WOBBLE){                                   /* (FORM 2) the character bass, once a sample */
   int32_t ce=32767-(int32_t)(frac>>1);
   if(cslide)v->ph[2]+=(uint32_t)(((int32_t)(cbt-v->ph[2]))>>5);
   cb=f1_cbass(v,cdom,tick,(beat*clr)<<12,step,ce,tone,cacc,hs);
   cb=cdom==F1D_ACID?(mrest?0:mulq15(cb,8192+(ce*3>>2))):mulq15(cb,32767-(int32_t)(frac>>4));
  }
  int32_t sources[2]={0};uint32_t ids[2]={am,bm};
  int32_t chenv=fvm_music_env((int32_t)(beat&0x3ffffu),4u*65536u);   /* (the chords' envelope: both sides', once a sample) */
  /* (FORM 3) the MORPH of the formulas, not of the level: each side's phase moved by the other's last sample (B on A
   * as MORPH goes up, A on B as it comes down; half a turn at most), its equation bent into a timbre of both */
  /* (BYTE 0.16, the owner: "the MORPH's middle is noise, whatever the pair") A and B each other's modulator, a sample
   * late, fed back: chaos. Now one way: A dry, B's phase moved by A's sample now (the same note: harmonics only), most
   * at the middle (morph (127 - morph), a quarter turn at most), none at the ends. FUSION: the kick's hit pushes both
   * sides' phase, the snare's their COLOR */
  for(uint32_t side=0;side<(am==bm?1u:2u);side++){
   uint32_t id=ids[side],dom=sdom[side],mt=pt_time(tick,2u)+(uint32_t)fk,ii=mi;   /* (the white punch-ins: its time) */
   if(side)mt+=(uint32_t)((sources[0]*gq)>>11);
   int32_t localcolor=slc[side];
   uint32_t voices=svo[side],count=scnt[side];
   int32_t lead=f1_wave(id,mt,ii,(uint32_t)age,localcolor,seed),bass,ch;
   if(i&1u||!i){uint32_t m2=i?mt+hs:mt;int32_t b2=id>=18&&id<F1_N1?f1_drop_bass(id,beat,m2*(bi/2u),localcolor):f1_wave(id,m2,bi,beat&65535u,localcolor/2,seed),c2=0;
    if(voices>1){for(uint32_t k=0;k<count;k++)c2+=f1_wave(id,m2,cd[k],beat&0x3ffffu,localcolor,seed);c2=(c2*RCOUNT[count])>>14;}
    if(i){bne[side]=b2;cne[side]=c2;bass=(bpe[side]+b2)>>1;ch=(cpe[side]+c2)>>1;}else{bpe[side]=bass=b2;cpe[side]=ch=c2;}}
   else{bpe[side]=bass=bne[side];cpe[side]=ch=cne[side];}
   int32_t x=mulq15(mulq15(lead,env),sec0)/2+mulq15(mulq15(ch,chenv),sec1)/2;
   if(voices>1&&(id<18||id>=F1_N1)&&dom<F1D_WOBBLE)x+=mulq15(bass,32767-(int32_t)(beat&65535u)/2)/3;
   if(dom==1){uint32_t mask=255u-(uint32_t)localcolor;int32_t bits=((x+32768)>>8)&255;x=((bits^(int32_t)((step+seed)&mask))*256-32768);}
   else if(dom==2)x=(x+sine_i(tick*mi)/3)*3/4;
   else if(dom==4){int32_t bits=(int32_t)(((tick*(mi>>16))^(tick>>5))&255u)*256-32768;x=(x*3+mulq15(bits,lead))/4;}
   if(id>=18&&id<F1_N1)x=x/2+bass;else if(dom>=F1D_WOBBLE)x=x/2+cb;
   x=soft_knee(x,26000);uint32_t slot=side;
   if(dom==3||id==4||id==12||id==17){int32_t fb=v->s[2+slot]/4;int32_t u=soft_knee(x+fb,26000),coef=clamp((2500+localcolor*120)*(int32_t)hs,0,32000);v->s[slot]+=mulq15(u-v->s[slot],coef);v->s[2+slot]=v->s[slot];x=id==12?(x-v->s[slot])/2:v->s[slot];}
   sources[side]=x;
  }
  if(am==bm)sources[1]=sources[0];
  {int32_t w=(int32_t)morph,e=w*(127-w)/48;                 /* (the mix: equal power, ~+3 dB at the middle, no dip) */
   tonal[i]=(f1_mix(sources[0],sources[1],morph)*(127+e))>>7;}   /* (no clamp: soft_knee below; its bound folded was a ROM-looking constant) */
  if(drive)tonal[i]=softclip((tonal[i]*(16+(int32_t)drive*2))>>4);
  if(fus&&v->kenv)tonal[i]=mulq15(tonal[i],32767-((v->kenv*fus*155)>>15));   /* (FUSION: the kick pumps it) */
  if(soft){v->lp+=((tonal[i]-v->lp)*(256-soft))>>8;tonal[i]=v->lp;}   /* (BYTE 0.16) SOFT */   /* (FORM 2) DRIVE: the sum (the sub inside it) */
  int32_t ds=0;
  for(uint32_t lane=0;lane<3;lane++){
   if(dstep!=drum_step)drum_age[lane]=f1_event(masks[0][lane+1],masks[1][lane+1],dstep,0,morph,density,seed+lane*19u,&drum_level[lane]);
   int32_t dl=drum_level[lane],da=(drum_age[lane]+(int32_t)dfrac)>>2;
   if(dl&&da<16384&&(mixf&F1X_RATCH||fill)){              /* (FORM 2) ratchets (ref 5), the fill (ref 8): the hit again in its step */
    uint32_t dv=(mixf&F1X_RATCH)&&lane!=1u?(uint32_t)(RATCH[(dstep+cyc)&31u]-'0'):1u;
    if(fill&&sectionn==7u&&(dstep&15u)>=12u)dv=lane==2u?4u:lane==1u?2u:dv;
    if(dv>1u)da&=(int32_t)((16384u>>(dv==2u?1u:dv==4u?2u:3u))-1u);
   }
   if(!dmq&&!fus)dl=0;                                    /* (LEVEL and FUSION 0: no drum to make) */
   int32_t ha=dl?f1_drum(kit,kit<3u&&morph>=64u?bm:am,lane,(uint32_t)da,tick,samples,color,seed,tonal[i],dxq):0;
   int32_t hb=am==bm||kit<3u?ha:dl?f1_drum(kit,bm,lane,(uint32_t)da,tick,samples,color,seed,tonal[i],dxq):0;
   int32_t hit=f1_mix(ha,hb,morph);
   ds+=lane==0?mulq15(hit,dl):lane==1?mulq15(hit,dl)*13/8:mulq15(hit,dl);   /* (BYTE 0.16: snare +4 dB, hats +6: the three heard) */
   if(fus&&lane<2u){int32_t ev=dl&&da<32768?mulq15(32767-da,dl):0;if(lane)v->senv=(int16_t)ev;else v->kenv=(int16_t)ev;}   /* (FUSION: the hit's envelope, ~a half beat) */
  }
  drum_step=dstep;
  if(riser&&sectionn==7u){int32_t nz=(int32_t)(fvm_hash(tick)>>16)-32768;ds+=mulq15(nz,(int32_t)((section&8191u)<<2))>>1;}   /* (FORM 2, ref 7) */
  drums[i]=mulq15(mulq15(ds,sec2),dmq)*3;               /* (BYTE 0.16: as loud as the formula at LEVEL 127, soft_knee's) */
  if((mixf&F1X_SIDE)&&drum_level[0]){int32_t da0=(drum_age[0]+(int32_t)dfrac)>>2;tonal[i]=mulq15(tonal[i],9830+mulq15(clamp(da0>>1,0,32767),22937));}   /* (ref 2) */
  {uint32_t d=dpos;carry+=rem;if(carry>=n){carry-=n;d++;}pos+=d;pm+=d;while(pm>=cl){pm-=cl;cyq++;}}
 }
 f1_life(t,v,a,b,morph,color,lastinc,v->ph[0],tonal,n,hs);
 uint32_t dcs=9u-(uint32_t)VH_SH();   /* (the DC cut, 512 samples at 44.1 kHz: a shift, not two divisions a sample) */
 for(uint32_t i=0;i<n;i++){int32_t x=soft_knee((tonal[i]+drums[i])>>1,16000);v->s[4]+=(x-v->s[4])>>dcs;x-=v->s[4];out[i]=clamp(x,-32767,32767);}   /* (FORM 3 mixes and levels it; BYTE 0.16: 6 dB down and a knee that tops at
                                                                     * 32383, the old one at 28000 went past full scale and clipped hard) */
 v->ph[0]=tick;
}
