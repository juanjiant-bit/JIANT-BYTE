/* SPDX-License-Identifier: GPL-3.0-only */
/* Cold boot and physical-panel input path, without applying text or resetting
 * envelope/filter/level internally. Optional stereo f32 output is unnormalized. */
#include "../web/emu/felucca_web.c"
#include <assert.h>
#include <math.h>
static double energy;static float peak;
static void frames(unsigned n,FILE*f){while(n){unsigned m=n>128?128:n;web_render(m);if(f)for(unsigned i=0;i<m;i++){float v[2]={out_l[i],out_r[i]};float mono=(v[0]+v[1])*.5f;energy+=(double)mono*mono;if(fabsf(mono)>peak)peak=fabsf(mono);fwrite(v,sizeof v,1,f);}n-=m;}}
int main(int argc,char**argv){
 assert(argc>=2);unsigned preset=argc>2?13:12;
 web_nor_erase();web_seed(123);web_boot();frames(110250,0);
 for(unsigned j=0;j<4&&trk_eng(TSEL)!=ENGI_LOFI;j++){
  web_buttons(1u<<B_EDIT);frames(2205,0);web_enc(EN_PRESET,1);frames(2205,0);web_buttons(0);frames(2205,0);
 }
 assert(trk_eng(TSEL)==ENGI_LOFI);
 for(unsigned j=0;j<20&&TSEL->preset!=preset;j++){web_enc(EN_PRESET,1);frames(2205,0);}
 assert(TSEL->preset==preset&&func_song(TSEL)&&lofi_prg[0].func&&lofi_prg[0].n);
 web_buttons(1u<<B_PLAY);frames(2205,0);web_buttons(0);
 FILE*f=fopen(argv[1],"wb");assert(f);frames(529200,f);fclose(f);
 double rms=sqrt(energy/529200);assert(song.playing&&rms>.008&&rms<.1&&peak<.25);
 web_buttons(1u<<B_PLAY);frames(2205,0);web_buttons(0);frames(FS,0);assert(!song.playing);
 printf("FUNC panel preset %u: boot/encoder/PLAY/STOP, unnormalized RMS %.6f peak %.6f PASS\n",preset,rms,peak);
 return 0;
}
