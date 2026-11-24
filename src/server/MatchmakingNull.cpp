#include "net_matchmaking.h"
// Public listings use KeeperHub HTTP; LAN and direct ENet do not use the old
// desktop WebSocket lobby or hole punching.
TbNetworkSessionNameEntry matchmaking_sessions[MATCHMAKING_SESSIONS_MAX] = {};
TbBool matchmaking_enabled = false;
char matchmaking_ws_url[MATCHMAKING_URL_MAX] = {};
char matchmaking_ip_url[MATCHMAKING_URL_MAX] = {};
int matchmaking_session_count = 0;
char join_lobby_id[MATCHMAKING_ID_MAX] = {};
void matchmaking_set_server(const char*){}
void matchmaking_connect_async(){}
int matchmaking_take_error(){return 0;}
const char* matchmaking_error_text(){return "Desktop matchmaking is unavailable in a dedicated build";}
const char* matchmaking_last_error(){return matchmaking_error_text();}
int matchmaking_connect(){return 0;}
int matchmaking_request_list(){return 0;}
void matchmaking_disconnect(){}
void matchmaking_finish_lobby(MatchmakingLobbyResult,int,const char*){}
void matchmaking_refresh_sessions(){}
int matchmaking_create(const char*,int,int){return 0;}
int matchmaking_punch(const char*,int,int,PunchAddresses*){return 0;}
int matchmaking_poll_punch(PunchAddresses*){return 0;}
