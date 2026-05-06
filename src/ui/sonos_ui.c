/*
 * sonos_ui.c  –  Multiroom Music Player UI
 *
 * Inspired by the Bluesound mobile UI. Designed for 800×480 touch panels
 * (Waveshare 4.3" / 7"). No overlays, no scrolling – all navigation is
 * direct tab-to-screen.
 *
 * Layout
 * ──────
 *   Screen (800×480)
 *   ┌─────────────────────────────────────────────┐
 *   │                                             │
 *   │   Content area  800 × 422                  │
 *   │                                             │
 *   ├─────────────────────────────────────────────┤
 *   │   Nav bar       800 × 58  (y = 422)         │
 *   └─────────────────────────────────────────────┘
 *
 *   Tab order:  [0] Rooms  [1] Now Playing  [2] Browse  [3] Queue
 *
 * Fonts used:  montserrat 14 (default), 16, 20, 24, 28
 */

#include "sonos_ui.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* ───────────────────────────── geometry ─────────────────────────────── */
#define SCR_W      800
#define SCR_H      480
#define NAV_H       58
#define CONTENT_H  (SCR_H - NAV_H)   /* 422 */

/* ───────────────────────────── palette ──────────────────────────────── */
#define C_BG         0x1C1C1E   /* screen background  */
#define C_SURFACE    0x2C2C2E   /* raised surface     */
#define C_CARD       0x3A3A3C   /* card background    */
#define C_CARD_HI    0x48484A   /* highlighted card   */
#define C_ACCENT     0x0A84FF   /* iOS blue           */
#define C_PLAYING    0x32D74B   /* green dot          */
#define C_TEXT       0xFFFFFF   /* primary text       */
#define C_DIM        0xAEAEB2   /* secondary text     */
#define C_HINT       0x636366   /* tertiary / hints   */
#define C_SEP        0x38383A   /* separator lines    */
#define C_NAV        0x111111   /* nav bar bg         */
#define C_NAV_ACT    0x0A84FF   /* active tab text    */

/* ───────────────────────────── font shorthands ───────────────────────── */
#define F14  (&lv_font_montserrat_14)
#define F16  (&lv_font_montserrat_16)
#define F20  (&lv_font_montserrat_20)
#define F24  (&lv_font_montserrat_24)
#define F28  (&lv_font_montserrat_28)

/* ───────────────────────────── state ────────────────────────────────── */
#define TAB_ROOMS   0
#define TAB_NOW     1
#define TAB_BROWSE  2
#define TAB_QUEUE   3
#define TAB_COUNT   4

static lv_obj_t *s_screens[TAB_COUNT];
static int       s_tab     = TAB_NOW;

/* nav widgets (live on lv_layer_top) */
static lv_obj_t *s_nav_btn[TAB_COUNT];
static lv_obj_t *s_nav_lbl[TAB_COUNT];

/* now-playing updatables */
static lv_obj_t *np_lbl_track;
static lv_obj_t *np_lbl_artist;
static lv_obj_t *np_lbl_album;
static lv_obj_t *np_lbl_room;
static lv_obj_t *np_dot_playing;
static lv_obj_t *np_bar_prog;
static lv_obj_t *np_lbl_pos;
static lv_obj_t *np_lbl_total;
static lv_obj_t *np_lbl_playpause;
static lv_obj_t *np_slider_vol;
static bool      np_playing = true;

/* rooms updatables */
#define ROOM_COUNT 4
static lv_obj_t *rm_lbl_name   [ROOM_COUNT];
static lv_obj_t *rm_lbl_playing[ROOM_COUNT];
static lv_obj_t *rm_dot        [ROOM_COUNT];
static lv_obj_t *rm_lbl_pp    [ROOM_COUNT];   /* play/pause icon label */
static lv_obj_t *rm_slider_vol [ROOM_COUNT];
static bool      rm_playing    [ROOM_COUNT];

/* queue updatables */
#define QUEUE_COUNT 7
static lv_obj_t *q_row       [QUEUE_COUNT];
static lv_obj_t *q_lbl_track [QUEUE_COUNT];
static lv_obj_t *q_lbl_dur   [QUEUE_COUNT];

/* ═══════════════════════ internal helpers ═══════════════════════════════ */

static void no_scroll(lv_obj_t *o) {
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *make_screen(void) {
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_size(s, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    no_scroll(s);
    return s;
}

static lv_obj_t *make_box(lv_obj_t *p, int x, int y, int w, int h,
                           uint32_t bg, lv_opa_t opa, int radius) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    no_scroll(o);
    return o;
}

static lv_obj_t *make_lbl(lv_obj_t *p, const char *txt,
                           int x, int y,
                           const lv_font_t *font, uint32_t col) {
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    return l;
}

/* button: rounded, flat – label is added by caller */
static lv_obj_t *make_btn(lv_obj_t *p, int x, int y, int w, int h,
                           uint32_t bg, int radius,
                           lv_event_cb_t cb, void *udata) {
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, radius, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    no_scroll(b);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, udata);
    return b;
}

/* separator line */
static void make_sep(lv_obj_t *p, int x, int y, int w, int h) {
    lv_obj_t *s = lv_obj_create(p);
    lv_obj_set_pos(s, x, y);
    lv_obj_set_size(s, w, h);
    lv_obj_set_style_bg_color(s, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    no_scroll(s);
}

/* ══════════════════════ navigation ══════════════════════════════════════ */

static void navigate_to(int tab);

static void nav_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    navigate_to(idx);
}

static void navigate_to(int tab) {
    if (tab < 0 || tab >= TAB_COUNT) return;
    s_tab = tab;
    lv_screen_load_anim(s_screens[tab], LV_SCR_LOAD_ANIM_FADE_IN, 120, 0, false);

    /* update nav highlight */
    for (int i = 0; i < TAB_COUNT; i++) {
        uint32_t col = (i == tab) ? C_NAV_ACT : C_DIM;
        lv_obj_set_style_text_color(s_nav_lbl[i], lv_color_hex(col), 0);
        lv_obj_set_style_bg_color(s_nav_btn[i],
                                   lv_color_hex(i == tab ? 0x1C2A3A : C_NAV), 0);
    }
}

/* ══════════════════════ nav bar (top layer) ══════════════════════════════ */

static const char *tab_names[TAB_COUNT] = {"Rooms", "Now Playing", "Browse", "Queue"};

static void create_nav_bar(void) {
    lv_obj_t *top = lv_layer_top();
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);

    int btn_w = SCR_W / TAB_COUNT;  /* 200 */

    for (int i = 0; i < TAB_COUNT; i++) {
        uint32_t bg  = (i == s_tab) ? 0x1C2A3A : C_NAV;
        uint32_t col = (i == s_tab) ? C_NAV_ACT : C_DIM;

        s_nav_btn[i] = make_btn(top, i * btn_w, SCR_H - NAV_H,
                                 btn_w, NAV_H, bg, 0, nav_cb,
                                 (void *)(intptr_t)i);

        /* top accent line for active tab */
        if (i == s_tab) {
            lv_obj_t *bar = lv_obj_create(s_nav_btn[i]);
            lv_obj_set_pos(bar, 0, 0);
            lv_obj_set_size(bar, btn_w, 3);
            lv_obj_set_style_bg_color(bar, lv_color_hex(C_ACCENT), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(bar, 0, 0);
            lv_obj_set_style_radius(bar, 0, 0);
            lv_obj_set_style_pad_all(bar, 0, 0);
            no_scroll(bar);
        }

        s_nav_lbl[i] = lv_label_create(s_nav_btn[i]);
        lv_label_set_text(s_nav_lbl[i], tab_names[i]);
        lv_obj_set_style_text_font(s_nav_lbl[i], F14, 0);
        lv_obj_set_style_text_color(s_nav_lbl[i], lv_color_hex(col), 0);
        lv_obj_align(s_nav_lbl[i], LV_ALIGN_CENTER, 0, 0);
    }

    /* nav bg underline / top edge */
    make_sep(top, 0, SCR_H - NAV_H, SCR_W, 1);
}

/* ══════════════════════ NOW PLAYING screen ══════════════════════════════ */

static void play_pause_cb(lv_event_t *e) {
    (void)e;
    np_playing = !np_playing;
    lv_label_set_text(np_lbl_playpause,
                       np_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void create_screen_now_playing(void) {
    lv_obj_t *scr = make_screen();
    s_screens[TAB_NOW] = scr;

    /* ── left panel: album art ────────────────────────────────── */
    /* left panel fills 0..383 */
    int art_x = 52, art_y = 62, art_sz = 280;

    lv_obj_t *art = make_box(scr, art_x, art_y, art_sz, art_sz,
                              C_CARD, LV_OPA_COVER, 14);

    /* music-note placeholder in art area */
    lv_obj_t *art_ico = lv_label_create(art);
    lv_label_set_text(art_ico, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(art_ico, F28, 0);
    lv_obj_set_style_text_color(art_ico, lv_color_hex(C_HINT), 0);
    lv_obj_set_style_bg_opa(art_ico, LV_OPA_TRANSP, 0);
    lv_obj_align(art_ico, LV_ALIGN_CENTER, 0, 0);

    /* room name below art */
    np_lbl_room = lv_label_create(scr);
    lv_obj_set_pos(np_lbl_room, art_x, art_y + art_sz + 14);
    lv_obj_set_size(np_lbl_room, art_sz, LV_SIZE_CONTENT);
    lv_label_set_text(np_lbl_room, "Living Room");
    lv_obj_set_style_text_font(np_lbl_room, F14, 0);
    lv_obj_set_style_text_color(np_lbl_room, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_align(np_lbl_room, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(np_lbl_room, LV_OPA_TRANSP, 0);

    /* vertical divider */
    make_sep(scr, 385, 12, 1, CONTENT_H - 24);

    /* ── right panel: track info + controls ──────────────────── */
    int rx = 396;   /* right panel content start x */
    int rw = 395;   /* usable width in right panel  */

    /* playing status row */
    np_dot_playing = make_box(scr, SCR_W - 100, 14, 9, 9,
                               C_PLAYING, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    make_lbl(scr, "Playing", SCR_W - 88, 10, F14, C_PLAYING);

    /* horizontal rule below status */
    make_sep(scr, rx - 4, 32, rw + 8, 1);

    /* track title */
    np_lbl_track = make_lbl(scr, "Gone Girl", rx, 42, F28, C_TEXT);
    lv_obj_set_size(np_lbl_track, rw, LV_SIZE_CONTENT);
    lv_label_set_long_mode(np_lbl_track, LV_LABEL_LONG_DOT);

    /* artist */
    np_lbl_artist = make_lbl(scr, "Massive Attack", rx, 84, F20, C_DIM);
    lv_obj_set_size(np_lbl_artist, rw, LV_SIZE_CONTENT);
    lv_label_set_long_mode(np_lbl_artist, LV_LABEL_LONG_DOT);

    /* album */
    np_lbl_album = make_lbl(scr, "Mezzanine", rx, 114, F16, C_HINT);
    lv_obj_set_size(np_lbl_album, rw, LV_SIZE_CONTENT);
    lv_label_set_long_mode(np_lbl_album, LV_LABEL_LONG_DOT);

    /* ── progress bar ────────────────────────────────────────── */
    np_bar_prog = lv_bar_create(scr);
    lv_obj_set_pos(np_bar_prog, rx, 150);
    lv_obj_set_size(np_bar_prog, rw, 8);
    lv_bar_set_range(np_bar_prog, 0, 1000);
    lv_bar_set_value(np_bar_prog, 320, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(np_bar_prog, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(np_bar_prog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_bar_prog, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(np_bar_prog, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_bar_prog, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(np_bar_prog, 4, LV_PART_INDICATOR);

    /* time labels */
    np_lbl_pos   = make_lbl(scr, "1:03", rx,        166, F14, C_HINT);
    np_lbl_total = make_lbl(scr, "3:13", rx + rw - 30, 166, F14, C_HINT);

    /* ── transport buttons ───────────────────────────────────── */
    /*   layout (centred in right panel, panel centre = 385+395/2 = 582)
     *   [shuf 44] 14 [prev 44] 14 [play 60] 14 [next 44] 14 [rep 44]
     *   total = 44+14+44+14+60+14+44+14+44 = 292 → start = 582-146 = 436 */
    int ty = 196;
    int tx = 436;

    /* Shuffle */
    lv_obj_t *b_shuf = make_btn(scr, tx, ty + 8, 44, 44,
                                  C_SURFACE, 22, NULL, NULL);
    lv_obj_t *l_shuf = make_lbl(b_shuf, LV_SYMBOL_SHUFFLE, 0, 0, F14, C_DIM);
    lv_obj_align(l_shuf, LV_ALIGN_CENTER, 0, 0);

    /* Prev */
    lv_obj_t *b_prev = make_btn(scr, tx + 58, ty + 8, 44, 44,
                                  C_SURFACE, 22, NULL, NULL);
    lv_obj_t *l_prev = make_lbl(b_prev, LV_SYMBOL_PREV, 0, 0, F16, C_TEXT);
    lv_obj_align(l_prev, LV_ALIGN_CENTER, 0, 0);

    /* Play / Pause  (larger, accent background) */
    lv_obj_t *b_play = make_btn(scr, tx + 116, ty, 60, 60,
                                  C_ACCENT, 30, play_pause_cb, NULL);
    np_lbl_playpause = make_lbl(b_play,
                                  np_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY,
                                  0, 0, F20, C_TEXT);
    lv_obj_align(np_lbl_playpause, LV_ALIGN_CENTER, 0, 0);

    /* Next */
    lv_obj_t *b_next = make_btn(scr, tx + 190, ty + 8, 44, 44,
                                  C_SURFACE, 22, NULL, NULL);
    lv_obj_t *l_next = make_lbl(b_next, LV_SYMBOL_NEXT, 0, 0, F16, C_TEXT);
    lv_obj_align(l_next, LV_ALIGN_CENTER, 0, 0);

    /* Repeat */
    lv_obj_t *b_rep = make_btn(scr, tx + 248, ty + 8, 44, 44,
                                 C_SURFACE, 22, NULL, NULL);
    lv_obj_t *l_rep = make_lbl(b_rep, LV_SYMBOL_LOOP, 0, 0, F14, C_DIM);
    lv_obj_align(l_rep, LV_ALIGN_CENTER, 0, 0);

    /* ── volume row ──────────────────────────────────────────── */
    int vy = 300;
    make_lbl(scr, LV_SYMBOL_MUTE,       rx,          vy + 4, F14, C_HINT);
    make_lbl(scr, LV_SYMBOL_VOLUME_MAX, rx + rw - 24, vy + 4, F14, C_HINT);

    np_slider_vol = lv_slider_create(scr);
    lv_obj_set_pos(np_slider_vol, rx + 26, vy);
    lv_obj_set_size(np_slider_vol, rw - 52, 28);
    lv_slider_set_value(np_slider_vol, 65, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(np_slider_vol, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(np_slider_vol, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_slider_vol, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(np_slider_vol, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(np_slider_vol, lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_radius(np_slider_vol, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(np_slider_vol, 4, LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_slider_vol, 10, LV_PART_KNOB);
    lv_obj_set_style_pad_all(np_slider_vol, 4, LV_PART_KNOB);

    /* ── section: mini equaliser bars (decorative) ───────────── */
    /* Three small animated-looking bars under the transport area  */
    int eq_x = rx + 2, eq_y = 360;
    int eq_heights[5] = {8, 18, 14, 22, 10};
    for (int i = 0; i < 5; i++) {
        int bh = eq_heights[i];
        lv_obj_t *eq = make_box(scr,
                                  eq_x + i * 10, eq_y + (24 - bh),
                                  6, bh, C_ACCENT, LV_OPA_40, 2);
        (void)eq;
    }
    make_lbl(scr, "Equaliser", eq_x + 56, eq_y + 6, F14, C_HINT);
}

/* ══════════════════════ ROOMS screen ════════════════════════════════════ */

static void room_play_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    rm_playing[idx] = !rm_playing[idx];
    lv_label_set_text(rm_lbl_pp[idx],
                       rm_playing[idx] ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_bg_color(rm_dot[idx],
                               lv_color_hex(rm_playing[idx] ? C_PLAYING : C_HINT), 0);
}

static void go_to_now_playing_cb(lv_event_t *e) {
    (void)e;
    navigate_to(TAB_NOW);
}

static void create_room_card(lv_obj_t *parent, int idx,
                              int x, int y, int w, int h,
                              const char *name,
                              const char *now_playing,
                              bool playing, int vol) {
    rm_playing[idx] = playing;

    lv_obj_t *card = make_box(parent, x, y, w, h, C_SURFACE, LV_OPA_COVER, 10);

    /* room name */
    rm_lbl_name[idx] = make_lbl(card, name, 14, 12, F20, C_TEXT);

    /* status dot */
    rm_dot[idx] = make_box(card, 14, 42, 8, 8,
                             playing ? C_PLAYING : C_HINT,
                             LV_OPA_COVER, LV_RADIUS_CIRCLE);

    /* now-playing text */
    rm_lbl_playing[idx] = make_lbl(card, now_playing, 28, 36, F14, C_DIM);
    lv_obj_set_size(rm_lbl_playing[idx], w - 86, LV_SIZE_CONTENT);
    lv_label_set_long_mode(rm_lbl_playing[idx], LV_LABEL_LONG_DOT);

    /* play / pause button (top-right) */
    uint32_t pp_bg = playing ? C_ACCENT : C_CARD;
    lv_obj_t *btn = make_btn(card, w - 58, 10, 44, 44, pp_bg, 22,
                               room_play_cb, (void *)(intptr_t)idx);
    rm_lbl_pp[idx] = make_lbl(btn, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY,
                                0, 0, F16, C_TEXT);
    lv_obj_align(rm_lbl_pp[idx], LV_ALIGN_CENTER, 0, 0);

    /* separator above volume */
    make_sep(card, 14, h - 62, w - 28, 1);

    /* volume label */
    char vol_str[8];
    snprintf(vol_str, sizeof(vol_str), "%d%%", vol);
    make_lbl(card, vol_str, w - 50, h - 50, F14, C_HINT);

    /* volume slider */
    rm_slider_vol[idx] = lv_slider_create(card);
    lv_obj_set_pos(rm_slider_vol[idx], 14, h - 50);
    lv_obj_set_size(rm_slider_vol[idx], w - 70, 24);
    lv_slider_set_value(rm_slider_vol[idx], vol, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(rm_slider_vol[idx],
                               lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_color(rm_slider_vol[idx],
                               lv_color_hex(playing ? C_ACCENT : C_HINT),
                               LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(rm_slider_vol[idx],
                               lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_radius(rm_slider_vol[idx], 3, LV_PART_MAIN);
    lv_obj_set_style_radius(rm_slider_vol[idx], 3, LV_PART_INDICATOR);
    lv_obj_set_style_radius(rm_slider_vol[idx], 8, LV_PART_KNOB);
    lv_obj_set_style_pad_all(rm_slider_vol[idx], 3, LV_PART_KNOB);

    /* tap card body → switch to Now Playing for this room */
    lv_obj_add_event_cb(card, go_to_now_playing_cb, LV_EVENT_CLICKED, NULL);
}

static void create_screen_rooms(void) {
    lv_obj_t *scr = make_screen();
    s_screens[TAB_ROOMS] = scr;

    /* title bar */
    lv_obj_t *title_bar = make_box(scr, 0, 0, SCR_W, 40, C_SURFACE, LV_OPA_COVER, 0);
    make_lbl(title_bar, "Rooms", 16, 10, F20, C_TEXT);
    make_sep(scr, 0, 40, SCR_W, 1);

    /* 2 × 2 grid of room cards
     * gap=6, card w=(800-18)/2=391, card h=(422-47)/2=187 */
    int gap   = 6;
    int cw    = (SCR_W - 3 * gap) / 2;   /* 394 */
    int ch    = (CONTENT_H - 40 - 3 * gap) / 2; /* 184 */
    int row1y = 40 + gap;
    int row2y = row1y + ch + gap;
    int col1x = gap;
    int col2x = col1x + cw + gap;

    create_room_card(scr, 0, col1x, row1y, cw, ch,
                      "Living Room",
                      LV_SYMBOL_AUDIO " Gone Girl - Massive Attack",
                      true, 72);
    create_room_card(scr, 1, col2x, row1y, cw, ch,
                      "Kitchen",
                      LV_SYMBOL_AUDIO " Morning Edition - NPR",
                      true, 45);
    create_room_card(scr, 2, col1x, row2y, cw, ch,
                      "Bedroom",
                      "Not Playing",
                      false, 30);
    create_room_card(scr, 3, col2x, row2y, cw, ch,
                      "Dining Room",
                      LV_SYMBOL_AUDIO " Exile Vilify - The National",
                      true, 60);
}

/* ══════════════════════ BROWSE screen ═══════════════════════════════════ */

typedef struct {
    const char *name;
    const char *icon_sym;
    uint32_t    accent;
} browse_source_t;

static const browse_source_t sources[] = {
    { "Radio",         LV_SYMBOL_AUDIO,    0xE8502A },
    { "My Music",      LV_SYMBOL_LIST,     0x0A84FF },
    { "Playlists",     LV_SYMBOL_PLAY,     0x32D74B },
    { "Spotify",       LV_SYMBOL_AUDIO,    0x1DB954 },
    { "Apple Music",   LV_SYMBOL_AUDIO,    0xFA2D55 },
    { "TuneIn",        LV_SYMBOL_AUDIO,    0x1B6AC9 },
};
#define SOURCE_COUNT ((int)(sizeof(sources)/sizeof(sources[0])))

static void browse_src_cb(lv_event_t *e) {
    /* placeholder – will trigger a Sonos browse action */
    (void)e;
}

static void create_screen_browse(void) {
    lv_obj_t *scr = make_screen();
    s_screens[TAB_BROWSE] = scr;

    /* title bar */
    lv_obj_t *title_bar = make_box(scr, 0, 0, SCR_W, 40, C_SURFACE, LV_OPA_COVER, 0);
    make_lbl(title_bar, "Browse", 16, 10, F20, C_TEXT);
    make_sep(scr, 0, 40, SCR_W, 1);

    /* 3 × 2 tile grid
     * gap=6, tile w=(800-4*6)/3=258, tile h=(422-40-3*6)/2=183 */
    int gap  = 6;
    int tw   = (SCR_W - 4 * gap) / 3;           /* 258 */
    int th   = (CONTENT_H - 40 - 3 * gap) / 2;  /* 183 */
    int ty0  = 40 + gap;

    for (int i = 0; i < SOURCE_COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        int tx  = gap + col * (tw + gap);
        int ty  = ty0 + row * (th + gap);

        lv_obj_t *tile = make_btn(scr, tx, ty, tw, th,
                                    C_SURFACE, 12, browse_src_cb,
                                    (void *)(intptr_t)i);

        /* coloured accent strip at top */
        lv_obj_t *strip = make_box(tile, 0, 0, tw, 6,
                                     sources[i].accent, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(strip, 0, 0);
        (void)strip;

        /* icon */
        lv_obj_t *ico = make_lbl(tile, sources[i].icon_sym,
                                   0, 0, F28, sources[i].accent);
        lv_obj_set_style_bg_opa(ico, LV_OPA_TRANSP, 0);
        lv_obj_align(ico, LV_ALIGN_CENTER, 0, -16);

        /* source name */
        lv_obj_t *lbl = make_lbl(tile, sources[i].name,
                                   0, 0, F16, C_TEXT);
        lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -16);
    }
}

/* ══════════════════════ QUEUE screen ════════════════════════════════════ */

typedef struct {
    const char *title;
    const char *artist;
    const char *dur;
    bool        current;
} queue_item_t;

static const queue_item_t demo_queue[QUEUE_COUNT] = {
    { "Angel",        "Massive Attack",  "6:17", false },
    { "Teardrop",     "Massive Attack",  "5:29", false },
    { "Gone Girl",    "Massive Attack",  "3:13", true  },
    { "Risingson",    "Massive Attack",  "4:54", false },
    { "Inertia Creeps","Massive Attack", "5:56", false },
    { "Exchange",     "Massive Attack",  "4:26", false },
    { "Man Next Door","Massive Attack",  "4:42", false },
};

static void create_screen_queue(void) {
    lv_obj_t *scr = make_screen();
    s_screens[TAB_QUEUE] = scr;

    /* title bar */
    lv_obj_t *title_bar = make_box(scr, 0, 0, SCR_W, 40,
                                     C_SURFACE, LV_OPA_COVER, 0);
    make_lbl(title_bar, "Up Next", 16, 10, F20, C_TEXT);
    char count_str[24];
    snprintf(count_str, sizeof(count_str), "%d tracks", QUEUE_COUNT);
    lv_obj_t *lbl_cnt = make_lbl(title_bar, count_str, 0, 10, F14, C_DIM);
    lv_obj_align(lbl_cnt, LV_ALIGN_RIGHT_MID, -16, 0);
    make_sep(scr, 0, 40, SCR_W, 1);

    /* rows: (CONTENT_H - 41) / QUEUE_COUNT ≈ 54 px each */
    int row_h = (CONTENT_H - 41) / QUEUE_COUNT; /* 54 */

    for (int i = 0; i < QUEUE_COUNT; i++) {
        int ry   = 41 + i * row_h;
        uint32_t row_bg = demo_queue[i].current ? 0x1C2A3A : C_BG;

        q_row[i] = make_box(scr, 0, ry, SCR_W, row_h,
                              row_bg, LV_OPA_COVER, 0);

        /* track index */
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        lv_obj_t *lbl_n = make_lbl(q_row[i], num,
                                     16, (row_h - 16) / 2 - 4, F14,
                                     demo_queue[i].current ? C_ACCENT : C_HINT);
        (void)lbl_n;

        /* playing icon on current row */
        if (demo_queue[i].current) {
            lv_obj_t *ico = make_lbl(q_row[i], LV_SYMBOL_AUDIO,
                                       16, (row_h - 16) / 2 - 4, F14, C_ACCENT);
            (void)ico;
        }

        /* title */
        char title_artist[64];
        snprintf(title_artist, sizeof(title_artist),
                 "%s", demo_queue[i].title);
        q_lbl_track[i] = make_lbl(q_row[i], title_artist,
                                    50, (row_h / 2) - 18,
                                    F16,
                                    demo_queue[i].current ? C_TEXT : C_TEXT);
        lv_obj_set_size(q_lbl_track[i], 560, LV_SIZE_CONTENT);
        lv_label_set_long_mode(q_lbl_track[i], LV_LABEL_LONG_DOT);

        /* artist under title */
        lv_obj_t *lbl_art = make_lbl(q_row[i], demo_queue[i].artist,
                                       50, (row_h / 2),
                                       F14, C_DIM);
        lv_obj_set_size(lbl_art, 380, LV_SIZE_CONTENT);
        lv_label_set_long_mode(lbl_art, LV_LABEL_LONG_DOT);

        /* duration (right-aligned) */
        q_lbl_dur[i] = make_lbl(q_row[i], demo_queue[i].dur,
                                  SCR_W - 60, (row_h - 14) / 2 - 4,
                                  F14, C_HINT);

        /* separator (not on last row) */
        if (i < QUEUE_COUNT - 1)
            make_sep(q_row[i], 8, row_h - 1, SCR_W - 16, 1);
    }
}

/* ══════════════════════ public API ══════════════════════════════════════ */

void sonos_ui_init(void) {
    /* Create all screens up-front (fast switching, no lazy jitter) */
    create_screen_rooms();
    create_screen_now_playing();
    create_screen_browse();
    create_screen_queue();

    /* Nav bar sits on the persistent top layer */
    create_nav_bar();

    /* Start on Now Playing */
    lv_screen_load(s_screens[TAB_NOW]);
}

void sonos_ui_tick(void) {
    /* reserved for animation ticks, progress updates, etc. */
}

/* ── now playing setters ─────────────────────────────────────────────── */

void sonos_ui_set_track(const char *title, const char *artist, const char *album) {
    if (np_lbl_track)  lv_label_set_text(np_lbl_track,  title  ? title  : "");
    if (np_lbl_artist) lv_label_set_text(np_lbl_artist, artist ? artist : "");
    if (np_lbl_album)  lv_label_set_text(np_lbl_album,  album  ? album  : "");
}

void sonos_ui_set_progress(int pos_sec, int total_sec) {
    if (!np_bar_prog) return;
    int val = (total_sec > 0) ? (pos_sec * 1000 / total_sec) : 0;
    lv_bar_set_value(np_bar_prog, val, LV_ANIM_OFF);

    if (np_lbl_pos) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d:%02d", pos_sec / 60, pos_sec % 60);
        lv_label_set_text(np_lbl_pos, buf);
    }
    if (np_lbl_total && total_sec > 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d:%02d", total_sec / 60, total_sec % 60);
        lv_label_set_text(np_lbl_total, buf);
    }
}

void sonos_ui_set_playing(bool playing) {
    np_playing = playing;
    if (np_lbl_playpause)
        lv_label_set_text(np_lbl_playpause,
                           playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    if (np_dot_playing)
        lv_obj_set_style_bg_color(np_dot_playing,
                                   lv_color_hex(playing ? C_PLAYING : C_HINT), 0);
}

void sonos_ui_set_volume(int vol_pct) {
    if (np_slider_vol)
        lv_slider_set_value(np_slider_vol, vol_pct, LV_ANIM_OFF);
}

void sonos_ui_set_room(const char *room_name) {
    if (np_lbl_room && room_name)
        lv_label_set_text(np_lbl_room, room_name);
}

/* ── room setters ────────────────────────────────────────────────────── */

void sonos_ui_set_room_state(int idx, const char *name,
                               const char *now_playing,
                               bool playing, int vol_pct) {
    if (idx < 0 || idx >= ROOM_COUNT) return;
    rm_playing[idx] = playing;
    if (rm_lbl_name[idx]    && name)        lv_label_set_text(rm_lbl_name[idx], name);
    if (rm_lbl_playing[idx] && now_playing) lv_label_set_text(rm_lbl_playing[idx], now_playing);
    if (rm_dot[idx])
        lv_obj_set_style_bg_color(rm_dot[idx],
                                   lv_color_hex(playing ? C_PLAYING : C_HINT), 0);
    if (rm_lbl_pp[idx])
        lv_label_set_text(rm_lbl_pp[idx],
                           playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    if (rm_slider_vol[idx])
        lv_slider_set_value(rm_slider_vol[idx], vol_pct, LV_ANIM_OFF);
}

/* ── queue setters ───────────────────────────────────────────────────── */

void sonos_ui_set_queue_item(int idx, const char *title,
                               const char *artist,
                               const char *duration,
                               bool is_current) {
    if (idx < 0 || idx >= QUEUE_COUNT) return;
    if (q_lbl_track[idx] && title)
        lv_label_set_text(q_lbl_track[idx], title);
    if (q_lbl_dur[idx] && duration)
        lv_label_set_text(q_lbl_dur[idx], duration);
    if (q_row[idx]) {
        lv_obj_set_style_bg_color(q_row[idx],
                                   lv_color_hex(is_current ? 0x1C2A3A : C_BG), 0);
    }
    (void)artist;   /* artist label not stored as ptr – kept static */
    (void)is_current;
}
