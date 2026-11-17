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
static __attribute__((noinline)) void fl_run(const fvm_prog_t *p,const fvm_env_t *e,fvm_scratch_t *sc,uint32_t n,uint32_t *out)
{
    uint32_t id=fl_ctx[MC_CPU()&1u].id, i, other, donor=FL_VOICES, depth;
    int32_t offset[FVM_BLK], signal[FL_POINTS];
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
            if(fd) for(i=1;i<FL_VOICES;i++) {
                uint32_t d=(id+i)%FL_VOICES;
                const fl_node_t *a=&fl.node[fl.rd][d];
                if(fl.gate[d]&&a->n&&p->n+a->n<=FVM_MAXINS) {donor=d;break;}
            }
            if(donor<FL_VOICES) {
                const fl_node_t *a=&fl.node[fl.rd][donor];
                fvm_prog_t piece={0};piece.ins=(fvm_ins_t *)a->ins;piece.n=a->n;piece.cap=FL_PREFIX;piece.ty=a->ty;
                /* No states or tables in an admitted fragment. Its original
                 * instructions run on the destination's t/w/A/n/v, never on audio output. */
                fvm_env_t receiver=*e;
                uint32_t phase[FVM_BLK],seconds[FVM_BLK];
                /* BYTE has no w array; express its 256-step cycle in FIX turns.
                 * Missing T follows the receiving formula clock at 8 kHz. */
                if(!receiver.arr[FA_W]) {
                    for(i=0;i<n;i++)phase[i]=e->arr[FA_T]?(e->arr[FA_T][i]&255u)<<8:0;
                    receiver.arr[FA_W]=phase;
                }
                if(!receiver.arr[FA_TS]) {
                    for(i=0;i<n;i++){uint32_t t=e->arr[FA_T]?e->arr[FA_T][i]:0;seconds[i]=(t/125u)*1024u+(t%125u)*1024u/125u;}
                    receiver.arr[FA_TS]=seconds;
                }
                receiver.u[FU_NOTE]=fl_ctx[MC_CPU()&1u].note;
                fvm_run(&piece,&receiver,sc,n,fragment);
                fd=fd*fl.gate[donor]/256u;
            } else fd=0;
            for(i=0;i<FL_POINTS;i++) signal[i]=(fl.sum[i]-(int32_t)self->wave[i]*g)/(int32_t)(other*256u);
            for(i=0;i<n;i++) {
                int32_t v=ld?fl_point(signal,i,n)*(int32_t)ld/128:0;
                if(fd) v+=fvm_sample(fl.node[fl.rd][donor].ty,fragment[i])*(int32_t)fd/128;
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
