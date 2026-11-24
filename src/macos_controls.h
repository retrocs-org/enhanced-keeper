#ifndef KFX_MACOS_CONTROLS_H
#define KFX_MACOS_CONTROLS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t kfx_menu_confirm_button(void);
uint64_t kfx_menu_back_button(void);
float kfx_controller_cursor_speed(void);
float kfx_controller_deadzone(void);
float kfx_controller_rumble_strength(void);
extern float kfx_cursor_scale;
extern int kfx_edge_scroll;
extern int kfx_edge_border;
extern int kfx_mentor_captions;
extern int kfx_caption_seconds;
extern float kfx_text_scale;
extern int kfx_window_cursor_lock;
extern float kfx_camera_pan_scale;
extern float kfx_caption_scale,kfx_tooltip_scale;
extern int kfx_reduce_flashes,kfx_high_contrast;
float kfx_camera_response(void);
int kfx_allow_cursor_grab(int requested,int fullscreen,int panel_open);
extern int kfx_controller_guide_visible;
extern int kfx_controller_guide_ps5;
#ifdef __cplusplus
}
#endif
#endif
