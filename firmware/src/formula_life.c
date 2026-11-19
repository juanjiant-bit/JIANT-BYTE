/* SPDX-License-Identifier: GPL-3.0-only
 * LIFE exchanges band-limited waveform fragments, never raw clocks or audio-rate
 * offsets. Donors are evaluated once per voice/control block on a complete cycle;
 * their DC-free harmonic coefficients are grafted at the receiver's own phase. */
static uint32_t isqrt32(uint32_t); /* shared integer square root, filt.c */
#define FL_VOICES (NTRK * NVOICE)
#define FL_HARM 8u
#define FL_POINTS (FL_HARM * 2u)
typedef struct {
    int16_t wave[FL_POINTS]; /* sine/cosine coefficients, not decimated audio */
    uint16_t envelope, number;
    uint8_t note,valid,has_number,n;
} fl_node_t;
typedef struct {int32_t h[FL_POINTS],num,link,level,color,mix,num_mix,ein,eout,gain;} fl_shape_t;
static struct {
    fl_node_t node[2][FL_VOICES];
    fl_shape_t shape[FL_VOICES];
    int32_t sum[FL_POINTS],numbers;
    uint32_t strikes;
    uint16_t strike[FL_VOICES];
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
    int32_t state[FVM_MAXSTATE];
    fvm_env_t en=*e;
    if(p->func){memcpy(state,e->state,p->nstate*sizeof *state);en.state=state;en.u[FU_HZ]=(FS/32u)*65536u;}
    for(uint32_t i=0;i<FVM_BLK;i++){T[i]=i*8u;W[i]=i*2048u;S[i]=i*8u;}
    en.arr[FA_T]=T;en.arr[FA_W]=W;en.arr[FA_TS]=S;
    fvm_run(&piece,&en,sc,FVM_BLK,v);
    for(uint32_t i=0;i<FVM_BLK;i++)y[i]=fvm_sample(piece.ty,v[i]);
    fl_project(a,y);
}
static int32_t fl_other_h(uint32_t id,uint32_t k,uint32_t weight);
/* Classic DRUM receives a slow COLOR operand and donates its COLOR number.
 * It has no compiled fragment: no fabricated bytecode or live-noise resampling. */
static int32_t fl_classic_color(int32_t color)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;if(!fl.on||!id)return color;
    id--;uint32_t g=fl.gate[id],own=g&&fl.node[fl.rd][id].has_number;
    uint32_t count=fl.ncount-own,w=fl.number_weight-(own?g:0);
    int32_t target=0,ft=0;
    if(count&&w){
        int32_t num=(fl.numbers-(own?fl.node[fl.rd][id].number*(int32_t)g:0))/(int32_t)w;
        int32_t depth=(int32_t)fl_ctx[c].depth*(fl.numbers_amt+fl.link_amt)/200;
        target=clamp((num/64-color)*depth/512,-32,32)*256;
    }
    {   /* (DRUM-W) the others' FRAGMENTS too: their spectrum's centroid pulls the colour (the pad's places) */
        uint32_t wf=fl.weight-g;
        if(fl.count-(g!=0)&&wf&&fl.fragments_amt){
            int32_t sp=0,ms=0;
            for(uint32_t k=0;k<FL_POINTS;k++){int32_t h=fl_other_h(id,k,wf);h=h<0?-h:h;sp+=h*(int32_t)(k/2u+1u);ms+=h;}
            if(ms>64){
                int32_t cen=sp*127/(ms*(int32_t)FL_HARM);
                ft=clamp((cen-color)*(int32_t)fl_ctx[c].depth*fl.fragments_amt/200,-32*256,32*256);   /* (in 1/256: a small LIFE heard too) */
            }
        }
    }
    target=clamp(target+ft,-48*256,48*256);
    {int32_t d=target-fl.shape[id].num;fl.shape[id].num+=d/16+(d>0)-(d<0);}   /* (a small target reached too) */
    return clamp(color+(fl.shape[id].num>>8),0,127);
}
static void fl_classic_capture(int32_t color,int32_t envelope,const int32_t*y,uint32_t n)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;if(!fl.on||!id)return;
    fl_node_t*a=&fl.node[fl.rd^1u][id-1u];
    a->valid=1;a->note=fl_ctx[c].note;a->n=0;a->has_number=1;a->number=(uint16_t)(color*64);
    a->envelope=(uint16_t)clamp(envelope,0,32767);memset(a->wave,0,sizeof a->wave);
    if(y&&n==FVM_BLK)fl_project(a,y);   /* (DRUM-W) its table's cycle playing now: a FRAGMENT too */
}
/* Signed coefficients must stay signed through subtraction AND division. */
static int32_t fl_other_h(uint32_t id,uint32_t k,uint32_t weight)
{
    return (fl.sum[k]-(int32_t)fl.node[fl.rd][id].wave[k]*(int32_t)fl.gate[id])/(int32_t)weight;
}
/* (byte-cpu-light) LIFE on a formula's output already made (fl_run's, or WAVE's table read: eng_wave.c) */
static uint8_t fl_cap_m=0xFF;           /* (byte-cpu-light, life.c) bit p: part p's voices captured (its LIFE on, not shed) */
static __attribute__((noinline)) void fl_post(const fvm_prog_t*p,const fvm_env_t*e,fvm_scratch_t*sc,uint32_t n,uint32_t*out)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;
    if(!fl.on||!id||!p->n||!n)return;
    if(n>FVM_BLK)n=FVM_BLK;
    id--;const fl_node_t*self=&fl.node[fl.rd][id];fl_node_t*write=&fl.node[fl.rd^1u][id];
    uint32_t g=fl.gate[id],other=fl.count-(g!=0),weight=fl.weight-g,depth=fl_ctx[c].depth;
    if(!write->valid&&(g||depth||((fl_cap_m>>(id/NVOICE))&1u)))fl_capture(p,e,sc,write); /* once, even for WAVE's A/B/unison;
                                         * (byte-cpu-light) not for a voice LIFE neither hears nor gives (shed) */
    fl_shape_t*z=&fl.shape[id];
    if(!depth||!(fl.numbers_amt|fl.fragments_amt|fl.link_amt)){memset(z,0,sizeof *z);return;}
    uint32_t connected=other&&weight;
    if(!connected&&z->mix<256&&z->num_mix<256){z->mix=z->num_mix=0;return;}
    if(!connected)depth=0;
    uint32_t nd=depth*fl.numbers_amt/100u,fd=depth*fl.fragments_amt/100u,ld=depth*fl.link_amt/100u;

    uint32_t own=g&&self->has_number,nw=fl.number_weight-(own?g:0);
    int32_t number=nw?(fl.numbers-(own?self->number*(int32_t)g:0))/(int32_t)nw:0;
    /* Envelope/LEVEL choose and weight donors, not a second wet-depth control.
     * The receiver already applies its own amplitude envelope after the VM. */
    int32_t target[FL_POINTS];
    uint32_t edge=depth>126u?(depth-126u)*256u/130u:0u;
    uint32_t impact=weight?(fl.strikes-fl.strike[id]*g)/weight:0;
    uint32_t travel=fd*3u/2u+ld/2u;
    uint32_t mix=travel*256u/(160u+travel); /* default: 50 ~24%, 100 ~64% */
    for(uint32_t k=0;k<FL_POINTS;k++){
        int32_t donor=connected?fl_other_h(id,k,weight):0;
        target[k]=(fd+ld)?(donor*(int32_t)fd+(int32_t)write->wave[k]*(int32_t)ld)/(int32_t)(fd+ld):0;
    }
    /* Upper quarter: exchange the donor's spectral pieces at harmonic multiples.
     * This changes the formula's shape, not its gain or oscillator clock. Every
     * donor contributes; LINK adds a 45 ms response to fresh attacks. Keep the
     * established low/mid range exactly unchanged. All new partials still pass
     * the receiver's pitch-dependent harmonic budget below. */
    if(edge&&(fd||ld)){
        int32_t expanded[FL_POINTS]={0};
        uint32_t shape=(uint32_t)clamp(number,0,8192)*192u/8192u;
        uint32_t collision=clamp((int32_t)other-1,0,3)*16u;
        uint32_t motion=(impact*3u/4u+collision)*ld/256u;
        uint32_t third=clamp((int32_t)(shape+motion),0,256);
        uint32_t amount=edge*(fd+ld)/512u;
        for(uint32_t k=0;k<FL_POINTS;k++){
            uint32_t h=k/2u+1u,part=k&1u;
            expanded[k]+=target[k]/4;
            if(h*2u<=FL_HARM)expanded[(h*2u-1u)*2u+part]+=target[k]*(int32_t)(256u-third)*3/1024;
            if(h*3u<=FL_HARM)expanded[(h*3u-1u)*2u+part]+=target[k]*(int32_t)third*3/1024;
        }
        for(uint32_t k=0;k<FL_POINTS;k++)target[k]+=(expanded[k]-target[k])*(int32_t)amount/256;
        mix+=(256u-mix)*amount/256u;
    }
    int32_t target_mass=0;
    for(uint32_t k=0;k<FL_POINTS;k++)target_mass+=target[k]<0?-target[k]:target[k];
    if(target_mass>64)for(uint32_t k=0;k<FL_POINTS;k++)target[k]=target[k]*24576/target_mass;
    else mix=0;
    int32_t spectral=0,mass=0;
    for(uint32_t k=0;k<FL_POINTS;k++){int32_t h=connected?fl_other_h(id,k,weight):0;h=h<0?-h:h;spectral+=h*(int32_t)(k/2u+1u);mass+=h;}
    int32_t signature=clamp(number+(mass?spectral*256/mass:0),0,8192);
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
    int32_t input_energy=0,output_energy=0,output_peak=0;
    /* (byte-cpu-light) The slews run once a call, not once a sample: each moves by the share n samples of its
     * one-pole would (n*256/(D+n/2) of the way, within 1/256 of (1-1/D)^n up to 32), the harmonics' levels and the
     * two mixes stepped linearly across the call. The partials from one sine and one cosine (a recurrence), not one
     * table read each. The sound as before within the slews' rounding; a fraction of the instructions. */
    int32_t num_target=number>0?number*32767/(number+512):0;
    uint32_t ns=n<<VH_SH();                     /* (byte-cpu-light) at half the rate a sample is two: the slews in time */
    int32_t f128=(int32_t)(ns*256u/(128u+ns/2u)),f64=(int32_t)(ns*256u/(64u+ns/2u)),f256=(int32_t)(ns*256u/(256u+ns/2u));
    int32_t num_mix0=z->num_mix,mix0=z->mix,link0=z->link,num0=z->num,color0=z->color;
    z->num+=(int32_t)(((int64_t)(num_target*256-z->num)*f128)>>8);
    z->link+=(int32_t)(((int64_t)(link_target*256-z->link)*f128)>>8);
    z->num_mix+=(int32_t)(((int64_t)((harmonics>=3u?(int32_t)(nd*3u/4u)*256:0)-z->num_mix)*f64)>>8);
    z->mix+=(int32_t)(((int64_t)((int32_t)mix*256-z->mix)*f64)>>8);
    z->color+=(int32_t)(((int64_t)(signature*256-z->color)*f256)>>8);
    int32_t hq[FL_POINTS];
    for(uint32_t k=0;k<FL_POINTS;k++){
        int32_t htarget=k/2u<harmonics?target[k]*256:0;
        z->h[k]+=(int32_t)(((int64_t)(htarget-z->h[k])*f128)>>8);
        hq[k]=z->h[k]>>8;                            /* (a call's levels held: their slew's step is a call's) */
    }
    int32_t lev=z->level>>8;
    int32_t nm=num_mix0,dnm=(z->num_mix-num_mix0)/(int32_t)n,mx=mix0,dmx=(z->mix-mix0)/(int32_t)n;
    int32_t index=((num0+z->num)/2)>>8,lnk=((link0+z->link)/2)>>8;
    uint32_t col=(uint32_t)(((color0+z->color)/2)>>8)<<16;
    int32_t nscale=lev*32768/32767,hscale=lev*65536/24576;   /* (Q15 / Q16 for the two divisions a sample) */
    uint32_t nh=harmonics;
    /* The partials at half the rate, the odd samples between two even ones: they stay under fs / 8 (harmonics
     * from the phase's step, above), so a 22 kHz grid holds them (linear: -1.8 dB at fs / 8 at most). */
    int32_t H[FVM_BLK/2u+1u];
    uint32_t nhalf=(n+1u)/2u;
    if(mix0>=256||z->mix>=256){
        for(uint32_t j=0;j<nhalf;j++){
            uint32_t i=2u*j;
            uint32_t w=e->arr[FA_W]?e->arr[FA_W][i]:e->arr[FA_T]?e->arr[FA_T][i]<<8:0;
            int32_t warp=lnk?mulq15(sine_i((w<<17)+col),lnk):0;
            w+=(uint32_t)warp; /* <=1/8 turn / active harmonic, slewed, no raw-audio PM */
            uint32_t ph=w<<16;
            int32_t s1=sine_i(ph),c1=sine_i(ph+0x40000000u),c2=c1*2;   /* sin, cos (Q15) */
            int32_t sp=0,cp=32767,sk=s1,ck=c1,harmonic=0;
            for(uint32_t k=0;k<nh;k++){
                harmonic+=(hq[2u*k]*sk+hq[2u*k+1u]*ck)>>15;
                int32_t sn=((c2*sk)>>15)-sp,cn=((c2*ck)>>15)-cp;   /* sin/cos((k+2)w) */
                sp=sk;cp=ck;sk=sn;ck=cn;
            }
            H[j]=(int32_t)(((int64_t)harmonic*hscale)>>16);
        }
        H[nhalf]=H[nhalf-1u];
    }
    for(uint32_t i=0;i<n;i++){
        int32_t base=fvm_sample(p->ty,out[i]);
        input_energy+=(base*base)>>6;
        /* NUM turns the exchanged literal/VAR into a bounded coefficient of
         * a phase-locked 2nd/3rd-harmonic expression. Unlike a cubic applied to
         * a quiet signal, this remains a timbre change at any patch level. */
        int32_t shaped=base;
        if(nm>=256&&harmonics>=3u){
            uint32_t w=e->arr[FA_W]?e->arr[FA_W][i]:e->arr[FA_T]?e->arr[FA_T][i]<<8:0;
            int32_t numeric=mulq15(sine_i(w*2u<<16),32767-index)+mulq15(sine_i(w*3u<<16),index);
            numeric=(numeric*nscale)>>15;
            shaped=base+(((numeric-base)*(nm>>8))>>8);
        }
        int32_t sample=shaped;
        if(mx>=256){
            int32_t harmonic=i&1u?(H[i/2u]+H[i/2u+1u])/2:H[i/2u];
            sample=shaped+(((harmonic-shaped)*(mx>>8))>>8);
        }
        nm+=dnm;mx+=dmx;
        sample=clamp(sample,-32767,32767);
        output_energy+=(sample*sample)>>6;
        int32_t abs_sample=sample<0?-sample:sample;if(abs_sample>output_peak)output_peak=abs_sample;
        out[i]=(uint32_t)sample;
    }
    /* Redistribution must not turn the wet sound into a quieter patch. Follow
     * energy slowly, cap makeup at +3.5 dB and at available peak headroom. */
    z->ein+=(input_energy-z->ein)/32;z->eout+=(output_energy-z->eout)/32;
    uint32_t ei=(uint32_t)z->ein,eo=(uint32_t)z->eout;
    while(ei>65535u||eo>65535u){ei>>=1;eo>>=1;}
    int32_t gain=eo?(int32_t)isqrt32((ei<<16)/eo):256;
    gain=clamp(gain,256,384);
    if(!z->gain)z->gain=65536;
    z->gain+=(gain*256-z->gain)/8;gain=z->gain>>8;
    if(output_peak&&gain>32767*256/output_peak)gain=32767*256/output_peak;
    for(uint32_t i=0;i<n;i++){
        int32_t sample=(int32_t)out[i]*gain/256;
        out[i]=p->ty==FT_FIX?(uint32_t)(sample*2):(uint32_t)clamp((sample+32640+128)/256,0,255);
    }
}

static __attribute__((noinline)) void fl_run(const fvm_prog_t*p,const fvm_env_t*e,fvm_scratch_t*sc,uint32_t n,uint32_t*out)
{
    fvm_run(p,e,sc,n,out);
    fl_post(p,e,sc,n,out);
}
