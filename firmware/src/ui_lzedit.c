/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.5) EDIT's lens panels (ui_lens.c), as the approved mock-up's; included by ui_graph.c. The canvas is the
 * lens's panel (240 x 140, x 4..236).
 *   SOUND   (EDIT 1 / EDIT 2 of a synth) the engine's being on the left (graph_being's, alive with the sound), the
 *           engine's page name over it and the sound's name under it; on the right the scope in a well and the eight
 *           E parameters as chips, the page's four lit. DRUM's KIT keeps its colony (graph_colony)
 *   FILTER  the response of TYPE at CUT with RES (LP HP BP COMB), CUT a line, DRIVE a row of cells
 *   VOICE   the track's voices one a row, each as full as its envelope now; VOICE and PRIO (VOICE 2: ALLOC); GLIDE's
 *           curve
 *   FOCUS   the parameter as a gauge, its value large, the MOD slot that moves it ("M1 -> +63%"); E1..E8 as tabs
 * The others (MORPH, RETRIG, ROTOR, the kit's, the operators') keep their panels. */
#define ED_CH 17                                        /* a chip's height (two lines of AF_X) */
static void ed_cap(int32_t x, int32_t y, const char *s, uint16_t c)   /* a caption, its capitals from y */
{
    cv_text(x, y - AF_X_CAP_Y, &AF_X, s, c);
}
static __attribute__((noinline)) void graph_sound_lz(const track_t *t)
{
    int32_t x = 122, w = 106, ty = 2;
    /* (V10.7.15, the owner) no parameter list beside the being: the knobs' cards above say the same. SCOPE OFF (the
     * default): the creature (or the generic being) on the whole panel; SCOPE ON: the being left, the scope tall right */
    if (!ui_scope || trk_engp(t) == &ENG_LOFI || trk_engp(t) == &ENG_WAVE) {   /* (JIANT BYTE: a formula's panel whole) */
        if (!vx_draw(t, 4, 2, 232, (int32_t)cv_h - 4, -1))
            graph_being_at(t, 120 * 16, ((int32_t)cv_h / 2) * 16, 34 * 16);
    } else {
        if (!vx_draw(t, 8, 14, 108, 112, -1))          /* (0.9.4 / 0.9.6: the engines' own creatures: ui_engvis.c) */
            graph_being_at(t, 60 * 16, 68 * 16, 22 * 16);
        cv_rect(0, 0, 4, (int32_t)cv_h, T_BG);
        cv_rect(116, 0, 60, (int32_t)cv_h, T_SURF);
        /* (V10.7.19, the owner: the users like the trace on the background) no tile behind the scope in any skin
         * (COLORS drew a RAISE card): the trace and its title on the panel itself */
        GFX_HOOK_ALIGN(0, ty + 4, 0, ty + 4 + AF_X_CAP_H, AL_V | AL_PASS, "EDIT scope title");
        cv_text_on(x, ty + 4 - AF_X_CAP_Y, &AF_X, "SCOPE", T_TEXT, T_SURF);
        scope_at(x + 4, w - 8, ty + 68, 48, T_THEME);
    }
    /* (the page's name stood at the top left and the sound's at the foot: the breadcrumb and the header say them) */
}
/* FILTER: y of the response at x (px, 10..226), the cut at xc, res 0..1000; the unity line at y 62 */
static int32_t ed_resp(uint32_t ty, int32_t x, int32_t xc, int32_t res)
{
    int32_t d = x - xc, a = d < 0 ? -d : d, pk = res * 46 / 1000, y;
    int32_t bump = pk * 64 / (64 + d * d / 2);
    if (ty == 3u || ty == 4u)                           /* COMB: teeth CUT apart, as deep as RES (COMB-: between) */
        return 62 - (int32_t)(((ty == 4u ? -1 : 1) * og_cos((uint32_t)x * (uint32_t)(4096 + xc * 160)) * (8 + pk / 2)) >> 15);
    if (ty == 5u || ty == 6u)                           /* (V10.22) LP24 (filters2: MS20's 5 its alias): twice as steep */
        return clamp(62 + (d > 0 ? 4 * d : 0) - bump, 8, 104);
    y = ty == 0u ? (d > 0 ? 2 * d : 0) : ty == 1u ? (d < 0 ? 2 * a : 0) : 2 * a / 3;   /* LP HP BP */
    return clamp(62 + y - bump - (ty == 2u ? pk / 3 : 0), 8, 104);
}
/* (BYTE, the HUD) FILTER: its response a trace in the track's colour on a dotted grid (the decades, unity), hatched
 * under; CUT the yellow cursor and its reading, RES a node on the peak; the type a tag; IMAGE (KNOB 4) a matrix */
static void graph_filter_lz(const track_t *t, const page_t *pg)
{
    static const char *const FX[3] = {"100", "1K", "10K"};
    int16_t *vp;
    const param_desc_t *d;
    int32_t r[4], x, py = 0, xc, k, nd, yc;
    uint32_t ty = ft_of(t->p);
    uint16_t tc = TRK_COL[(uint32_t)(t - trk) % 6u];
    char b[32];
    for (k = 0; k < 4; k++) {
        d = page_desc(pg, (uint32_t)k, &vp);
        r[k] = d ? clamp(RATIO(d, enum_rank(d, *vp)), 0, 1000) : 0;
    }
    xc = 10 + r[1] * 216 / 1000;
    for (k = 0; k < 3; k++)                             /* the decades, dotted; unity */
        for (x = 18; x < 105; x += 3) cv_pset(64 + k * 54, x, T_LINE);
    for (x = 10; x < 227; x += 3) cv_pset(x, 62, T_LINE);
    for (x = 10; x <= 226; x++) {
        int32_t y = ed_resp(ty, x, xc, r[2]);
        if (!(x % 3)) cv_rect(x, y + 2, 1, 105 - y - 2 > 0 ? 105 - y - 2 : 0, ux_mix(T_BG, tc, 22));   /* (hatched under) */
        if (x > 10)
            cv_line_t(x - 1, py, x, y, tc, 2);
        py = y;
    }
    for (k = 0; k < 3; k++)
        cv_text(67 + k * 54, 108 - AF_X_CAP_Y, &AF_X, FX[k], T_DIM);
    yc = ed_resp(ty, xc, xc, r[2]);
    cv_rect(xc, 16, 1, 89, HUD_YE);                     /* CUT: the cursor, its reading */
    cv_rect(xc - 2, 16, 5, 1, HUD_YE); cv_rect(xc - 1, 17, 3, 1, HUD_YE);
    cv_rect(xc - 2, yc - 2, 5, 5, HUD_YE);
    d = page_desc(pg, 1, &vp);
    if (d) {
        const char *u = "";
        param_format(d, *vp, b, &u);
        str_cpy(b + str_len(b), u, 32 - str_len(b));
        cv_text_on(xc + 4 < 180 ? xc + 4 : xc - 4 - text_w(&AF_X, b), 20 - AF_X_CAP_Y, &AF_X, b, HUD_YE, T_BG);
    }
    hud_tag(10, 2, N_FTYPE[ty], (uint32_t)(t - trk) % 6u >= 4u ? 0xFFFFu : 0x0000u, tc);
    cv_text_on(14 + text_w(&AF_X, N_FTYPE[ty]) + 4, 3 - AF_X_CAP_Y, &AF_X, trk_eng(t) == 0u ? "PER VOICE" : "ON THE PART", T_MID, T_BG);
    d = page_desc(pg, 3, &vp);
    if (d) {
        const char *u = "";
        str_cpy(b, d->label, 32);
        str_cpy(b + str_len(b), " ", 2);
        param_format(d, *vp, b + str_len(b), &u);
        str_cpy(b + str_len(b), u, 32 - str_len(b));
        cv_text_on(10, 116 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_BG);
    }
    nd = r[3] * 20 / 1000;
    for (k = 0; k < 20; k++)
        cv_rect(110 + k * 6, 116, 4, 8, k < nd ? heat_col(40 + k * 11) : T_SURF);
}
static void graph_voice_lz(const track_t *t, const page_t *pg)
{
    uint32_t i, nv = trk_nvoice(t) ? trk_nvoice(t) : NVOICE;
    int32_t glide = t->p[P_GLIDE], py = 0, x;
    const char *u = "";
    char b[40];
    uint32_t active = 0;
    for (i = 0; i < NVOICE; i++) active += t->v[i].active && t->v[i].stage != 4u;
    fmt_int(b, (int32_t)active); str_cpy(b + str_len(b), " ACTIVE / ", 40 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)nv); str_cpy(b + str_len(b), " MAX", 40 - str_len(b));
    ed_cap(10, 6, b, T_TEXT);
    for (i = 0; i < NVOICE; i++) {
        int32_t y = 17 + (int32_t)i * 9, lv = 0;
        char n[2] = {(char)('1' + i), 0};
        if (t->v[i].stage && t->v[i].stage < 4u)
            lv = (int32_t)(((int64_t)t->v[i].env * 196) >> 24);
        {   /* (BYTE, the HUD) its number a tag, its level 34 cells */
            int32_t c, nc = lv * 34 / 196;
            ed_cap(14, y, n, lv > 0 ? HUD_YE : i < nv ? T_TEXT : T_DIM);
            for (c = 0; c < 34; c++)
                cv_rect(26 + c * 6, y, 4, 7, c < nc ? heat_col(60 + c * 190 / 33) : i < nv ? T_LINE : T_SURF);
        }
    }
    param_format(track_desc(t, P_VOICE), t->p[P_VOICE], b, &u);
    str_cpy(b + str_len(b), pg->id[0] == P_ALLOC ? " \xB7 ALLOC " : " \xB7 PRIO ", 40 - str_len(b));
    param_format(track_desc(t, pg->id[0] == P_ALLOC ? P_ALLOC : P_PRIO), t->p[pg->id[0] == P_ALLOC ? P_ALLOC : P_PRIO],
                 b + str_len(b), &u);
    ed_cap(10, 95, b, T_TEXT);
    str_cpy(b, "GLIDE ", 40);
    param_format(track_desc(t, P_GLIDE), t->p[P_GLIDE], b + 6, &u);
    str_cpy(b + str_len(b), u, 40 - str_len(b));
    ed_cap(10, 120, b, T_TEXT);
    for (x = 0; x <= 104; x++) {                        /* the glide: low note to high, as slow as GLIDE */
        int32_t tm = glide ? x * 127 * 6 / (glide * 4 + 1) : 1000, k = tm > 255 ? 255 : tm;
        int32_t y = x < 16 ? 130 : 130 - 22 * (k * (255 - (255 - k) * (255 - k) / 255)) / 65025;
        if (x)
            cv_line_t(122 + x - 1, py, 122 + x, y, TRK_COL[(uint32_t)(t - trk) % 6u], 2);
        py = y;
    }
}
/* (V10.7.33, the owner) EDIT > AUTO KNOBS (was FOCUS): "pick a knob and what moves it". On top the knob picked (KNOB 1) and its
 * value (KNOB 2) as a bar of the thermal ramp (heat_col); under it what moves it and how much (KNOB 3 SRC, KNOB 4 AMT: the
 * MOD slot aimed at it, the same MATRIX shows); the eight knobs at the foot, the one picked lit. (The gauge's circle gone) */
/* (as JIANT FM V10.8.17, the owner) FOCUS: the parameter's name and value large, its place (OSC 1/4) small; a thermal bar to
 * its value, a yellow marker there, a MOD slot aimed at it its reach in the track's colour over the bar and its words; the
 * eight as two rows of four, their names whole, the one picked a tag in the track's colour, each a small bar of its value */
static void graph_focus_lz(const track_t *t)
{
    const engine_t *e = trk_engp(t);
    uint32_t k = focus_k(), i, tr = (uint32_t)(t - trk) % 6u;
    int32_t j = focus_slot(k), r, fill, px;
    const param_desc_t *d = track_desc(t, P_E0 + k);
    const char *u = "", *pt = e->page_title[k >> 2];
    uint16_t tc = TRK_COL[tr], ink = tr >= 4u ? 0xFFFFu : 0x0000u;
    char b[24];
    param_format(d, t->p[P_E0 + k], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_text(8, 4, &AF_M, d->label, T_TEXT);
    cv_text_r(232, 2, large_face_has(b) ? &AF_L : &AF_M, b, HUD_YE, T_SURF);
    str_cpy(b, pt && pt[0] != '-' ? pt : e->name, 16);
    str_cpy(b + str_len(b), " ", 2);
    fmt_int(b + str_len(b), (int32_t)(k & 3u) + 1);
    str_cpy(b + str_len(b), "/4", 3);
    cv_text(8, 26, &AF_X, b, T_DIM);
    r = clamp(RATIO(d, enum_rank(d, t->p[P_E0 + k])), 0, 1000);
    fill = r * 220 / 1000;
    for (px = 0; px < 220; px += 2)                     /* the bar: thermal to the value, dotted past it */
        if (px < fill) cv_rect(10 + px, 50, 2, 18, heat_col(px * 256 / 220));
        else if (!(px % 6)) cv_pset(10 + px, 59, T_LINE);
    cv_rect(9 + fill, 45, 2, 28, HUD_YE);
    if (j >= 0) {                                       /* MOD: its reach over the bar, its words */
        const param_desc_t *da = track_desc(t, P_M1AMT + 3u * (uint32_t)j);
        int32_t am = t->p[P_M1AMT + 3u * (uint32_t)j], sp = (am < 0 ? -am : am) * 220 / (da->max > 0 ? 2 * da->max : 1);
        int32_t x0 = clamp(fill - sp, 0, 220), x1 = clamp(fill + sp, 0, 220);
        char a[16];
        cv_rect(10 + x0, 42, x1 - x0 + 1, 3, tc);
        cv_rect(10 + x0, 40, 1, 5, tc); cv_rect(10 + x1, 40, 1, 5, tc);
        str_cpy(b, "MOD: ", 24);
        str_cpy(b + 5, N_MSRC[clamp(t->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)], 19);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        param_format(da, am, a, &u);
        str_cpy(b + str_len(b), a, 24 - str_len(b));
        str_cpy(b + str_len(b), u, 24 - str_len(b));
        cv_text(10, 74, &AF_X, b, tc);
    } else
        cv_text(10, 74, &AF_X, "NO MOD", T_DIM);
    for (i = 0; i < 8u; i++) {                          /* the eight: two rows of four */
        const param_desc_t *di = track_desc(t, P_E0 + i);
        int32_t x = 6 + (int32_t)(i & 3u) * 57, y = 90 + (int32_t)(i >> 2) * 24;
        uint16_t bg = i == k ? tc : T_RAISE;
        r = di ? clamp(RATIO(di, enum_rank(di, t->p[P_E0 + i])), 0, 1000) : 0;
        cv_rect(x, y, 54, 16, bg);
        if (di && di->label && di->label[0] != '-')
            cv_clip_text(x + 2, y + 3 - AF_X_CAP_Y + 2, &AF_X, di->label, i == k ? ink : T_TEXT, bg, 50);
        cv_rect(x, y + 18, 54 * r / 1000 + 1, 3, heat_col(r * 256 / 1000));
    }
}
/* (BYTE, the owner's references: telemetry) the panel a flight recorder: OUT the drone's wave in its track's colour;
 * under it the four modulators, each a trace of what it adds now, swept left to right; a yellow cursor reads it; each
 * its tag (lit when it strikes), its shape > DEST; the MOD page's one white */
#define FT_N 68
#define FT_BL 0x29FFu
#define FT_YE 0xEF8Bu
/* (the history: ui_byte.c's hud_q, lanes 0..3 what each adds, 8 its zero; 4..7 its strikes) */
#define ft_x hud_x
static int32_t ft_v(uint32_t m,uint32_t i){return (int32_t)hud_get(m,i)-8;}
static void ft_tag(int32_t x,int32_t y,const char*s,uint16_t fg,uint16_t bg){int32_t w=text_w(&AF_X,s)+4;cv_rect(x,y,w,11,bg);cv_text_on(x+2,y+1-AF_X_CAP_Y,&AF_X,s,fg,bg);}
static void ft_frame(int32_t y,int32_t h){int32_t x;
 cv_rect(32,y,206,1,T_SURF);cv_rect(32,y+h-1,206,1,T_SURF);
 for(x=32;x<238;x+=3)cv_rect(x,y+h/2,1,1,T_LINE);
 for(x=32;x<238;x+=34)cv_rect(x,y+h-4,1,3,T_DIM);}
/* (BYTE) DRONE CLAY's panel: OUT the track's level as it sounds; under it the four modulators, each a trace of what it
 * adds now, swept left to right, a strike colouring its line; each its tag (lit when it strikes), its SHAPE > DEST
 * (the destination's own name: a sound knob's label); the pages' MOD white (CLAY MOD, CLAY RHYTHM); OFF: the four dim,
 * CLAY OFF */
static __attribute__((noinline)) void graph_clay_lz(const track_t*t){
 uint32_t tr=(uint32_t)(t-trk)%NTRK,m,i,sel=4u;
 const clay_t*c=clay[tr];uint16_t tc=TRK_COL[tr%6u];
 const page_t*pg=cur_page();
 if(pg->id[0]==ID_CL0+CL_SEL)sel=(uint32_t)clay_km[tr]&3u;
 cv_oy=0;form_scope_tr=(uint8_t)tr;
 if(clay_card_on()&&clay_ke.tr==tr){                 /* (the owner: what the key does) the last key's card, a moment */
  char a[24],b[40];int32_t y;
  if(clay_ke.kind==1u){uint32_t sh=clay_ke.a%FORM_NSH;const char*h=CLAY_HOW[sh],*p=h;char l[24];uint32_t n=0;
   cv_text_on(8,18-AF_L_CAP_Y,&AF_L,FORM_SHAPE[sh],FT_YE,T_BG);
   clay_card_text(a,b);{const char*q=a;while(*q&&!(q[0]=='M'&&q[1]>='1'&&q[1]<='4'))q++;ft_tag(8,48,*q?q:"-",0x0000,tc);}
   while(*p&&*p!='|'&&n<23u)l[n++]=*p++;l[n]=0;cv_text(8,72-AF_M_CAP_Y,&AF_M,l,T_TEXT);
   if(*p=='|')cv_text(8,92-AF_M_CAP_Y,&AF_M,p+1,T_TEXT);
   cv_text(8,116-AF_X_CAP_Y,&AF_X,clay_ke.held?"HELD":"LET GO",T_DIM);}
  else if(clay_card_text(a,b)){cv_text_on(8,24-AF_L_CAP_Y,&AF_L,a,FT_YE,T_BG);
   y=60;{const char*p=b;char l[24];uint32_t n;while(*p){n=0;while(*p&&n<20u){l[n++]=*p++;if(p[-1]==' '&&n>14u)break;}l[n]=0;cv_text(8,y-AF_M_CAP_Y,&AF_M,l,T_TEXT);y+=20;}}}
  return;
 }
 if(hud_tick(0x20u|tr))
  for(m=0;m<4u;m++){hud_put(m,hud_x,(uint32_t)(clamp(clay_vk(tr,m)>>11,-8,7)+8));hud_put(4u+m,hud_x,((form_lit[tr]>>m)&1u)*15u);}
 {int32_t px=0,py=0,pk=16;ft_frame(1,22);ft_tag(1,6,"OUT",0x0000,tc);
  for(i=0;i<64u;i++){int32_t v=form_scope[i]<0?-form_scope[i]:form_scope[i];if(v>pk)pk=v;}
  for(i=0;i<64u;i++){uint32_t j=(form_sc_i+i)&63u;int32_t x=32+(int32_t)i*205/63,y=20-form_scope[j]*18/pk;if(i)cv_line_t(px,py,x,y,c[CL_ON]?tc:T_DIM,1);px=x;py=y;}}
 for(m=0;m<4u;m++){
  int32_t y=26+(int32_t)m*24,mid=y+11,px=0,py=0,cx=32+(int32_t)ft_x*3;
  uint32_t lit=(form_lit[tr]>>m)&1u,on=sel==m;char b[24];int16_t*dv;
  const param_desc_t*dd=clay_desc(t,CL_MF(m,0u),&dv);
  uint16_t cl=!c[CL_ON]?T_DIM:on?T_TEXT:FT_BL;
  ft_frame(y,23);
  b[0]='M';b[1]=(char)('1'+m);b[2]=0;ft_tag(1,y+1,b,lit||on?0x0000:T_TEXT,lit?FT_YE:on?T_TEXT:FT_BL);
  str_cpy(b,FORM_SHAPE[clay_sh(c[CL_MF(m,1u)])],12);
  {uint32_t n=str_len(b);b[n]=' ';b[n+1]='\xB7';b[n+2]=' ';str_cpy(b+n+3,dd&&dd->names?dd->names[clamp(c[CL_MF(m,0u)],dd->min,dd->max)]:"-",10);}
  {int32_t pk=2;for(i=0;i<FT_N;i++){int32_t a=ft_v(m,i)<0?-ft_v(m,i):ft_v(m,i);if(a>pk)pk=a;}
  for(i=0;i<FT_N;i++){int32_t v=ft_v(m,i),x=32+(int32_t)i*3,yy=mid-v*10/pk;
   {uint32_t j,hot=0;for(j=0;j<4u&&!hot;j++)hot=hud_get(4u+m,(i+FT_N-j)%FT_N)!=0u;
    if(i&&(i<=ft_x||i>ft_x+3u))cv_line_t(px,py,x,yy,hot&&c[CL_ON]?tc:cl,on||hot?2:1);}px=x;py=yy;
   if(hud_get(4u+m,i)&&!hud_get(4u+m,(i+FT_N-1u)%FT_N))cv_rect(x,y+19,2,3,T_TEXT);}}
  cv_rect(cx,y,1,23,FT_YE);cv_rect(cx-2,y,5,1,FT_YE);cv_rect(cx-1,y+1,3,1,FT_YE);
  {int32_t w=text_w(&AF_X,b)+6;cv_rect(238-w,y,w,13,T_BG);cv_text_on(241-w,y+2-AF_X_CAP_Y,&AF_X,b,c[CL_ON]?T_TEXT:T_DIM,T_BG);}   /* (the owner: "the name can't be read": over everything, white) */
 }
 if(!c[CL_ON]){int32_t w=text_w(&AF_M,"CLAY OFF")+8;cv_rect(120-w/2,52,w,20,T_BG);cv_text_on(124-w/2,56-AF_M_CAP_Y,&AF_M,"CLAY OFF",T_TEXT,T_BG);}
}
/* the lens's panel for EDIT's page pg (1), else 0 (its own) */
static int graph_edit_lz(const track_t *t, const page_t *pg)
{
    uint32_t e = trk_eng(t);
    if ((pg->scope == SC_ENGINE && e != ENGI_DRUM) || pg->graph == GR_SWARM)
        graph_sound_lz(t);
    else if (pg->id[0] == P_FTYPE)
        graph_filter_lz(t, pg);
    else if (pg->id[0] == P_VOICE || pg->id[0] == P_ALLOC)
        graph_voice_lz(t, pg);
    else if (pg->graph == GR_FOCUS)
        graph_focus_lz(t);
    else if (pg->fam == FAM_EDIT && pg->graph == GR_NONE && (trk_engp(t) == &ENG_WAVE || trk_engp(t) == &ENG_LOFI))
        graph_sound_lz(t);                              /* (the owner: the pages WAVE, MATH inherit: their engine's panel) */
    else
        return 0;
    return 1;
}
