/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
int main(int argc,char**argv){
 if(argc<4)return 2;const char*genre=argv[1];unsigned amt=atoi(argv[2]),g=0;
 for(;g<NGENRE;g++)if(strstr(GENRE[g].name,genre))break;if(g==NGENRE)return 3;
 ui_power_on();gp.genre=g+1;gp.seed=37;gp.energy=65;gp.form=2;gp.complex=60;gp.human=0;gp.swing=GA_AUTO;for(unsigned k=0;k<GP_N;k++)gp.amt[k]=GA_AUTO;
 gen_go();if(life_v[LIFE_AMT])return 4;
 nav_put(SEC_HOME,"LIFE");frame();for(unsigned k=0;k<amt;k++)turn(EN_K1,1);
 printf("genre %s LIFE before PLAY %d default controls %d/%d/%d tracks%u\n",GENRE[g].name,life_v[0],life_v[1],life_v[2],life_v[3],trk_live);
 press(B_PLAY);song.master_q12=4096;
 printf("LIFE after PLAY %d rows%u\n",life_v[0],chain_config.count);
 /* UI performance gesture after playback starts is the listening comparison. */
 nav_put(SEC_HOME,"LIFE");frame();while(life_v[0]<(int)amt)turn(EN_K1,1);
 FILE*f=fopen(argv[3],"wb");if(!f)return 5;int32_t out[2*CTL];uint64_t gates=0,depths=0,mixes=0,voices=0;unsigned blocks=0,connected=0,rows=0,min=100,max=0;
 for(unsigned o=0;o<FS*(strstr(genre,"FUGA")?72u:24u);o+=CTL){
  mix_block(out,CTL);song_poll();fwrite(out,sizeof out,1,f);blocks++;
  if(life_v[0]<min)min=life_v[0];if(life_v[0]>max)max=life_v[0];rows|=1u<<chain.row;
  for(unsigned id=0;id<FL_VOICES;id++)if(fl.gate[id]){
   voices++;gates+=fl.gate[id];depths+=fl.depth[id/NVOICE];
   if(fl.count>1){mixes+=(unsigned)(fl.shape[id].mix>>8);connected++;}
  }
 }
 fclose(f);printf("range%u..%u rows%x gate%.1f depth%.1f effective_mix%.2f%% connected%u samples%u\n",min,max,rows,voices?(double)gates/voices:0,voices?(double)depths/voices:0,connected?(double)mixes/connected/2.56:0,connected,blocks*CTL);return min!=amt||max!=amt||(amt&&strstr(genre,"FUGA")&&(!connected||rows==1));
}
