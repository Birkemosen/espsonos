#ifndef SONOS_UI_H
#define SONOS_UI_H

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- lifecycle ---------- */
void sonos_ui_init(void);
void sonos_ui_tick(void);

/* ---------- now playing updates ---------- */
void sonos_ui_set_track(const char *title, const char *artist, const char *album);
void sonos_ui_set_progress(int pos_sec, int total_sec);  /* total_sec=0 → hide */
void sonos_ui_set_playing(bool playing);
void sonos_ui_set_volume(int vol_pct);                   /* 0-100 */
void sonos_ui_set_room(const char *room_name);

/* ---------- room updates (index 0-3) ---------- */
void sonos_ui_set_room_state(int idx, const char *name,
                              const char *now_playing,
                              bool playing, int vol_pct);

/* ---------- queue updates (index 0-6) ---------- */
void sonos_ui_set_queue_item(int idx, const char *title,
                              const char *artist,
                              const char *duration,
                              bool is_current);

#ifdef __cplusplus
}
#endif

#endif /* SONOS_UI_H */
