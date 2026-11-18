/* SPDX-License-Identifier: GPL-3.0-only */
/* FUNC executes ordered statements once per sample. No allocation, jumps,
 * recursion or data-dependent loops: at most FVM_MAXINS operations/sample. */
static __attribute__((noinline)) void fvm_func_run(const fvm_prog_t *p,const fvm_env_t *e,uint32_t n,uint32_t *out)
{
    if(n>FVM_BLK)n=FVM_BLK;
    for(uint32_t i=0;i<n;i++){
        uint32_t stack[FVM_DEPTH],local[10]={0},sp=0;
        for(uint32_t pc=0;pc<p->n;pc++){
            const fvm_ins_t*in=&p->ins[pc];uint32_t op=in->op,b=in->k,a;
            if(op==FO_SAVE){
                a=stack[--sp];
                if(in->md==FM_STATE)e->state[b]=(int32_t)a;else local[b]=a;
                continue;
            }
            if(in->md==FM_ARR)b=e->arr[b]?e->arr[b][i]:0;
            else if(in->md==FM_UNI)b=e->u[b];
            else if(in->md==FM_LOCAL)b=local[b];
            else if(in->md==FM_STATE)b=(uint32_t)e->state[b];
            if(op==FO_PUSH){stack[sp++]=b;continue;}
            if(FO_ISBIN(op)||op==FO_LP||op==FO_HP){if(in->md==FM_STK)b=stack[--sp];}
            a=stack[sp-1];
            if(FO_ISBIN(op)){stack[sp-1]=fvm_op2(op,a,b);continue;}
            if(FO_ISUN(op)){stack[sp-1]=fvm_op1(op,a);continue;}
            switch(op){
            case FO_SEL:{uint32_t y=stack[--sp],x=stack[--sp];stack[sp-1]=stack[sp-1]?x:y;break;}
            case FO_TAB:stack[sp-1]=p->pool[(in->k>>8)+a%(in->k&255u)];break;
            case FO_PH:{uint32_t q=(uint32_t)e->state[in->sl&7u];q+=(uint32_t)(((int64_t)(int32_t)a*FVM_PH_K)>>16);e->state[in->sl&7u]=(int32_t)q;stack[sp-1]=q>>16;break;}
            case FO_LP:case FO_HP:{int32_t *q=&e->state[in->sl&7u],k=clamp((int32_t)b,0,65536);*q+=(int32_t)((((int64_t)(int32_t)a-*q)*k)>>16);stack[sp-1]=op==FO_LP?(uint32_t)*q:a-(uint32_t)*q;break;}
            }
        }
        out[i]=sp?stack[0]:0;
    }
}
