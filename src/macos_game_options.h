#ifndef KFX_MACOS_GAME_OPTIONS_H
#define KFX_MACOS_GAME_OPTIONS_H
#ifdef __cplusplus
extern "C" {
#endif
struct GuiButton;
int kfx_options_select_replay(const char *file);
int kfx_options_community_action(int action);
const char *kfx_options_community_status(void);
void kfx_options_initialize(void);
void kfx_save_display_label(long slot,const char *fallback,char *buffer,unsigned int size);
void kfx_options_hide(void);
int kfx_quick_game_key(long key);
void kfx_options_apply_display(void);
void kfx_options_recover_display(void);
int kfx_options_confirm_display(int keep);
int kfx_options_set_display(int fullscreen,int width,int height);
int kfx_options_startup_pending(void);
const char *kfx_options_profile_name(void);
void kfx_options_show_page(int page);
void kfx_options_preview(int page);
void kfx_options_open(struct GuiButton *button);
void kfx_options_button(struct GuiButton *button);
void kfx_options_draw(void);
void kfx_caption_speech(int sample);
void kfx_save_feedback(int success,int automatic,const char *reason);
void kfx_options_display(struct GuiButton *button);
#ifdef __cplusplus
}
union SDL_Event;
bool kfx_options_event(const SDL_Event *event);
#endif
#endif
