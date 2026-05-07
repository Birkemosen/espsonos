/*
 * sonos_ui.c  —  Multiroom Music Player UI
 *
 * Eversolo-inspired layout for 800×480 touch LCD.
 *   • Right-side icon navigation strip (60 px wide, 4 tabs × 120 px)
 *   • Full-height content panels (740 × 480), no separate header bar
 *   • Dark palette, large touch targets, generous spacing
 *
 * Tabs: [0] Home  [1] Now Playing  [2] Multi Room  [3] Settings
 */

#include "sonos_ui.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* ── geometry ────────────────────────────────────────────────────── */
#define SCR_W      800
#define SCR_H      480
#define PANEL_W    740   /* content area width  */
#define PANEL_H    480   /* content area height */
#define NAV_X      740   /* right nav x-origin  */
#define NAV_W       60   /* right nav width     */
#define NAV_BTN_H  120   /* 480 / 4 tabs        */

/* ── palette ─────────────────────────────────────────────────────── */
#define C_BG      0x0C0C10   /* screen background   */
#define C_SURFACE 0x161618   /* panel / card        */
#define C_CARD    0x1E1E24   /* elevated card       */
#define C_CARD_HI 0x28282E   /* slider track / btn  */
#define C_NAV     0x080A0C   /* nav strip bg        */
#define C_NAV_ACT 0x141418   /* active nav btn bg   */
#define C_SEP     0x222228   /* separator           */
#define C_ACCENT  0x0A84FF   /* primary accent      */
#define C_PLAYING 0x34C759   /* green = playing     */
#define C_TEXT    0xF2F2F7   /* primary text        */
#define C_DIM     0x8E8E93   /* secondary text      */
#define C_HINT    0x3A3A40   /* tertiary / inactive */

/* ── fonts ───────────────────────────────────────────────────────── */
#define F14 (&lv_font_montserrat_14)
#define F16 (&lv_font_montserrat_16)
#define F20 (&lv_font_montserrat_20)
#define F24 (&lv_font_montserrat_24)
#define F28 (&lv_font_montserrat_28)

/* ── tabs ────────────────────────────────────────────────────────── */
#define TAB_HOME     0
#define TAB_MUSIC    1
#define TAB_PLAYERS  2
#define TAB_SETTINGS 3
#define TAB_COUNT    4

static const char *tab_icons[TAB_COUNT] = {
    LV_SYMBOL_HOME, LV_SYMBOL_AUDIO, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS
};

/* ── state ───────────────────────────────────────────────────────── */
static int        s_tab = TAB_PLAYERS;
static lv_obj_t  *s_root;
static lv_obj_t  *s_panels[TAB_COUNT];
static lv_obj_t  *s_nav_btn[TAB_COUNT];
static lv_obj_t  *s_nav_ico[TAB_COUNT];
static lv_obj_t  *s_nav_bar[TAB_COUNT];

#define PLAYER_COUNT 2
static lv_obj_t  *pl_slider[PLAYER_COUNT];
static lv_obj_t  *pl_dot[PLAYER_COUNT];
static bool       pl_playing[PLAYER_COUNT] = {true, true};

static lv_obj_t  *np_lbl_track;
static lv_obj_t  *np_lbl_artist;
static lv_obj_t  *np_lbl_album;
static lv_obj_t  *np_lbl_room;
static lv_obj_t  *np_bar_prog;
static lv_obj_t  *np_lbl_pos;
static lv_obj_t  *np_lbl_total;
static lv_obj_t  *np_lbl_pp;
static lv_obj_t  *np_slider_vol;
static bool       np_playing = true;

/* ================================================================= */
/* Helpers                                                           */
/* ================================================================= */

static void no_scroll(lv_obj_t *o) {
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *make_box(lv_obj_t *p, int x, int y, int w, int h,
                           uint32_t bg, lv_opa_t opa, int r) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    no_scroll(o);
    return o;
}

static lv_obj_t *make_box_bordered(lv_obj_t *p, int x, int y, int w, int h,
                                    uint32_t bg, uint32_t bc, int bw, int r) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(bc), 0);
    lv_obj_set_style_border_width(o, bw, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    no_scroll(o);
    return o;
}

static lv_obj_t *make_lbl(lv_obj_t *p, const char *txt,
                           int x, int y, const lv_font_t *f, uint32_t col) {
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    return l;
}

static lv_obj_t *make_btn(lv_obj_t *p, int x, int y, int w, int h,
                           uint32_t bg, int r, lv_event_cb_t cb, void *ud) {
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, r, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    no_scroll(b);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

static void make_sep(lv_obj_t *p, int x, int y, int w) {
    lv_obj_t *s = lv_obj_create(p);
    lv_obj_set_pos(s, x, y);
    lv_obj_set_size(s, w, 1);
    lv_obj_set_style_bg_color(s, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    no_scroll(s);
}

/* ── volume helpers ─────────────────────────────────────────────── */

static lv_obj_t *make_vol_slider(lv_obj_t *p, int x, int y,
                                  int w, int val, bool active) {
    lv_obj_t *sl = lv_slider_create(p);
    lv_obj_set_pos(sl, x, y);
    lv_obj_set_size(sl, w, 8);
    lv_slider_set_value(sl, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(active ? C_ACCENT : C_DIM), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sl, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 8, LV_PART_KNOB);
    return sl;
}

static void vol_minus_cb(lv_event_t *e) {
    lv_obj_t *sl = (lv_obj_t *)lv_event_get_user_data(e);
    int v = lv_slider_get_value(sl);
    if (v > 5) lv_slider_set_value(sl, v - 5, LV_ANIM_OFF);
}
static void vol_plus_cb(lv_event_t *e) {
    lv_obj_t *sl = (lv_obj_t *)lv_event_get_user_data(e);
    int v = lv_slider_get_value(sl);
    if (v < 100) lv_slider_set_value(sl, v + 5, LV_ANIM_OFF);
}

/* [speaker] [===slider===] [−] [+]   returns the slider widget */
static lv_obj_t *make_vol_row(lv_obj_t *p, int x, int y,
                               int total_w, int val, bool active) {
    const int btn_sz = 40;
    const int icon_w = 26;
    const int gap    = 12;
    int sl_w = total_w - icon_w - gap - btn_sz - gap - btn_sz - gap;
    if (sl_w < 40) sl_w = 40;

    make_lbl(p, LV_SYMBOL_VOLUME_MID, x, y + 8, F16,
             active ? C_TEXT : C_HINT);

    lv_obj_t *sl = make_vol_slider(p, x + icon_w + gap, y + 16,
                                   sl_w, val, active);

    int bx = x + icon_w + gap + sl_w + gap;

    lv_obj_t *bm = make_btn(p, bx, y, btn_sz, btn_sz,
                            C_CARD_HI, LV_RADIUS_CIRCLE, vol_minus_cb, sl);
    lv_obj_t *lm = make_lbl(bm, "-", 0, 0, F20, C_TEXT);
    lv_obj_align(lm, LV_ALIGN_CENTER, 0, -1);

    lv_obj_t *bp = make_btn(p, bx + btn_sz + gap, y, btn_sz, btn_sz,
                            C_CARD_HI, LV_RADIUS_CIRCLE, vol_plus_cb, sl);
    lv_obj_t *lp = make_lbl(bp, "+", 0, 0, F20, C_TEXT);
    lv_obj_align(lp, LV_ALIGN_CENTER, 0, -1);

    return sl;
}

/* ================================================================= */
/* Right-side navigation strip                                       */
/* ================================================================= */

static void navigate_to(int tab) {
    if (tab < 0 || tab >= TAB_COUNT) return;
    if (s_panels[s_tab]) lv_obj_add_flag(s_panels[s_tab], LV_OBJ_FLAG_HIDDEN);
    s_tab = tab;
    if (s_panels[tab]) lv_obj_remove_flag(s_panels[tab], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < TAB_COUNT; i++) {
        bool act = (i == tab);
        if (s_nav_btn[i])
            lv_obj_set_style_bg_color(s_nav_btn[i],
                                      lv_color_hex(act ? C_NAV_ACT : C_NAV), 0);
        if (s_nav_ico[i])
            lv_obj_set_style_text_color(s_nav_ico[i],
                                        lv_color_hex(act ? C_ACCENT : C_HINT), 0);
        if (s_nav_bar[i])
            lv_obj_set_style_bg_color(s_nav_bar[i],
                                      lv_color_hex(act ? C_ACCENT : C_NAV), 0);
    }
}

static void nav_cb(lv_event_t *e) {
    navigate_to((int)(intptr_t)lv_event_get_user_data(e));
}

static void create_nav_strip(lv_obj_t *root) {
    /* vertical separator line between content and nav */
    lv_obj_t *sep = lv_obj_create(root);
    lv_obj_set_pos(sep, NAV_X, 0);
    lv_obj_set_size(sep, 1, SCR_H);
    lv_obj_set_style_bg_color(sep, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);

    /* nav strip container */
    lv_obj_t *nav = make_box(root, NAV_X + 1, 0, NAV_W - 1, SCR_H,
                             C_NAV, LV_OPA_COVER, 0);

    for (int i = 0; i < TAB_COUNT; i++) {
        bool act = (i == s_tab);
        int by = i * NAV_BTN_H;

        s_nav_btn[i] = make_btn(nav, 0, by, NAV_W - 1, NAV_BTN_H,
                                act ? C_NAV_ACT : C_NAV, 0,
                                nav_cb, (void *)(intptr_t)i);

        /* active indicator — 3 px bar on the left edge */
        s_nav_bar[i] = make_box(s_nav_btn[i], 0, (NAV_BTN_H - 44) / 2,
                                3, 44,
                                act ? C_ACCENT : C_NAV,
                                LV_OPA_COVER, 2);

        /* icon — centred in the button */
        s_nav_ico[i] = lv_label_create(s_nav_btn[i]);
        lv_label_set_text(s_nav_ico[i], tab_icons[i]);
        lv_obj_set_style_text_font(s_nav_ico[i], F20, 0);
        lv_obj_set_style_text_color(s_nav_ico[i],
                                    lv_color_hex(act ? C_ACCENT : C_HINT), 0);
        lv_obj_set_style_bg_opa(s_nav_ico[i], LV_OPA_TRANSP, 0);
        lv_obj_align(s_nav_ico[i], LV_ALIGN_CENTER, 3, 0);

        /* thin separator between nav buttons */
        if (i < TAB_COUNT - 1) {
            make_sep(nav, 8, (i + 1) * NAV_BTN_H, NAV_W - 16);
        }
    }
}

/* ================================================================= */
/* HOME panel  (740 × 480)                                          */
/*   Title  |  4 source tiles  |  Playing Now rows                  */
/* ================================================================= */

static void create_panel_home(lv_obj_t *panel) {
    make_lbl(panel, "Home", 20, 18, F24, C_TEXT);
    make_sep(panel, 0, 58, PANEL_W);

    /* 4 source tiles in a row */
    static const char *tnames[4] = {"Spotify", "Radio", "Library", "Podcasts"};
    static const char *ticons[4] = {
        LV_SYMBOL_AUDIO, LV_SYMBOL_WIFI, LV_SYMBOL_DRIVE, LV_SYMBOL_BELL
    };
    int tw = 155, th = 148, tg = 12;
    int tm = (PANEL_W - 4 * tw - 3 * tg) / 2;  /* ~30 */

    for (int i = 0; i < 4; i++) {
        int tx = tm + i * (tw + tg);
        lv_obj_t *t = make_box(panel, tx, 70, tw, th, C_CARD, LV_OPA_COVER, 14);

        lv_obj_t *ico = lv_label_create(t);
        lv_label_set_text(ico, ticons[i]);
        lv_obj_set_style_text_font(ico, F28, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_bg_opa(ico, LV_OPA_TRANSP, 0);
        lv_obj_align(ico, LV_ALIGN_CENTER, 0, -18);

        lv_obj_t *lbl = lv_label_create(t);
        lv_label_set_text(lbl, tnames[i]);
        lv_obj_set_style_text_font(lbl, F16, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(C_TEXT), 0);
        lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -12);
    }

    /* Playing Now section */
    int py = 70 + th + 20;
    make_lbl(panel, "Playing Now", 20, py, F16, C_DIM);
    py += 32;

    static const char *rooms[]  = {"Living Room", "Bathroom"};
    static const char *tracks[] = {"Forever Chemicals – Placebo",
                                   "Karma Police – Radiohead"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *row = make_box(panel, 16, py, PANEL_W - 32, 54,
                                 C_SURFACE, LV_OPA_COVER, 10);

        /* playing dot */
        make_box(row, 14, 23, 10, 10, C_PLAYING, LV_OPA_COVER, LV_RADIUS_CIRCLE);

        /* room */
        make_lbl(row, rooms[i],  32, 8,  F16, C_TEXT);
        /* track */
        make_lbl(row, tracks[i], 32, 28, F14, C_DIM);

        /* chevron */
        lv_obj_t *chev = lv_label_create(row);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(chev, F16, 0);
        lv_obj_set_style_text_color(chev, lv_color_hex(C_HINT), 0);
        lv_obj_set_style_bg_opa(chev, LV_OPA_TRANSP, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -14, 0);

        py += 62;
    }
}

/* ================================================================= */
/* MUSIC / NOW PLAYING panel  (740 × 480)                           */
/* ================================================================= */

static void np_pp_cb(lv_event_t *e) {
    (void)e;
    np_playing = !np_playing;
    const char *sym = np_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    if (np_lbl_pp) lv_label_set_text(np_lbl_pp, sym);
    /* reflect in nav icon */
    if (s_nav_ico[TAB_MUSIC])
        lv_label_set_text(s_nav_ico[TAB_MUSIC],
                          np_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_AUDIO);
}

static void create_panel_music(lv_obj_t *panel) {
    /* ── left column: album art ── */
    const int art_sz = 262;
    const int art_x  = 24;
    const int art_y  = (PANEL_H - art_sz) / 2;  /* ~109 */

    lv_obj_t *art = make_box(panel, art_x, art_y, art_sz, art_sz,
                             C_CARD, LV_OPA_COVER, 14);
    lv_obj_t *aic = lv_label_create(art);
    lv_label_set_text(aic, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(aic, F28, 0);
    lv_obj_set_style_text_color(aic, lv_color_hex(C_HINT), 0);
    lv_obj_set_style_bg_opa(aic, LV_OPA_TRANSP, 0);
    lv_obj_align(aic, LV_ALIGN_CENTER, 0, 0);

    /* ── right column ── */
    const int rx = art_x + art_sz + 24;   /* ~310 */
    const int rw = PANEL_W - rx - 16;     /* ~414 */

    /* room */
    np_lbl_room = make_lbl(panel, LV_SYMBOL_LIST " Living Room",
                           rx, 34, F14, C_DIM);

    /* track / artist / album */
    np_lbl_track  = make_lbl(panel, "Forever Chemicals", rx, 68,  F24, C_TEXT);
    np_lbl_artist = make_lbl(panel, "Placebo",           rx, 103, F20, C_DIM);
    np_lbl_album  = make_lbl(panel, "Never Let Me Go",   rx, 132, F16, C_HINT);

    /* progress bar */
    np_bar_prog = lv_bar_create(panel);
    lv_obj_set_pos(np_bar_prog, rx, 172);
    lv_obj_set_size(np_bar_prog, rw, 8);
    lv_bar_set_range(np_bar_prog, 0, 1000);
    lv_bar_set_value(np_bar_prog, 350, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(np_bar_prog, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(np_bar_prog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(np_bar_prog, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_bar_prog, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(np_bar_prog, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_bar_prog, 4, LV_PART_INDICATOR);

    np_lbl_pos   = make_lbl(panel, "1:52",  rx,          186, F14, C_DIM);
    np_lbl_total = make_lbl(panel, "3:28",  rx + rw - 40, 186, F14, C_DIM);

    /* transport: [prev 48] [gap 18] [play 60] [gap 18] [next 48] */
    int ts_total = 48 + 18 + 60 + 18 + 48;  /* 192 */
    int ts_x     = rx + (rw - ts_total) / 2;
    int ts_y     = 222;

    lv_obj_t *bprev = make_btn(panel, ts_x,            ts_y + 6, 48, 48,
                               C_SURFACE, LV_RADIUS_CIRCLE, NULL, NULL);
    lv_obj_t *lprev = make_lbl(bprev, LV_SYMBOL_PREV, 0, 0, F20, C_TEXT);
    lv_obj_align(lprev, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *bplay = make_btn(panel, ts_x + 48 + 18,  ts_y, 60, 60,
                               C_ACCENT, LV_RADIUS_CIRCLE, np_pp_cb, NULL);
    np_lbl_pp = make_lbl(bplay, LV_SYMBOL_PAUSE, 0, 0, F20, C_TEXT);
    lv_obj_set_style_bg_opa(np_lbl_pp, LV_OPA_TRANSP, 0);
    lv_obj_align(np_lbl_pp, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *bnext = make_btn(panel, ts_x + 48+18+60+18, ts_y + 6, 48, 48,
                               C_SURFACE, LV_RADIUS_CIRCLE, NULL, NULL);
    lv_obj_t *lnext = make_lbl(bnext, LV_SYMBOL_NEXT, 0, 0, F20, C_TEXT);
    lv_obj_align(lnext, LV_ALIGN_CENTER, 0, 0);

    /* volume row */
    np_slider_vol = make_vol_row(panel, rx, 308, rw, 70, true);
}

/* ================================================================= */
/* MULTI ROOM panel  (740 × 480)                                    */
/*   Header  |  Player card 0  |  Player card 1                     */
/* ================================================================= */

static void pl_toggle_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= PLAYER_COUNT) return;
    pl_playing[idx] = !pl_playing[idx];
    if (pl_dot[idx])
        lv_obj_set_style_bg_color(pl_dot[idx],
                                  lv_color_hex(pl_playing[idx] ? C_PLAYING : C_HINT), 0);
}

static lv_obj_t *make_player_card(lv_obj_t *panel,
                                  int cy, bool bordered,
                                  const char *name, const char *model,
                                  int player_idx, int vol) {
    const int cx = 14;
    const int cw = PANEL_W - 28;  /* 712 */
    const int ch = 182;

    lv_obj_t *card = bordered
        ? make_box_bordered(panel, cx, cy, cw, ch, C_SURFACE, C_ACCENT, 2, 12)
        : make_box         (panel, cx, cy, cw, ch, C_SURFACE, LV_OPA_COVER, 12);

    /* status dot — tap card to toggle */
    pl_dot[player_idx] = make_box(card, cw - 22, 14, 12, 12,
                                  C_PLAYING, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_add_event_cb(pl_dot[player_idx], pl_toggle_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)player_idx);

    /* device art placeholder */
    lv_obj_t *art = make_box(card, 14, 14, 60, 60, C_CARD, LV_OPA_COVER, 8);
    lv_obj_t *aic = lv_label_create(art);
    lv_label_set_text(aic, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(aic, F16, 0);
    lv_obj_set_style_text_color(aic, lv_color_hex(C_HINT), 0);
    lv_obj_set_style_bg_opa(aic, LV_OPA_TRANSP, 0);
    lv_obj_align(aic, LV_ALIGN_CENTER, 0, 0);

    /* name + model */
    make_lbl(card, name,  84, 16, F20, C_TEXT);
    make_lbl(card, model, 84, 43, F16, C_DIM);

    /* volume row */
    pl_slider[player_idx] = make_vol_row(card, 14, ch - 56,
                                         cw - 28, vol, true);
    return card;
}

static void create_panel_players(lv_obj_t *panel) {
    /* header */
    make_lbl(panel, "Multi Room", 20, 18, F20, C_TEXT);

    /* refresh icon (top-right) */
    lv_obj_t *ri = lv_label_create(panel);
    lv_label_set_text(ri, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(ri, F16, 0);
    lv_obj_set_style_text_color(ri, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_bg_opa(ri, LV_OPA_TRANSP, 0);
    lv_obj_align(ri, LV_ALIGN_TOP_RIGHT, -20, 20);

    make_sep(panel, 0, 56, PANEL_W);

    /* player cards */
    make_player_card(panel, 68,  false, "Bathroom",    "PULSE FLEX 2i", 0, 60);
    make_player_card(panel, 68 + 182 + 12,
                             true,  "Living Room", "SOUNDBAR+",    1, 78);
}

/* ================================================================= */
/* SETTINGS panel  (740 × 480)                                      */
/* ================================================================= */

static void create_panel_settings(lv_obj_t *panel) {
    make_lbl(panel, "Settings", 20, 18, F24, C_TEXT);
    make_sep(panel, 0, 58, PANEL_W);

    static const struct {
        const char *icon;
        const char *title;
        const char *value;
    } items[] = {
        { LV_SYMBOL_WIFI,       "Network",  "Connected"    },
        { LV_SYMBOL_BLUETOOTH,  "Bluetooth","On"           },
        { LV_SYMBOL_VOLUME_MID, "Audio",    "Settings"     },
        { LV_SYMBOL_EYE_OPEN,   "Display",  "Auto"         },
        { LV_SYMBOL_WARNING,    "About",    "v1.0.0"       },
    };

    int row_h = 84;
    for (int i = 0; i < 5; i++) {
        int ry = 68 + i * row_h;

        /* row bg — subtle highlight */
        lv_obj_t *row = make_box(panel, 16, ry, PANEL_W - 32, row_h - 4,
                                 C_SURFACE, LV_OPA_COVER, 10);

        /* icon */
        lv_obj_t *ico = lv_label_create(row);
        lv_label_set_text(ico, items[i].icon);
        lv_obj_set_style_text_font(ico, F20, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_bg_opa(ico, LV_OPA_TRANSP, 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 18, 0);

        /* title */
        make_lbl(row, items[i].title, 62, (row_h - 4) / 2 - 11, F20, C_TEXT);

        /* value */
        lv_obj_t *val = lv_label_create(row);
        lv_label_set_text(val, items[i].value);
        lv_obj_set_style_text_font(val, F16, 0);
        lv_obj_set_style_text_color(val, lv_color_hex(C_DIM), 0);
        lv_obj_set_style_bg_opa(val, LV_OPA_TRANSP, 0);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -48, 0);

        /* chevron */
        lv_obj_t *chev = lv_label_create(row);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(chev, F16, 0);
        lv_obj_set_style_text_color(chev, lv_color_hex(C_HINT), 0);
        lv_obj_set_style_bg_opa(chev, LV_OPA_TRANSP, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -16, 0);
    }
}

/* ================================================================= */
/* Init                                                              */
/* ================================================================= */

void sonos_ui_init(void) {
    /* root screen — full 800×480 */
    s_root = lv_obj_create(NULL);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    no_scroll(s_root);

    /* content panels — 740 × 480, only one visible at a time */
    for (int i = 0; i < TAB_COUNT; i++) {
        s_panels[i] = make_box(s_root, 0, 0, PANEL_W, PANEL_H,
                               C_BG, LV_OPA_COVER, 0);
        if (i != s_tab)
            lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    }

    create_panel_home    (s_panels[TAB_HOME]);
    create_panel_music   (s_panels[TAB_MUSIC]);
    create_panel_players (s_panels[TAB_PLAYERS]);
    create_panel_settings(s_panels[TAB_SETTINGS]);

    /* right nav strip */
    create_nav_strip(s_root);

    lv_screen_load(s_root);
}

void sonos_ui_tick(void) {}

/* ================================================================= */
/* Public API                                                        */
/* ================================================================= */

void sonos_ui_set_track(const char *title, const char *artist, const char *album) {
    if (np_lbl_track)  lv_label_set_text(np_lbl_track,  title  ? title  : "");
    if (np_lbl_artist) lv_label_set_text(np_lbl_artist, artist ? artist : "");
    if (np_lbl_album)  lv_label_set_text(np_lbl_album,  album  ? album  : "");
}

void sonos_ui_set_progress(int pos_sec, int total_sec) {
    if (!np_bar_prog) return;
    lv_bar_set_value(np_bar_prog,
                     total_sec > 0 ? pos_sec * 1000 / total_sec : 0, LV_ANIM_OFF);
    char b[8];
    if (np_lbl_pos) {
        snprintf(b, sizeof(b), "%d:%02d", pos_sec / 60, pos_sec % 60);
        lv_label_set_text(np_lbl_pos, b);
    }
    if (np_lbl_total && total_sec > 0) {
        snprintf(b, sizeof(b), "%d:%02d", total_sec / 60, total_sec % 60);
        lv_label_set_text(np_lbl_total, b);
    }
}

void sonos_ui_set_playing(bool playing) {
    np_playing = playing;
    const char *sym = playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    if (np_lbl_pp) lv_label_set_text(np_lbl_pp, sym);
    /* update nav icon to reflect current play state */
    if (s_nav_ico[TAB_MUSIC])
        lv_label_set_text(s_nav_ico[TAB_MUSIC],
                          playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_AUDIO);
}

void sonos_ui_set_volume(int v) {
    if (np_slider_vol) lv_slider_set_value(np_slider_vol, v, LV_ANIM_OFF);
}

void sonos_ui_set_room(const char *name) {
    if (np_lbl_room && name) lv_label_set_text(np_lbl_room, name);
}

void sonos_ui_set_room_state(int idx, const char *name,
                              const char *now_playing, bool playing, int vol_pct) {
    if (idx < 0 || idx >= PLAYER_COUNT) return;
    pl_playing[idx] = playing;
    if (pl_dot[idx])
        lv_obj_set_style_bg_color(pl_dot[idx],
                                  lv_color_hex(playing ? C_PLAYING : C_HINT), 0);
    if (pl_slider[idx])
        lv_slider_set_value(pl_slider[idx], vol_pct, LV_ANIM_OFF);
    (void)name; (void)now_playing;
}

void sonos_ui_set_queue_item(int idx, const char *title, const char *artist,
                              const char *duration, bool is_current) {
    (void)idx; (void)title; (void)artist; (void)duration; (void)is_current;
}
