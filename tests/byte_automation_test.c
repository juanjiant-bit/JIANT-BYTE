/* SPDX-License-Identifier: GPL-3.0-only */
#include <time.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int failures;
#define REQUIRE(x,label) do{failures+=check(label,!!(x));}while(0)
static int32_t before[FS],after[FS];
static void setup_user(void){
 ui_power_on();song.sel=0;select_engine(ENGI_LOFI);frame();int32_t settled[2*CTL];for(unsigned i=0;i<8;i++)mix_block(settled,CTL);
 TSEL->p[P_E1]=RW_USER;TSEL->p[P_E2]=45;TSEL->p[P_E3]=0;TSEL->p[P_CAOS]=0;
 TSEL->p[P_SUS]=127;TSEL->p[P_ATK]=0;TSEL->p[P_DIST]=TSEL->p[P_DLY]=TSEL->p[P_REV]=TSEL->p[P_CHOR]=0;
 memset(lyr_txt[0],0,sizeof lyr_txt[0]);math_request[0]=0;math_seed[0]=-1;math_poll();
}
static void render_route(unsigned destination,unsigned source,int amount,int32_t*audio){
 setup_user();TSEL->p[P_M1SRC]=source;TSEL->p[P_M1DST]=destination;TSEL->p[P_M1AMT]=amount;TSEL->p[P_LRATE]=50;
 trk_note_on(TSEL,60,110);unsigned o=0;int32_t b[CTL];
 while(o<FS){mod_begin(TSEL);track_render(TSEL,b,CTL);mod_end(TSEL);
  unsigned n=FS-o<CTL?FS-o:CTL;memcpy(audio+o,b,n*sizeof *b);o+=n;fm1_ms+=2;math_poll();}
 REQUIRE(TSEL->p[P_E2]==45&&TSEL->p[P_E3]==0,"automation leaves stored seed/mutation unchanged");
}
static double difference(int32_t*a,int32_t*b){double x=0,y=1;for(unsigned i=0;i<FS;i++){double d=(double)a[i]-b[i];x+=d*d;y+=(double)a[i]*a[i];}return sqrt(x/y);}
int main(void){
 setup_user();REQUIRE(mod_dst_ok(TSEL,MD_E1+2)&&mod_dst_ok(TSEL,MD_E1+3),"SEED and MUTA are valid destinations");
 REQUIRE(!strcmp(mod_dst_name_bt(TSEL,MD_E1+2),"SEED")&&!strcmp(mod_dst_name_bt(TSEL,MD_E1+3),"MUTA"),"matrix names match the actual USER controls");
 unsigned initial=math_ver[0];TSEL->p[P_M1SRC]=MS_MODW;TSEL->p[P_M1DST]=MD_E1+2;TSEL->p[P_M1AMT]=63;TSEL->mw=127;
 mod_begin(TSEL);REQUIRE(TSEL->p[P_E2]==191,"SEED modulation clamps to191");unsigned requested=math_request[0];mod_end(TSEL);
 REQUIRE(TSEL->p[P_E2]==45&&math_ver[0]==initial,"audio only requests, never compiles or persists");
 fm1_ms+=64;math_poll();REQUIRE(math_seed[0]==191&&math_ver[0]!=initial,"main loop applies effective SEED");
 REQUIRE((requested&255u)==191,"latest effective request captured");initial=math_ver[0];
 for(unsigned n=0;n<10;n++){TSEL->mw=(uint8_t)(20+n);mod_begin(TSEL);mod_end(TSEL);fm1_ms++;math_poll();}
 REQUIRE(math_ver[0]==initial,"rapid source changes are coalesced within32ms");
 fm1_ms+=40;math_poll();REQUIRE(math_ver[0]!=initial,"latest request applied after control interval");
 TSEL->p[P_M1AMT]=0;math_poll();REQUIRE(math_seed[0]==45,"route removal restores base formula without rewriting patch");
 render_route(MD_E1+2,MS_LFO,0,before);render_route(MD_E1+2,MS_LFO,50,after);printf("SEED LFO audio delta %.3f\n",difference(before,after));REQUIRE(difference(before,after)>.15,"LFO to SEED changes actual audio");
 render_route(MD_E1+3,MS_ENV,0,before);render_route(MD_E1+3,MS_ENV,50,after);printf("MUTA ENV audio delta %.3f\n",difference(before,after));REQUIRE(difference(before,after)>.15,"ENV to MUTA changes actual audio");
 render_route(MD_E1+2,MS_LFO,0,before);render_route(MD_E1+3,MS_ENV,0,after);
 REQUIRE(!memcmp(before,after,sizeof before),"zero amount routes are bit-exact neutral");
 setup_user();TSEL->p[P_M1SRC]=MS_MODW;TSEL->p[P_M1DST]=MD_E1+3;TSEL->p[P_M1AMT]=63;TSEL->mw=127;
 TSEL->p[P_E3]=64;mod_begin(TSEL);REQUIRE(TSEL->p[P_E3]==127,"MUTA clamps to127");mod_end(TSEL);
 TSEL->p[P_E3]=0;TSEL->p[P_M1AMT]=-63;mod_begin(TSEL);REQUIRE(TSEL->p[P_E3]==0,"MUTA clamps to0");mod_end(TSEL);
 static project_t q,r;static project_store_t st;project_capture(&q);
 REQUIRE(proj_pack(&st,&q)&&proj_import(&r,st.raw,sizeof st.raw)&&
  r.t[0].p[P_M1DST]==MD_E1+3&&r.t[0].p[P_M1AMT]==-63&&r.t[0].p[P_E2]==45&&r.t[0].p[P_E3]==0,
  "project roundtrip preserves route and original seed/mutation");
 setup_user();TSEL->p[P_M1SRC]=MS_MODW;TSEL->p[P_M1DST]=MD_E1+2;TSEL->p[P_M1AMT]=50;
 clock_t started=clock();for(unsigned j=0;j<64;j++){TSEL->mw=j*2;mod_begin(TSEL);mod_end(TSEL);fm1_ms+=40;math_poll();}
 printf("USER main-loop rebuild mean %.3f ms (host only)\n",1000.0*(clock()-started)/CLOCKS_PER_SEC/64);
 /* FM49 gestures: first turn loads immediately without replacing the page. */
 ui_power_on();song.sel=0;select_engine(ENGI_WAVE);frame();ui.home=1;unsigned e=TSEL->eng_req,p=TSEL->preset;
 turn(EN_PRESET,1);REQUIRE(ui.home&&TSEL->eng_req==e&&TSEL->preset!=p,"PRESET first detent loads sound directly, no modal list");
 int32_t mixed[2*CTL];for(unsigned j=0;j<40;j++)events_block(CTL),mix_block(mixed,CTL);
 trk_note_on(TSEL,60,100);for(unsigned j=0;j<20;j++)mix_block(mixed,CTL);
 turn(EN_PRESET,1);for(unsigned j=0;j<40;j++)events_block(CTL),mix_block(mixed,CTL);
 unsigned held=0;for(unsigned j=0;j<NVOICE;j++)held|=TSEL->v[j].active&&TSEL->v[j].gate&&TSEL->v[j].note==60;
 REQUIRE(held,"held note survives direct preset audition");trk_note_off(TSEL,60);
 held=0;for(unsigned j=0;j<NVOICE;j++)held|=TSEL->v[j].active&&TSEL->v[j].gate&&TSEL->v[j].note==60;
 REQUIRE(!held,"carried note releases normally");
 unsigned total=ENGINES[e]->npresets;for(unsigned n=0;n<total+1;n++)turn(EN_PRESET,1);
 REQUIRE(TSEL->eng_req==e,"PRESET wraps within the current engine");
 fm1_in.buttons|=1u<<panel.btn[B_EDIT];host_pressed|=1u<<panel.btn[B_EDIT];frame();turn(EN_PRESET,1);fm1_in.buttons=0;frame();
 REQUIRE(TSEL->eng_req!=e&&ui.home,"EDIT plus PRESET jumps engine and keeps the page");
 fm1_in.buttons|=1u<<panel.btn[B_SAVE];host_pressed|=1u<<panel.btn[B_SAVE];frame();turn(EN_PRESET,1);fm1_in.buttons=0;frame();
 REQUIRE(!ui.home&&cur_page()->graph==GR_BROWSE,"SAVE plus PRESET explicitly opens the browser");
 puts(failures?"byte_automation: FAIL":"byte_automation: all ok");return !!failures;
}
