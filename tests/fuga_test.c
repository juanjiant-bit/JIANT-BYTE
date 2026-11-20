/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
int main(void){int bad=0;unsigned g=0;for(;g<NGENRE;g++)if(GENRE[g].spec&GSP_FUGUE)break;
for(unsigned six=0;six<2;six++)for(unsigned seed=37;seed<=38;seed++){
 ui_power_on();if(six)ui_rec_prefs|=PREF_SIX>>8;else ui_rec_prefs&=~(PREF_SIX>>8);rp_apply();
 gp.genre=g+1;gp.seed=seed;gp.energy=60;gp.form=2;gp.complex=60;gp.human=0;gp.swing=GA_AUTO;for(unsigned k=0;k<GP_N;k++)gp.amt[k]=GA_AUTO;
 gen_go();bad+=check("FUGA initial subject belongs to an active melodic track",gs.trk_of[RL_LEAD]<trk_live&&!drum_track(&trk[gs.trk_of[RL_LEAD]]));
 bad+=check("generated LIFE stays zero",life_v[LIFE_AMT]==0);
 nav_put(SEC_HOME,"GEN 1");frame();press(B_PLAY);song.master_q12=4096;
 int32_t out[2*CTL];unsigned peak=0,rowPeak[CHAIN_ROWS]={0},seen=0;double rowE[CHAIN_ROWS]={0},rowN[CHAIN_ROWS]={0};
 unsigned bars=1;for(unsigned r=0;r<chain_config.count;r++)bars+=chain_config.row[r].bars;
 for(unsigned i=0;i<bars*chain_bar()/CTL;i++){
  unsigned row=chain.row;mix_block(out,CTL);song_poll();if(row<CHAIN_ROWS)seen|=1u<<row;
  for(unsigned j=0;j<2*CTL;j++){unsigned v=(unsigned)abs(out[j]);if(i<FS*4/CTL&&v>peak)peak=v;if(row<CHAIN_ROWS&&v>rowPeak[row])rowPeak[row]=v;if(row<CHAIN_ROWS){rowE[row]+=(double)out[j]*out[j];rowN[row]++;}}
  if(i>FS*4/CTL&&chain.row==0&&seen>1)break;
 }
 printf("FUGA tracks %u seed %u intro peak %u rows %u\n",trk_live,seed,peak,chain_config.count);
 bad+=check("FUGA produces audible audio from its intro",peak>100);
 printf("seen %x lastrow %u playing %u bpm %d\n",seen,chain.row,song.playing,song.g[G_BPM]);for(unsigned r=0;r<chain_config.count;r++)printf(" row%u peak%u bars%u\n",r,rowPeak[r],chain_config.row[r].bars);
 unsigned heard=0;for(unsigned r=0;r<chain_config.count;r++)if(rowPeak[r]>100)heard++;
 bad+=check("full FUGA arrangement has audible sections",heard==chain_config.count&&heard>1);
 {double lo=0;for(unsigned r=0;r<chain_config.count;r++){double db=20*log10(sqrt(rowE[r]/(rowN[r]+1))/32768+1e-9);if(!r||db<lo)lo=db;}
  printf("FUGA quietest section %.1f dB\n",lo);bad+=check("FUGA: every section heard, not only the full ones (the subject alone above -40 dB)",lo>-40);}   /* (BYTE 0.16) */
 seq_stop();
}
printf("fuga: %s\n",bad?"FAIL":"all ok");return !!bad;}
