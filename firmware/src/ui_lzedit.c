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
/* a chip: the stripe, the label, the value (on: the page's) */
static void ed_chip(int32_t x, int32_t y, int32_t w, const param_desc_t *d, int16_t v, int on)
{
    char b[24];
    const char *u = "";
    uint16_t bg = on ? T_TINT : T_RAISE;
    cv_rect(x, y, w, ED_CH - 1, bg);
    cv_rect(x, y, 2, ED_CH - 1, on ? T_THEME : T_LINE);
    if (!d || !d->label || d->label[0] == '-')
        return;
    param_format(d, v, b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_clip_text(x + 5, y - AF_X_CAP_Y + 1, &AF_X, d->label, T_TEXT, bg, w - 6);
    cv_clip_text(x + 5, y - AF_X_CAP_Y + 9, &AF_X, b, on ? T_TEXT : T_DIM, bg, w - 6);
}
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
static void graph_filter_lz(const track_t *t, const page_t *pg)
{
    static const char *const FX[3] = {"100", "1k", "10k"};
    int16_t *vp;
    const param_desc_t *d;
    int32_t r[4], x, py = 0, xc, k, nd;
    uint32_t ty = ft_of(t->p);
    char b[32];
    for (k = 0; k < 4; k++) {
        d = page_desc(pg, (uint32_t)k, &vp);
        r[k] = d ? clamp(RATIO(d, enum_rank(d, *vp)), 0, 1000) : 0;
    }
    xc = 10 + r[1] * 216 / 1000;
    for (k = 0; k < 3; k++)
        cv_rect(64 + k * 54, 6, 1, 99, T_RAISE);
    for (x = 10; x <= 226; x++) {
        int32_t y = ed_resp(ty, x, xc, r[2]);
        cv_rect(x, y, 1, 105 - y, ux_mix(T_SURF, heat_col(40 + x * 160 / 226), 30));
        if (x > 10)
            cv_line_t(x - 1, py, x, y, T_THEME, 2);
        py = y;
    }
    for (k = 0; k < 3; k++)
        cv_text(67 + k * 54, 108 - AF_X_CAP_Y, &AF_X, FX[k], T_TEXT);
    cv_rect(xc, 6, 1, 99, T_ACCENT);
    og_node(xc * 16, ed_resp(ty, xc, xc, r[2]) * 16, 4, T_ACCENT);
    str_cpy(b, N_FTYPE[ty], 32);
    str_cpy(b + str_len(b), trk_eng(t) == 0u ? " \xB7 PER VOICE" : " \xB7 ON THE PART", 32 - str_len(b));
    ed_cap(10, 6, b, T_TEXT);
    d = page_desc(pg, 3, &vp);
    if (d) {
        const char *u = "";
        str_cpy(b, d->label, 32);
        str_cpy(b + str_len(b), " ", 2);
        param_format(d, *vp, b + str_len(b), &u);
        str_cpy(b + str_len(b), u, 32 - str_len(b));
        ed_cap(10, 120, b, T_TEXT);
    }
    nd = r[3] * 20 / 1000;
    for (k = 0; k < 20; k++)
        cv_rect(110 + k * 6, 116, 4, 12, k < nd ? T_THEME : T_RAISE);
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
        ed_cap(14, y, n, i < nv ? T_TEXT : T_DIM);
        cv_rect(26, y, 202, 7, i < nv ? T_RAISE : T_LINE);
        if (lv > 0)
            cv_rect(26, y, lv, 7, heat_col(60 + lv));
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
            cv_line_t(122 + x - 1, py, 122 + x, y, T_THEME, 2);
        py = y;
    }
}
/* (V10.7.33, the owner) EDIT > AUTO KNOBS (was FOCUS): "pick a knob and what moves it". On top the knob picked (KNOB 1) and its
 * value (KNOB 2) as a bar of the thermal ramp (heat_col); under it what moves it and how much (KNOB 3 SRC, KNOB 4 AMT: the
 * MOD slot aimed at it, the same MATRIX shows); the eight knobs at the foot, the one picked lit. (The gauge's circle gone) */
static void focus_bar(int32_t y, int32_t f)             /* a track 220 px, 0..f (0..1000) of it in the ramp (heat_bar) */
{
    cv_rect(10, y, 220, 12, T_RAISE);
    heat_bar(10, y, f * 220 / 1000, 220, 12, T_THEME);
}
static void graph_focus_lz(const track_t *t)
{
    uint32_t k = focus_k(), i;
    int32_t j = focus_slot(k), a = 0;
    const param_desc_t *d = track_desc(t, P_E0 + k);
    const char *u = "";
    char b[24];
    param_format(d, t->p[P_E0 + k], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_text(10, 6, &AF_M, d->label, T_TEXT);
    cv_text_r(230, 6, &AF_M, b, T_THEME, T_SURF);
    focus_bar(28, clamp(RATIO(d, enum_rank(d, t->p[P_E0 + k])), 0, 1000));
    ed_cap(10, 54, "MOVED BY", T_TEXT);
    if (j >= 0) {
        const param_desc_t *da = track_desc(t, P_M1AMT + 3u * (uint32_t)j);
        a = t->p[P_M1AMT + 3u * (uint32_t)j];
        str_cpy(b, N_MSRC[clamp(t->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)], 24);
        cv_text_r(230, 64, &AF_M, b, T_THEME, T_SURF);
        param_format(da, a, b, &u);
        str_cpy(b + str_len(b), u, 24 - str_len(b));
        cv_text(10, 64, &AF_M, b, T_TEXT);
        a = RATIO(da, a < 0 ? -a : a);
        a = a * 2 - 1000;                               /* (as much as it moves: a bipolar AMT's size) */
    } else
        cv_text(10, 64, &AF_M, "NOTHING", T_DIM);
    focus_bar(86, a);
    for (i = 0; i < 8u; i++) {                          /* E1..E8 */
        const param_desc_t *di = track_desc(t, P_E0 + i);
        int32_t x = 8 + (int32_t)i * 28;
        uint16_t bg = i == k ? T_THEME : T_RAISE;
        cv_rect(x, 118, 26, 13, bg);
        if (di && di->label && di->label[0] != '-')
            cv_clip_text(x + 2, 121 - AF_X_CAP_Y, &AF_X, di->label, i == k ? T_INK : T_TEXT, bg, 23);
    }
}
/* (FORM 3) the panel: the PROGRESSION in roman numerals, the chord playing lit; FORMULA A's equation (A, the DEFORM,
 * lit in it), B's under it while MORPH is between; the bar's 16 steps, the one playing lit */
static void form_cap(int32_t x,int32_t y,const char*s,uint16_t c){cv_text_on(x,y-AF_S_CAP_Y,&AF_S,s,c,T_BG);}
static uint32_t form_view_clock(const track_t*t){
 uint32_t c=0;
 if(func_song(t))c=func_clock_begin;
 else for(uint32_t i=0;i<NVOICE;i++)if(t->v[i].active){c=(uint32_t)(((uint64_t)t->v[i].ph[0]<<16)/((uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300)));break;}
 return c;
}
/* an equation in lines of cols characters from (x, y) in the formula font (6 px a character), A lit; at most rows
 * lines, the last ending "~" when it goes on */
static void form_eq(int32_t x,int32_t y,const char*s,uint32_t cols,uint32_t rows,uint16_t ink,uint16_t lit)
{
 uint32_t n=str_len(s),row,i;
 for(row=0;row<rows&&row*cols<n;row++){
  char ln[48];uint32_t k=0,o=row*cols;
  for(i=0;i<cols&&o+i<n&&k<sizeof ln-2u;i++)ln[k++]=s[o+i];
  if(row==rows-1u&&o+cols<n&&k)ln[k-1]='~';
  ln[k]=0;
  cv_text_on(x,y+(int32_t)row*11-AF_F_CAP_Y,&AF_F,ln,ink,T_BG);
  for(i=0;i<k;i++)if(ln[i]=='A'){char c1[2]={'A',0},pre[48];str_cpy(pre,ln,i+1u);cv_text_on(x+text_w(&AF_F,pre),y+(int32_t)row*11-AF_F_CAP_Y,&AF_F,c1,lit,T_BG);}
 }
}
/* a scope of 64 half-rate samples in (x, y, w, h), each segment as hot as it is loud, dimmed by lvl (0 .. 100) */
static void form_scope_draw(const int16_t*sc,int32_t x,int32_t y,int32_t w,int32_t h,int32_t lvl)
{
 int32_t i,px=x,py=y+h/2;
 cv_rect(x,y+h/2,w,1,ux_mix(T_BG,T_DIM,50));
 for(i=0;i<64;i++){
  int32_t v=sc[i],nx=x+i*w/63,ny=y+h/2-v*(h/2-1)/32768,a=v<0?-v:v;
  if(i)cv_line_t(px,py,nx,ny,ux_mix(T_BG,heat_col(70+a*186/32768),lvl),2);
  px=nx;py=ny;
 }
}
/* a side's text: a bank formula's equation (A lit), or FORM 1's state: its melody and roots as written, its timbre */
static void form_text(int32_t x,int32_t y,uint32_t f,uint32_t rows,uint16_t ink)
{
 if(f>=F1_N){form_eq(x,y,FORM_BANK[f-F1_N],37u,rows,ink,heat_col(230));return;}
 {const f1_state_t*S=&F1_STATE[f];char b[48];
  str_cpy(b,"mel ",sizeof b);str_cpy(b+4,S->melody,sizeof b-4u);form_eq(x,y,b,37u,1u,ink,ink);
  if(rows>1u){str_cpy(b,"roots ",sizeof b);str_cpy(b+6,S->roots,sizeof b-6u);form_eq(x,y+11,b,37u,1u,ink,ink);}
  if(rows>2u)form_eq(x,y+22,F1_DOMAIN[S->domain%11u],37u,1u,heat_col(200),heat_col(200));}
}
static __attribute__((noinline)) void graph_form_lz(const track_t*t){
 uint32_t a=(uint32_t)clamp(t->p[P_E0],0,FORM_NT-1),b=(uint32_t)clamp(t->p[P_E1],0,FORM_NT-1),m=(uint32_t)clamp(t->p[P_E2],0,127);
 uint32_t pr=(uint32_t)clamp(t->p[P_AGATE],0,FORM_NPROG-1),len=form_plen(pr),clock=form_view_clock(t),step=(clock<<2)>>16,k,first=cur_page()->id[0]==P_E0;
 form_scale_t sc;form_scale(t,&sc);form_note_t fn;form_harmony(t,clock,step,&fn);
 int32_t mx=6+(int32_t)(228u*m/127u);
 cv_oy=0;form_scope_tr=(uint8_t)(t-trk);
 if(first){                                              /* FORM: the two waveforms playing, MORPH between them */
  form_cap(6,4,"A",heat_col(220));form_cap(18,4,FORM_NAMES[a],T_TEXT);
  form_cap(126,4,"B",heat_col(220));form_cap(138,4,FORM_NAMES[b],T_TEXT);
  cv_rect(6,14,108,62,T_SURF);cv_rect(126,14,108,62,T_SURF);
  form_scope_draw(form_scope[0],8,16,104,58,100-(int32_t)m*70/127);
  form_scope_draw(form_scope[1],128,16,104,58,30+(int32_t)m*70/127);
  heat_bar(6,82,228,228,6,T_DIM);
  cv_rect(mx-2,79,5,12,T_TEXT);
 }else{                                                  /* the others: the equation */
  form_cap(6,4,"A",heat_col(220));form_cap(18,4,FORM_NAMES[a],T_TEXT);
  if(m&&a!=b){form_cap(136,4,"B",heat_col(220));form_cap(148,4,FORM_NAMES[b],T_TEXT);}
  heat_bar(6,13,228,228,2,T_DIM);cv_rect(mx-1,11,3,6,T_TEXT);
  form_text(6,22,m==127u?b:a,m&&m<127u&&a!=b?3u:6u,m<64u?T_TEXT:T_DIM);
  if(m&&m<127u&&a!=b)form_text(6,57,b,3u,m>=64u?T_TEXT:T_DIM);
 }
 {int32_t bw=len>4u?28:54,x0=(240-(int32_t)len*bw)/2;     /* the progression, the chord playing hot */
  for(k=0;k<len;k++){char nb[8];int32_t x=x0+(int32_t)k*bw,on=k==fn.chord;uint16_t c=on?heat_col(200):T_SURF;
   form_roman(&sc,FORM_PROGS[pr][k],nb);
   cv_rect(x+1,94,bw-2,14,c);
   cv_text_on(x+(bw-text_w(&AF_S,nb))/2,97-AF_S_CAP_Y,&AF_S,nb,on?T_BG:T_TEXT,c);}}
 {uint32_t hits=(uint32_t)clamp(t->p[P_KEY],1,16),rot=(uint32_t)clamp(t->p[P_DETUNE],0,15);   /* the bar's steps: the EUCLID's hits hot */
  for(k=0;k<16u;k++){int32_t x=6+(int32_t)k*14;uint32_t kk=(k+rot)&15u,hit=hits>=16u||((kk+1u)*hits>>4)!=(kk*hits>>4);
   cv_rect(x,112,12,6,k==(step&15u)?(hit?heat_col(240):T_TEXT):hit?heat_col(90+(k&3u?0:40)):T_SURF);}}
}
/* the lens's panel for EDIT's page pg (1), else 0 (its own) */
static int graph_edit_lz(const track_t *t, const page_t *pg)
{
    uint32_t e = trk_eng(t);
    if(e==ENGI_FORM&&(pg->scope==SC_ENGINE||pg->scope==SC_FORM))
        graph_form_lz(t);
    else if ((pg->scope == SC_ENGINE && e != ENGI_DRUM && !(FELUCCA_FM4 && e == ENGI_DIGITAL)) || pg->graph == GR_SWARM)
        graph_sound_lz(t);
    else if (pg->id[0] == P_FTYPE)
        graph_filter_lz(t, pg);
    else if (pg->id[0] == P_VOICE || pg->id[0] == P_ALLOC)
        graph_voice_lz(t, pg);
    else if (pg->graph == GR_FOCUS)
        graph_focus_lz(t);
    else
        return 0;
    return 1;
}
