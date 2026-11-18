/* SPDX-License-Identifier: GPL-3.0-only */
#define LIFE_TEST_LIBRARY 1
#include "life_byte_test.c"
static void pair(unsigned receiver,unsigned donor,unsigned amount,int mute,int32_t*out){
 config(amount,100,100,100);memset(drum_kit,0,sizeof drum_kit);dx_mute=0;
 unsigned eng[2]={receiver,donor};
 for(unsigned k=0;k<2;k++){
  if(eng[k]==ENGI_DRUM){host_drums(&trk[k]);dx_kit_load(DX_NKIT);}else host_preset(&trk[k],eng[k],eng[k]==ENGI_FORM?0:eng[k]==ENGI_WAVE?1:9);
  trk[k].p[P_VOICE]=V_POLY;trk[k].p[P_SUS]=127;trk[k].p[P_DRIFT]=127;trk[k].p[P_DIST]=trk[k].p[P_CHOR]=trk[k].p[P_DLY]=trk[k].p[P_REV]=0;
  if(eng[k]==ENGI_FORM)trk[k].p[P_LRTRG]=0;
  trk_note_on(&trk[k],eng[k]==ENGI_DRUM?36:60+k*7,110);
 }
 if(mute)trk[1].p[P_MUTE]=1;
 int16_t saved[P_COUNT];memcpy(saved,trk[0].p,sizeof saved);int32_t buf[CTL];
 for(unsigned o=0;o<NS;o+=CTL){life_formula_block(0);for(unsigned k=0;k<2;k++){track_render(&trk[k],buf,CTL);if(!k)memcpy(out+o,buf,sizeof buf);}}
 CHECK(!memcmp(saved,trk[0].p,sizeof saved),"FORM interaction leaves stored parameters unchanged");
}
int main(void){unsigned ids[]={ENGI_WAVE,ENGI_LOFI,ENGI_DRUM};
 for(unsigned j=0;j<3;j++)for(unsigned direction=0;direction<2;direction++){
  unsigned r=direction?ids[j]:ENGI_FORM,d=direction?ENGI_FORM:ids[j];pair(r,d,0,0,dry);pair(r,d,100,0,lwet);double diff=delta(dry,lwet);printf("FORM LIFE %u -> %u %.5f\n",d,r,diff);CHECK(diff>.01,"bidirectional receiving audio changes");
  pair(r,d,100,1,other);printf("muted delta %.9f\n",delta(dry,other));CHECK(!memcmp(dry,other,sizeof dry),"muted donor bypasses exactly");
 }
 puts(fails?"FORM LIFE FAIL":"FORM LIFE 6 directed connections + mute + patch invariance PASS");return !!fails;
}
