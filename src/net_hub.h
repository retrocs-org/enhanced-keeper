#ifndef KFX_NET_HUB_H
#define KFX_NET_HUB_H
#ifdef __cplusplus
extern "C" {
#endif
void kfx_hub_content(const char *fingerprint);
void kfx_hub_initialize(const char *folder);
const char *kfx_hub_url(void);
int kfx_hub_set_url(const char *url);
int kfx_hub_request(const char *tag,const char *route,const char *body,int credentials);
int kfx_hub_tick(int hosting,int players,int playing,const char *map);
const char *kfx_hub_result_tag(void);
long kfx_hub_result_status(void);
const char *kfx_hub_result(void);
int kfx_hub_probe(const char *endpoint);
int kfx_hub_busy(void);
int kfx_lobby_manage_available(void);
int kfx_lobby_manage_locked(void);
const char *kfx_lobby_manage_player(int index);
const char *kfx_lobby_manage_audit(void);
const char *kfx_lobby_manage_last_action(void);
const char *kfx_lobby_manage_notice(void);
void kfx_lobby_manage_request(const char *action,int index);
void kfx_lobby_manage_poll(int in_lobby);
const char *kfx_hub_account_name(void);
const char *kfx_hub_account_id(void);
int kfx_hub_has_session(void);
int kfx_hub_rejoin_seconds(void);
const char *kfx_hub_rejoin_id(void);
void kfx_hub_rejoin_poll(void);
void kfx_hub_sign_out(void);
int kfx_hub_session_remembered(void);
void kfx_hub_use_local_profile(void);
int kfx_hub_forget_cached_profile(const char *id);
const char *kfx_hub_cached_id(void);
const char *kfx_hub_cached_name(void);
int kfx_hub_probe_picker(void);
int kfx_hub_resume_account(const char *id);
void kfx_hub_client_relay(const char *host,int port,const char *token);
int kfx_hub_relay_registration(int index,char *host,int size,int *port,char *body,int body_size);
const char *kfx_hub_identity_claim(void);
void kfx_hub_set_login_ticket(const char *ticket);
const char *kfx_hub_login_ticket(void);
int kfx_hub_requires_login_ticket(void);
int kfx_hub_admit_peer(int peer, unsigned occupied, const char *ticket);
void kfx_hub_receive_identity(int peer,const char *ticket);
const char *kfx_hub_peer_account(int peer);
void kfx_hub_spectator_start(const char *id);
const char *kfx_hub_spectator_file(void);
int kfx_hub_spectator_ready(void);
int kfx_hub_spectator_finished(void);
int kfx_live_replay_ready(void);
int kfx_hub_spectator_active(void);
void kfx_hub_spectator_stop(void);
void kfx_hub_new_match(void);
void kfx_hub_record_terminal(int peer,int outcome);
void kfx_hub_record_departure(int peer);
int kfx_hub_terminal_outcome(int peer);
int kfx_hub_peer_departed(int peer);
int kfx_hub_terminal_verified(int peer);
void kfx_hub_outcome(const char *result);
#ifdef __cplusplus
}
#endif
#endif
