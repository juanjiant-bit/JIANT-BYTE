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
    if(!life.init){life.init=1;for(i=0;i<NTRK;i++){life.bx[i]=10000+i*8500;life.by[i]=i<3u?16000:46000;}}
    for(i=0;i<NTRK;i++){
        int32_t e=i<trk_live&&!trk[i].p[P_MUTE]?clamp(track_env[i]*2,0,32767):0;
        life.en[i]+=(int16_t)((e-life.en[i])/8);
        if((mns_clk&7u)==i)life_swim(i,life.en[i]);
        if(life.on&&i<trk_live&&!trk[i].p[P_MUTE])life.fol|=1u<<i;
    }
    life.fol&=(uint8_t)~track_follow_mask;track_follow_mask|=life.fol;
}
/* After note events and performance mutes, before either render core starts. */
static LIFE_SZ void life_formula_block(uint32_t hush)
{
    uint32_t i,j;
    fl.rd^=1u;
    if(fl.on&&!life_v[LIFE_AMT])memset(fl.shape,0,sizeof fl.shape);
    fl.on=(uint8_t)(life_v[LIFE_AMT]>0);
    fl.numbers_amt=(uint8_t)clamp(life_v[LIFE_NUMBERS],0,100);
    fl.fragments_amt=(uint8_t)clamp(life_v[LIFE_FRAGMENTS],0,100);
    fl.link_amt=(uint8_t)clamp(life_v[LIFE_LINK],0,100);
    fl.count=fl.ncount=fl.weight=fl.number_weight=0;fl.numbers=0;fl.strikes=0;
    memset(fl.sum,0,sizeof fl.sum);memset(life.w,0,sizeof life.w);
    for(i=0;i<NTRK;i++) {
        int32_t target=fl.on?life_pt(i,0):0;
        fl.depth[i]=(uint16_t)(target?fl.depth[i]+clamp(target-fl.depth[i],-4,4):0);
    }
    for(i=0;i<FL_VOICES;i++) {
        uint32_t tr=i/NVOICE, vi=i%NVOICE,g=0;
        const voice_t *v=&trk[tr].v[vi];const fl_node_t *p=&fl.node[fl.rd][i];
        fl.node[fl.rd^1u][i].valid=0;
        if(fl.on&&p->valid&&tr<trk_live&&!trk[tr].p[P_MUTE]&&!((hush>>tr)&1u)&&
           life_v[LIFE_N+tr]&&v->active&&v->stage!=4u&&v->note==p->note&&
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
static void life_reset(void){memset(&life,0,sizeof life);memset(&fl,0,sizeof fl);memset(fl_ctx,0,sizeof fl_ctx);}
