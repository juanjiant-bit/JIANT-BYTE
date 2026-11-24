/* SPDX-License-Identifier: GPL-3.0-only */
/* Stateless, bounded musical voices for FUNC. Notes use an integer sample clock,
 * not a quantized beat phase, so changing tempo does not detune the oscillators.
 * The program supplies pitch, age in beats and timbre; it owns the score. */
static int32_t fvm_music_env(int32_t age,uint32_t duration)
{
    if(age<0||(uint32_t)age>=duration)return 0;
    uint32_t a=(uint32_t)age,up=a<1024u?a*32u:32767u;
    /* Four-beat durations need two guard bits before the 32-bit product.
     * At most one Q15 unit of error; no 64-bit division in the envelope. */
    uint32_t shift=duration>65536u?2u:0u,down;
    if(!(duration&(duration-1u)))               /* (byte-cpu-light) a power of two (FORM's 1, 4 beats; the kick): a shift,
                                                 * the same value (no division a sample on the FM-1) */
        down=(((duration-a)>>shift)*32767u)>>(31u-(uint32_t)__builtin_clz(duration>>shift));
    else if(duration==24576u&&!shift)           /* (engopt) the snare's 3 x 8192: a shift and / 3 as a product, exact */
        down=(uint32_t)(((uint64_t)((((duration-a)*32767u)>>13))*0xAAAAAAABu)>>33);
    else
        down=(((duration-a)>>shift)*32767u)/(duration>>shift);
    return (int32_t)(up*down/32768u);
}
static int32_t fvm_music_tone(int32_t note,uint32_t tick,int32_t color)
{
    uint32_t inc=pitch_inc((uint32_t)clamp(note,0,127)*16u),phase=inc*tick;
    int32_t a=sine_i(phase),b=inc<0x20000000u?sine_i(phase*2u):0,c=inc<0x15555555u?sine_i(phase*3u):0;
    color=clamp(color,0,65536);
    return (a*3+mulq15(b+c/2,color>>1))/5;
}
/* (funcplay) a call through FUNC's groove (fu_cx_t): 1 muted; its age (y a note's, x a drum's) and pitch moved */
static __attribute__((noinline)) int fu_music(uint32_t kind,uint32_t*x,uint32_t*y,const fvm_env_t*e,uint32_t i)
{
    const fu_cx_t*c=e->fx;uint32_t*age=kind<2u?y:x,P=e->arr[FA_BEAT]?e->arr[FA_BEAT][i]:0u,h;
    if(c->mask>>(kind<2u)&1u)return 1;
    h=fvm_hash(((P-*age)>>14)*0x9E3779B1u^c->seed);   /* (the 1/16 it began on) */
    if((int32_t)(h&127u)<-2*c->dens)return 1;
    if((int32_t)(h>>8&127u)<2*c->dens&&(int32_t)*age>=0)*age&=0x3FFFu;
    if(kind<2u){int32_t r=(int32_t)(h>>16&127u),o=c->range>0&&r<2*c->range?12:c->range<0&&r<-2*c->range?-12:0;
        if(c->notes)o+=(int32_t)(0xC050700u>>(h>>24&3u)*8u&255u);*x+=(uint32_t)o;}
    return 0;
}
static uint32_t fvm_music(uint32_t kind,uint32_t x,uint32_t y,uint32_t z,const fvm_env_t*e,uint32_t i)
{
    uint32_t tick=e->arr[FA_T]?e->arr[FA_T][i]:0;
    int32_t age=(int32_t)(kind<2u?y:x),env,signal=0;
    if(kind==0u){env=fvm_music_env(age,65536u);signal=fvm_music_tone((int32_t)x,tick,(int32_t)z);}
    else if(kind==1u){
        env=fvm_music_env(age,4u*65536u);int32_t root=clamp((int32_t)x,0,103),color=clamp((int32_t)z,0,65536);
        int32_t third=color<32768?3:4,top=color>49152?10:12;
        if(color>=32768)root=clamp(root+12,0,115);
        signal=(fvm_music_tone(root,tick,0)+fvm_music_tone(root+third,tick,0)+fvm_music_tone(root+7,tick,0)+fvm_music_tone(root+top,tick,0))/4;
    }else{
        uint32_t duration=kind==2u?32768u:kind==3u?24576u:8192u;
        env=fvm_music_env(age,duration);if(!env)return 0;
        uint32_t samples=(uint32_t)(((uint64_t)(uint32_t)age*(e->u[FU_BPS]?e->u[FU_BPS]:FS/2u))>>16);
        int32_t noise=(int32_t)(fvm_hash(tick+kind*977u)>>16)-32768;
        if(kind==2u){uint32_t phase=samples*pitch_inc(33u*16u),d=samples+900u;
            /* (engopt) the sweep, 3 x 2^32 x s / (s + 900) mod 2^32 = -ceil(2700 x 2^32 / d): two 32-bit divisions in
             * 16-bit steps for d < 2^16 (a half beat down to ~41 BPM), not the 64-bit one a sample (a library call on
             * the FM-1); exact for every s */
            if(d<65536u){uint32_t q1=(2700u<<16)/d,r1=(2700u<<16)-q1*d,t=r1<<16,q2=t/d;phase-=(q1<<16)+q2+(t!=q2*d);}
            else phase+=(uint32_t)(((uint64_t)samples*0x300000000ull)/d);
            signal=sine_i(phase);}
        else if(kind==3u)signal=noise*3/5+sine_i(samples*pitch_inc(54u*16u))/3;
        else signal=noise/2;
    }
    return (uint32_t)(mulq15(signal,env)*2);
}
