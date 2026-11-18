/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded FUNC language: func { state q; let zz=...; fn tone(z)=...;
 * q=...; return ...; }. State starts at zero for each new voice.
 * Helpers are one-argument expressions, inlined at compile time; arguments run once. */
static int fvm_ident(fvm_cc_t*c,char name[8])
{
    fvm_ws(c);uint32_t n=0;
    if(!fvm_isal(fvm_ch(c))){fvm_fail(c,FE_NAME);return 0;}
    while(fvm_isal(fvm_ch(c))||fvm_isdg(fvm_ch(c))){if(n>=7u){fvm_fail(c,FE_NAME);return 0;}name[n++]=c->s[c->i++];}
    name[n]=0;fvm_ws(c);return 1;
}
static int fvm_word(fvm_cc_t*c,const char*s)
{
    uint32_t old=c->i;if(!fvm_eat(c,s))return 0;
    if(fvm_isal(fvm_ch(c))||fvm_isdg(fvm_ch(c))){c->i=old;return 0;}return 1;
}
static int fvm_free_name(fvm_cc_t*c,const char*s)
{
    for(uint32_t k=0;k<c->nb;k++)if(!fvm_cmp(c->binding[k].name,s))return 0;
    for(uint32_t k=0;k<c->nfn;k++)if(!fvm_cmp(c->fn[k].name,s))return 0;
    for(uint32_t k=0;k<NELEM(FVM_VARS);k++)if(!fvm_cmp(FVM_VARS[k].nm,s))return 0;
    for(uint32_t k=0;k<NELEM(FVM_CALLS);k++)if(!fvm_cmp(FVM_CALLS[k].nm,s))return 0;
    return fvm_cmp(s,"sr")&&fvm_cmp(s,"PI")&&fvm_cmp(s,"let")&&fvm_cmp(s,"state")&&fvm_cmp(s,"return")&&fvm_cmp(s,"fn")&&fvm_cmp(s,"func");
}
static uint32_t fvm_func_parse(fvm_cc_t*c)
{
    uint32_t result=0;
    if(!fvm_eat(c,"{")){fvm_fail(c,FE_SYNTAX);return 0;}
    while(!c->err.code){
        char name[8];uint32_t root,k;
        if(fvm_word(c,"return")){
            result=fvm_tofix(c,fvm_expr(c));
            if(!fvm_eat(c,";")||!fvm_eat(c,"}"))fvm_fail(c,FE_SYNTAX);
            return result;
        }
        if(fvm_word(c,"fn")){
            if(c->nfn>=2u||!fvm_ident(c,name)||!fvm_free_name(c,name)){fvm_fail(c,FE_NAME);break;}
            memcpy(c->fn[c->nfn].name,name,8);
            if(!fvm_eat(c,"(")||!fvm_ident(c,c->fn[c->nfn].arg)||!fvm_free_name(c,c->fn[c->nfn].arg)||!fvm_eat(c,")")||!fvm_eat(c,"=")){fvm_fail(c,FE_SYNTAX);break;}
            c->in_fn=1;root=fvm_expr(c);c->in_fn=0;
            if(!fvm_eat(c,";")){fvm_fail(c,FE_SYNTAX);break;}
            c->fn[c->nfn++].root=(uint8_t)root;continue;
        }
        uint32_t state=fvm_word(c,"state"),local=!state&&fvm_word(c,"let");
        if(!fvm_ident(c,name))break;
        if(state||local){
            if(c->nb>=8u||!fvm_free_name(c,name)||(local&&c->nlocal>=6u)){fvm_fail(c,FE_NAME);break;}
            k=c->nb++;
            memcpy(c->binding[k].name,name,8);c->binding[k].kind=(uint8_t)(state?NK_STATE:NK_LOCAL);
            if(state){
                if(c->p->nstate>=2u){fvm_fail(c,FE_STATE);break;}
                c->binding[k].slot=c->p->nstate++;c->binding[k].ty=FT_FIX;
                if(fvm_eat(c,"=")){root=fvm_expr(c);if(ND(root).kind!=NK_NUM||ND(root).v){fvm_fail(c,FE_STATE);break;}}
                if(!fvm_eat(c,";"))fvm_fail(c,FE_SYNTAX);
                continue;
            }
            c->binding[k].slot=c->nlocal++;
            /* Hide the new local while parsing its initializer. */
            c->nb--;
        }else{
            for(k=0;k<c->nb;k++)if(!fvm_cmp(name,c->binding[k].name))break;
            if(k==c->nb){fvm_fail(c,FE_NAME);break;}
        }
        if(!fvm_eat(c,"=")){fvm_fail(c,FE_SYNTAX);break;}
        root=fvm_expr(c);
        if(local){c->nb++;c->binding[k].ty=ND(root).ty;}
        else root=c->binding[k].ty==FT_FIX?fvm_tofix(c,root):fvm_toint(c,root);
        if(!fvm_eat(c,";")){fvm_fail(c,FE_SYNTAX);break;}
        fvm_gen(c,root);fvm_emit(c,FO_SAVE,c->binding[k].kind==NK_STATE?FM_STATE:FM_LOCAL,c->binding[k].slot,0,0,0);fvm_push_sp(c,-1);
    }
    return result;
}
