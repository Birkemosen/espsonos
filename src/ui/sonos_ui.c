/*
 * sonos_ui.c  —  Multiroom Music Player UI
 *
 * Eversolo-inspired layout for 800×480 touch LCD.
 *
 * Layout (800×480):
 *   ┌──────────────────────────────┬────────┐  y=0
 *   │  top tab bar  (740 × 52)     │        │
 *   ├──────────────────────────────┤ 60 px  │
 *   │  content panels (740 × 428)  │ right  │
 *   │                              │ strip  │
 *   └──────────────────────────────┴────────┘  y=480
 *
 * App overlays (740 × 480) appear over the full content zone
 * (covering the top bar) when launched; each has a × button.
 * The right strip is created last → always on top.
 *
 * Tabs : [0] Home  [1] Music  [2] Rooms  [3] Settings
 * Apps : [0] Now Playing overlay  [1] Multi-Room overlay
 */

#include "sonos_ui.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* ── geometry ────────────────────────────────────────────────────── */
#define SCR_W      800
#define SCR_H      480
#define STRIP_X    740   /* right strip x-origin  */
#define STRIP_W     60   /* right strip width      */
#define CONTENT_W  740   /* content zone width     */
#define TOPBAR_H    52   /* top tab-bar height     */
#define CONTENT_Y   52   /* content starts here    */
#define CONTENT_H  (SCR_H - TOPBAR_H)   /* 428 */

/* ── palette ─────────────────────────────────────────────────────── */
#define C_BG      0x202428   /* RGB(32,36,40) for all backgrounds */
#define C_SURFACE 0x202428   /* RGB(32,36,40) for cards/tiles */
#define C_CARD    0x202428   /* RGB(32,36,40) for elevated */
#define C_CARD_HI 0x202428   /* RGB(32,36,40) for highlight/slider track */
#define C_TOPBAR  0x202428   /* RGB(32,36,40) for top bar */
#define C_STRIP   0x202428   /* RGB(32,36,40) for right strip */
#define C_SEP     0x202428   /* RGB(32,36,40) for separator/borders */
#define C_ACCENT  0xAC8B2D   /* accent: rgb(172,139,45) */
#define C_PLAYING 0xAC8B2D   /* accent: rgb(172,139,45) */
#define C_TEXT    0xFFFFFF   /* white text */
#define C_DIM     0xB0B0B0   /* soft gray for secondary text */
#define C_HINT    0x444444   /* hint/inactive */

/* ── fonts ───────────────────────────────────────────────────────── */
#define F14 (&lv_font_montserrat_14)
#define F16 (&lv_font_montserrat_16)
#define F20 (&lv_font_montserrat_20)
#define F24 (&lv_font_montserrat_24)
#define F28 (&lv_font_montserrat_28)

/* ── tabs ────────────────────────────────────────────────────────── */
#define TAB_HOME     0
#define TAB_MUSIC    1
#define TAB_ROOMS    2
#define TAB_SETTINGS 3
#define TAB_COUNT    4

static const char *tab_names[TAB_COUNT] = {"Home", "Music", "Rooms", "Settings"};

/* ── app overlays ────────────────────────────────────────────────── */
#define APP_PLAYER   0
#define APP_ROOMS_OV 1
#define APP_COUNT    2

/* ── state ───────────────────────────────────────────────────────── */
static int        s_tab = TAB_HOME;
static lv_obj_t  *s_root;
static lv_obj_t  *s_panels[TAB_COUNT];
static lv_obj_t  *s_tab_btn[TAB_COUNT];
static lv_obj_t  *s_tab_lbl[TAB_COUNT];
static lv_obj_t  *s_apps[APP_COUNT];

static lv_obj_t  *strip_pp_ico;
static bool       strip_playing = true;

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

static void close_overlay(void);   /* forward decl */

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

static uint32_t adjust_rgb(uint32_t c, int delta) {
    int r = ((c >> 16) & 0xFF) + delta;
    int g = ((c >> 8) & 0xFF) + delta;
    int b = (c & 0xFF) + delta;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
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

static lv_obj_t *make_vol_slider(lv_obj_t *p, int x, int y, int w, int val) {
    lv_obj_t *sl = lv_slider_create(p);
    lv_obj_set_pos(sl, x, y);
    lv_obj_set_size(sl, w, 8);
    lv_slider_set_value(sl, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
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

/* [vol-icon] [===slider===] [−] [+]  returns the slider */
static lv_obj_t *make_vol_row(lv_obj_t *p, int x, int y, int tw, int val) {
    const int btn_sz = 40, icon_w = 26, gap = 12;
    int sl_w = tw - icon_w - gap - btn_sz - gap - btn_sz - gap;
    if (sl_w < 40) sl_w = 40;

    make_lbl(p, LV_SYMBOL_VOLUME_MID, x, y + 16, F16, C_TEXT);
    lv_obj_t *sl = make_vol_slider(p, x + icon_w + gap, y + 20, sl_w, val);

    int bx = x + icon_w + gap + sl_w + gap;
    lv_obj_t *bm = make_btn(p, bx,           y + 4, btn_sz, btn_sz,
                            C_CARD_HI, LV_RADIUS_CIRCLE, vol_minus_cb, sl);
    lv_obj_t *lm = make_lbl(bm, "-", 0, 0, F20, C_TEXT);
    lv_obj_align(lm, LV_ALIGN_CENTER, 0, -1);

    lv_obj_t *bp = make_btn(p, bx + btn_sz + gap, y + 4, btn_sz, btn_sz,
                            C_CARD_HI, LV_RADIUS_CIRCLE, vol_plus_cb, sl);
    lv_obj_t *lp = make_lbl(bp, "+", 0, 0, F20, C_TEXT);
    lv_obj_align(lp, LV_ALIGN_CENTER, 0, -1);

    return sl;
}

/* ================================================================= */
/* App overlay management                                            */
/* ================================================================= */

static void close_overlay(void) {
    for (int i = 0; i < APP_COUNT; i++)
        if (s_apps[i]) lv_obj_add_flag(s_apps[i], LV_OBJ_FLAG_HIDDEN);
}

static void close_overlay_cb(lv_event_t *e) {
    (void)e;
    close_overlay();
}

static void open_app(int idx) {
    if (idx < 0 || idx >= APP_COUNT) return;
    close_overlay();
    if (s_apps[idx]) lv_obj_remove_flag(s_apps[idx], LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================= */
/* Top tab bar                                                       */
/* ================================================================= */

static void navigate_to(int tab) {
    if (tab < 0 || tab >= TAB_COUNT) return;
    if (s_panels[s_tab]) lv_obj_add_flag(s_panels[s_tab], LV_OBJ_FLAG_HIDDEN);
    s_tab = tab;
    if (s_panels[tab]) lv_obj_remove_flag(s_panels[tab], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < TAB_COUNT; i++) {
        bool act = (i == tab);
        if (s_tab_btn[i])
            lv_obj_set_style_bg_color(s_tab_btn[i],
                                      lv_color_hex(act ? C_ACCENT : C_CARD), 0);
        if (s_tab_lbl[i])
            lv_obj_set_style_text_color(s_tab_lbl[i],
                                        lv_color_hex(act ? C_TEXT : C_DIM), 0);
    }
}

static void tab_btn_cb(lv_event_t *e) {
    close_overlay();
    navigate_to((int)(intptr_t)lv_event_get_user_data(e));
}

static void create_top_bar(lv_obj_t *root) {
    lv_obj_t *bar = make_box(root, 0, 0, CONTENT_W, TOPBAR_H,
                             C_TOPBAR, LV_OPA_COVER, 0);
    make_sep(bar, 0, TOPBAR_H - 1, CONTENT_W);

    /* pill-shaped tab buttons, centred in the bar */
    const int tab_w = 158, tab_h = 34, tab_gap = 8;
    int total = TAB_COUNT * tab_w + (TAB_COUNT - 1) * tab_gap;
    int sx    = (CONTENT_W - total) / 2;

    for (int i = 0; i < TAB_COUNT; i++) {
        bool act = (i == s_tab);
        int tx = sx + i * (tab_w + tab_gap);
        s_tab_btn[i] = make_btn(bar, tx, 9, tab_w, tab_h,
                    act ? C_ACCENT : C_CARD, 17,
                    tab_btn_cb, (void *)(intptr_t)i);
        /* Sonos: active tab = orange bg, white text; inactive = dark card, gray text */
        s_tab_lbl[i] = make_lbl(s_tab_btn[i], tab_names[i], 0, 0,
                    F16, act ? C_TEXT : C_DIM);
        lv_obj_align(s_tab_lbl[i], LV_ALIGN_CENTER, 0, 0);
    }
}

/* ================================================================= */
/* Right music-control strip (60 px wide, full height, always last) */
/* ================================================================= */

static void strip_pp_cb(lv_event_t *e) {
    (void)e;
    strip_playing = !strip_playing;
    np_playing    = strip_playing;
    const char *sym = strip_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    if (strip_pp_ico) lv_label_set_text(strip_pp_ico, sym);
    if (np_lbl_pp)    lv_label_set_text(np_lbl_pp,    sym);
}

static void strip_home_cb(lv_event_t *e) {
    (void)e;
    close_overlay();
    navigate_to(TAB_HOME);
}

static void strip_back_cb(lv_event_t *e) {
    (void)e;
    for (int i = 0; i < APP_COUNT; i++) {
        if (s_apps[i] && !lv_obj_has_flag(s_apps[i], LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(s_apps[i], LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }
    navigate_to(TAB_HOME);
}

static void strip_next_cb(lv_event_t *e) { (void)e; }
static void strip_prev_cb(lv_event_t *e) { (void)e; }

static void create_right_strip(lv_obj_t *root) {
    lv_obj_t *strip = make_box(root, STRIP_X, 0, STRIP_W, SCR_H,
                               C_STRIP, LV_OPA_COVER, 0);

    /* left border */
    lv_obj_t *brd = lv_obj_create(strip);
    lv_obj_set_pos(brd, 0, 0);
    lv_obj_set_size(brd, 1, SCR_H);
    lv_obj_set_style_bg_color(brd, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_bg_opa(brd, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(brd, 0, 0);
    lv_obj_set_style_pad_all(brd, 0, 0);

    /* album art circle */
    lv_obj_t *art = make_box(strip, 8, 18, 44, 44,
                             C_SURFACE, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_t *aic = lv_label_create(art);
    lv_label_set_text(aic, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(aic, F14, 0);
    lv_obj_set_style_text_color(aic, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_bg_opa(aic, LV_OPA_TRANSP, 0);
    lv_obj_align(aic, LV_ALIGN_CENTER, 0, 0);

    /* 5 control buttons from y=80, each 80 px tall */
    struct { const char *icon; lv_event_cb_t cb; bool is_pp; } ctrls[] = {
        { LV_SYMBOL_PAUSE, strip_pp_cb,   true  },
        { LV_SYMBOL_NEXT,  strip_next_cb, false },
        { LV_SYMBOL_PREV,  strip_prev_cb, false },
        { LV_SYMBOL_HOME,  strip_home_cb, false },
        { LV_SYMBOL_LEFT,  strip_back_cb, false },
    };
    const int n = 5, start_y = 80;
    const int btn_h = (SCR_H - start_y) / n;   /* 80 */

    for (int i = 0; i < n; i++) {
        int by = start_y + i * btn_h;
        lv_obj_t *b = make_btn(strip, 0, by, STRIP_W, btn_h,
                               C_STRIP, 0, ctrls[i].cb, NULL);
        lv_obj_t *ico = make_lbl(b, ctrls[i].icon, 0, 0, F20, C_TEXT);
        lv_obj_align(ico, LV_ALIGN_CENTER, 0, 0);
        if (ctrls[i].is_pp) strip_pp_ico = ico;
        if (i < n - 1) make_sep(strip, 8, by + btn_h, STRIP_W - 16);
    }
}

/* ================================================================= */
/* Panel: HOME — 6 app tiles in a 3 × 2 grid                       */
/* ================================================================= */

static void tile_cb(lv_event_t *e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    switch (action) {
        case 0: open_app(APP_PLAYER);        break;
        case 1: open_app(APP_ROOMS_OV);      break;
        case 2: navigate_to(TAB_MUSIC);      break;
        case 3: navigate_to(TAB_SETTINGS);   break;
        default: break;
    }
}

static void create_panel_home(lv_obj_t *panel) {
    static const struct {
        const char *name, *icon;
        int action;
        uint32_t color;
    } tiles[] = {
        { "Now Playing", LV_SYMBOL_AUDIO,    0,  0xB88A0A }, // amber
        { "Rooms",       LV_SYMBOL_LIST,     1,  0x3A7CA5 }, // blue
        { "Music",       LV_SYMBOL_WIFI,     2,  0x7C3AA5 }, // purple
        { "Library",     LV_SYMBOL_DRIVE,   -1,  0xA53A3A }, // red
        { "Queue",       LV_SYMBOL_BELL,    -1,  0x3AA55C }, // green
        { "Settings",    LV_SYMBOL_SETTINGS, 3,  0x888888 }, // gray
    };

    // 3 cols × 2 rows, 16 px margin, 12 px gap
    const int pad = 16, gap = 12;
    const int tw  = (CONTENT_W - 2 * pad - 2 * gap) / 3;   // 220
    const int th  = (CONTENT_H - 2 * pad - gap) / 2;       // 196
    const int ibox = (int)(92 * 1.3); // 30% bigger
    const int new_gap = 4; // less spacing between tiles

    for (int i = 0; i < 6; i++) {
        int col = i % 3, row = i / 3;
        int tx = pad + col * (tw + new_gap);
        int ty = CONTENT_Y + pad + row * (th + new_gap);

        lv_obj_t *tile = make_box(panel, tx, ty, tw, th, C_BG, LV_OPA_COVER, 16);
        if (tiles[i].action >= 0) {
            lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(tile, tile_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)tiles[i].action);
        }

        // colored icon box (rounded square)
        uint32_t icon_top = adjust_rgb(tiles[i].color, 28);
        uint32_t icon_bot = adjust_rgb(tiles[i].color, -28);
        lv_obj_t *ibox_bg = make_box(tile, (tw-ibox)/2, 10, ibox, ibox, icon_top, LV_OPA_COVER, 24);
        lv_obj_set_style_bg_grad_color(ibox_bg, lv_color_hex(icon_bot), 0);
        lv_obj_set_style_bg_grad_dir(ibox_bg, LV_GRAD_DIR_VER, 0);
        if (tiles[i].action >= 0) {
            lv_obj_add_flag(ibox_bg, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(ibox_bg, tile_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)tiles[i].action);
        }
        // icon centered in box
        lv_obj_t *ico = lv_label_create(ibox_bg);
        lv_label_set_text(ico, tiles[i].icon);
        lv_obj_set_style_text_font(ico, F28, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(ico, LV_OPA_TRANSP, 0);
        lv_obj_align(ico, LV_ALIGN_CENTER, 0, 0);
        if (tiles[i].action >= 0) {
            lv_obj_add_flag(ico, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(ico, tile_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)tiles[i].action);
        }

        // app name centered below icon box
        lv_obj_t *lbl = lv_label_create(tile);
        lv_label_set_text(lbl, tiles[i].name);
        lv_obj_set_style_text_font(lbl, F16, 0);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(tiles[i].action >= 0 ? C_TEXT : C_DIM), 0);
        lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -18);
        if (tiles[i].action >= 0) {
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(lbl, tile_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)tiles[i].action);
        }
    }
}

/* ================================================================= */
/* Panel: MUSIC — service tiles                                     */
/* ================================================================= */

static void create_panel_music(lv_obj_t *panel) {
    make_lbl(panel, "Music Services", 20, CONTENT_Y + 16, F20, C_TEXT);
    make_sep(panel, 0, CONTENT_Y + 50, CONTENT_W);

    static const char *svc[]  = { "Spotify", "Radio", "Library", "Podcasts", "TuneIn", "Tidal" };
    static const char *ico[]  = {
        LV_SYMBOL_AUDIO, LV_SYMBOL_WIFI,   LV_SYMBOL_DRIVE,
        LV_SYMBOL_BELL,  LV_SYMBOL_REFRESH, LV_SYMBOL_LOOP,
    };

    const int pad = 16, gap = 12;
    const int tw = (CONTENT_W - 2 * pad - 2 * gap) / 3;
    const int th = 130;

    for (int i = 0; i < 6; i++) {
        int col = i % 3, row = i / 3;
        int tx = pad + col * (tw + gap);
        int ty = CONTENT_Y + 64 + row * (th + gap);

        lv_obj_t *tile = make_box(panel, tx, ty, tw, th, C_BG, LV_OPA_COVER, 12);

        lv_obj_t *ic = lv_label_create(tile);
        lv_label_set_text(ic, ico[i]);
        lv_obj_set_style_text_font(ic, F24, 0);
        lv_obj_set_style_text_color(ic, lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_bg_opa(ic, LV_OPA_TRANSP, 0);
        lv_obj_align(ic, LV_ALIGN_CENTER, 0, -16);

        lv_obj_t *lb = lv_label_create(tile);
        lv_label_set_text(lb, svc[i]);
        lv_obj_set_style_text_font(lb, F14, 0);
        lv_obj_set_style_text_color(lb, lv_color_hex(C_TEXT), 0);
        lv_obj_set_style_bg_opa(lb, LV_OPA_TRANSP, 0);
        lv_obj_align(lb, LV_ALIGN_BOTTOM_MID, 0, -10);
    }
}

/* ================================================================= */
/* Panel: ROOMS — room list                                         */
/* ================================================================= */

static void create_panel_rooms(lv_obj_t *panel) {
    make_lbl(panel, "Rooms", 20, CONTENT_Y + 16, F20, C_TEXT);
    make_sep(panel, 0, CONTENT_Y + 50, CONTENT_W);

    static const char *rname[]  = { "Living Room", "Bathroom", "Kitchen", "Bedroom" };
    static const char *rtrack[] = {
        "Forever Chemicals – Placebo",
        "Karma Police – Radiohead",
        "Not Playing", "Not Playing",
    };
    static const bool rplay[] = { true, true, false, false };

    for (int i = 0; i < 4; i++) {
        int ry = CONTENT_Y + 64 + i * 74;
        lv_obj_t *row = make_box(panel, 16, ry, CONTENT_W - 32, 62,
                                 C_SURFACE, LV_OPA_COVER, 10);

        make_box(row, 14, 25, 12, 12,
             rplay[i] ? C_PLAYING : C_HINT, LV_OPA_COVER, LV_RADIUS_CIRCLE);
        make_lbl(row, rname[i],  34, 8,  F16, C_TEXT);
        make_lbl(row, rtrack[i], 34, 30, F14, C_DIM);

        lv_obj_t *chev = lv_label_create(row);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(chev, F16, 0);
        lv_obj_set_style_text_color(chev, lv_color_hex(C_HINT), 0);
        lv_obj_set_style_bg_opa(chev, LV_OPA_TRANSP, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -14, 0);
    }
}

/* ================================================================= */
/* Panel: SETTINGS                                                  */
/* ================================================================= */

static void create_panel_settings(lv_obj_t *panel) {
    make_lbl(panel, "Settings", 20, CONTENT_Y + 16, F24, C_TEXT);
    make_sep(panel, 0, CONTENT_Y + 56, CONTENT_W);

    static const struct { const char *icon, *title, *val; } rows[] = {
        { LV_SYMBOL_WIFI,       "Network",   "Connected" },
        { LV_SYMBOL_BLUETOOTH,  "Bluetooth", "On"        },
        { LV_SYMBOL_VOLUME_MID, "Audio",     "Settings"  },
        { LV_SYMBOL_EYE_OPEN,   "Display",   "Auto"      },
        { LV_SYMBOL_WARNING,    "About",     "v1.0.0"    },
    };

    for (int i = 0; i < 5; i++) {
        int ry = CONTENT_Y + 68 + i * 72;
        lv_obj_t *row = make_box(panel, 16, ry, CONTENT_W - 32, 60,
                                 C_SURFACE, LV_OPA_COVER, 10);

        lv_obj_t *ic = lv_label_create(row);
        lv_label_set_text(ic, rows[i].icon);
        lv_obj_set_style_text_font(ic, F20, 0);
        lv_obj_set_style_text_color(ic, lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_bg_opa(ic, LV_OPA_TRANSP, 0);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 18, 0);

        make_lbl(row, rows[i].title, 62, 20, F16, C_TEXT);

        lv_obj_t *vl = lv_label_create(row);
        lv_label_set_text(vl, rows[i].val);
        lv_obj_set_style_text_font(vl, F14, 0);
        lv_obj_set_style_text_color(vl, lv_color_hex(C_DIM), 0);
        lv_obj_set_style_bg_opa(vl, LV_OPA_TRANSP, 0);
        lv_obj_align(vl, LV_ALIGN_RIGHT_MID, -48, 0);

        lv_obj_t *ch = lv_label_create(row);
        lv_label_set_text(ch, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(ch, F16, 0);
        lv_obj_set_style_text_color(ch, lv_color_hex(C_HINT), 0);
        lv_obj_set_style_bg_opa(ch, LV_OPA_TRANSP, 0);
        lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -16, 0);
    }
}

/* ================================================================= */
/* App overlay: NOW PLAYING (full-screen music player)              */
/* ================================================================= */

static void np_pp_cb(lv_event_t *e) {
    (void)e;
    np_playing = strip_playing = !np_playing;
    const char *sym = np_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    if (np_lbl_pp)    lv_label_set_text(np_lbl_pp,    sym);
    if (strip_pp_ico) lv_label_set_text(strip_pp_ico, sym);
}

static void create_app_player(lv_obj_t *ov) {
    /* header */
    make_lbl(ov, "Now Playing", 58, 14, F20, C_TEXT);
    lv_obj_t *xb = make_btn(ov, 8, 8, 40, 40, C_CARD_HI, 20,
                            close_overlay_cb, NULL);
    lv_obj_t *xl = make_lbl(xb, LV_SYMBOL_CLOSE, 0, 0, F16, C_TEXT);
    lv_obj_align(xl, LV_ALIGN_CENTER, 0, 0);
    make_sep(ov, 0, 56, CONTENT_W);

    /* album art — left column */
    const int art_sz = 230, art_x = 20;
    const int art_y  = 56 + (SCR_H - 56 - art_sz) / 2;   /* ~117 */
    lv_obj_t *art = make_box(ov, art_x, art_y, art_sz, art_sz,
                             C_SURFACE, LV_OPA_COVER, 14);
    lv_obj_t *aic = lv_label_create(art);
    lv_label_set_text(aic, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(aic, F28, 0);
    lv_obj_set_style_text_color(aic, lv_color_hex(C_HINT), 0);
    lv_obj_set_style_bg_opa(aic, LV_OPA_TRANSP, 0);
    lv_obj_align(aic, LV_ALIGN_CENTER, 0, 0);

    /* right column */
    const int rx = art_x + art_sz + 20;   /* ~270 */
    const int rw = CONTENT_W - rx - 12;   /* ~458 */
    const int rb = art_y;                 /* top of right col aligns with art */

    np_lbl_room   = make_lbl(ov, LV_SYMBOL_LIST " Living Room",
                             rx, rb,      F14, C_DIM);
    np_lbl_track  = make_lbl(ov, "Forever Chemicals", rx, rb + 22,  F24, C_TEXT);
    np_lbl_artist = make_lbl(ov, "Placebo",           rx, rb + 58,  F20, C_DIM);
    np_lbl_album  = make_lbl(ov, "Never Let Me Go",   rx, rb + 84,  F16, C_HINT);

    /* progress bar */
    np_bar_prog = lv_bar_create(ov);
    lv_obj_set_pos(np_bar_prog, rx, rb + 118);
    lv_obj_set_size(np_bar_prog, rw, 8);
    lv_bar_set_range(np_bar_prog, 0, 1000);
    lv_bar_set_value(np_bar_prog, 350, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(np_bar_prog, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(np_bar_prog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(np_bar_prog, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_bar_prog, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(np_bar_prog, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_bar_prog, 4, LV_PART_INDICATOR);

    np_lbl_pos   = make_lbl(ov, "1:52", rx,          rb + 130, F14, C_DIM);
    np_lbl_total = make_lbl(ov, "3:28", rx + rw - 40, rb + 130, F14, C_DIM);

    /* transport */
    const int ts_total = 44 + 16 + 56 + 16 + 44;  /* 176 */
    int ts_x = rx + (rw - ts_total) / 2;
    int ts_y = rb + 160;

    lv_obj_t *bprev = make_btn(ov, ts_x,            ts_y + 6, 44, 44,
                               C_SURFACE, LV_RADIUS_CIRCLE, NULL, NULL);
    lv_obj_t *lpr = make_lbl(bprev, LV_SYMBOL_PREV, 0, 0, F20, C_TEXT);
    lv_obj_align(lpr, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *bplay = make_btn(ov, ts_x + 44 + 16, ts_y, 56, 56,
                               C_ACCENT, LV_RADIUS_CIRCLE, np_pp_cb, NULL);
    np_lbl_pp = make_lbl(bplay, LV_SYMBOL_PAUSE, 0, 0, F20, C_TEXT);
    lv_obj_set_style_bg_opa(np_lbl_pp, LV_OPA_TRANSP, 0);
    lv_obj_align(np_lbl_pp, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *bnext = make_btn(ov, ts_x + 44 + 16 + 56 + 16, ts_y + 6, 44, 44,
                               C_SURFACE, LV_RADIUS_CIRCLE, NULL, NULL);
    lv_obj_t *lnx = make_lbl(bnext, LV_SYMBOL_NEXT, 0, 0, F20, C_TEXT);
    lv_obj_align(lnx, LV_ALIGN_CENTER, 0, 0);

    /* volume */
    np_slider_vol = make_vol_row(ov, rx, ts_y + 76, rw, 70);
}

/* ================================================================= */
/* App overlay: MULTI-ROOM                                          */
/* ================================================================= */

static void pl_toggle_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= PLAYER_COUNT) return;
    pl_playing[idx] = !pl_playing[idx];
    if (pl_dot[idx])
        lv_obj_set_style_bg_color(pl_dot[idx],
                                  lv_color_hex(pl_playing[idx] ? C_PLAYING : C_HINT), 0);
}

static void make_player_card(lv_obj_t *parent, int cy, bool bordered,
                             const char *name, const char *model,
                             int idx, int vol) {
    const int cx = 14, cw = CONTENT_W - 28, ch = 158;

    lv_obj_t *card = make_box(parent, cx, cy, cw, ch, C_SURFACE, LV_OPA_COVER, 12);
    if (bordered) {
        lv_obj_set_style_border_color(card, lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_border_width(card, 2, 0);
    }

    pl_dot[idx] = make_box(card, cw - 22, 12, 12, 12,
                           C_PLAYING, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_add_event_cb(pl_dot[idx], pl_toggle_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)idx);

    lv_obj_t *art = make_box(card, 14, 14, 52, 52, C_SURFACE, LV_OPA_COVER, 8);
    lv_obj_t *aic = lv_label_create(art);
    lv_label_set_text(aic, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(aic, F14, 0);
    lv_obj_set_style_text_color(aic, lv_color_hex(C_HINT), 0);
    lv_obj_set_style_bg_opa(aic, LV_OPA_TRANSP, 0);
    lv_obj_align(aic, LV_ALIGN_CENTER, 0, 0);

    make_lbl(card, name,  74, 14, F16, C_TEXT);
    make_lbl(card, model, 74, 36, F14, C_DIM);

    pl_slider[idx] = make_vol_row(card, 14, ch - 52, cw - 28, vol);
}

static void create_app_rooms(lv_obj_t *ov) {
    make_lbl(ov, "Multi Room", 58, 14, F20, C_TEXT);
    lv_obj_t *xb = make_btn(ov, 8, 8, 40, 40, C_CARD_HI, 20,
                            close_overlay_cb, NULL);
    lv_obj_t *xl = make_lbl(xb, LV_SYMBOL_CLOSE, 0, 0, F16, C_TEXT);
    lv_obj_align(xl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *ri = lv_label_create(ov);
    lv_label_set_text(ri, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(ri, F16, 0);
    lv_obj_set_style_text_color(ri, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_bg_opa(ri, LV_OPA_TRANSP, 0);
    lv_obj_align(ri, LV_ALIGN_TOP_RIGHT, -20, 16);

    make_sep(ov, 0, 56, CONTENT_W);

    make_player_card(ov, 68,             false, "Bathroom",    "PULSE FLEX 2i", 0, 60);
    make_player_card(ov, 68 + 158 + 12,  true,  "Living Room", "SOUNDBAR+",     1, 78);
}

/* ================================================================= */
/* Init                                                              */
/* ================================================================= */

void sonos_ui_init(void) {
    /* root screen */
    s_root = lv_obj_create(NULL);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    no_scroll(s_root);

    /* 1. content panels (740 × 480, content starts at y=CONTENT_Y) */
    for (int i = 0; i < TAB_COUNT; i++) {
        s_panels[i] = make_box(s_root, 0, 0, CONTENT_W, SCR_H,
                               C_BG, LV_OPA_COVER, 0);
        if (i != s_tab) lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    }
    create_panel_home    (s_panels[TAB_HOME]);
    create_panel_music   (s_panels[TAB_MUSIC]);
    create_panel_rooms   (s_panels[TAB_ROOMS]);
    create_panel_settings(s_panels[TAB_SETTINGS]);

    /* 2. top tab bar (renders above panels, below overlays) */
    create_top_bar(s_root);

    /* 3. app overlays (render above top bar when shown) */
    for (int i = 0; i < APP_COUNT; i++) {
        s_apps[i] = make_box(s_root, 0, 0, CONTENT_W, SCR_H,
                             C_BG, LV_OPA_COVER, 0);
        lv_obj_add_flag(s_apps[i], LV_OBJ_FLAG_HIDDEN);
    }
    create_app_player(s_apps[APP_PLAYER]);
    create_app_rooms (s_apps[APP_ROOMS_OV]);

    /* 4. right strip — created last → always on top */
    create_right_strip(s_root);

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
    np_playing = strip_playing = playing;
    const char *sym = playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    if (np_lbl_pp)    lv_label_set_text(np_lbl_pp,    sym);
    if (strip_pp_ico) lv_label_set_text(strip_pp_ico, sym);
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
