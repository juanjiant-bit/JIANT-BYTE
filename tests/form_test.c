/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
static void form_setup(unsigned state){
 ui_power_on();song.sel=0;select_engine(ENGI_FORM);frame();
 for(unsigned k=1;k<NTRK;k++)trk[k].p[P_MUTE]=1;
 apply_preset_to(TSEL,state);frame();song.master_q12=4096;
 int32_t b[CTL*2];for(unsigned j=0;j<100;j++)mix_block(b,CTL);
 assert(TSEL->engine==ENGI_FORM&&func_song(TSEL));
}
static const unsigned form_ids[]={P_E0,P_E1,P_E2,P_E3,P_E4,P_E5,P_E6,P_E7,P_LRTRG,P_SDIV,P_SLEN,P_SSWING,P_ROOT,P_SCALE,P_CHRD,P_CVOI,P_CAOS,P_KEY,P_DETUNE,P_DLY};
static int form_checks(void){
 form_setup(0);ui.home=1;press(B_EDIT);assert(cur_page()->id[0]==P_E0);turn(EN_SELECT,1);assert(cur_page()->id[0]==P_E4);turn(EN_SELECT,1);assert(cur_page()->scope==SC_FORM&&cur_page()->id[0]==P_LRTRG);turn(EN_SELECT,1);assert(cur_page()->scope==SC_FORM&&cur_page()->id[0]==P_ROOT);turn(EN_SELECT,1);assert(cur_page()->scope==SC_FORM&&cur_page()->id[0]==P_CAOS);static uint16_t screen_before[240*240];frames(800);memcpy(screen_before,host_screen,sizeof host_screen);ui.force=1;frame();unsigned mismatch=0;for(unsigned y=90;y<220;y++)for(unsigned x=0;x<240;x++)mismatch+=screen_before[y*240+x]!=host_screen[y*240+x];printf("FORM graph forced redraw mismatch %u\n",mismatch);assert(!mismatch);assert(NENGINES==17&&NENG_SHOWN>=4&&eng_vis(0)==ENGI_FORM);
 for(unsigned i=0;i<8;i++){assert(mod_dst_ok(TSEL,MD_E1+i));assert(!strcmp(mod_dst_name_bt(TSEL,MD_E1+i),ENG_FORM.edit[i].label));}
 assert(!strcmp(mod_dst_name_bt(TSEL,MD_SHP),"COLOR"));
 for(unsigned j=0;j<sizeof PAGES/sizeof PAGES[0];j++)if(PAGES[j].scope==SC_FORM){
  assert(page_visible(j));ui.home=0;ui.page=j;ui.force=1;frame();
  for(unsigned k=0;k<4;k++){int16_t*vp=0;const param_desc_t*d=page_desc(&PAGES[j],k,&vp);assert(d&&vp==&TSEL->p[PAGES[j].id[k]]);int old=*vp;turn(EN_K1+k,old==d->max?-1:1);assert(*vp!=old);}
 }
 static project_t captured,restored;static project_store_t store;project_capture(&captured);assert(proj_pack(&store,&captured)&&proj_import(&restored,store.raw,sizeof store.raw));
 assert(restored.t[0].engine==ENGI_FORM);for(unsigned k=0;k<sizeof form_ids/sizeof form_ids[0];k++)assert(restored.t[0].p[form_ids[k]]==TSEL->p[form_ids[k]]);
 int16_t saved[P_COUNT];memcpy(saved,TSEL->p,sizeof saved);assert(up_store(0,"FORM TEST"));for(unsigned k=0;k<sizeof form_ids/sizeof form_ids[0];k++)TSEL->p[form_ids[k]]=0;assert(up_load(0)==0);for(unsigned k=0;k<sizeof form_ids/sizeof form_ids[0];k++)assert(TSEL->p[form_ids[k]]==saved[form_ids[k]]);
 persist_t old;memset(&old,0,sizeof old);old.magic=0x50455234u;old.favorites.user=1u<<3;old.favorites.factory[ENGI_LOFI][0]=2;assert(settings_import(&old,sizeof old-32)==2);assert(favorite_has(NENGINES,3)&&favorite_has(ENGI_LOFI,1)&&!favorite_has(ENGI_FORM,3));assert(favorite_set(ENGI_FORM,3,1));assert(favorite_has(ENGI_FORM,3)&&favorite_has(NENGINES,3));settings_export(&old);memset(&favorites,0,sizeof favorites);assert(settings_import(&old,sizeof old)==1&&favorite_has(ENGI_FORM,3)&&favorite_has(NENGINES,3));
 form_setup(0);TSEL->p[P_LRTRG]=0;uint8_t ns[KB_CH+1];assert(chord_build(TSEL,60,ns)==1&&ns[0]==60);trk_note_on(TSEL,60,110);int32_t out[2*CTL];double energy=0;for(unsigned j=0;j<FS/CTL;j++){mix_block(out,CTL);for(unsigned i=0;i<CTL*2;i++)energy+=(double)out[i]*out[i];}assert(energy>1e8);trk_note_off(TSEL,60);for(unsigned j=0;j<FS/CTL;j++)mix_block(out,CTL);for(unsigned k=0;k<NVOICE;k++)assert(!TSEL->v[k].active);
 puts("FORM pages, project/USER roundtrip, PER4 migration, separate USER/FORM favorites, NOTE chords: PASS");return 0;
}

static void form_record(const char*name,unsigned a,unsigned b,unsigned morph,int id,int value,unsigned secs,unsigned life){
 form_setup(a);TSEL->p[P_E1]=b;TSEL->p[P_E2]=morph;if(id>=0)TSEL->p[id]=value;
 if(life){life_v[LIFE_AMT]=100;life_v[LIFE_NUMBERS]=life_v[LIFE_FRAGMENTS]=life_v[LIFE_LINK]=100;trk[1].p[P_MUTE]=0;set_engine_of(&trk[1],ENGI_WAVE);trk_note_on(&trk[1],67,110);}
 char path[160];snprintf(path,sizeof path,"build/form/%s.i32",name);FILE*f=fopen(path,"wb");assert(f);int32_t out[CTL*2];transport_req=1;
 for(unsigned j=0;j<FS*secs/CTL;j++){mix_block(out,CTL);fwrite(out,sizeof out,1,f);}fclose(f);
}
static int form_probes(void){
 static const int params[]={P_E0,P_E1,P_E2,P_E3,P_E4,P_E5,P_E6,P_E7,P_SDIV,P_SLEN,P_SSWING,P_ROOT,P_SCALE,P_CHRD,P_CVOI,P_CAOS,P_KEY,P_DETUNE,P_DLY};
 static const int lo[]={0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0};
 static const int hi[]={21,21,127,127,7,127,127,127,3,32,100,11,8,9,3,4,127,127,127};
 mkdir("build/form",0777);
 for(unsigned k=0;k<sizeof params/sizeof params[0];k++)for(unsigned edge=0;edge<2;edge++){char nm[80];snprintf(nm,sizeof nm,"control-%02u-%u",k,edge);form_record(nm,0,20,64,params[k],edge?hi[k]:lo[k],18,0);}
 for(unsigned k=0;k<5;k++){char nm[80];snprintf(nm,sizeof nm,"morph-byte-float-%u",k);form_record(nm,14,9,k==4?127:k*32,-1,0,12,0);snprintf(nm,sizeof nm,"morph-byte-func-%u",k);form_record(nm,14,17,k==4?127:k*32,-1,0,12,0);}
 form_record("life-off",0,20,64,-1,0,4,0);form_record("life-on",0,20,64,-1,0,4,1);
 puts("FORM 19 continuous/discrete controls, two morph paths and LIFE renders ready");return 0;
}

int main(int argc,char**argv){
 if(argc>1&&!strcmp(argv[1],"probes"))return form_probes();
 if(argc>1&&!strcmp(argv[1],"checks"))return form_checks();
 if(argc>1&&!strcmp(argv[1],"kernel")){
  volatile int32_t sum=0;
  for(unsigned s=0;s<22;s++)for(unsigned c=0;c<128;c+=127)for(unsigned i=0;i<20000;i++){
   int32_t y=form_wave(s,i,pitch_inc((24+i%73)*16),i*17,c,i%128);assert(y>=-32767&&y<=32767);sum^=y;
  }
  ui_power_on();select_engine(ENGI_FORM);TSEL->engine=ENGI_FORM;fl.on=0;
  vmod_t m={.amp0=32767,.amp1=32767,.shape=64*256,.pitch16=60*16};
  int32_t out[CTL];voice_t v={.note=60};TSEL->p[P_LRTRG]=0;
  for(unsigned s=0;s<22;s++)for(unsigned dom=0;dom<5;dom++)for(unsigned c=0;c<128;c+=127)for(unsigned morph=0;morph<128;morph+=63){
   TSEL->p[P_E0]=s;TSEL->p[P_E1]=(s+7)%22;TSEL->p[P_E2]=morph;TSEL->p[P_E3]=c;TSEL->p[P_CAOS]=dom;form_note_on(TSEL,&v);
   for(unsigned j=0;j<64;j++){memset(out,0,sizeof out);form_render(TSEL,&v,out,CTL,&m);for(unsigned i=0;i<CTL;i++)assert(out[i]>=-32768&&out[i]<=32767);}
  }
  printf("FORM 22 kernels + render domains/colors/morph extremes: %d PASS\n",sum);return 0;
 }
 mkdir("build/form",0777);unsigned seconds=argc>1?atoi(argv[1]):2;
 FILE*csv=fopen("build/form/states.csv","w");assert(csv);fprintf(csv,"state,rms,peak,dc,host_realtime_ratio\n");
 for(unsigned s=0;s<22;s++){
  form_setup(s);int32_t out[CTL*2];char name[100];snprintf(name,sizeof name,"build/form/state-%02u.i32",s+1);FILE*f=fopen(name,"wb");assert(f);
  double energy=0,dc=0;int32_t peak=0;unsigned count=0;clock_t start=clock();transport_req=1;
  for(unsigned j=0;j<FS*seconds/CTL;j++){mix_block(out,CTL);fwrite(out,sizeof out,1,f);for(unsigned i=0;i<CTL*2;i++){int32_t x=out[i];energy+=(double)x*x;dc+=x;if(abs(x)>peak)peak=abs(x);count++;}}
  double ratio=(double)(clock()-start)/CLOCKS_PER_SEC/seconds,rms=sqrt(energy/count);fclose(f);
  printf("%02u %s rms %.1f peak %d dc %.1f ratio %.4f\n",s+1,FORM_NAMES[s],rms,peak,dc/count,ratio);fflush(stdout);fprintf(csv,"%u,%.4f,%d,%.4f,%.6f\n",s+1,rms,peak,dc/count,ratio);
  assert(rms>100&&peak<32768);unsigned active=0;for(unsigned k=0;k<NVOICE;k++)active+=TSEL->v[k].active;assert(active==1);
  transport_req=2;for(unsigned j=0;j<FS/CTL;j++)mix_block(out,CTL);for(unsigned k=0;k<NVOICE;k++)assert(!TSEL->v[k].active);
 }
 fclose(csv);puts("FORM all 22 PLAY/STOP: PASS");return 0;
}
