/* SPDX-License-Identifier: GPL-3.0-only
 * LIFE: typed formula exchange, gated by overlapping active notes. Audio lives
 * inside formula_life.c/FVM; these are control-block snapshots and visual motion. */
#define LIFE_POOL __attribute__((section(".pool")))
#define LIFE_SZ __attribute__((noinline, minsize))
typedef struct { int32_t bx[NTRK],by[NTRK];uint32_t bclk[NTRK];
 int16_t en[NTRK],breath[NTRK],w[NTRK][NTRK];uint8_t dir[NTRK],fol,on,init;
} life_t;
static life_t life LIFE_POOL;
static __attribute__((noinline)) int32_t life_noise(uint32_t seed, uint32_t clk, uint32_t lg) { return drift_noise(seed, clk, lg); }
/* Audio-pot taper: broad low/mid travel; the extreme lives in the last quarter.
 * 25 -> 3.9%, 50 -> 18.8%, 75 -> 49.2%, 100 -> 100%. Integer, monotonic, exact zero. */
static int32_t life_curve(int32_t v)
{
    v = clamp(v, 0, 100);
    return v * v * (100 + v) * 256 / 2000000;
}
static int32_t life_pt(uint32_t ti, int32_t lof)
{
    return life_curve(life_v[LIFE_AMT]) * clamp(life_v[LIFE_N + ti % NTRK] + lof, 0, 100) / 100;
}
static LIFE_SZ int32_t life_src(const track_t *t, uint32_t s)
{
    uint32_t i = (uint32_t)(t - trk) % NTRK;
    return s == MS_LIFE ? life.breath[i] : clamp(32767 - life.by[i], -32768, 32767);
}
static LIFE_SZ void life_swim(uint32_t i, int32_t en)
{
    uint32_t k, c = life.bclk[i] += 8u * (24u + ((uint32_t)en >> 8));   /* (loud: ~6 x the pace) */
    for (k = 0; k < 2u; k++) {                          /* x, then y */
        int32_t *q = k ? &life.by[i] : &life.bx[i];
        int32_t v = (life_noise(i * 977u + k * 0x3b0u + 11u, c >> 4, 11u) * (96 + (en >> 7))) >> 17;
        *q += (life.dir[i] >> k) & 1u ? -v : v;
        if (*q < 4000 || *q > 61535) {
            life.dir[i] ^= (uint8_t)(1u << k);
            *q = clamp(*q, 4000, 61535);
        }
    }
    life.breath[i] = (int16_t)life_noise(i * 31u + 7u, c >> 3, 9u);
}

static LIFE_SZ void life_master(void)
{
    uint32_t i;
    life.on=life_v[LIFE_AMT]>0;life.fol=0;
#if FELUCCA_VIS
    if(vis_fol)vis_fol--;                              /* (visgen: the levels VIS reads, while it asks) */
#endif
    if(!life.init){life.init=1;for(i=0;i<NTRK;i++){life.bx[i]=10000+i*8500;life.by[i]=i<3u?16000:46000;}}
    for(i=0;i<NTRK;i++){
        int32_t e=i<trk_live&&!trk[i].p[P_MUTE]?clamp(track_env[i]*2,0,32767):0;
        life.en[i]+=(int16_t)((e-life.en[i])/8);
        if((mns_clk&7u)==i)life_swim(i,life.en[i]);
#if FELUCCA_VIS
        if((life.on||vis_fol)&&i<trk_live&&!trk[i].p[P_MUTE])life.fol|=1u<<i;
#else
        if(life.on&&i<trk_live&&!trk[i].p[P_MUTE])life.fol|=1u<<i;
#endif
    }
    life.fol&=(uint8_t)~track_follow_mask;track_follow_mask|=life.fol;
}
/* After note events and performance mutes, before either render core starts. */
static uint32_t fl_blk;
static volatile uint8_t load_tight;     /* (voice.c: the overload guard tripped) */
static uint16_t fl_shed;                /* (byte-cpu-light) blocks left of LIFE shed by the guard (~2 s after it lets go):
                                         * the parts but the selected one without LIFE, their depth gliding to 0 */
static uint8_t fl_idle;                 /* (byte-cpu-light) LIFE off and its state cleared */                 /* (byte-cpu-light) blocks, for the captures' turn */
static LIFE_SZ void life_formula_block(uint32_t hush)
{
    uint32_t i,j;
    fl.rd^=1u;
    fl_blk++;
    punch_exchange_tick();
    if(!life_v[LIFE_AMT]&&fl_idle&&!(punch_amount[0]|punch_amount[1]|punch_amount[2])){
        /* (devcpu2) LIFE 0, no punch, cleared already: what the pass below does then, alone */
        fl.on=0;fl.numbers_amt=fl.fragments_amt=fl.link_amt=0;
        fl.count=fl.ncount=fl.weight=fl.number_weight=0;fl.numbers=0;fl.strikes=0;
        return;
    }
    int punch=0;
    for(unsigned k=0;k<3;k++)if(punch_amount[k]>punch)punch=punch_amount[k];
    if(fl.on&&!life_v[LIFE_AMT]&&!punch)memset(fl.shape,0,sizeof fl.shape);
    fl.on=(uint8_t)(life_v[LIFE_AMT]>0 || punch);
    fl.numbers_amt=(uint8_t)clamp((life_v[LIFE_AMT]?life_v[LIFE_NUMBERS]*(100-punch)/100:0)+punch_amount[0],0,100);
    fl.fragments_amt=(uint8_t)clamp((life_v[LIFE_AMT]?life_v[LIFE_FRAGMENTS]*(100-punch)/100:0)+punch_amount[1],0,100);
    fl.link_amt=(uint8_t)clamp((life_v[LIFE_AMT]?life_v[LIFE_LINK]*(100-punch)/100:0)+punch_amount[2],0,100);
    fl.count=fl.ncount=fl.weight=fl.number_weight=0;fl.numbers=0;fl.strikes=0;
    if(!fl.on){                             /* (byte-cpu-light) LIFE 0, no punch: cleared once, then not run */
        if(!fl_idle){
            memset(fl.sum,0,sizeof fl.sum);memset(life.w,0,sizeof life.w);
            memset(fl.gate,0,sizeof fl.gate);memset(fl.strike,0,sizeof fl.strike);memset(fl.depth,0,sizeof fl.depth);
            for(i=0;i<FL_VOICES;i++)fl.node[0][i].valid=fl.node[1][i].valid=0;
            fl_idle=1;
        }
        return;
    }
    fl_idle=0;
    memset(fl.sum,0,sizeof fl.sum);memset(life.w,0,sizeof life.w);
    if(load_tight)fl_shed=(uint16_t)(2u*FS/CTL);else if(fl_shed)fl_shed--;
    uint32_t shed=fl_shed?~(1u<<(song.sel%NTRK))&((1u<<NTRK)-1u):0u;   /* (bit p: part p's LIFE shed) */
    fl_cap_m=0;
    for(i=0;i<NTRK;i++)if((punch||life_v[LIFE_N+i])&&!((shed>>i)&1u))fl_cap_m|=(uint8_t)(1u<<i);
    for(i=0;i<NTRK;i++) {
        int32_t target=fl.on?life_pt(i,0):0;
        if(target<punch*2)target=punch*2;
        if((shed>>i)&1u)target=0;
        fl.depth[i]=(uint16_t)(target?fl.depth[i]+clamp(target-fl.depth[i],-4,4):0);
    }
    for(i=0;i<FL_VOICES;i++) {
        uint32_t tr=i/NVOICE, vi=i%NVOICE,g=0;
        const voice_t *v=&trk[tr].v[vi];const fl_node_t *p=&fl.node[fl.rd][i];
        {   /* (byte-cpu-light) a donor's spectrum captured one block in four (2.9 ms): the other three keep the
             * last one while the same note sounds (formula_life.c fl_capture: a VM run and a 16 x 32 projection) */
            fl_node_t *w=&fl.node[fl.rd^1u][i];
            if((fl_blk&3u)&&p->valid&&v->active&&v->note==p->note)*w=*p;
            else w->valid=0;
        }
        if(fl.on&&p->valid&&tr<trk_live&&!trk[tr].p[P_MUTE]&&!((hush>>tr)&1u)&&
           (punch||life_v[LIFE_N+tr])&&!((shed>>tr)&1u)&&v->active&&v->stage!=4u&&v->note==p->note&&
           (trk[tr].engine!=ENGI_DRUM||!dx_lane_muted((uint32_t)v->s[0]))) {
            g=(uint32_t)clamp(v->env_out,0,32767)>>7;
            g=g*(uint32_t)clamp(trk[tr].p[P_LEVEL],0,127)/127u;
            g=g*p->envelope/32767u;
        }
        uint32_t rise=g>fl.gate[i]?g-fl.gate[i]:0;
        uint32_t pulse=fl.strike[i]*31u/32u;
        if(rise*4u>pulse)pulse=rise*4u;
        fl.strike[i]=(uint16_t)(g?clamp((int32_t)pulse,0,255):0);
        fl.strikes+=fl.strike[i]*g;
        fl.gate[i]=(uint16_t)g;
        if(!g)continue;
        fl.count++;fl.weight+=(uint16_t)g;
        for(j=0;j<FL_POINTS;j++)fl.sum[j]+=p->wave[j]*(int32_t)g;
        if(p->has_number){fl.ncount++;fl.number_weight+=(uint16_t)g;fl.numbers+=p->number*(int32_t)g;}
    }
    for(i=0;i<FL_VOICES;i++)if(fl.gate[i])for(j=0;j<FL_VOICES;j++)if(i!=j&&fl.gate[j])
        life.w[i/NVOICE][j/NVOICE]=8192;
}
static LIFE_SZ int32_t life_ctx(uint32_t ti,int32_t *c)
{
    c[0]=clamp((int32_t)fl.depth[ti]+life_curve(life_v[LIFE_AMT])*(MOD.on?MOD.lof:0)/100,0,256);
    c[1]=c[2]=0;return c[0];
}
static void life_reset(void){formula_punch=0;memset(punch_amount,0,sizeof punch_amount);memset(&life,0,sizeof life);memset(&fl,0,sizeof fl);memset(fl_ctx,0,sizeof fl_ctx);}
