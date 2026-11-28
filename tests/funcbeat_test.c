/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static fvm_cc_t cc;static fvm_full_t code;static fvm_scratch_t scratch;
static void compile(const char*s){fvm_err_t e;unsigned r=fvm_compile(&cc,FVM_FULL(&code),s,strlen(s),&e);if(r)printf("compile error %u at %u: %s\n",r,e.pos,s);assert(!r);}
static void reject(const char*s){assert(fvm_compile(&cc,FVM_FULL(&code),s,strlen(s),0));}
int main(void){
 uint32_t out[32],one[32];int32_t state[8]={0},other[8]={0};fvm_env_t e={{0},{0},state};
 compile("func { state z; z=z+0.01; return z; }");
 fvm_run(&code.p,&e,&scratch,32,out);assert(state[0]==32*655);
 e.state=other;for(unsigned i=0;i<32;i++)fvm_run(&code.p,&e,&scratch,1,one+i);
 assert(!memcmp(out,one,sizeof out)&&!memcmp(state,other,sizeof state));
 memset(other,0,sizeof other);fvm_run(&code.p,&e,&scratch,1,one);assert(one[0]==655&&state[0]==32*655);
 compile("func { fn tone(q)=sin(q)+0.25*sin(q*2); let y=tone(A); return y; }");e.u[FU_AF]=8192;fvm_run(&code.p,&e,&scratch,1,out);assert((int32_t)out[0]>40000);
 compile("func { state z=0; let f=freq/sr; z=(z+f)%1.0; return sin(z); }");e.u[FU_HZ]=440*65536u;memset(other,0,sizeof other);fvm_run(&code.p,&e,&scratch,32,out);assert(other[0]>0);
 compile("func { let zz=2; zz=zz+3; return fix(zz); }");fvm_run(&code.p,&e,&scratch,1,out);assert(out[0]==5*65536);
 compile("func{fn square(q)=q*q;return square(ph(440));}");assert(code.p.nstate==1);
 memset(other,0,sizeof other);fvm_run(&code.p,&e,&scratch,32,out);assert((uint32_t)other[0]==32u*(uint32_t)(((int64_t)(440*65536)*FVM_PH_K)>>16));
 reject("func { state a; return a; }"); /* musical control cannot be shadowed */
 reject("func { state q=1; return q; }");
 reject("func { state q; state r; state z; return z; }");
 reject("func { fn f(q)=f(q); return f(1); }");
 reject("func { let q=q+1; return q; }");
 reject("func { while(1){} return 0; }");
 reject("func { let q=1; }");
 reject("func { return 1; } garbage");
 compile("func{return sin(ph(freq));}");memset(other,0,sizeof other);e.u[FU_HZ]=440*65536u;
 unsigned crossings=0;int32_t previous=0;for(unsigned j=0;j<FS;j+=32){unsigned count=FS-j<32?FS-j:32;fvm_run(&code.p,&e,&scratch,count,out);for(unsigned k=0;k<count;k++){int32_t now=(int32_t)out[k];crossings+=previous<=0&&now>0;previous=now;}}
 assert(crossings>=439&&crossings<=441);
 reject("func{return - - - - - - - - - - - - - - - - - - - - 1.0;}");
 { /* (v1.1b2) the recipes packed (tools/gen_fvm_bank.py, fvm_bank.c) as eng_lofi.c writes them, FORM 1 .. in order */
   static const char*const src[]={MATH_FUNC_DEFAULT_TEXT,MATH_FUNC_BEAT_TEXT,MATH_FUNC_GROWL_TEXT,MATH_FUNC_HALF_TEXT,MATH_FUNC_R_ACID_TEXT,MATH_FUNC_R_REESE_TEXT,MATH_FUNC_R_HOUSE_TEXT,MATH_FUNC_R_STRUM_TEXT,MATH_FUNC_R_JUST_TEXT,MATH_FUNC_R_PLUCK_TEXT,MATH_FUNC_R_NIBBLE_TEXT,MATH_FUNC_R_BITS_TEXT,MATH_FUNC_R_BEATS_TEXT,MATH_FUNC_R_SCREECH_TEXT,MATH_FUNC_R_DUB_TEXT,MATH_FUNC_R_AMEN_TEXT,MATH_FUNC_R_TRANCE_TEXT,MATH_FUNC_R_AMBIENT_TEXT,MATH_FUNC_R_MINIMAL_TEXT,MATH_FUNC_R_JUNGLE_TEXT};
   assert(NELEM(src)==FVM_NRECIPE&&FUNC_BANK==FVM_NRECIPE+1u);
   for(unsigned i=0;i<NELEM(src);i++)assert(!strcmp(func_form_text((int32_t)i+1),src[i]));
   assert(!func_form_text(0)&&!func_form_text((int32_t)FUNC_BANK)); }
 for(unsigned f=1;f<FUNC_BANK;f++){compile(func_form_text((int32_t)f));assert(f<3||!code.p.nstate);}   /* every recipe in the FORM
                                                         * list compiles, and stateless (they run by blocks) */
 { /* (func-tame, the owner: "B moves the drums out of tempo, out of phase") every drum hit of every recipe, at every
    * value of every knob, on the 1/32 grid of the song's beat: the program run as fvm_func_run runs it, a sample at a
    * time, kick() snare() hat()'s age read where they are called; a hit where the age falls back */
   unsigned checked=0,per[32]={0};
   for(unsigned f=0;f+1u<FUNC_BANK;f++){
     compile(func_form_text((int32_t)f+1));
     unsigned drums=0;for(unsigned i=0;i<code.p.n;i++)drums|=code.p.ins[i].op==FO_MUSIC&&code.p.ins[i].k>=2u;
     if(!drums)continue;
     for(unsigned kn=0;kn<4;kn++)for(unsigned val=0;val<128;val=val<126u?val+3u:val+1u){
       uint32_t u[FU_N]={0},Pq=0,prevP=0,age[3]={0},last[3]={0};int32_t st8[8]={0};
       for(unsigned k=0;k<4;k++){unsigned a=k==kn?val:(unsigned)(64>>k);u[FU_A+k]=a*2u;u[FU_AF+k]=a*65536u/127u;}
       u[FU_NOTE]=48;u[FU_BPS]=FS/2u;
       for(unsigned step=0;step<48u*256u;step++){
         uint32_t T0=step,Tz=0;Pq=step*256u;const uint32_t*ar[FA_N]={0};ar[FA_T]=&T0;ar[FA_W]=&Tz;ar[FA_TS]=&Tz;ar[FA_BEAT]=&Pq;
         uint32_t stack[FVM_DEPTH],local[10]={0},sp=0;
         for(uint32_t pc=0;pc<code.p.n;pc++){
           const fvm_ins_t*in=&code.p.ins[pc];uint32_t op=in->op,b=in->k,a;
           if(op==FO_SAVE){local[b]=stack[--sp];continue;}
           if(in->md==FM_ARR)b=ar[b]?ar[b][0]:0;else if(in->md==FM_UNI)b=u[b];else if(in->md==FM_LOCAL)b=local[b];
           if(op==FO_PUSH){stack[sp++]=b;continue;}
           if(FO_ISBIN(op)&&in->md==FM_STK)b=stack[--sp];
           a=stack[sp-1];
           if(FO_ISBIN(op)){stack[sp-1]=fvm_op2(op,a,b);continue;}
           if(FO_ISUN(op)){stack[sp-1]=fvm_op1(op,a);continue;}
           if(op==FO_MUSIC){if(in->k<2u){sp-=2;stack[sp-1]=0;}else{age[in->k-2u]=stack[sp-1];stack[sp-1]=0;}}
           else if(op==FO_SEL){uint32_t y=stack[--sp],x=stack[--sp];stack[sp-1]=stack[sp-1]?x:y;}
           else if(op==FO_TAB)stack[sp-1]=code.p.pool[(in->k>>8)+a%(in->k&255u)];
         }
         for(unsigned d=0;d<3;d++){
           if(step&&(int32_t)age[d]<(int32_t)last[d]){   /* a hit between the sample before and this one: a 1/32 there */
             int ok=(Pq&~2047u)>prevP||!(Pq&2047u);
             if(!ok)printf("FORM %u knob %c=%u: drum %u off the grid at beat %.4f\n",f+1,'A'+kn,val,d,Pq/65536.);
             assert(ok);checked++;per[f+1]++;}
           last[d]=age[d];}
         prevP=Pq;
       }
     }
   }
   assert(checked>1000u);
   assert(per[15]&&per[16]&&per[19]&&per[20]);   /* (BYTE 0.17) DUB AMEN MINIMAL JUNGLE's drums: hits, each on the grid */
   /* every FORM its four names (the cards' six letters) and a line of help each; CUSTOM, and a LOOP past them, A B C D */
   static track_t ft;ft.p[P_E1]=RW_FUNC;
   for(int f=0;f<=(int)FUNC_NFORM;f++){ft.p[P_E7]=(int16_t)(f==(int)FUNC_NFORM?127:f);
     for(unsigned k=0;k<4;k++){const param_desc_t*d=lofi_desc(&ft,2u+k);const char*tip=func_knob_tip(d);
       int bk=f==(int)FUNC_BANK&&k<2u;   /* (funcbank) BANK's FORM A, FORM B: the bank's names */
       assert(d&&d->label[0]&&strlen(d->label)<=6u&&d->min==0&&d->max==(bk?(int)FB_N-1:127)&&tip&&strlen(d->label)+2u+strlen(tip)<=26u);
       assert(!bk==!d->names&&(!bk||(!strcmp(d->names[0],"PAD")&&!d->names[FB_N])));
       for(const char*c=d->label;*c;c++)assert((*c>='A'&&*c<='Z')||(*c>='0'&&*c<='9')||(bk&&*c==' '&&c[1]));
       if(f==0||f>=(int)FUNC_NFORM)assert(d->label[0]=='A'+(int)k&&!d->label[1]);else assert(strlen(d->label)>1u);}
     assert(!func_knob_tip(lofi_desc(&ft,0))&&!func_knob_tip(lofi_desc(&ft,7)));}
   ft.p[P_E7]=5;assert(!strcmp(lofi_desc(&ft,2)->label,"ENVMOD")&&!strcmp(lofi_desc(&ft,5)->label,"SEED")); }
 puts("funcbeat: FORM drums on the beat's grid at every knob value, every FORM's knobs named PASS");
 puts("funcbeat: ordered state, chunk invariance, voice isolation, helpers, controls, sample rate and rejection PASS");return 0;
}
