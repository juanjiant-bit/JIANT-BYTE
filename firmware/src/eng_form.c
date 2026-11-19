/* SPDX-License-Identifier: GPL-3.0-only
 * FORM: bounded native adaptations of the 22 user-supplied reference equations.
 * Source/credits: docs/references/bytebeat-floatbeat-funcbeat.txt.
 * Only selected A/B programs run. No recorded loops, JS heap or per-voice delay allocation. */
#define FORM_N 36u                       /* (FORM 2: 22 of the first references, 14 of Juan's: docs/FORM-REFERENCIAS.md) */
#define FORM_N1 22u                      /* the first 22: as they were */
static const char *const FORM_NAMES[]={"01 SCORE","02 TIME RULE","03 COUNTER","04 FIVE PART","05 TUNED MEM","06 SWITCH","07 BASS ECHO","08 DETUNE","09 RATIOS","10 NESTED FM","11 BIT MORPH","12 CRUSH CHD","13 FILTER CHD","14 SWEEPS","15 BIT GROOVE","16 MASK SONG","17 NONLIN CHD","18 GENERATIVE","19 SHAPED ARP","20 BUILD UP","21 BIT PAIRS","22 NESTED EQ",
 "23 STRUM 332","24 HOUSE M9","25 CHIRP","26 DRIFT","27 BITPACK","28 BUILD","29 RISER","30 FILLS","31 SUITE","32 DEGREES","33 NIBBLE","34 GROWL","35 WOBBLE","36 ACID"};
static const char *const FORM_GROOVE[]={"ORIGINAL","EUCLID","BREAKS","POLY","REVERSE","HALF","DOUBLE","ROTATE"};
static const char *const FORM_DOMAIN[]={"AUTO","BYTE","FLOAT","FUNC","HYBRID","WOBBLE","REESE","GROWL","RING","SCREECH","ACID"};
/* (FORM 2) the GENES page: HARMONY (P_AORDER: 0 the state's own), DRIVE (P_ASWING), WOBBLE (P_ARATE), TONE (P_APROB); FORM's
 * ARP slots (no arpeggio on FORM: its song plays itself). Their defaults (0, 0, 2, 127) are the classic sound */
static const char *const FORM_HARM[]={"AUTO","CLASSIC","DEGREES","FAMILY","PEDAL","BITS"};
static const char *const FORM_WOB[]={"1/2","1/4","1/8","1/8T","1/16","STEPS","JUMPS","BAR"};
static const char *const FORM_VOICE[]={"CLOSE","LEAD","OPEN","+BASS","DROP","QUARTAL","NEAR","WIDE"};
enum{FD_WOBBLE=5,FD_REESE,FD_GROWL,FD_RING,FD_SCREECH,FD_ACID};   /* DOMAIN 5..10: the character bass (form_cbass) */
static const char *const FORM_RUN[]={"NOTE","SONG"};
typedef struct {const char*melody,*roots;uint16_t kick,snare,hat,gate;uint8_t domain,rate,voices,drums,echo,space,structure,color;} form_state_t;
/* Each row keeps a different motif/clock, layer topology and mathematical voice;
 * state number also selects the corresponding equation topology in form_wave. */
static const form_state_t FORM_STATE[FORM_N]={
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
 /* (FORM 2) Juan's references (docs/FORM-REFERENCIAS.md): their genes in FORM_GENE */
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
 {"0000000000000000","0000",0x0401,0x0100,0x5555,0xffff,FD_GROWL,4,1,120,10,30,34,90},   /* 34 GROWL (ref 12) */
 {"0000000000000000","0000",0x0401,0x0100,0x5555,0xffff,FD_WOBBLE,4,1,120,10,30,35,90},  /* 35 WOBBLE (ref 4) */
 {"0000000000000000","0000",0x1111,0x1010,0x4444,0xffff,FD_ACID,4,1,110,30,20,36,90}     /* 36 ACID (ref 5) */
};
/* (FORM 2) each state's genes: 0 = as the first 22 were (their rows above, untouched). harm: the progression
 * (FH_*); fam: its family (FORM_FAM, SEED moves it); mel: the melody (FM_*: the row's string, an arpeggio with rests,
 * a packed sequence, bits, just ratios); form: FF_* (sections, BUILD, RISER, FILL); mix: FX_SIDE the kick's
 * sidechain, FX_RATCH the drums' ratchets; pack: 8 nibbles (degrees; 15 a rest) for FM_PACK and the bass jumps */
enum{FH_CLASSIC,FH_DEGREES,FH_FAMILY,FH_PEDAL,FH_BITS};
enum{FM_STR,FM_ARP,FM_PACK,FM_BITS,FM_JUST};
enum{FF_SECT,FF_BUILD,FF_RISER,FF_FILL};
#define FX_SIDE 1u
#define FX_RATCH 2u
typedef struct{uint8_t harm,fam,mel,form,mix;uint32_t pack;} form_gene_t;
static const form_gene_t FORM_GENE[FORM_N]={
 [22]={FH_FAMILY,0,FM_STR,FF_SECT,0,0},
 [23]={FH_FAMILY,0,FM_STR,FF_SECT,FX_SIDE,0},
 [24]={FH_DEGREES,6,FM_JUST,FF_SECT,0,0},
 [25]={FH_BITS,3,FM_BITS,FF_SECT,0,0},
 [26]={FH_PEDAL,2,FM_PACK,FF_SECT,FX_RATCH,0x40F2F40Fu},
 [27]={FH_FAMILY,4,FM_STR,FF_BUILD,FX_RATCH,0},
 [28]={FH_DEGREES,5,FM_STR,FF_RISER,0,0},
 [29]={FH_FAMILY,5,FM_ARP,FF_FILL,0,0},
 [30]={FH_FAMILY,6,FM_STR,FF_SECT,0,0},
 [31]={FH_BITS,1,FM_ARP,FF_SECT,0,0},
 [32]={FH_FAMILY,0,FM_ARP,FF_SECT,0,0},
 [33]={FH_PEDAL,2,FM_PACK,FF_SECT,FX_SIDE,0x00FF5FF0u},
 [34]={FH_PEDAL,0,FM_PACK,FF_SECT,FX_SIDE,0x0F7F0F00u},
 [35]={FH_FAMILY,2,FM_PACK,FF_SECT,FX_RATCH,0x7F2F04F0u},
};
static int form_owned_param(uint32_t id){return id==P_SDIV||id==P_SLEN||id==P_SSWING||id==P_ROOT||id==P_SCALE||id==P_CHRD||id==P_CVOI||id==P_AORDER||id==P_ASWING||id==P_ARATE||id==P_APROB;}   /* (FORM 2: its GENES) */
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t detune={"DETUNE",F_PCT,0,127,24,0};
 static const param_desc_t run={"RUN",F_ENUM,0,1,1,FORM_RUN},domain={"STYLE",F_ENUM,0,10,0,FORM_DOMAIN},drums={"DRUMS",F_PCT,0,127,100,0},cycle={"CYCLE",F_INT,1,64,32,0};
 static const param_desc_t harm={"HARMONY",F_ENUM,0,5,0,FORM_HARM},drive={"DRIVE",F_PCT,0,100,0,0},wob={"WOBBLE",F_ENUM,0,7,2,FORM_WOB},tone={"TONE",F_PCT,0,127,64,0},voice={"VOICING",F_ENUM,0,7,0,FORM_VOICE};
 return id==P_DETUNE?&detune:id==P_LRTRG?&run:id==P_CAOS?&domain:id==P_KEY?&drums:id==P_SLEN?&cycle:id==P_AORDER?&harm:id==P_ASWING?&drive:
        id==P_ARATE?&wob:id==P_APROB?&tone:id==P_CVOI?&voice:0;
}
#define FP(k,label,echo,space) {label,{k,(k+1)%FORM_N1,0,64,0,100,0,64},{0,0,127,24},0,TAG(FX),FX(0,24,echo,space),0,MODS(P_LRTRG,1,P_CAOS,0,P_KEY,100,P_SLEN,32,P_ROOT,0,P_SCALE,2,P_CHRD,1,P_CVOI,0,P_DETUNE,24,P_AMODE,0,P_AORDER,0,P_ASWING,0,P_ARATE,2,P_APROB,127)}
/* (FORM 2) k, B, label, sends; the scale, the chord (degree modes: 1-2 triad, 3-5 7th, 6-8 9th, 9-11 sus), the voicing,
 * DRIVE, WOBBLE, TONE */
#define FP2(k,b,label,echo,space,scale,chrd,cvoi,drive,wob,tone) {label,{k,b,0,64,0,100,0,64},{0,0,127,24},0,TAG(FX),FX(0,24,echo,space),0,MODS(P_LRTRG,1,P_CAOS,0,P_KEY,100,P_SLEN,32,P_ROOT,0,P_SCALE,scale,P_CHRD,chrd,P_CVOI,cvoi,P_DETUNE,24,P_AMODE,0,P_AORDER,0,P_ASWING,drive,P_ARATE,wob,P_APROB,tone)}
static const preset_t FORM_PRESETS[]={
 FP(0,"01 SCORE",10,20),FP(1,"02 TIME RULE",0,5),FP(2,"03 COUNTER",72,45),FP(3,"04 FIVE PART",40,25),FP(4,"05 TUNED MEM",100,60),FP(5,"06 SWITCH",0,8),FP(6,"07 BASS ECHO",96,20),FP(7,"08 DETUNE",112,60),FP(8,"09 RATIOS",18,15),FP(9,"10 NESTED FM",32,40),FP(10,"11 BIT MORPH",14,5),FP(11,"12 CRUSH CHD",110,25),FP(12,"13 FILTER CHD",100,55),FP(13,"14 SWEEPS",5,5),FP(14,"15 BIT GROOVE",0,8),FP(15,"16 MASK SONG",24,12),FP(16,"17 NONLIN CHD",58,48),FP(17,"18 GENERATIVE",127,90),FP(18,"19 SHAPED ARP",50,35),FP(19,"20 BUILD UP",30,22),FP(20,"21 BIT PAIRS",52,30),FP(21,"22 NESTED EQ",28,28),
 FP2(22,23,"23 STRUM 332",60,40,2,4,6,20,2,64),FP2(23,30,"24 HOUSE M9",40,30,2,7,4,0,2,64),FP2(24,31,"25 CHIRP",30,40,2,1,0,0,2,64),
 FP2(25,24,"26 DRIFT",70,80,2,4,6,0,2,64),FP2(26,35,"27 BITPACK",10,10,2,1,0,30,4,90),FP2(27,22,"28 BUILD",30,30,2,4,6,0,2,64),
 FP2(28,27,"29 RISER",20,20,2,4,0,10,2,64),FP2(29,26,"30 FILLS",10,10,2,1,0,20,4,64),FP2(30,23,"31 SUITE",45,50,2,7,7,0,2,64),
 FP2(31,22,"32 DEGREES",50,60,2,4,6,0,2,64),FP2(32,28,"33 NIBBLE",30,30,2,1,0,10,2,64),FP2(33,34,"34 GROWL",10,30,2,1,0,60,5,70),
 FP2(34,33,"35 WOBBLE",10,30,2,1,0,70,6,60),FP2(35,26,"36 ACID",30,20,2,1,0,30,4,100)};
#undef FP
static int32_t scale_snap(const track_t*,int32_t); /* seq.c: shared musical scale, not a second tuning table */
static int32_t form_mix(int32_t a,int32_t b,uint32_t w){return a+(b-a)*(int32_t)w/127;}
static uint32_t form_char(const char*s,uint32_t i){uint32_t c=(uint8_t)s[i%str_len(s)];return c>='a'?c-'a'+10u:c-'0';}
static int32_t form_sin(uint32_t tick,uint32_t inc,uint32_t num,uint32_t den){return sine_i(tick*(inc/den*num));}
static int32_t form_tri(uint32_t ph){int32_t x=(int32_t)(ph>>16);return x<32768?x*2-32767:98303-x*2;}
/* Equation topologies, indexed by the reference number. Integer and nonlinear
 * paths are deliberate timbres; singular divisions use a finite denominator. */
static __attribute__((noinline)) int32_t form_wave(uint32_t state,uint32_t tick,uint32_t inc,uint32_t age,int32_t color,uint32_t seed)
{
 uint32_t ph=tick*inc,bt=ph>>22,c=(uint32_t)clamp(color,0,127),slow=tick>>11;
 int32_t s=0,q=0,n=0,y=0;   /* (byte-cpu-light) the sine, triangle and noise only for the states that read them */
 if(state==11u||state==18u||state==21u)s=sine_i(ph);
 if(state==1u||state==9u||state==16u||state==18u||state==21u)q=form_tri(ph);
 if(state==21u)n=(int32_t)(fvm_hash(tick+seed*977u)>>16)-32768;
 switch(state){
 case 0:y=sine_i(ph+(uint32_t)(form_sin(tick,inc,2,1)*(int32_t)(c+16)*160));break;
 case 1:y=(q+(int32_t)(((bt^(bt>>3))&255u)*256u)-32768)/2;break;
 case 2:y=(sine_i(ph+(uint32_t)(form_sin(tick,inc,3,2)*(int32_t)c*96))+form_sin(tick,inc,1003,1000))/2;break;
 case 3:for(uint32_t k=1;k<=9;k+=2){int32_t e=32767-(int32_t)((slow/k)&2047u)*16;y+=mulq15(form_sin(tick,inc,k,1),e)*(int32_t)(k==1?127u:16u+c)/381;}y=soft_knee(y,24000);break;
 case 4:y=(osc_saw(ph,inc)+osc_pulse(tick*(inc/1000u*1003u),inc,0x40000000u+c*0x800000u))/2;break;
 case 5:y=(int32_t)((((bt*(3u+(slow>>4&3u)))^(bt>>(1u+(slow&3u))))&255u)*256u)-32768;break;
 case 6:y=(osc_saw(ph,inc)+osc_saw(tick*(inc/10000u*(9999u-c)),inc)+form_sin(tick,inc,1,2))/3;break;
 case 7:y=(osc_saw(ph,inc)+osc_saw(tick*(inc/10000u*(10001u+c)),inc)+osc_saw(tick*(inc/10000u*(9999u-c)),inc)+form_sin(tick,inc,3,2))/4;break;
 case 8:{static const uint8_t nr[]={1,4,3,1,1},dr[]={1,5,4,2,4};uint32_t k=(slow>>5)%8u;if(k>4u)k=8u-k;y=form_mix(form_sin(tick,inc,nr[k],dr[k]),form_tri(tick*(inc/5u*4u)),c);break;}
 case 9:{int32_t l=sine_i(tick*110349u);y=sine_i(ph+(uint32_t)(mulq15(q+(form_sin(tick,inc,8,1)/5),16384+l/2)*(int32_t)(c+4)*180));break;}
 case 10:y=(int32_t)((((bt&(63u+c))^(bt*(1u+(slow&3u))))&255u)*256u)-32768;break;
 case 11:y=softclip(s*(2+(int32_t)c/16));y=(int32_t)((uint32_t)y&~((1u<<(1u+(slow>>4&3u)))-1u));break;
 case 12:y=(osc_pulse(ph,inc,0x20000000u+c*0xa00000u)+form_sin(tick,inc,1,2))/2;break;
 case 13:y=sine_i(ph+(0x60000000u/(1u+(age>>7)))*(256u+c*24u));break;
 case 14:y=(int32_t)((((bt*(1u+c/16u+(slow>>3&7u)))>>(1u+(slow&3u)))|(bt>>2))&255u)*256-32768;break;
 case 15:y=(int32_t)(((bt*(1u+c/16u+(slow>>6&2u))*(1u+(slow>>2&3u)))^(bt>>(slow&7u)))&255u)*256-32768;break;
 case 16:{int32_t a=mulq15(q,q),b=mulq15(a,a);y=sine_i((uint32_t)mulq15(b,q)*(c+16u)*600u);break;}
 case 17:y=softclip((form_tri(ph)+form_tri(tick*(inc/3u*2u)))*(1+(int32_t)c/32));break;
 case 18:{int32_t a=mulq15(s,s);y=softclip(mulq15(a,a)*(2+(int32_t)c/12)-8192+q/3);break;}
 case 19:y=(int32_t)((((bt*(1u+(slow&3u)))^(bt>>2))&255u)*256u)-32768;y=(y+form_tri(tick*(inc/10000u*(10001u+c))))/2;break;
 case 20:y=(int32_t)((((bt*(bt>>3|(129u+c/2u)))&128u)^((bt>>2)*(bt>>5|53u)&191u))*256u)-32768;break;
 /* (FORM 2) the references' timbres (docs/FORM-REFERENCIAS.md): a sample's work each about one of the above */
 case 22:y=softclip(sine_i(ph)*(1+(int32_t)((tick>>13)&3u))+(int32_t)c*64);break;                /* ref 1: tanh, its drive breathing */
 case 23:{uint32_t w=((tick>>9)&31u)|(c>>3);y=(int32_t)(((ph>>27)&w)<<11)-32768;y=(y*3+sine_i(ph))/4;break;}   /* ref 2: the saw's bits masked, the mask sweeping */
 case 24:{uint32_t u=ph>>16,q2=(u*(131072u-u))>>16;y=sine_i((q2*(3u+(c>>5)))<<16);break;}           /* ref 3: the phase's root (a chirp in the cycle) */
 case 25:y=sine_i(ph+(uint32_t)(sine_i(ph*2u+(uint32_t)(sine_i(tick*3u)<<12))*((int32_t)c+8)*64));break;   /* ref 4: FM fed back, drifting */
 case 26:{uint32_t u=ph>>21,a=slow&7u;y=(int32_t)((((u*(u>>9|1932555120u>>a))&128u)^(((u>>2)*(u>>11|58132275u>>a))&191u))*256u)-32768;break;}   /* ref 5: 8 bits */
 case 27:y=(form_tri(ph)+form_tri(tick*(inc+(inc>>13))))/2;break;                                  /* ref 6: two triangles phasing */
 case 28:y=(int32_t)((((bt*((slow&1u)?7u:5u))>>((slow>>1)&3u))&255u)*256u)-32768;y=(y+sine_i(ph))/2;break;   /* ref 7: the formula by section */
 case 29:y=(sine_i(ph)+sine_i(ph*3u)/3+sine_i(ph*((tick>>12&1u)?5u:7u))/(5-(int32_t)(c>>6)))*2/3;break;      /* ref 8: 5ths and 7ths */
 case 30:{int32_t x=(int32_t)(ph>>16)-32768,x2=mulq15(x,x),x4=mulq15(x2,x2),x8=mulq15(x4,x4),x9=mulq15(x8,x);   /* ref 9: the pluck (M%2-1)^9 */
  y=sine_i(((uint32_t)x9<<16)+(uint32_t)(sine_i(ph)<<15)+(uint32_t)c*0x1000000u);break;}
 case 31:y=form_mix(form_tri(ph),form_tri(ph*2u),c/2u);break;                                       /* ref 10: triangles, an octave */
 case 32:{int32_t a=sine_i(ph),b=a<0?-a:a,b2=mulq15(b,b),b4=mulq15(b2,b2);uint32_t k=(slow>>4)&3u;   /* ref 11: the pulse breathing */
  int32_t w=k==0?b:k==1?b2:k==2?b4:mulq15(b4,b4);y=w*2-32767+(int32_t)(((tick*((inc>>3)*(80u+(c>>3))/80u))>>16)&8191u)-4096;break;}
 case 33:case 34:case 35:y=(osc_saw(ph,inc)+sine_i(ph*2u))/2;break;                                 /* (the bass is the character: form_cbass) */
 default:{int32_t a=sine_i(ph*3u),b=mulq15(mulq15(s,s),s);y=softclip(mulq15(a,b)*(2+(int32_t)c/8)+q/3+n/16);break;}
 }
 return clamp(y,-32767,32767);
}
/* Bass phrases for references 19..22. beat is Q16 quarter notes.
 * Keep the fundamental below the equation layer so its gates can leave space. */
static __attribute__((noinline)) int32_t form_drop_bass(uint32_t id, uint32_t beat, uint32_t phase, int32_t color)
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
  growl=form_mix(growl,bits,(bar&3u)*30u);
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
static __attribute__((noinline)) int32_t form_drum(uint32_t state,uint32_t lane,uint32_t age,uint32_t tick,uint32_t samples,int32_t color,uint32_t seed){
 uint32_t dom=FORM_STATE[state].domain,family=dom==1u?3u:dom==2u?2u:dom==3u?0u:1u;
 uint32_t duration=lane==0?32768u:lane==1?24576u:4096u+(uint32_t)color*48u;
 int32_t env=fvm_music_env((int32_t)age,duration);if(!env)return 0;
 uint32_t ns=(uint32_t)(umul64(age,samples)>>16),phase;
 int32_t noise=(int32_t)(fvm_hash(tick+seed*977u+state*1297u)>>16)-32768,y;
 if(lane==0){
  /* 12-turn reciprocal sweep: 32-bit division, bounded to <1/16384 turn
   * quantization, replacing the costly 64-bit division in a generic helper. */
  uint32_t sweep=(ns*49152u/(ns+900u))<<18;
  phase=ns*pitch_inc((uint32_t)(31+color/32)*16u)+sweep;
  if(family==0)y=sine_i(phase);
  else if(family==1)y=sine_i(phase+(uint32_t)(sine_i(phase*3u)*(color+8)*96));
  else if(family==2)y=(sine_i(phase)*3+form_tri(phase))/4;
  else y=softclip(form_tri(phase)*(1+color/32));
 }else if(lane==1){
  phase=ns*pitch_inc((uint32_t)(50+color/16)*16u);
  if(family==0)y=noise*3/5+sine_i(phase)/3;
  else if(family==1){int32_t clap=age<8192u?32767-(int32_t)(age&2047u)*16:32767-(int32_t)(age-8192u);y=mulq15(noise,clap)*3/4+sine_i(phase)/5;}
  else if(family==2)y=(mulq15(sine_i(phase*7u),sine_i(phase*11u))+noise)/2;
  else{uint32_t z=ns*(13u+(uint32_t)color/8u);y=((int32_t)((z^(z>>3)^(z>>7))&255u)-128)*192+noise/4;}
 }else{
  if(family==0)y=noise/2;
  else if(family==1)y=(noise+mulq15(sine_i(tick*193827485u),sine_i(tick*317829103u)))/3;
  else if(family==2)y=(sine_i(tick*193827485u)+sine_i(tick*317829103u)+sine_i(tick*231847011u))/4;
  else y=((int32_t)(((ns*31u)^(ns>>2)^(ns*17u))&255u)-128)*128;
 }
 return mulq15(clamp(y,-32767,32767),env);
}

static uint32_t form_rot(uint32_t mask,uint32_t r){r&=15u;return ((mask<<r)|(mask>>(16u-r)))&65535u;}
static uint32_t form_pattern(uint32_t mask,uint32_t groove,uint32_t seed,uint32_t cycle){
 switch(groove){case 1:mask=0x4925u;break;case 2:mask^=0x0444u;break;case 3:mask|=0x1249u;break;case 4:{uint32_t r=0;for(uint32_t k=0;k<16;k++)r=(r<<1)|((mask>>k)&1u);mask=r;break;}case 5:mask&=0x5555u;break;case 6:mask|=form_rot(mask,1);break;case 7:mask=form_rot(mask,cycle&7u);break;}
 return form_rot(mask,seed&15u);
}
/* Return the age and continuously weighted onset of the latest event in a bar.
 * A/B masks coexist at intermediate weights; density adds/removes events with
 * a short continuous amplitude margin, not random toggles per audio block. */
static int32_t form_event(uint32_t maskA,uint32_t maskB,uint32_t step,uint32_t frac,uint32_t morph,uint32_t density,uint32_t seed,int32_t*level){
 for(uint32_t back=0;back<16u;back++){
  uint32_t at=(step-back)&15u,aa=(maskA>>at)&1u,bb=(maskB>>at)&1u;
  int32_t amp=form_mix(aa?32767:0,bb?32767:0,morph);
  uint32_t rank=(fvm_hash(at+seed*131u)>>24)&127u;
  int32_t gate=density?clamp((int32_t)density-(int32_t)rank+16,0,32)*1024:0;
  amp=mulq15(amp,gate>32767?32767:gate);
  if(amp>0){*level=amp;return (int32_t)((back<<16)+frac);}
 }
 *level=0;return 0;
}
static int32_t form_section(uint32_t state,uint32_t phase,uint32_t layer,uint32_t evolve){
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
 default:on=state>=FORM_N1||layer!=1||part<6;break;
 }
 return form_mix(32767,on?32767:0,evolve);
}
static uint32_t form_pitch(const track_t*t,int32_t note){return pitch_inc((uint32_t)clamp(scale_snap(t,note),24,96)*16u);}

/* ---- (FORM 2) the harmony by degrees, the melodies' genes, the character bass (docs/FORM-REFERENCIAS.md) ---- */
static uint32_t scale_mask(const track_t *t);           /* seq.c */
typedef struct{int8_t d[12];int32_t n;} form_scale_t;
static void form_scale(const track_t*t,form_scale_t*sc)  /* the part's scale as degrees (chromatic or few: natural minor) */
{
 uint32_t m=scale_mask(t)&0xFFFu;
 if(m==0xFFFu||__builtin_popcount(m)<5)m=0x5ADu;
 sc->n=0;for(uint32_t k=0;k<12u;k++)if((m>>k)&1u)sc->d[sc->n++]=(int8_t)k;
}
static int32_t form_deg(const form_scale_t*sc,int32_t d){int32_t o=(d>=0?d:d-sc->n+1)/sc->n;return sc->d[d-o*sc->n]+12*o;}   /* degree -> semitones */
static int32_t form_degof(const form_scale_t*sc,int32_t semi){int32_t o=(semi>=0?semi:semi-11)/12,r=semi-o*12,k=0;while(k+1<sc->n&&sc->d[k+1]<=r)k++;return k+o*sc->n;}
/* the progressions (degrees of a 7-note scale; another scale: its own degrees): i-VI-III-VII (ref 2), ii-V-I, i-iv vamp,
 * the bass down, by fourths, i-VI-iv-V, I-V-vi-IV, i-III-VI-ii */
static const int8_t FORM_FAM[8][4]={{0,5,2,6},{1,4,0,0},{0,3,0,3},{0,6,5,4},{0,3,6,2},{0,5,3,4},{0,4,5,3},{0,2,5,1}};
static const int8_t FORM_ARP[8]={0,2,4,7,-1,9,-1,4};      /* ref 11: the arpeggio with its rests */
static const int8_t FORM_JUST[8]={0,0,2,4,0,0,2,1};       /* ref 3: 1 1 1.2 1.5 .. 1.1 as degrees */
static const int8_t FORM_JUMP[8]={0,0,12,0,7,0,12,19};    /* ref 12: the bass's octaves and fifths */
typedef struct{uint32_t mi,bi,ci[4],cb;int16_t mn,cn[4];uint8_t rest,acc,slide;} form_step_t;   /* (mn, cn: the notes, the screen's) */
/* a step's notes: S the state the melody comes from, R the one the progression comes from (the composing MORPH picks
 * them), pp the progression's place (a section pair), ix the melody's. Classic (the first 22 as they were): the rows'
 * strings in semitones; the other harmonies: chords stacked in the scale's degrees, the melody on the chord */
static __attribute__((noinline)) void form_step(const track_t*t,uint32_t sid,uint32_t rid,uint32_t step,uint32_t sectionn,uint32_t cyc,uint32_t seed,uint32_t mutation,int32_t root,form_step_t*o)
{
 const form_state_t*S=&FORM_STATE[sid],*R=&FORM_STATE[rid];const form_gene_t*G=&FORM_GENE[sid],*H=&FORM_GENE[rid];
 form_scale_t sc;form_scale(t,&sc);
 uint32_t hm=t->p[P_AORDER]?(uint32_t)clamp(t->p[P_AORDER]-1,0,4):H->harm,cv=(uint32_t)clamp(t->p[P_CVOI],0,7),ix=step+seed+mutation,pp=sectionn/2u;
 int32_t md=0,mel,bass,sk[4],chord=clamp(t->p[P_CHRD],0,11);o->rest=0;
 switch(G->mel){                                          /* the melody: semitones (classic string) or degrees */
 case FM_ARP:{int32_t a=FORM_ARP[(step+(((step>>2)&3u)^((step>>3)&1u))+seed)&7u];if(a<0)o->rest=1;else md=a;break;}
 case FM_PACK:{uint32_t nb=(G->pack>>(((step+seed+mutation)&7u)*4u))&15u;if(nb==15u)o->rest=1;else md=(int32_t)nb;break;}
 case FM_BITS:{uint32_t X=step+cyc*16u+seed;md=(int32_t)((((X*((X&7u)+1u))^(X>>3|X>>6)))%8u);break;}
 case FM_JUST:md=FORM_JUST[(step+seed)&7u];break;
 default:md=(int32_t)form_char(S->melody,ix);if(hm!=FH_CLASSIC)md=form_degof(&sc,md);break;
 }
 if(hm==FH_CLASSIC){                                      /* the first 22's way: the rows' strings, intervals in semitones */
  int32_t rr=(int32_t)form_char(R->roots,pp),third=chord==4?2:chord==5||chord==11?5:chord==6||chord==8||chord==9?4:3,top=chord==2||chord==8||chord==10?10:chord==9?11:chord==3?14:12;
  int32_t ints[4]={0,third,7,top};if(cv==5u){ints[1]=5;ints[2]=10;ints[3]=15;}else if(cv==7u){ints[1]=7;ints[2]=14;ints[3]=16;}
  mel=root+12+(G->mel==FM_STR?md:form_deg(&sc,md));bass=root+rr-12;
  for(uint32_t k=0;k<4u;k++)sk[k]=root+rr+ints[k];
 }else{
  int32_t rd,bd;uint32_t f=(H->fam+seed/16u)&7u;
  if(hm==FH_DEGREES)rd=form_degof(&sc,(int32_t)form_char(R->roots,pp));
  else if(hm==FH_BITS){uint32_t b=cyc*4u+pp;rd=(int32_t)(((b^(b>>3))+3u+seed)%(uint32_t)sc.n);}   /* ref 10 */
  else rd=FORM_FAM[f][pp&3u];
  if(mutation&4u)rd+=2;else if((mutation&2u)&&(pp&1u))rd+=5;   /* EVOLVE: a substitution (the relative, a fourth on) */
  bd=hm==FH_PEDAL?0:rd;
  static const int8_t EXT[4][4]={{0,2,4,7},{0,2,4,6},{2,4,6,8},{0,3,4,7}};   /* triad, 7th, 9th (rootless), sus */
  const int8_t*e=EXT[chord<=2?0:chord<=5?1:chord<=8?2:3];
  int8_t q[4]={e[0],e[1],e[2],e[3]};if(e==EXT[0])q[3]=(int8_t)sc.n;
  if(cv==5u){q[0]=0;q[1]=3;q[2]=6;q[3]=9;}else if(cv==7u){q[0]=0;q[1]=4;q[2]=8;q[3]=11;}   /* QUARTAL; WIDE (ref 9) */
  mel=root+12+form_deg(&sc,rd+md);bass=root+form_deg(&sc,bd)-12;
  for(uint32_t k=0;k<4u;k++)sk[k]=root+form_deg(&sc,rd+q[k]);
 }
 for(uint32_t k=0;k<4u;k++){                              /* the voicing: 0..3 as ever; DROP; NEAR: every tone to the
                                                           * octave above the root (the next chord's common tones stay) */
  int32_t sh=cv==2u?(k&1u?12:0):cv==3u&&k==0?-12:cv==1u&&(sectionn&1u)&&k==3?-12:cv==4u?(k==0?-12:k==2?12:0):0;
  if(cv==6u){int32_t d=sk[k]-root-1;d=((d%12)+12)%12;sk[k]=root+1+d;}
  o->cn[k]=(int16_t)clamp(scale_snap(t,sk[k]+sh),24,96);o->ci[k]=form_pitch(t,sk[k]+sh);
 }
 o->mn=(int16_t)clamp(scale_snap(t,mel),24,96);o->mi=form_pitch(t,mel);o->bi=form_pitch(t,bass);
 {uint32_t h=fvm_hash(step*2654435761u^cyc*977u^seed);   /* the character bass: ACID the melody two octaves down, its
                                                           * accents and slides (ref 5); the others the jumps (ref 12) */
  o->acc=(uint8_t)((h>>8)&1u);o->slide=(uint8_t)(((h>>9)&3u)==0u);
  o->cb=form_pitch(t,FORM_STATE[sid].domain==FD_ACID||(uint32_t)clamp(t->p[P_CAOS],0,10)==FD_ACID?mel-24:bass+FORM_JUMP[(step+seed)&7u]-12);}
}
/* the character bass (DOMAIN 5..10), one a voice: its own phase (ph[1]) and pitch (ph[2]: a slide glides), a filter's two
 * states packed in s[7]. lph the wobble's phase (WOBBLE: its rate), tone TONE, env the step's, acc its accent */
static __attribute__((noinline)) int32_t form_cbass(voice_t*v,uint32_t dom,uint32_t tick,uint32_t lph,uint32_t step,int32_t env,uint32_t tone,uint32_t acc,uint32_t hs)
{
 uint32_t inc=v->ph[2],u=v->ph[1]+=inc*hs;
 int32_t l=sine_i(lph);l=clamp(l*3/2,-32767,32767);int32_t lu=(l+32767)>>1,y=0,sub=sine_i(u);   /* (the wobble: trapezoid, ref 12) */
 int32_t f1=(int16_t)(v->s[7]&0xFFFF),f2=(int16_t)(v->s[7]>>16);
 switch(dom){
 case FD_WOBBLE:{int32_t c=clamp((300+mulq15(lu,2500+(int32_t)tone*90))*(int32_t)hs,0,30000);   /* a saw, two poles opening */
  f1+=mulq15(osc_saw(u,inc)-f1,c);f2+=mulq15(f1-f2,c);y=f2*2+sub/2;break;}
 case FD_REESE:{uint32_t d=(inc>>9)*(1u+tone/16u);int32_t c=clamp((1200+mulq15(lu,3000))*(int32_t)hs,0,30000);   /* detuned saws, phasing */
  int32_t x=(osc_saw(u,inc)+osc_saw(u+tick*d,inc+d)+osc_saw(u-tick*d,inc-d))/3;f1+=mulq15(x-f1,c);y=f1*2+sub/2;break;}
 case FD_GROWL:{int32_t l2=sine_i(lph+0x40000000u);l2=(clamp(l2*3/2,-32767,32767)+32767)>>1;   /* ref 12: two formants synced */
  uint32_t r1=384u+(uint32_t)((lu*(256+(int32_t)tone*8))>>15),r2=2048u+(uint32_t)((l2*(1024+(int32_t)tone*48))>>15);
  int32_t a=sine_i((uint32_t)(((uint64_t)u*r1)>>8)),b=sine_i((uint32_t)(((uint64_t)u*r2)>>8)),w=32767-(int32_t)(u>>17);
  y=mulq15(a+b/2,w)+sub/2;break;}
 case FD_RING:{int32_t l2=mulq15(l,l),l5=mulq15(mulq15(l2,l2),l);uint32_t k=1u+((step^(step>>3|step>>7))&7u);   /* ref 4 */
  int32_t c=sine_i(u*(6u+tone/16u)),c3=mulq15(mulq15(c,c),c);y=mulq15(mulq15(l5,sine_i(u*k)),c3)*4+sub/2;break;}
 case FD_SCREECH:{int32_t ix=4000+mulq15(lu,30000+(int32_t)tone*200);y=sine_i(u+(uint32_t)(sine_i(u*3u)*ix))/2+sub/2;break;}   /* FM, high */
 default:{int32_t f=clamp((1500+mulq15(env,9000+(int32_t)tone*100)+(int32_t)acc*3000)*(int32_t)hs,0,20000);   /* ACID (ref 5): a
   * resonant state variable on a saw, its envelope a step's, the accent; the squelch a soft clip of the band */
  int32_t damp=6000+(127-(int32_t)tone)*150,x=osc_saw(u,inc)/2;f1+=mulq15(f,f2);int32_t hp=x-f1-mulq15(damp,f2);f2+=mulq15(f,hp);
  f1=clamp(f1,-32767,32767);f2=clamp(f2,-32767,32767);y=f1+mulq15(softclip(f2*(2+(int32_t)acc*2)),12000);break;}
 }
 v->s[7]=(int32_t)((uint32_t)(uint16_t)f1|((uint32_t)(uint16_t)f2<<16));
 return clamp(y,-32767,32767);
}
static uint32_t form_cost(const track_t*t){return t->p[P_E2]>0&&t->p[P_E2]<127&&t->p[P_E0]!=t->p[P_E1]?12u:7u;}
static void form_note_on(track_t*t,voice_t*v){memset(v->s,0,sizeof v->s);memset(v->ph,0,sizeof v->ph);v->s[5]=t->p[P_E2]*256;v->s[6]=t->p[P_E3]*256;}
static __attribute__((noinline)) void form_life(track_t*t,voice_t*v,uint32_t a,uint32_t b,uint32_t morph,int32_t color,uint32_t inc,uint32_t tick,int32_t*y,uint32_t n,uint32_t hs){
 if(!fl.on)return;
 uint32_t id=fl_ctx[MC_CPU()&1u].id;if(!id)return;
 fl_node_t*node=&fl.node[fl.rd^1u][id-1u];
 if(!node->valid){int32_t cycle[FVM_BLK];for(uint32_t i=0;i<FVM_BLK;i++)cycle[i]=form_mix(form_wave(a,i,0x100000000ull/FVM_BLK,0,color,0),form_wave(b,i,0x100000000ull/FVM_BLK,0,color,0),morph);node->valid=1;node->note=v->note;node->envelope=32767;node->has_number=1;node->number=(uint16_t)(color*64);node->n=1;fl_project(node,cycle);}
 static fvm_ins_t in={FO_PUSH,FM_ARR,0,FA_XF};static const fvm_prog_t p={.ins=&in,.cap=1,.n=1,.depth=1,.ty=FT_FIX};
 for(uint32_t off=0;off<n;off+=FVM_BLK){uint32_t count=n-off<FVM_BLK?n-off:FVM_BLK,W[FVM_BLK],X[FVM_BLK],O[FVM_BLK];for(uint32_t i=0;i<count;i++){W[i]=((tick+(off+i)*hs)*inc)>>16;X[i]=(uint32_t)(y[off+i]*2);}fvm_env_t e={{0},{0},0};e.arr[FA_W]=W;e.arr[FA_XF]=X;fl_run(&p,&e,&lofi_work[MC_CPU()&1u].s,count,O);for(uint32_t i=0;i<count;i++)y[off+i]=fvm_sample(FT_FIX,O[i]);}
 (void)t;
}
static volatile uint8_t load_tight;              /* (voice.c: the overload guard tripped) */
static __attribute__((noinline)) void form_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 if(n>CTL)n=CTL;if(!n)return;
 /* (byte-cpu-light) at half the rate (voice.c voice_half, .half 2): n is CTL / 2 (or / 4) and each sample is hs on.
  * FORM's time is its tick (phases tick*inc, the song's clock, the ages): the tick steps hs, the one-poles' coefs hs times */
 uint32_t hs=1u<<VH_SH();
 uint32_t a=(uint32_t)clamp(t->p[P_E0],0,FORM_N-1),b=(uint32_t)clamp(t->p[P_E1],0,FORM_N-1),seed=(uint32_t)clamp(t->p[P_E6],0,127),groove=(uint32_t)clamp(t->p[P_E4],0,7),density=(uint32_t)clamp(t->p[P_E5],0,127),evolve=(uint32_t)clamp(t->p[P_E7],0,127),domain=(uint32_t)clamp(t->p[P_CAOS],0,10);
 const form_state_t*A=&FORM_STATE[a],*B=&FORM_STATE[b];
 uint32_t samples=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300),tick=v->ph[0],pos,end;
 if(func_song(t)){pos=func_clock_begin;end=func_clock_end;}else{pos=(uint32_t)(((uint64_t)tick<<16)/samples);end=(uint32_t)(((uint64_t)(tick+n*hs)<<16)/samples);}
 uint32_t dpos=(end-pos)/n,rem=(end-pos)%n,carry=0;
 static const uint8_t RATE_N[]={1,2,4,8,3,6,1,1,1,1},RATE_D[]={4,4,4,4,4,4,8,16,32,64};uint32_t rate=(uint32_t)clamp(t->p[P_SDIV],0,9),cycle=(uint32_t)clamp(t->p[P_SLEN],1,64),last=0xffffffffu,mi=0,bi=0,ci[4]={0};
 int32_t tonal[CTL],drums[CTL];uint32_t lastinc=pitch_inc((uint32_t)clamp(v->note,24,96)*16u);
 v->s[5]+=(t->p[P_E2]*256-v->s[5])/4;v->s[6]+=(t->p[P_E3]*256-v->s[6])/4;
 uint32_t morph=(uint32_t)clamp(v->s[5]>>8,0,127);int32_t color=clamp((v->s[6]>>8)+(m->shape>>8)-64,0,127);
 uint32_t am=a,bm=b;if(morph==127)am=b;if(morph==0)bm=a;
 /* (byte-cpu-light) The overload guard tripped (voice.c load_tight): one state, the nearer, not both. A SONG's FORM is
  * one held voice the guard never sheds, and A to B is twice its work: a jump to the nearer state, not a reset. */
 if(load_tight){if(morph<64u)bm=a;else am=b;}
 uint32_t masks[2][4],cached_cycle=~0u,rateq=A->rate*(127u-morph)+B->rate*morph;
 uint32_t event_step=~0u,drum_step=~0u;int32_t event_age=0,event_level=0,drum_age[3]={0},drum_level[3]={0};
 int32_t pitchroot=m->pitch16/16+t->p[P_ROOT]-12;
 int32_t swing=clamp(t->p[P_SSWING],0,100)*160;uint32_t split=32768u+(uint32_t)swing;
 uint32_t recip1=(uint32_t)(0x80000000ull/split),recip2=(uint32_t)(0x80000000ull/(65536u-split));
 uint32_t mcyc=~0u,msec=~0u,mutation=0;int32_t sec0=0,sec1=0,sec2=0;uint32_t cd[4]={0};
 /* (byte-cpu-light) no division by a variable a sample (the FM-1's are dear): the cycle's position kept on (pm, cyc),
  * the section by a reciprocal (exact: pm < 2^22), RATE_D a shift, the age's /rateq and the chord's /count by
  * reciprocals, the drums' level a block's */
 static const uint8_t RATE_S[]={2,2,2,2,2,2,3,4,5,6};static const int16_t RCOUNT[5]={0,16384,8192,5461,4096};
 uint32_t cl=cycle<<16,pm=pos%cl,cyq=pos/cl,rcy=(uint32_t)(0xFFFFFFFFu/cycle)+1u,rag=(127u<<16)/(rateq?rateq:1u);
 int32_t dmq=form_mix(A->drums,B->drums,morph)*clamp(t->p[P_KEY],0,127)/127*258;   /* (x/127 as x*258>>15) */
 /* (FORM 2) the genes a block: the mix (sidechain, ratchets) and the form (FILL, RISER) of the states sounding; the
  * character bass's domain (the nearer state's with DOMAIN AUTO); the GENES page (DRIVE, WOBBLE, TONE) */
 static const char RATCH[]="11111111111144222211111122224488";   /* (ref 5: a hit in 2, 4, 8) */
 uint32_t mixf=FORM_GENE[am].mix|(am!=bm?FORM_GENE[bm].mix:0u),fill=FORM_GENE[am].form==FF_FILL||FORM_GENE[bm].form==FF_FILL,riser=FORM_GENE[am].form==FF_RISER||FORM_GENE[bm].form==FF_RISER;
 uint32_t cdom=domain?domain:FORM_STATE[morph<64u?a:b].domain,tone=(uint32_t)clamp(t->p[P_APROB],0,127),wob=(uint32_t)clamp(t->p[P_ARATE],0,7),drive=(uint32_t)clamp(t->p[P_ASWING],0,100);
 uint32_t mrest=0,cacc=0,cslide=0,cbt=0,clr=32;
 for(uint32_t i=0;i<n;i++,tick+=hs){
  uint32_t beat=(pm*RATE_N[rate])>>RATE_S[rate],section=(uint32_t)(((uint64_t)pm*rcy)>>32);
  uint32_t cyc=cyq,swingpos=beat&65535u;
  if(cached_cycle!=cyc){for(uint32_t side=0;side<2;side++){const form_state_t*st=side?B:A;uint32_t raw[4]={st->gate,st->kick,st->snare,st->hat};for(uint32_t k=0;k<4;k++)masks[side][k]=form_pattern(raw[k],groove,seed,cyc);}cached_cycle=cyc;event_step=drum_step=~0u;}
  if(swingpos<split)swingpos=(swingpos*recip1)>>16;else swingpos=32768u+(((swingpos-split)*recip2)>>16);beat=(beat&0xffff0000u)|swingpos;
  uint32_t clock=(uint32_t)((beat*rateq)/127u),step=clock>>16,frac=clock&65535u,sectionn=section>>13;
  if(cyc!=mcyc||sectionn!=msec){   /* (byte-cpu-light) the mutation and the three layers' sections: once a section */
   mutation=(fvm_hash(seed+cyc*37u+sectionn*7u)>>(28u-evolve/43u))&7u;mutation=mutation*evolve/127u;
   sec0=form_mix(form_section(a,section,0,evolve),form_section(b,section,0,evolve),morph);sec1=form_mix(form_section(a,section,1,evolve),form_section(b,section,1,evolve),morph);
   sec2=form_mix(form_section(a,section,2,evolve),form_section(b,section,2,evolve),morph);mcyc=cyc;msec=sectionn;
  }
  if(step!=last){
   /* (FORM 2) the MORPH composes: the step's melody from A or B, the section's progression from A or B, by chance
    * (MORPH the odds of B; 0 and 127 the one state, as ever) */
   uint32_t hm=fvm_hash(step*2654435761u^cyc*977u^seed)>>25,hp=fvm_hash(sectionn*40503u^cyc*131u^seed)>>25;
   uint32_t sid=morph==127u||(morph&&hm<morph)?b:a,rid=morph==127u||(morph&&hp<morph)?b:a;
   form_step_t fs;form_step(t,sid,rid,step,sectionn,cyc,seed,mutation,pitchroot,&fs);
   mi=fs.mi;bi=fs.bi;for(uint32_t k=0;k<4u;k++)ci[k]=fs.ci[k];
   for(uint32_t k=0;k<4u;k++)cd[k]=ci[k]+(ci[k]>>15)*(uint32_t)(k*t->p[P_DETUNE]/8);   /* (byte-cpu-light: once a step) */
   mrest=fs.rest;cacc=fs.acc;cslide=fs.slide;cbt=fs.cb;if(!cslide||!v->ph[2])v->ph[2]=cbt;
   clr=wob==0?8u:wob==1?16u:wob==2?32u:wob==3?48u:wob==4?64u:wob==5?(uint32_t)"\x10\x20\x40\x30"[(step^(step>>3|step>>6))&3u]:
       wob==6?(uint32_t)"\x10\x20\x30\x40\x60\x80"[fvm_hash(step+seed)%6u]:4u;   /* the wobble's rate, Q4 a quarter (STEPS, JUMPS: ref 4) */
   last=step;lastinc=mi;
  }
  uint32_t dstep=beat>>14,dfrac=(beat&16383u)<<2;
  if(step!=event_step){event_age=form_event(masks[0][0],masks[1][0],step,0,morph,density,seed,&event_level);event_step=step;}
  int32_t age=(int32_t)(((uint64_t)((uint32_t)event_age+frac)*rag)>>16),env=fvm_music_env(age,65536u);env=mrest?0:mulq15(env,event_level);
  int32_t cb=0;
  if(cdom>=FD_WOBBLE){                                   /* (FORM 2) the character bass, once a sample */
   int32_t ce=32767-(int32_t)(frac>>1);
   if(cslide)v->ph[2]+=(uint32_t)(((int32_t)(cbt-v->ph[2]))>>5);
   cb=form_cbass(v,cdom,tick,(beat*clr)<<12,step,ce,tone,cacc,hs);
   cb=cdom==FD_ACID?(mrest?0:mulq15(cb,8192+(ce*3>>2))):mulq15(cb,32767-(int32_t)(frac>>4));
  }
  int32_t sources[2]={0};uint32_t ids[2]={am,bm};
  for(uint32_t side=0;side<(am==bm?1u:2u);side++){
   uint32_t id=ids[side],dom=domain?domain:FORM_STATE[id].domain,mt=tick,ii=mi;
   int32_t localcolor=clamp(color+(int32_t)FORM_STATE[id].color-64,0,127);
   uint32_t voices=FORM_STATE[id].voices;
   int32_t lead=form_wave(id,mt,ii,(uint32_t)age,localcolor,seed),bass=id>=18&&id<FORM_N1?form_drop_bass(id,beat,mt*(bi/2u),localcolor):form_wave(id,mt,bi,beat&65535u,localcolor/2,seed),ch=0;
   uint32_t count=t->p[P_CHRD]? (uint32_t)clamp(voices,1,4):1u;
   if(voices>1){for(uint32_t k=0;k<count;k++)ch+=form_wave(id,mt,cd[k],beat&0x3ffffu,localcolor,seed);ch=(ch*RCOUNT[count])>>14;}
   int32_t chenv=fvm_music_env((int32_t)(beat&0x3ffffu),4u*65536u);
   int32_t x=mulq15(mulq15(lead,env),sec0)/2+mulq15(mulq15(ch,chenv),sec1)/2;
   if(voices>1&&(id<18||id>=FORM_N1)&&dom<FD_WOBBLE)x+=mulq15(bass,32767-(int32_t)(beat&65535u)/2)/3;
   if(dom==1){uint32_t mask=255u-(uint32_t)localcolor;int32_t bits=((x+32768)>>8)&255;x=((bits^(int32_t)((step+seed)&mask))*256-32768);}
   else if(dom==2)x=(x+sine_i(tick*mi)/3)*3/4;
   else if(dom==4){int32_t bits=(int32_t)(((tick*(mi>>16))^(tick>>5))&255u)*256-32768;x=(x*3+mulq15(bits,lead))/4;}
   if(id>=18&&id<FORM_N1)x=x/2+bass;else if(dom>=FD_WOBBLE)x=x/2+cb;
   x=soft_knee(x,26000);uint32_t slot=side;
   if(dom==3||id==4||id==12||id==17){int32_t fb=v->s[2+slot]/4;int32_t u=soft_knee(x+fb,26000),coef=clamp((2500+localcolor*120)*(int32_t)hs,0,32000);v->s[slot]+=mulq15(u-v->s[slot],coef);v->s[2+slot]=v->s[slot];x=id==12?(x-v->s[slot])/2:v->s[slot];}
   sources[side]=x;
  }
  if(am==bm)sources[1]=sources[0];tonal[i]=clamp(form_mix(sources[0],sources[1],morph),-32767,32767);
  if(drive)tonal[i]=softclip((tonal[i]*(16+(int32_t)drive*2))>>4);   /* (FORM 2) DRIVE: the sum (the sub inside it) */
  int32_t ds=0;
  for(uint32_t lane=0;lane<3;lane++){
   if(dstep!=drum_step)drum_age[lane]=form_event(masks[0][lane+1],masks[1][lane+1],dstep,0,morph,density,seed+lane*19u,&drum_level[lane]);
   int32_t dl=drum_level[lane],da=(drum_age[lane]+(int32_t)dfrac)>>2;
   if(dl&&da<16384&&(mixf&FX_RATCH||fill)){              /* (FORM 2) ratchets (ref 5), the fill (ref 8): the hit again in its step */
    uint32_t dv=(mixf&FX_RATCH)&&lane!=1u?(uint32_t)(RATCH[(dstep+cyc)&31u]-'0'):1u;
    if(fill&&sectionn==7u&&(dstep&15u)>=12u)dv=lane==2u?4u:lane==1u?2u:dv;
    if(dv>1u)da&=(int32_t)((16384u>>(dv==2u?1u:dv==4u?2u:3u))-1u);
   }
   int32_t ha=dl?form_drum(am,lane,(uint32_t)da,tick,samples,color,seed):0;
   int32_t hb=am==bm?ha:dl?form_drum(bm,lane,(uint32_t)da,tick,samples,color,seed):0;
   int32_t hit=form_mix(ha,hb,morph);
   ds+=mulq15(hit,dl)>>(lane==2);
  }
  drum_step=dstep;
  if(riser&&sectionn==7u){int32_t nz=(int32_t)(fvm_hash(tick)>>16)-32768;ds+=mulq15(nz,(int32_t)((section&8191u)<<2))>>1;}   /* (FORM 2, ref 7) */
  drums[i]=mulq15(mulq15(ds,sec2),dmq);
  if((mixf&FX_SIDE)&&drum_level[0]){int32_t da0=(drum_age[0]+(int32_t)dfrac)>>2;tonal[i]=mulq15(tonal[i],9830+mulq15(clamp(da0>>1,0,32767),22937));}   /* (ref 2) */
  {uint32_t d=dpos;carry+=rem;if(carry>=n){carry-=n;d++;}pos+=d;pm+=d;while(pm>=cl){pm-=cl;cyq++;}}
 }
 form_life(t,v,a,b,morph,color,lastinc,v->ph[0],tonal,n,hs);
 for(uint32_t i=0;i<n;i++){int32_t x=soft_knee(tonal[i]+drums[i],28000);v->s[4]+=(x-v->s[4])/(int32_t)(512u/hs);x-=v->s[4];out[i]+=voice_amp(clamp(x,-32767,32767),m,i);}
 v->ph[0]=tick;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"STATES","RULES"},
 .edit={{"STATE A",F_ENUM,0,FORM_N-1,0,FORM_NAMES},{"STATE B",F_ENUM,0,FORM_N-1,1,FORM_NAMES},{"MORPH",F_PCT,0,127,0,0},{"COLOR",F_PCT,0,127,64,0},{"GROOVE",F_ENUM,0,7,0,FORM_GROOVE},{"DENSITY",F_PCT,0,127,100,0},{"SEED",F_INT,0,127,0,0},{"EVOLVE",F_PCT,0,127,64,0}},
 .presets=FORM_PRESETS,.npresets=FORM_N,.knob={P_E0,P_E1,P_E2,P_E3},.poly=1,.half=2,.note_on=form_note_on,.render=form_render};
