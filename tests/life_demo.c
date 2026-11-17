/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible 8-second comparison: cc -O2 -ffp-contract=off -w -Ibuild/gen
 * -Ifirmware/src tests/life_demo.c -lm -o build/host-life-demo
 * build/host-life-demo 0 build/life-0.wav (repeat with 50 / 100). */
#include "../web/emu/felucca_web.c"
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    web_nor_erase();web_boot();
    for(unsigned n=0;n<FS;n+=128)web_render(128);
    web_test_heavy();perf_held=0;
    life_v[LIFE_AMT]=clamp(atoi(argv[1]),0,100);
    life_v[LIFE_NUMBERS]=65;life_v[LIFE_FRAGMENTS]=60;life_v[LIFE_LINK]=40;
    web_buttons(1u<<B_PLAY);web_render(128);web_buttons(0);
    FILE *f=fopen(argv[2],"wb");if(!f)return 2;
    wav_hdr(f,FS*8);
    for(unsigned n=0;n<FS*8;n+=128){
        unsigned m=FS*8-n<128?FS*8-n:128;web_render(m);
        for(unsigned j=0;j<m;j++){
            int16_t s[2]={(int16_t)(out_l[j]*32767),(int16_t)(out_r[j]*32767)};
            fwrite(s,sizeof s,1,f);
        }
    }
    fclose(f);return 0;
}
