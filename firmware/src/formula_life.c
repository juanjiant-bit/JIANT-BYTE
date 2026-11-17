/* SPDX-License-Identifier: GPL-3.0-only
 * LIFE operates inside the instrument VM. A donor's compiled, stateless prefix
 * is evaluated in the receiving note's environment and grafted into a typed
 * time/phase operand. Constants and note signals have separate controls.
 * Read/write banks never alias: voices and cores cannot observe render order. */
#define FL_VOICES (NTRK * NVOICE)
#define FL_POINTS 8u
#define FL_PREFIX 8u
typedef struct {
    fvm_ins_t ins[FL_PREFIX];
    int16_t wave[FL_POINTS];
    uint16_t envelope, number;                        /* literal in Q8, 0..32 */
    uint8_t n, ty, note, valid, has_number;
} fl_node_t;
static struct {
    fl_node_t node[2][FL_VOICES];
    int32_t sum[FL_POINTS], numbers;
    uint16_t gate[FL_VOICES], depth[NTRK];
    uint16_t count, ncount, weight, number_weight;
    uint8_t rd, on, numbers_amt, fragments_amt, link_amt;
} fl; /* dedicated BSS: preserve the FX pool's 8 KiB runtime headroom */
static struct { uint16_t id, depth, envelope; uint8_t note; } fl_ctx[2];
static void fl_bind(const track_t *t,const voice_t *v,uint32_t depth)
{
    uint32_t c=MC_CPU()&1u;
    fl_ctx[c].id=(uint16_t)((t-trk)*NVOICE+(v-t->v)+1u);
    fl_ctx[c].depth=(uint16_t)depth;fl_ctx[c].note=v->note;fl_ctx[c].envelope=32767;
}
static void fl_envelope(int32_t gain) {fl_ctx[MC_CPU()&1u].envelope=(uint16_t)clamp(gain,0,32767);}
static void fl_unbind(void) {fl_ctx[MC_CPU()&1u].id=0;}
static int32_t fl_point(const int32_t *p,uint32_t i,uint32_t n)
{
    uint32_t x=n>1u?i*((FL_POINTS-1u)<<8)/(n-1u):0u,k=x>>8;
    if(k>=FL_POINTS-1u)return p[FL_POINTS-1u];
    return p[k]+((p[k+1u]-p[k])*(int32_t)(x&255u)>>8);
}
/* Largest complete stateless expression in each donor's fair share. */
static uint32_t fl_prefix(const fl_node_t *a,uint32_t budget)
{
    uint32_t i,sp=0,last=0;
    for(i=0;i<a->n&&i<budget;i++) {
        const fvm_ins_t *in=&a->ins[i];
        if(in->op==FO_PUSH)sp++;
        else if(FO_ISBIN(in->op)&&in->md==FM_STK)sp--;
        else if(in->op==FO_SEL)sp-=2;
        if(sp==1)last=i+1;
    }
    return last;
}
/* Classic DRUM has no formula: exchange its COLOR literal and actual signal,
 * and receive NUM/LINK through a temporary COLOR operand, never its saved patch. */
static int32_t fl_classic_color(int32_t color)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id,depth=fl_ctx[c].depth;
    if(!fl.on||!id||!depth)return color;
    id--;uint32_t g=fl.gate[id],own=g&&fl.node[fl.rd][id].has_number;
    uint32_t count=fl.ncount-own,w=fl.number_weight-(own?g:0),other=fl.count-(g!=0);
    int32_t value=color;
    if(count&&w&&fl.numbers_amt){
        int32_t num=(fl.numbers-(own?fl.node[fl.rd][id].number*(int32_t)g:0))/(int32_t)w;
        uint32_t amount=depth*fl.numbers_amt/100u*w/(count*256u);
        value+=(num/64-color)*(int32_t)amount/256;
    }
    if(other&&fl.link_amt){
        int32_t signal=0;for(uint32_t i=0;i<FL_POINTS;i++)signal+=(fl.sum[i]-(int32_t)fl.node[fl.rd][id].wave[i]*g)/(int32_t)(other*256u);
        signal/=FL_POINTS;
        value+=signal*(int32_t)(depth*fl.link_amt/100u)/131072;
    }
    return clamp(value,0,127);
}
static void fl_classic_capture(int32_t color,int32_t envelope,const int32_t *y,uint32_t n)
{
    uint32_t c=MC_CPU()&1u,id=fl_ctx[c].id;if(!fl.on||!id||!n)return;
    fl_node_t *a=&fl.node[fl.rd^1u][id-1u];
    a->valid=1;a->note=fl_ctx[c].note;a->n=0;a->has_number=1;a->number=(uint16_t)(color*64);
    a->envelope=(uint16_t)clamp(envelope,0,32767);
    for(uint32_t i=0;i<FL_POINTS;i++)a->wave[i]=(int16_t)clamp(y[i*(n-1u)/(FL_POINTS-1u)],-32767,32767);
}
static __attribute__((noinline)) void fl_run(const fvm_prog_t *p,const fvm_env_t *e,fvm_scratch_t *sc,uint32_t n,uint32_t *out)
{
    uint32_t id=fl_ctx[MC_CPU()&1u].id, i, other, depth;
    int32_t offset[FVM_BLK], signal[FL_POINTS], graft[FVM_BLK];
    uint32_t fragment[FVM_BLK];
    fvm_patch_t patch={0};
    fl_node_t *write;
    const fl_node_t *self;
    if(!fl.on||!id||!p->n) {fvm_run(p,e,sc,n,out);return;}
    if(n>FVM_BLK)n=FVM_BLK;
    id--;self=&fl.node[fl.rd][id];write=&fl.node[fl.rd^1u][id];
    depth=fl_ctx[MC_CPU()&1u].depth;
    /* No self feedback. A release/velocity gates its contribution; the average
     * is divided by voice count, so quiet tails stay quiet rather than normalized up. */
    other=fl.count-(fl.gate[id]!=0u);
    if(depth&&other) {
        uint32_t g=fl.gate[id], nd=depth*fl.numbers_amt/100u;
        uint32_t fd=depth*fl.fragments_amt/100u, ld=depth*fl.link_amt/100u;
        if(p->life_num&&nd) {
            uint32_t own=g&&self->has_number, w=fl.number_weight-(own?g:0u);
            uint32_t count=fl.ncount-own;
            if(w&&count) {
                const fvm_ins_t *in=&p->ins[p->life_num-1u];
                int32_t original=(in->sl&512u)?(int32_t)(in->k>>8):(int32_t)in->k*256;
                int32_t value=(fl.numbers-(own?self->number*(int32_t)g:0))/(int32_t)w;
                uint32_t amount=nd*w/(count*256u);
                value=original+(value-original)*(int32_t)amount/256;
                patch.number=p->life_num;patch.value=(uint32_t)((in->sl&512u)?value*256:(value+128)/256);
            }
        }
        if(p->life_in&&(fd||ld)) {
            uint32_t donors=0,d,quota;
            memset(graft,0,sizeof graft);
            if(fd)for(d=0;d<FL_VOICES;d++)if(d!=id&&fl.gate[d]&&fl.node[fl.rd][d].n)donors++;
            /* Every eligible donor gets the same budget. <=64 extra instructions
             * total, even at 48 snapshots; never select a neighbour or drop a voice. */
            quota=donors?FVM_MAXINS/donors:0;
            if(donors) {
                fvm_env_t receiver=*e;
                uint32_t phase[FVM_BLK],seconds[FVM_BLK];
                if(!receiver.arr[FA_W]) {
                    for(i=0;i<n;i++)phase[i]=e->arr[FA_T]?(e->arr[FA_T][i]&255u)<<8:0;
                    receiver.arr[FA_W]=phase;
                }
                if(!receiver.arr[FA_TS]) {
                    for(i=0;i<n;i++){uint32_t t=e->arr[FA_T]?e->arr[FA_T][i]:0;seconds[i]=(t/125u)*1024u+(t%125u)*1024u/125u;}
                    receiver.arr[FA_TS]=seconds;
                }
                receiver.u[FU_NOTE]=fl_ctx[MC_CPU()&1u].note;
                for(d=0;d<FL_VOICES;d++)if(d!=id&&fl.gate[d]&&fl.node[fl.rd][d].n) {
                    const fl_node_t *a=&fl.node[fl.rd][d];
                    fvm_prog_t piece={0};piece.ins=(fvm_ins_t *)a->ins;piece.n=fl_prefix(a,quota);piece.cap=FL_PREFIX;
                    piece.ty=(uint8_t)((a->ins[piece.n-1u].sl>>8)&1u);
                    fvm_run(&piece,&receiver,sc,n,fragment);
                    for(i=0;i<n;i++)graft[i]+=fvm_sample(piece.ty,fragment[i])*(int32_t)fl.gate[d];
                }
                for(i=0;i<n;i++)graft[i]/=(int32_t)(donors*256u);
            }
            for(i=0;i<FL_POINTS;i++) signal[i]=(fl.sum[i]-(int32_t)self->wave[i]*g)/(int32_t)(other*256u);
            for(i=0;i<n;i++) {
                int32_t v=ld?fl_point(signal,i,n)*(int32_t)ld/128:0;
                if(fd) v+=graft[i]*(int32_t)fd/128;
                /* FIX phase in turns; INT time in 256-step cycles. The clock
                 * itself, oscillator accumulator, MIDI note and sequencer never change. */
                offset[i]=(p->ins[p->life_in-1u].sl&512u)?v:v/256;
            }
            patch.input=p->life_in;patch.offset=offset;
        }
    }
    fvm_exec(p,e,sc,n,out,patch.input||patch.number?&patch:0);
    write->envelope=fl_ctx[MC_CPU()&1u].envelope;write->valid=1;write->note=fl_ctx[MC_CPU()&1u].note;
    write->n=p->life_fn;write->ty=p->life_ft;
    if(write->n)memcpy(write->ins,p->ins,write->n*sizeof *p->ins);
    write->has_number=p->life_num!=0;
    if(p->life_num) {const fvm_ins_t *in=&p->ins[p->life_num-1u];write->number=(uint16_t)((in->sl&512u)?in->k>>8:in->k<<8);}
    for(i=0;i<FL_POINTS;i++)write->wave[i]=(int16_t)fvm_sample(p->ty,out[n?i*(n-1u)/(FL_POINTS-1u):0u]);
}
