#ifndef KFX_SAVE_COMPAT_H
#define KFX_SAVE_COMPAT_H
#ifdef __cplusplus
extern "C" {
#endif
struct CatalogueEntry;
void kfx_save_write_requirements(const char *destination,const struct CatalogueEntry *entry);
/* 1 verified, 0 legacy/unrecorded content, negative incompatible or damaged. */
int kfx_save_check_requirements(const char *source,char *report,unsigned int size);
int kfx_save_preserve_before_load(const char *source);
#ifdef __cplusplus
}
#endif
#endif
