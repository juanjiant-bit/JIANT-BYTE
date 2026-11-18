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
    uint32_t shift=duration>65536u?2u:0u;
    uint32_t down=(((duration-a)>>shift)*32767u)/(duration>>shift);
    return (int32_t)(up*down/32768u);
}
static int32_t fvm_music_tone(int32_t note,uint32_t tick,int32_t color)
{
    uint32_t inc=pitch_inc((uint32_t)clamp(note,0,127)*16u),phase=inc*tick;
    int32_t a=sine_i(phase),b=inc<0x20000000u?sine_i(phase*2u):0,c=inc<0x15555555u?sine_i(phase*3u):0;
    color=clamp(color,0,65536);
    return (a*3+mulq15(b+c/2,color>>1))/5;
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
        if(kind==2u){uint32_t phase=samples*pitch_inc(33u*16u);phase+=(uint32_t)(((uint64_t)samples*0x300000000ull)/(samples+900u));signal=sine_i(phase);}
        else if(kind==3u)signal=noise*3/5+sine_i(samples*pitch_inc(54u*16u))/3;
        else signal=noise/2;
    }
    return (uint32_t)(mulq15(signal,env)*2);
}
