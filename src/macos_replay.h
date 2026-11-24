#ifndef KFX_MACOS_REPLAY_H
#define KFX_MACOS_REPLAY_H
#ifdef __cplusplus
extern "C" {
#endif
int kfx_replay_validate_file(const char *file,char *report,unsigned int size);
void kfx_replay_request_explicit(void);
void kfx_replay_begin(const char *file);
int kfx_replay_available(void);
int kfx_replay_paused(void);
int kfx_replay_should_hold(void);
int kfx_replay_control(int action);
double kfx_replay_speed(void);
const char *kfx_replay_status(void);
#ifdef __cplusplus
}
#endif
#endif
