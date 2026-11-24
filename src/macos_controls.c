#include "macos_controls.h"
#include <stdlib.h>
#include <math.h>
static uint64_t button(const char *key,uint64_t fallback){
 const char *text=getenv(key);if(!text)return fallback;
 char *end;unsigned long long value=strtoull(text,&end,10);
 if(!*text||*end||!value||(value&(value-1))||value>0x200000)return fallback;
 return value;
}
uint64_t kfx_menu_confirm_button(void){return button("KFX_MENU_CONFIRM_BUTTON",1);}
uint64_t kfx_menu_back_button(void){return button("KFX_MENU_BACK_BUTTON",2);}
float kfx_controller_cursor_speed(void){
 const char *text=getenv("KFX_CONTROLLER_CURSOR_SPEED");if(!text)return 1;
 char *end;float value=strtof(text,&end);return *text&&!*end&&isfinite(value)&&value>=0.25f&&value<=3?value:1;
}

int kfx_controller_guide_visible;
int kfx_controller_guide_ps5;
static float setting(const char *key,float fallback,float low,float high){
 const char *text=getenv(key);if(!text)return fallback;
 char *end;float value=strtof(text,&end);
 return *text&&!*end&&isfinite(value)&&value>=low&&value<=high?value:fallback;
}
float kfx_controller_deadzone(void){return setting("KFX_CONTROLLER_DEADZONE",8000.0f/32767,0.05f,0.5f);}
float kfx_controller_rumble_strength(void){return setting("KFX_CONTROLLER_RUMBLE",1,0,1);}

int kfx_window_cursor_lock;
int kfx_allow_cursor_grab(int requested,int fullscreen,int panel_open){return requested && !panel_open && (fullscreen || kfx_window_cursor_lock);}

float kfx_cursor_scale=1;
float kfx_text_scale=1;
int kfx_edge_scroll=1;
int kfx_edge_border=4;
int kfx_mentor_captions;
int kfx_caption_seconds=5;

float kfx_camera_pan_scale=1.0f;

float kfx_caption_scale=1,kfx_tooltip_scale=1;
int kfx_reduce_flashes=0,kfx_high_contrast=0;
float kfx_camera_response(void){return setting("KFX_CAMERA_RESPONSE",2,1,3);}
