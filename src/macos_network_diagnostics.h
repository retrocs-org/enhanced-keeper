#ifndef KFX_NETWORK_DIAGNOSTICS_H
#define KFX_NETWORK_DIAGNOSTICS_H
#ifdef __cplusplus
extern "C" {
#endif
void kfx_network_issue(const char *reason,unsigned long turn,unsigned int host_checksum,unsigned int peer_checksum);
#ifdef __cplusplus
}
#endif
#endif
