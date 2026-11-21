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
            case FO_MUSIC:{uint32_t z=0,y=0;if(in->k<2u){z=stack[--sp];y=stack[--sp];}stack[sp-1]=fvm_music(in->k,stack[sp-1],y,z,e,i);break;}
            case FO_SEL:{uint32_t y=stack[--sp],x=stack[--sp];stack[sp-1]=stack[sp-1]?x:y;break;}
            case FO_TAB:stack[sp-1]=p->pool[(in->k>>8)+a%(in->k&255u)];break;
            case FO_PH:{uint32_t q=(uint32_t)e->state[in->sl&7u];q+=(uint32_t)(((int64_t)(int32_t)a*FVM_PH_K)>>16);e->state[in->sl&7u]=(int32_t)q;stack[sp-1]=q>>16;break;}
            case FO_LP:case FO_HP:{int32_t *q=&e->state[in->sl&7u],k=clamp((int32_t)b,0,65536);*q+=(int32_t)((((int64_t)(int32_t)a-*q)*k)>>16);stack[sp-1]=op==FO_LP?(uint32_t)*q:a-(uint32_t)*q;break;}
            }
        }
        out[i]=sp?stack[0]:0;
    }
}

/* (BYTE 0.16, the owner: "MATH in FUNC is what costs most") a FUNC with no state of its own (no state, ph, lp, hp: every
 * sample its own, none depending on the one before) runs as the block machine does: each instruction over the block's
 * samples at once, not the whole program a sample (its decoding once a block, not 32 times). Its locals in the scratch's
 * slots above the stack's deepest (no stack, no RAM of its own); 0: it does not fit, the sample loop runs it */
static __attribute__((noinline)) int fvm_func_block(const fvm_prog_t *p,const fvm_env_t *e,fvm_scratch_t *sc,uint32_t n,uint32_t *out)
{
    uint32_t nloc=0,pc,i,sp=0,base;
    for(pc=0;pc<p->n;pc++){const fvm_ins_t*in=&p->ins[pc];
        if(in->md==FM_STATE||in->op==FO_PH||in->op==FO_LP||in->op==FO_HP)return 0;
        if(in->md==FM_LOCAL&&in->k+1u>nloc)nloc=in->k+1u;}
    if(p->depth+nloc>FVM_DEPTH)return 0;
    base=FVM_DEPTH-nloc;
    if(n>FVM_BLK)n=FVM_BLK;
    for(pc=0;pc<p->n;pc++){
        const fvm_ins_t*in=&p->ins[pc];uint32_t op=in->op,b=in->k,md=in->md;const uint32_t*R=0;uint32_t*L;
        if(op==FO_SAVE){sp--;uint32_t*d=sc->s[base+b];const uint32_t*q=sc->s[sp];for(i=0;i<n;i++)d[i]=q[i];continue;}
        if(md==FM_ARR){R=e->arr[b];if(!R)b=0;}
        else if(md==FM_UNI)b=e->u[b];
        else if(md==FM_LOCAL)R=sc->s[base+b];
        if(op==FO_PUSH){L=sc->s[sp++];if(R)for(i=0;i<n;i++)L[i]=R[i];else for(i=0;i<n;i++)L[i]=b;continue;}
        if(FO_ISBIN(op)&&md==FM_STK)R=sc->s[--sp];
        L=sc->s[sp-1];
        if(FO_ISBIN(op)){                              /* (the commonest inline, the rest the shared operations: the
                                                         * flash's room) */
            if(R){                                     /* (no copy of a constant: the stack's bytes are counted) */
                if(op==FO_ADD)for(i=0;i<n;i++)L[i]+=R[i];
                else if(op==FO_SUB)for(i=0;i<n;i++)L[i]-=R[i];
                else if(op==FO_MULF)for(i=0;i<n;i++)L[i]=U32(I32(((int64_t)I32(L[i])*I32(R[i]))>>16));
                else for(i=0;i<n;i++)L[i]=fvm_op2(op,L[i],R[i]);
            }else{
                if(op==FO_ADD)for(i=0;i<n;i++)L[i]+=b;
                else if(op==FO_SUB)for(i=0;i<n;i++)L[i]-=b;
                else if(op==FO_MULF)for(i=0;i<n;i++)L[i]=U32(I32(((int64_t)I32(L[i])*I32(b))>>16));
                else for(i=0;i<n;i++)L[i]=fvm_op2(op,L[i],b);
            }
            continue;}
        if(FO_ISUN(op)){
            if(op==FO_SINF)for(i=0;i<n;i++)L[i]=U32(sine_i(L[i]<<16)*2);
            else for(i=0;i<n;i++)L[i]=fvm_op1(op,L[i]);
            continue;}
        switch(op){
        case FO_MUSIC:{const uint32_t*Z=0,*Y=0;if(in->k<2u){Z=sc->s[--sp];Y=sc->s[--sp];}L=sc->s[sp-1];
            for(i=0;i<n;i++)L[i]=fvm_music(in->k,L[i],Y?Y[i]:0u,Z?Z[i]:0u,e,i);break;}
        case FO_SEL:{const uint32_t*Y=sc->s[--sp],*X=sc->s[--sp];L=sc->s[sp-1];for(i=0;i<n;i++)L[i]=L[i]?X[i]:Y[i];break;}
        case FO_TAB:{const uint8_t*tb=p->pool+(in->k>>8);uint32_t len=in->k&255u;for(i=0;i<n;i++)L[i]=tb[L[i]%len];break;}
        default:break;
        }
    }
    {const uint32_t*r=sp?sc->s[0]:0;for(i=0;i<n;i++)out[i]=r?r[i]:0u;}
    return 1;
}
