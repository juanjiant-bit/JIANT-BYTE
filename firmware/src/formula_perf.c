/* SPDX-License-Identifier: GPL-3.0-only
 * Momentary spectral exchange. Amounts select donor literals, harmonic
 * fragments and phase links in the shared active-voice network. */
static uint32_t formula_punch;
static uint8_t punch_amount[3];
static const uint8_t punch_profile[][3]={
    {80,10,10},{20,80,0},{10,15,85},{60,35,15},{45,65,0},
    {15,50,65},{65,10,60},{30,75,35},{50,45,45},{70,70,25},{55,55,70}
};
static void punch_exchange_tick(void)
{
    for(unsigned axis=0;axis<3;axis++){
        int target=0;
        for(uint32_t bits=formula_punch;bits;bits&=bits-1){
            int value=punch_profile[__builtin_ctz(bits)][axis];
            if(value>target)target=value;
        }
        int current=punch_amount[axis];
        punch_amount[axis]=(uint8_t)(current+clamp(target-current,-2,3));
    }
}
#define formula_run fl_run
