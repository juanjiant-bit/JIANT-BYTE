/* SPDX-License-Identifier: GPL-3.0-only
 * LIFE exchanges band-limited waveform fragments, never raw clocks or audio-rate
 * offsets. Donors are evaluated once per voice/control block on a complete cycle;
 * their DC-free harmonic coefficients are grafted at the receiver's own phase. */
#define FL_VOICES (NTRK * NVOICE)
#define FL_HARM 8u
#define FL_POINTS (FL_HARM * 2u)
typedef struct {
    int16_t wave[FL_POINTS]; /* sine/cosine coefficients, not decimated audio */
    uint16_t envelope, number;
    uint8_t note,valid,has_number,n;
} fl_node_t;
typedef struct {int32_t h[FL_POINTS],num,link,level,color;} fl_shape_t;
static struct {
    fl_node_t node[2][FL_VOICES];
    fl_shape_t shape[FL_VOICES];
    int32_t sum[FL_POINTS],numbers;
    uint16_t gate[FL_VOICES],depth[NTRK],count,ncount,weight,number_weight;
    uint8_t rd,on,numbers_amt,fragments_amt,link_amt;
} fl;
static struct {uint16_t id,depth,envelope;uint8_t note;} fl_ctx[2];
static void fl_bind(const track_t*t,const voice_t*v,uint32_t depth)
{
    uint32_t c=MC_CPU()&1u;
    fl_ctx[c].id=(uint16_t)((t-trk)*NVOICE+(v-t->v)+1u);
    fl_ctx[c].depth=(uint16_t)depth;fl_ctx[c].note=v->note;fl_ctx[c].envelope=32767;
}
static void fl_unbind(void){fl_ctx[MC_CPU()&1u].id=0;}
static void fl_envelope(int32_t gain){fl_ctx[MC_CPU()&1u].envelope=(uint16_t)clamp(gain,0,32767);}
/* Exact Fourier projection on a complete 32-point cycle; bounded peak, no DC.
 * A time/phase leaf is never admitted as a waveform fragment by the compiler. */
static void fl_project(fl_node_t *a,const int32_t *y)
{
    int32_t h[FL_POINTS],total=0;
    for(uint32_t k=0;k<FL_POINTS;k++){
        int32_t sum=0;
        for(uint32_t i=0;i<FVM_BLK;i++){
            uint32_t phase=(i*(k/2u+1u))<<27;
            if(k&1u)phase+=0x40000000u;
            sum+=(y[i]*sine_i(phase))>>5;
        }
        h[k]=sum>>14;total+=h[k]<0?-h[k]:h[k];
    }
    a->n=total>64;
    for(uint32_t k=0;k<FL_POINTS;k++)a->wave[k]=(int16_t)(total>64?h[k]*24576/total:0);
}
static void fl_capture(const fvm_prog_t*p,const fvm_env_t*e,fvm_scratch_t*sc,fl_node_t*a)
{
    uint32_t c=MC_CPU()&1u,T[FVM_BLK],W[FVM_BLK],S[FVM_BLK],v[FVM_BLK];
    int32_t y[FVM_BLK];
    a->valid=1;a->note=fl_ctx[c].note;a->envelope=fl_ctx[c].envelope;a->has_number=1;
    /* A/VAR and real literals donate numbers, but only as bounded shape indices.
     * They are never written into a clock multiplier, shift, mask or divisor. */
    a->number=(uint16_t)clamp((int32_t)e->u[FU_AF]>>3,0,8192);
    if(p->life_num){const fvm_ins_t *in=&p->ins[p->life_num-1u];a->number=(uint16_t)((in->sl&512u)?in->k>>8:in->k<<8);}
    a->n=0;memset(a->wave,0,sizeof a->wave);
    if(!p->life_fn)return;
    fvm_prog_t piece=*p;piece.n=p->life_fn;piece.ty=p->life_ft;
    fvm_env_t en=*e;
    for(uint32_t i=0;i<FVM_BLK;i++){T[i]=i*8u;W[i]=i*2048u;S[i]=i*8u;}
    en.arr[FA_T]=T;en.arr[FA_W]=W;en.arr[FA_TS]=S;
    fvm_run(&piece,&en,sc,FVM_BLK,v);
    for(uint32_t i=0;i<FVM_BLK;i++)y[i]=fvm_sample(piece.ty,v[i]);
    fl_project(a,y);
}
/* Classic DRUM receives a slow COLOR operand and donates its COLOR number.
 * It has no compiled fragment: no fabricated bytecode or live-noise resampling. */
static int32_t fl_classic_color(int32_t color)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;if(!fl.on||!id)return color;
    id--;uint32_t g=fl.gate[id],own=g&&fl.node[fl.rd][id].has_number;
    uint32_t count=fl.ncount-own,w=fl.number_weight-(own?g:0);
    if(!count||!w)return color;
    int32_t num=(fl.numbers-(own?fl.node[fl.rd][id].number*(int32_t)g:0))/(int32_t)w;
    int32_t depth=(int32_t)fl_ctx[c].depth*(fl.numbers_amt+fl.link_amt)/200;
    int32_t target=clamp((num/64-color)*depth/512,-32,32)*256;
    fl.shape[id].num+=(target-fl.shape[id].num)/16;
    return clamp(color+(fl.shape[id].num>>8),0,127);
}
static void fl_classic_capture(int32_t color,int32_t envelope,const int32_t*y,uint32_t n)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;(void)y;(void)n;if(!fl.on||!id)return;
    fl_node_t*a=&fl.node[fl.rd^1u][id-1u];
    a->valid=1;a->note=fl_ctx[c].note;a->n=0;a->has_number=1;a->number=(uint16_t)(color*64);
    a->envelope=(uint16_t)clamp(envelope,0,32767);memset(a->wave,0,sizeof a->wave);
}
/* Signed coefficients must stay signed through subtraction AND division. */
static int32_t fl_other_h(uint32_t id,uint32_t k,uint32_t weight)
{
    return (fl.sum[k]-(int32_t)fl.node[fl.rd][id].wave[k]*(int32_t)fl.gate[id])/(int32_t)weight;
}
static __attribute__((noinline)) void fl_run(const fvm_prog_t*p,const fvm_env_t*e,fvm_scratch_t*sc,uint32_t n,uint32_t*out)
{
    fvm_run(p,e,sc,n,out);
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;
    if(!fl.on||!id||!p->n||!n)return;
    if(n>FVM_BLK)n=FVM_BLK;
    id--;const fl_node_t*self=&fl.node[fl.rd][id];fl_node_t*write=&fl.node[fl.rd^1u][id];
    if(!write->valid)fl_capture(p,e,sc,write); /* once, even for WAVE's A/B/unison */
    uint32_t g=fl.gate[id],other=fl.count-(g!=0),weight=fl.weight-g,depth=fl_ctx[c].depth;
    if(!other||!weight||!depth)return;
    uint32_t nd=depth*fl.numbers_amt/100u,fd=depth*fl.fragments_amt/100u,ld=depth*fl.link_amt/100u;
    if(!(nd|fd|ld))return;
    uint32_t own=g&&self->has_number,nw=fl.number_weight-(own?g:0);
    int32_t number=nw?(fl.numbers-(own?self->number*(int32_t)g:0))/(int32_t)nw:0;
    uint32_t gate=weight/(other?other:1u);
    nd=nd*gate/256u;fd=fd*gate/256u;ld=ld*gate/256u;
    int32_t target[FL_POINTS];
    uint32_t mix=fd*3u/4u+ld/3u;if(mix>218u)mix=218u;
    for(uint32_t k=0;k<FL_POINTS;k++){
        int32_t donor=fl_other_h(id,k,weight);
        target[k]=(fd+ld)?(donor*(int32_t)fd+(int32_t)write->wave[k]*(int32_t)ld)/(int32_t)(fd+ld):0;
    }
    int32_t spectral=0,mass=0;
    for(uint32_t k=0;k<FL_POINTS;k++){int32_t h=fl_other_h(id,k,weight);h=h<0?-h:h;spectral+=h*(int32_t)(k/2u+1u);mass+=h;}
    int32_t signature=clamp(number+(mass?spectral*256/mass:0),0,8192);
    fl_shape_t*z=&fl.shape[id];
    uint32_t step=0;
    for(uint32_t i=1;i<n;i++){
        uint32_t a=e->arr[FA_W]?e->arr[FA_W][i]:e->arr[FA_T]?e->arr[FA_T][i]<<8:0;
        uint32_t b=e->arr[FA_W]?e->arr[FA_W][i-1]:e->arr[FA_T]?e->arr[FA_T][i-1]<<8:0;
        int32_t d=(int16_t)(a-b);uint32_t absd=(uint32_t)(d<0?-d:d);if(absd>step)step=absd;
    }
    uint32_t harmonics=step?8192u/step:FL_HARM;
    harmonics=harmonics>2?harmonics-2:1;if(harmonics>FL_HARM)harmonics=FL_HARM;
    /* No new ultra-high partials on high notes or discontinuous BYTE clocks. */
    if(step>8192u)mix=0;
    uint32_t active_harmonic=1;
    for(uint32_t k=0;k<harmonics;k++)if((target[k*2u]<0?-target[k*2u]:target[k*2u])+(target[k*2u+1u]<0?-target[k*2u+1u]:target[k*2u+1u])>256)active_harmonic=k+1u;
    int32_t link_target=(int32_t)ld*32/(int32_t)active_harmonic;
    int32_t peak=0;for(uint32_t i=0;i<n;i++){int32_t s=fvm_sample(p->ty,out[i]);int32_t a=s<0?-s:s;if(a>peak)peak=a;}
    z->level+=(peak*256-z->level)/8;
    for(uint32_t i=0;i<n;i++){
        int32_t base=fvm_sample(p->ty,out[i]);
        int32_t num_target=(int32_t)nd*clamp(number,0,2048)/8;
        z->num+=(num_target-z->num)/128;
        z->link+=(link_target*256-z->link)/128;
        /* NUM: a monotonic cubic shape coefficient, smoothly exchanged. It
         * cannot alter pitch/time/bit topology; max contribution is bounded. */
        int32_t cubic=mulq15(mulq15(base,base),base);
        int32_t shaped=base+mulq15(base-cubic,z->num>>2);
        shaped=shaped*256/(256+(z->num>>10));
        int32_t harmonic=0;
        uint32_t w=e->arr[FA_W]?e->arr[FA_W][i]:e->arr[FA_T]?e->arr[FA_T][i]<<8:0;
        z->color+=(signature*256-z->color)/256;
        int32_t warp=mulq15(sine_i((w<<17)+(uint32_t)((z->color>>8)<<16)),z->link>>8);
        w+=(uint32_t)warp; /* <=1/8 turn / active harmonic, slewed, no raw-audio PM */
        for(uint32_t k=0;k<FL_POINTS;k++){
            int32_t htarget=k/2u<harmonics?target[k]*256:0;
            z->h[k]+=(htarget-z->h[k])/128;
            if(k/2u>=harmonics)continue;
            uint32_t ph=w*(k/2u+1u)<<16;if(k&1u)ph+=0x40000000u;
            harmonic+=mulq15(z->h[k]>>8,sine_i(ph));
        }
        harmonic=harmonic*(z->level>>8)/24576;
        int32_t sample=shaped+(harmonic-shaped)*(int32_t)mix/256;
        sample=clamp(sample,-32767,32767);
        out[i]=p->ty==FT_FIX?(uint32_t)(sample*2):(uint32_t)clamp((sample+32640+128)/256,0,255);
    }
}
