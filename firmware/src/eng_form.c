/* SPDX-License-Identifier: GPL-3.0-only
 * FORM: bounded native adaptations of the 22 user-supplied reference equations.
 * Source/credits: docs/references/bytebeat-floatbeat-funcbeat.txt.
 * Only selected A/B programs run. No recorded loops, JS heap or per-voice delay allocation. */
#define FORM_N 22u
static const char *const FORM_NAMES[]={"01 SCORE","02 TIME RULE","03 COUNTER","04 FIVE PART","05 TUNED MEM","06 SWITCH","07 BASS ECHO","08 DETUNE","09 RATIOS","10 NESTED FM","11 BIT MORPH","12 CRUSH CHD","13 FILTER CHD","14 SWEEPS","15 BIT GROOVE","16 MASK SONG","17 NONLIN CHD","18 GENERATIVE","19 SHAPED ARP","20 BUILD UP","21 BIT PAIRS","22 NESTED EQ"};
static const char *const FORM_GROOVE[]={"ORIGINAL","EUCLID","BREAKS","POLY","REVERSE","HALF","DOUBLE","ROTATE"};
static const char *const FORM_DOMAIN[]={"AUTO","BYTE","FLOAT","FUNC","HYBRID"};
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
 {"03570a7503570a75","0530",0x1111,0x1010,0xaeee,0xbdbd,4,3,2,110,28,28,22,100}
};
static int form_owned_param(uint32_t id){return id==P_SDIV||id==P_SLEN||id==P_SSWING||id==P_ROOT||id==P_SCALE||id==P_CHRD||id==P_CVOI;}
static const param_desc_t *form_common_desc(uint32_t id){
 static const param_desc_t detune={"DETUNE",F_PCT,0,127,24,0};
 static const param_desc_t run={"RUN",F_ENUM,0,1,1,FORM_RUN},domain={"DOMAIN",F_ENUM,0,4,0,FORM_DOMAIN},drums={"DRUMS",F_PCT,0,127,100,0},cycle={"CYCLE",F_STEPS,1,64,32,0};
 return id==P_DETUNE?&detune:id==P_LRTRG?&run:id==P_CAOS?&domain:id==P_KEY?&drums:id==P_SLEN?&cycle:0;
}
#define FP(k,label,echo,space) {label,{k,(k+1)%FORM_N,0,64,0,100,0,64},{0,0,127,24},0,TAG(FX),FX(0,24,echo,space),0,MODS(P_LRTRG,1,P_CAOS,0,P_KEY,100,P_SLEN,32,P_ROOT,0,P_SCALE,2,P_CHRD,1,P_CVOI,0,P_DETUNE,24)}
static const preset_t FORM_PRESETS[]={
 FP(0,"01 SCORE",10,20),FP(1,"02 TIME RULE",0,5),FP(2,"03 COUNTER",72,45),FP(3,"04 FIVE PART",40,25),FP(4,"05 TUNED MEM",100,60),FP(5,"06 SWITCH",0,8),FP(6,"07 BASS ECHO",96,20),FP(7,"08 DETUNE",112,60),FP(8,"09 RATIOS",18,15),FP(9,"10 NESTED FM",32,40),FP(10,"11 BIT MORPH",14,5),FP(11,"12 CRUSH CHD",110,25),FP(12,"13 FILTER CHD",100,55),FP(13,"14 SWEEPS",5,5),FP(14,"15 BIT GROOVE",0,8),FP(15,"16 MASK SONG",24,12),FP(16,"17 NONLIN CHD",58,48),FP(17,"18 GENERATIVE",127,90),FP(18,"19 SHAPED ARP",50,35),FP(19,"20 BUILD UP",30,22),FP(20,"21 BIT PAIRS",52,30),FP(21,"22 NESTED EQ",28,28)};
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
 int32_t s=sine_i(ph),q=form_tri(ph),n=(int32_t)(fvm_hash(tick+seed*977u)>>16)-32768,y=0;
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
 default:on=layer!=1||part<6;break;
 }
 return form_mix(32767,on?32767:0,evolve);
}
static uint32_t form_pitch(const track_t*t,int32_t note){return pitch_inc((uint32_t)clamp(scale_snap(t,note),24,96)*16u);}
static uint32_t form_cost(const track_t*t){return t->p[P_E2]>0&&t->p[P_E2]<127&&t->p[P_E0]!=t->p[P_E1]?12u:7u;}
static void form_note_on(track_t*t,voice_t*v){memset(v->s,0,sizeof v->s);memset(v->ph,0,sizeof v->ph);v->s[5]=t->p[P_E2]*256;v->s[6]=t->p[P_E3]*256;}
static __attribute__((noinline)) void form_life(track_t*t,voice_t*v,uint32_t a,uint32_t b,uint32_t morph,int32_t color,uint32_t inc,uint32_t tick,int32_t*y,uint32_t n){
 if(!fl.on)return;
 uint32_t id=fl_ctx[MC_CPU()&1u].id;if(!id)return;
 fl_node_t*node=&fl.node[fl.rd^1u][id-1u];
 if(!node->valid){int32_t cycle[FVM_BLK];for(uint32_t i=0;i<FVM_BLK;i++)cycle[i]=form_mix(form_wave(a,i,0x100000000ull/FVM_BLK,0,color,0),form_wave(b,i,0x100000000ull/FVM_BLK,0,color,0),morph);node->valid=1;node->note=v->note;node->envelope=32767;node->has_number=1;node->number=(uint16_t)(color*64);node->n=1;fl_project(node,cycle);}
 static fvm_ins_t in={FO_PUSH,FM_ARR,0,FA_XF};static const fvm_prog_t p={.ins=&in,.cap=1,.n=1,.depth=1,.ty=FT_FIX};
 for(uint32_t off=0;off<n;off+=FVM_BLK){uint32_t count=n-off<FVM_BLK?n-off:FVM_BLK,W[FVM_BLK],X[FVM_BLK],O[FVM_BLK];for(uint32_t i=0;i<count;i++){W[i]=((tick+off+i)*inc)>>16;X[i]=(uint32_t)(y[off+i]*2);}fvm_env_t e={{0},{0},0};e.arr[FA_W]=W;e.arr[FA_XF]=X;fl_run(&p,&e,&lofi_work[MC_CPU()&1u].s,count,O);for(uint32_t i=0;i<count;i++)y[off+i]=fvm_sample(FT_FIX,O[i]);}
 (void)t;
}
static __attribute__((noinline)) void form_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m)
{
 if(n>CTL)n=CTL;if(!n)return;
 uint32_t a=(uint32_t)clamp(t->p[P_E0],0,21),b=(uint32_t)clamp(t->p[P_E1],0,21),seed=(uint32_t)clamp(t->p[P_E6],0,127),groove=(uint32_t)clamp(t->p[P_E4],0,7),density=(uint32_t)clamp(t->p[P_E5],0,127),evolve=(uint32_t)clamp(t->p[P_E7],0,127),domain=(uint32_t)clamp(t->p[P_CAOS],0,4);
 const form_state_t*A=&FORM_STATE[a],*B=&FORM_STATE[b];
 uint32_t samples=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300),tick=v->ph[0],pos,end;
 if(func_song(t)){pos=func_clock_begin;end=func_clock_end;}else{pos=(uint32_t)(((uint64_t)tick<<16)/samples);end=(uint32_t)(((uint64_t)(tick+n)<<16)/samples);}
 uint32_t dpos=(end-pos)/n,rem=(end-pos)%n,carry=0;
 static const uint8_t RATE_N[]={1,2,4,8,3,6,1,1,1,1},RATE_D[]={4,4,4,4,4,4,8,16,32,64};uint32_t rate=(uint32_t)clamp(t->p[P_SDIV],0,9),cycle=(uint32_t)clamp(t->p[P_SLEN],1,64),last=0xffffffffu,mi=0,bi=0,ci[4]={0};
 int32_t tonal[CTL],drums[CTL];uint32_t lastinc=pitch_inc((uint32_t)clamp(v->note,24,96)*16u);
 v->s[5]+=(t->p[P_E2]*256-v->s[5])/4;v->s[6]+=(t->p[P_E3]*256-v->s[6])/4;
 uint32_t morph=(uint32_t)clamp(v->s[5]>>8,0,127);int32_t color=clamp((v->s[6]>>8)+(m->shape>>8)-64,0,127);
 uint32_t am=a,bm=b;if(morph==127)am=b;if(morph==0)bm=a;
 uint32_t masks[2][4],cached_cycle=~0u,rateq=A->rate*(127u-morph)+B->rate*morph;
 uint32_t event_step=~0u,drum_step=~0u;int32_t event_age=0,event_level=0,drum_age[3]={0},drum_level[3]={0};
 int32_t pitchroot=m->pitch16/16+t->p[P_ROOT]-12;
 int32_t swing=clamp(t->p[P_SSWING],0,100)*160;uint32_t split=32768u+(uint32_t)swing;
 uint32_t recip1=(uint32_t)(0x80000000ull/split),recip2=(uint32_t)(0x80000000ull/(65536u-split));
 for(uint32_t i=0;i<n;i++,tick++){
  uint32_t beat=(pos%(cycle<<16))*RATE_N[rate]/RATE_D[rate],section=(pos%(cycle<<16))/cycle;
  uint32_t cyc=pos/(cycle<<16),swingpos=beat&65535u;
  if(cached_cycle!=cyc){for(uint32_t side=0;side<2;side++){const form_state_t*st=side?B:A;uint32_t raw[4]={st->gate,st->kick,st->snare,st->hat};for(uint32_t k=0;k<4;k++)masks[side][k]=form_pattern(raw[k],groove,seed,cyc);}cached_cycle=cyc;event_step=drum_step=~0u;}
  if(swingpos<split)swingpos=(swingpos*recip1)>>16;else swingpos=32768u+(((swingpos-split)*recip2)>>16);beat=(beat&0xffff0000u)|swingpos;
  uint32_t clock=(uint32_t)((beat*rateq)/127u),step=clock>>16,frac=clock&65535u,sectionn=section>>13;
  uint32_t mutation=(fvm_hash(seed+cyc*37u+sectionn*7u)>>(28u-evolve/43u))&7u;mutation=mutation*evolve/127u;
  if(step!=last){
   uint32_t ix=step+seed+mutation;int32_t nn=form_mix((int32_t)form_char(A->melody,ix),(int32_t)form_char(B->melody,ix),morph);
   int32_t root=pitchroot,rr=form_mix((int32_t)form_char(A->roots,sectionn/2u),(int32_t)form_char(B->roots,sectionn/2u),morph);
   mi=form_pitch(t,root+nn+12);bi=form_pitch(t,root+rr-12);
   int32_t chord=clamp(t->p[P_CHRD],0,11),third=chord==4?2:chord==5||chord==11?5:chord==6||chord==8||chord==9?4:3,top=chord==2||chord==8||chord==10?10:chord==9?11:chord==3?14:12;
   int32_t ints[4]={0,third,7,top};for(uint32_t k=0;k<4;k++){int32_t shift=t->p[P_CVOI]==2?(k&1u?12:0):t->p[P_CVOI]==3&&k==0?-12:t->p[P_CVOI]==1&&(sectionn&1u)&&k==3?-12:0;ci[k]=form_pitch(t,root+rr+ints[k]+shift);}
   last=step;lastinc=mi;
  }
  uint32_t dstep=beat>>14,dfrac=(beat&16383u)<<2;
  if(step!=event_step){event_age=form_event(masks[0][0],masks[1][0],step,0,morph,density,seed,&event_level);event_step=step;}
  int32_t age=(int32_t)(((uint32_t)event_age+frac)*127u/rateq),env=fvm_music_env(age,65536u);env=mulq15(env,event_level);
  int32_t sec0=form_mix(form_section(a,section,0,evolve),form_section(b,section,0,evolve),morph),sec1=form_mix(form_section(a,section,1,evolve),form_section(b,section,1,evolve),morph);
  int32_t sources[2]={0};uint32_t ids[2]={am,bm};
  for(uint32_t side=0;side<(am==bm?1u:2u);side++){
   uint32_t id=ids[side],dom=domain?domain:FORM_STATE[id].domain,mt=tick,ii=mi;
   if(formula_punch&1u)ii>>=1;if(formula_punch&2u)ii<<=1;if(formula_punch&4u)mt^=128u;
   int32_t localcolor=clamp((formula_punch&8u?127-color:color)+(int32_t)FORM_STATE[id].color-64,0,127);
   uint32_t voices=FORM_STATE[id].voices;
   int32_t lead=form_wave(id,mt,ii,(uint32_t)age,localcolor,seed),bass=id>=18?form_drop_bass(id,beat,mt*(bi/2u),localcolor):form_wave(id,mt,bi,beat&65535u,localcolor/2,seed),ch=0;
   uint32_t count=t->p[P_CHRD]? (uint32_t)clamp(voices,1,4):1u;
   if(voices>1)for(uint32_t k=0;k<count;k++)ch+=form_wave(id,mt,ci[k]+(ci[k]>>15)*(uint32_t)(k*t->p[P_DETUNE]/8),beat&0x3ffffu,localcolor,seed)/(int32_t)count;
   int32_t chenv=fvm_music_env((int32_t)(beat&0x3ffffu),4u*65536u);
   int32_t x=mulq15(mulq15(lead,env),sec0)/2+mulq15(mulq15(ch,chenv),sec1)/2;
   if(voices>1 && id<18)x+=mulq15(bass,32767-(int32_t)(beat&65535u)/2)/3;
   if(dom==1){uint32_t mask=255u-(uint32_t)localcolor;int32_t bits=((x+32768)>>8)&255;x=((bits^(int32_t)((step+seed)&mask))*256-32768);}
   else if(dom==2)x=(x+sine_i(tick*mi)/3)*3/4;
   else if(dom==4){int32_t bits=(int32_t)(((tick*(mi>>16))^(tick>>5))&255u)*256-32768;x=(x*3+mulq15(bits,lead))/4;}
   if(id>=18)x=x/2+bass;
   x=soft_knee(x,26000);uint32_t slot=side;
   if(dom==3||id==4||id==12||id==17){int32_t fb=v->s[2+slot]/4;int32_t u=soft_knee(x+fb,26000),coef=2500+localcolor*120;v->s[slot]+=mulq15(u-v->s[slot],coef);v->s[2+slot]=v->s[slot];x=id==12?(x-v->s[slot])/2:v->s[slot];}
   sources[side]=x;
  }
  if(am==bm)sources[1]=sources[0];tonal[i]=clamp(form_mix(sources[0],sources[1],morph),-32767,32767);
  int32_t ds=0;
  for(uint32_t lane=0;lane<3;lane++){
   if(dstep!=drum_step)drum_age[lane]=form_event(masks[0][lane+1],masks[1][lane+1],dstep,0,morph,density,seed+lane*19u,&drum_level[lane]);
   int32_t dl=drum_level[lane],da=(drum_age[lane]+(int32_t)dfrac)/4;
   int32_t ha=dl?form_drum(am,lane,(uint32_t)da,tick,samples,color,seed):0;
   int32_t hb=am==bm?ha:dl?form_drum(bm,lane,(uint32_t)da,tick,samples,color,seed):0;
   int32_t hit=form_mix(ha,hb,morph);
   ds+=mulq15(hit,dl)/(lane==2?2:1);
  }
  drum_step=dstep;
  int32_t sec2=form_mix(form_section(a,section,2,evolve),form_section(b,section,2,evolve),morph),dm=form_mix(A->drums,B->drums,morph)*clamp(t->p[P_KEY],0,127)/127;
  drums[i]=mulq15(ds,sec2)*dm/127;
  pos+=dpos;carry+=rem;if(carry>=n){carry-=n;pos++;}
 }
 form_life(t,v,a,b,morph,color,lastinc,v->ph[0],tonal,n);
 for(uint32_t i=0;i<n;i++){int32_t x=soft_knee(tonal[i]+drums[i],28000);v->s[4]+=(x-v->s[4])/512;x-=v->s[4];out[i]+=voice_amp(clamp(x,-32767,32767),m,i);}
 v->ph[0]=tick;
}
static const engine_t ENG_FORM={.name="FORM",.page_title={"STATES","RULES"},
 .edit={{"STATE A",F_ENUM,0,21,0,FORM_NAMES},{"STATE B",F_ENUM,0,21,1,FORM_NAMES},{"MORPH",F_PCT,0,127,0,0},{"COLOR",F_PCT,0,127,64,0},{"GROOVE",F_ENUM,0,7,0,FORM_GROOVE},{"DENSITY",F_PCT,0,127,100,0},{"SEED",F_INT,0,127,0,0},{"EVOLVE",F_PCT,0,127,64,0}},
 .presets=FORM_PRESETS,.npresets=FORM_N,.knob={P_E0,P_E1,P_E2,P_E3},.poly=1,.note_on=form_note_on,.render=form_render};
