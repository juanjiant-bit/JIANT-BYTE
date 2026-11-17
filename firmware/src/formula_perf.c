/* SPDX-License-Identifier: GPL-3.0-only
 * Momentary formula operators. Only the VM's view changes; the running clock,
 * compiled expression, patch and project remain untouched. D4..F5 = bits 0..9.
 * Called only for instrument formulas, never DIST, macros or the formula compiler. */
static uint32_t formula_punch;
static __attribute__((noinline)) void formula_run(const fvm_prog_t *p, const fvm_env_t *src,
                                                fvm_scratch_t *scratch, uint32_t n, uint32_t *out)
{
    uint32_t mask=formula_punch;
    if (!mask) { fl_run(p,src,scratch,n,out); return; }
    fvm_env_t e=*src;
    uint32_t x[3][FVM_BLK],i,j;
    if(n>FVM_BLK)n=FVM_BLK;
    for(j=0;j<3;j++) if(src->arr[j] && ((p->uses>>j)&1u)) {
        for(i=0;i<n;i++) {
            uint32_t v=src->arr[j][i], shift=j==FA_T?0u:11u;
            if(mask&1u)v>>=1;
            if(mask&2u)v<<=1;
            if(mask&4u)v^=255u<<shift;
            if(mask&8u)v+=(uint32_t)(sine_i(v<<(j==FA_T?24u:16u))>>(j==FA_T?9u:2u));
            if(mask&32u)v&=j==FA_T?~7u:j==FA_W?~4095u:~511u;
            if(mask&64u)v&=j==FA_T?255u:j==FA_W?16383u:8191u;
            x[j][i]=v;
        }
        e.arr[j]=x[j];
    }
    if(mask&16u) { e.u[FU_A]=(e.u[FU_A]+5u)&127u;e.u[FU_AF]=(e.u[FU_AF]+24576u)&65535u; }
    fl_run(p,&e,scratch,n,out);
    if(mask&896u) for(i=0;i<n;i++) {
        int32_t v=p->ty==FT_FIX?clamp((int32_t)out[i],-65536,65535):((int32_t)(out[i]&255u)-128)*512;
        if(mask&128u) { v*=3;v=v>65535?131070-v:v < -65536?-131072-v:v; }
        if(mask&256u)v=(int32_t)(((uint32_t)(v+65536)^0x6666u)&131071u)-65536;
        if(mask&512u) {
            uint32_t phase=src->arr[FA_W]?src->arr[FA_W][i]<<17:src->arr[FA_T]?src->arr[FA_T][i]<<25:0u;
            v=mulq15(v,sine_i(phase));
        }
        out[i]=p->ty==FT_FIX?(uint32_t)v:(uint32_t)clamp((v>>9)+128,0,255);
    }
}
