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
 { const char*const fixed[]={MATH_FUNC_DEFAULT,MATH_FUNC_BEAT,MATH_FUNC_GROWL,MATH_FUNC_HALF}; /* every recipe in the FORM list compiles, and stateless (they run by blocks) */
   for(unsigned i=0;i<NELEM(fixed)+NELEM(MATH_FUNC_R);i++){compile(i<NELEM(fixed)?fixed[i]:MATH_FUNC_R[i-NELEM(fixed)]);assert(i<2||!code.p.nstate);} }
 puts("funcbeat: ordered state, chunk invariance, voice isolation, helpers, controls, sample rate and rejection PASS");return 0;
}
