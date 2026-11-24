/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
static project_t a,b;static project_store_t stored;
static const char program[]="func{state q;q=q+0.01;return sin(w)*(.2+A*.5);}";
int main(void){
 ui_power_on();song.sel=0;select_engine(ENGI_FUNC);frame();int32_t out[2*CTL];for(unsigned j=0;j<16;j++)mix_block(out,CTL);
 TSEL->p[P_E7]=0;TSEL->p[P_E0]=4;lyr_set(0,program);math_poll();assert(lofi_prg[0].func&&lofi_prg[0].nstate==1);
 TSEL->p[P_SUS]=127;voice_limit[0]=1;trk_note_on(TSEL,60,110);
 life_v[0]=100;life_v[1]=65;life_v[2]=60;life_v[3]=40;
 life_formula_block(0);track_render(TSEL,out,CTL);assert(TSEL->v[0].s[6]==(int32_t)CTL*655);
 puts("funcui: LIFE donor capture does not advance live program state PASS");
 unsigned version=math_ver[0];TSEL->p[P_M1SRC]=MS_MODW;TSEL->p[P_M1AMT]=40;TSEL->mw=127;
 for(unsigned k=0;k<4;k++){const char*name[]={"A","B","C","D"};assert(!strcmp(mod_dst_name_bt(TSEL,MD_E1+2+k),name[k]));TSEL->p[P_M1DST]=MD_E1+2+k;int old=TSEL->p[P_E2+k];mod_begin(TSEL);assert(TSEL->p[P_E2+k]>old);mod_end(TSEL);math_poll();assert(math_ver[0]==version&&TSEL->p[P_E2+k]==old);}
 assert(!lofi_desc(TSEL,1));
 name_open(NK_FUNC,0);str_cpy(nm.s,"func{state q;return bad;}",sizeof nm.s);nm.len=str_len(nm.s);name_ok();assert(name_on()&&nm.ferr&&!strcmp(lyr_get(0),program));name_close();
 name_open(NK_FUNC,0);str_cpy(nm.s,MATH_FUNC_DEFAULT,sizeof nm.s);nm.len=str_len(nm.s);name_ok();assert(!name_on()&&!strcmp(lyr_get(0),MATH_FUNC_DEFAULT));
 select_engine(ENGI_LOFI);TSEL->p[P_E1]=RW_USER;math_poll();assert(!lofi_prg[0].func);
 TSEL->p[P_E1]=RW_FUNC;math_poll();assert(TSEL->eng_req==ENGI_FUNC&&lofi_prg[0].func&&!strcmp(lyr_get(0),MATH_FUNC_DEFAULT));   /* (a MATH WAVE FUNC: FUNC's) */
 TSEL->p[P_E7]=0;
 lyr_set(0,"  func{return sin(w)*A;}");math_poll();assert(lofi_prg[0].func&&lofi_prg[0].nstate==0);
 char longcode[256];memset(longcode,' ',255);memcpy(longcode,program,strlen(program));longcode[255]=0;
 for(unsigned j=0;j<NTRK;j++){lyr_set(j,longcode);trk[j].p[P_E1]=RW_FUNC;trk[j].eng_req=ENGI_LOFI;}
 project_capture(&a);assert(proj_pack(&stored,&a));assert(proj_import(&b,stored.raw,sizeof stored.raw));assert(!memcmp(a.lyr,b.lyr,sizeof a.lyr));
 for(unsigned j=0;j<NTRK;j++)assert(a.t[j].engine==ENGI_LOFI&&b.t[j].engine==ENGI_FUNC);   /* (BYTE 0.17: stored MATH WAVE FUNC, read FUNC) */
 assert(!project_restore_runtime(&b));assert(!strcmp(lyr_get(0),longcode)&&trk[0].eng_req==ENGI_FUNC&&trk[0].p[P_E1]==RW_FUNC);
 puts("funcui: controls/matrix, transactional editor, six255-character programs and project reload PASS");return 0;
}
