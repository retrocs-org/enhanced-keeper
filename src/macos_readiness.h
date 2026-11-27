#ifndef KFX_MACOS_READINESS_H
#define KFX_MACOS_READINESS_H
#ifdef __cplusplus
extern "C" {
#endif
int kfx_readiness_message(int source,int sender,const char *message);
int kfx_readiness_submit_start(const char *message);
void kfx_readiness_tick(void);
void kfx_readiness_check(void);
void kfx_readiness_override(void);
void kfx_readiness_cancel(void);
const char *kfx_readiness_report(void);
const char *kfx_readiness_details(void);
void kfx_readiness_content_masks(unsigned *passed,unsigned *failed);
#ifdef __cplusplus
}
#endif
#endif
