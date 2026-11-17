/* SPDX-License-Identifier: GPL-3.0-only */
#define LIFE_TEST_LIBRARY 1
#include "life_byte_test.c"
#include <sys/stat.h>
static void save_sound(const char*name,const int32_t*x){char path[160];snprintf(path,sizeof path,"build/life-sound/%s.i32",name);FILE*f=fopen(path,"wb");if(!f)exit(2);fwrite(x,sizeof dry,1,f);fclose(f);}
int main(void){
 mkdir("build/life-sound",0777);
 unsigned ids[3]={ENGI_WAVE,ENGI_LOFI,ENGI_DRUM};char name[64];
 for(unsigned r=0;r<3;r++)for(unsigned level=0;level<=100;level+=25){
  network(ids[r],ids[(r+1)%3],level,1,-1,0,dry);snprintf(name,sizeof name,"engine-%u-life-%u",r,level);save_sound(name,dry);
 }
 target_text="sin(w)";
 const char*forms[]={"sin(w)","tri(w)","sqr(w)","sin(w)+sin(w*3)*0.7+sin(w*5)*0.4"};
 for(unsigned f=0;f<4;f++){
  vm(forms[f],100,0,100,0,0,0,dry);snprintf(name,sizeof name,"shape-%u",f);save_sound(name,dry);
 }
 puts("life_sound: rendered tonal range and shape contrasts");return fails!=0;
}
