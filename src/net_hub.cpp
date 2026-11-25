// Optional community service client. No service is contacted until the user opens the browser.
#include "net_hub.h"
#include "net_lobby.h"
#include <curl/curl.h>
#include "json-dom.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <future>
#include <deque>
#include <array>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <sys/stat.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include <unistd.h>
namespace {
std::string identity_ticket, login_ticket;std::array<std::string,4> identities;
bool initializing=false;
bool remember_session=false;
bool remember_next_login=false;
bool local_profile=false;
bool host_started=false;
std::string base="https://keeper-enhanced.retrocs.org",folder,account,name,account_id,server_id,server_secret,match_id,last_tag,last_body;
struct Registration{std::string host,token,role;int port;};std::vector<Registration> registrations;
long last_status=0;
struct Reply{std::string tag,body;long status=0;};std::future<Reply> job;Uint64 heartbeat_at=0;
std::string lobby_epoch;
struct Request{std::string tag,route,body,token;};std::deque<Request> queue;
void live_reply(const VALUE *);void live_tick();
std::string quote(const std::string &s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';if(c>=32)out+=c;}return out+'"';}
struct Document{VALUE root;Document(const std::string &s){value_init_null(&root);json_dom_parse(s.data(),s.size(),nullptr,0,&root,nullptr);}~Document(){value_fini(&root);}};
std::string field(const VALUE *r,const char *key){const VALUE *v=r?value_dict_get(r,key):nullptr;return v&&value_type(v)==VALUE_STRING?value_string(v):"";}
size_t receive(char *p,size_t size,size_t n,void *data){auto *body=(std::string*)data;size_t bytes=size*n;if(bytes>3*1024*1024||body->size()>3*1024*1024-bytes)return 0;body->append(p,bytes);return bytes;}
Reply probe(const std::string &tag)
{
    const auto endpoint=tag.substr(5);
    const auto colon=endpoint.rfind(':');
    int ping=-1;
    if(colon!=std::string::npos){
        auto hostname=endpoint.substr(0,colon);
        const int port=atoi(endpoint.c_str()+colon+1);
        sockaddr_storage dest={};
        socklen_t length=0;
        int family=AF_INET;
#ifdef _WIN32
        static const bool network_ready=[](){
            WSADATA data{};
            return WSAStartup(MAKEWORD(2,2),&data)==0;
        }();
        if(!network_ready)return {tag,"{\"address\":"+quote(endpoint)+",\"ping_ms\":-1}",200};
#endif
        if(hostname.size()>2&&hostname.front()=='['&&hostname.back()==']'){
            hostname=hostname.substr(1,hostname.size()-2);
            family=AF_INET6;
            auto *address=reinterpret_cast<sockaddr_in6*>(&dest);
            address->sin6_family=AF_INET6;
            address->sin6_port=htons(port);
            if(inet_pton(AF_INET6,hostname.c_str(),&address->sin6_addr)==1)length=sizeof(*address);
        }else{
            auto *address=reinterpret_cast<sockaddr_in*>(&dest);
            address->sin_family=AF_INET;
            address->sin_port=htons(port);
            if(inet_pton(AF_INET,hostname.c_str(),&address->sin_addr)==1)length=sizeof(*address);
        }
        if(length&&port>0&&port<65536){
            auto sock=socket(family,SOCK_DGRAM,0);
#ifdef _WIN32
            if(sock!=INVALID_SOCKET){
                const int select_width=0;
#else
            if(sock>=0){
                const int select_width=sock+1;
#endif
                const auto packet="KFXQ1 "+std::to_string(SDL_GetPerformanceCounter());
                const auto began=SDL_GetTicks();
                sendto(sock,packet.data(),packet.size(),0,reinterpret_cast<sockaddr*>(&dest),length);
                fd_set sockets;
                FD_ZERO(&sockets);
                FD_SET(sock,&sockets);
                timeval timeout={0,400000};
                if(select(select_width,&sockets,nullptr,nullptr,&timeout)>0){
                    char bytes[80];
                    const auto count=recv(sock,bytes,sizeof(bytes),0);
                    if(count>=0&&static_cast<size_t>(count)==packet.size()&&
                       !memcmp(bytes,"KFXA1 ",6)&&!memcmp(bytes+6,packet.data()+6,packet.size()-6))
                        ping=static_cast<int>(SDL_GetTicks()-began);
                }
#ifdef _WIN32
                closesocket(sock);
#else
                close(sock);
#endif
            }
        }
    }
    return {tag,"{\"address\":"+quote(endpoint)+",\"ping_ms\":"+std::to_string(ping)+"}",200};
}
Reply http(std::string url,std::string tag,std::string route,std::string body,std::string token){
 if(tag.rfind("ping:",0)==0)return probe(tag);
 Reply reply;reply.tag=tag;CURL *c=curl_easy_init();if(!c){reply.body="{\"error\":\"HTTP initialization failed\"}";return reply;}
 curl_slist *headers=nullptr;headers=curl_slist_append(headers,"Content-Type: application/json");if(!token.empty())headers=curl_slist_append(headers,("Authorization: Bearer "+token).c_str());
 curl_easy_setopt(c,CURLOPT_URL,(url+route).c_str());curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,2000L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,5000L);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(c,CURLOPT_WRITEDATA,&reply.body);
 if(!body.empty()){curl_easy_setopt(c,CURLOPT_POSTFIELDS,body.c_str());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,(long)body.size());}
 CURLcode code=curl_easy_perform(c);curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&reply.status);
 // Retry only a web-server HTML 404, not an API JSON error or an uncertain request.
 char *type=nullptr;curl_easy_getinfo(c,CURLINFO_CONTENT_TYPE,&type);
 if(code==CURLE_OK&&reply.status==404&&type&&!strncmp(type,"text/html",9)){
  char *escaped=curl_easy_escape(c,route.c_str(),(int)route.size());
  if(escaped){std::string direct=url+"/index.php?route="+escaped;curl_free(escaped);reply.body.clear();curl_easy_setopt(c,CURLOPT_URL,direct.c_str());code=curl_easy_perform(c);curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&reply.status);}
 }
 if(code!=CURLE_OK)reply.body="{\"error\":\"Community service unavailable. LAN and direct play remain available.\"}";
 curl_slist_free_all(headers);curl_easy_cleanup(c);return reply;
}
void restrict_user_file(FILE *file)
{
#ifndef _WIN32
    fchmod(fileno(file),0600);
#else
    // Files inherit access controls from the user's profile directory.
    (void)file;
#endif
}
bool flush_user_file(FILE *file)
{
    if(fflush(file)!=0)return false;
#ifdef _WIN32
    return _commit(fileno(file))==0;
#else
    return fsync(fileno(file))==0;
#endif
}
bool replace_user_file(const char *source,const char *target)
{
#ifdef _WIN32
    return MoveFileExA(source,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(source,target)==0;
#endif
}
void save_token(){if(folder.empty())return;std::string p=folder+"/community-session.cfg",temp=p+".pending";if(!remember_session||name.empty()){unlink(p.c_str());return;}FILE *f=fopen(temp.c_str(),"w");if(!f)return;restrict_user_file(f);bool ok=fprintf(f,"%s\n%s\n%s\n%s\n",base.c_str(),account.c_str(),name.c_str(),account_id.c_str())>0;ok=flush_user_file(f)&&ok;if(fclose(f))ok=false;if(!ok||!replace_user_file(temp.c_str(),p.c_str()))unlink(temp.c_str());}
void apply_channels(const VALUE *){registrations.clear();}
}
extern "C" void kfx_hub_initialize(const char *save){initializing=true;folder=save?save:"";if(getenv("KFX_PRIVATE_SERVER")){initializing=false;return;}const char *env_url=getenv("KFX_HUB_URL"),*env_id=getenv("KFX_HUB_SERVER_ID"),*env_secret=getenv("KFX_HUB_SERVER_SECRET");if(env_url&&env_id&&env_secret){kfx_hub_set_url(env_url);server_id=env_id;server_secret=env_secret;initializing=false;return;}FILE *f=fopen((folder+"/community-session.cfg").c_str(),"r");if(!f){initializing=false;return;}char lines[4][512]={};for(auto &line:lines)if(fgets(line,sizeof(line),f))line[strcspn(line,"\r\n")]=0;fclose(f);if(kfx_hub_set_url(lines[0])){account=lines[1];name=lines[2];account_id=lines[3];remember_session=!name.empty();}initializing=false;}
extern "C" const char *kfx_hub_url(){return base.c_str();}
extern "C" int kfx_hub_set_url(const char *url){if(job.valid()||!queue.empty()||!url||strlen(url)>240)return 0;std::string s=url;while(!s.empty()&&s.back()=='/')s.pop_back();if(s.find_first_of("\r\n?#@")!=std::string::npos)return 0;bool secure=s.rfind("https://",0)==0;bool local=s.rfind("http://127.0.0.1:",0)==0||s.rfind("http://localhost:",0)==0;if(!secure&&!local)return 0;if(base!=s){account.clear();name.clear();account_id.clear();server_id.clear();server_secret.clear();registrations.clear();}base=s;if(!initializing)save_token();return 1;}
extern "C" int kfx_hub_request(const char *tag,const char *route,const char *body,int credentials){if(getenv("KFX_PRIVATE_SERVER"))return 0;if(queue.size()>=8||!tag||!route||strncmp(route,"/api/",5)||strlen(route)>200||strchr(route,'\r')||strchr(route,'\n'))return 0;std::string token=credentials==2?server_secret:credentials==1&&!local_profile?account:"";queue.push_back({tag,route,body?body:"",token});return 1;}
namespace {std::string advertised_content;}
extern "C" void kfx_hub_content(const char *value){advertised_content=value?value:"";}
extern "C" int kfx_hub_tick(int hosting,int players,int playing,const char *map){
 int ready=0;if(job.valid()&&job.wait_for(std::chrono::seconds(0))==std::future_status::ready){Reply r=job.get();last_tag=r.tag;last_body=r.body;last_status=r.status;Document d(r.body);std::string token=field(&d.root,"token");if((r.tag=="pair-poll"||r.tag=="cloud-verify"||r.tag=="cloud-login")&&!token.empty()){
     local_profile=false;remember_session=r.tag=="pair-poll"||remember_next_login;
     account=token;name=field(&d.root,"name");account_id=field(&d.root,"id");save_token();
 }
 if(((r.tag=="me"&&!local_profile)||r.tag=="picker-me")&&r.status==401){account.clear();save_token();}
 if(r.tag=="me"&&!local_profile&&!field(&d.root,"id").empty()){account_id=field(&d.root,"id");name=field(&d.root,"name");save_token();}
 if((r.tag=="publish"||r.tag=="join")&&!field(&d.root,"match_id").empty())match_id=field(&d.root,"match_id");
 if(r.tag=="publish"&&!field(&d.root,"secret").empty()){server_id=field(&d.root,"id");server_secret=field(&d.root,"secret");lobby_epoch.clear();heartbeat_at=0;}
 if(r.tag=="join"){
     const VALUE *required=value_dict_get(&d.root,"login_ticket_required");
     identity_ticket=required&&value_type(required)==VALUE_BOOL&&value_bool(required)?"":field(&d.root,"identity_ticket");
 }
 if(r.tag.rfind("identity:",0)==0){int peer=atoi(r.tag.c_str()+9);if(peer>0&&peer<4)identities[peer]=field(&d.root,"user");}
 if(r.tag=="stream")live_reply(&d.root);
 if(r.tag=="heartbeat")apply_channels(&d.root);
 ready=1;}
 if(hosting)host_started=true;
 if((hosting||host_started)&&!server_id.empty()&&!job.valid()&&queue.empty()&&SDL_GetTicks()>=heartbeat_at){
     heartbeat_at=SDL_GetTicks()+(hosting&&!playing?500:3000);
     std::string body="{\"mods\":"+quote(advertised_content)+",\"players\":"+std::to_string(std::clamp(players,0,4))
         +",\"status\":"+quote(hosting?(playing?"playing":"lobby"):"offline")+(map&&*map?",\"map\":"+quote(map):"");
     if(hosting&&!playing&&kfx_hub_requires_login_ticket()){
         unsigned occupied=1;
         for(NetUserId peer=1;peer<netstate.max_users;++peer){
             if(netstate.users[peer].progress==USER_LOGGEDIN)occupied|=1u<<peer;
         }
         body+=",\"occupied\":"+std::to_string(occupied)+",\"lobby_epoch\":"+quote(lobby_epoch);
     }
     body+="}";
     kfx_hub_request("heartbeat",("/api/servers/"+server_id+"/heartbeat").c_str(),body.c_str(),2);
     if(!hosting){server_id.clear();server_secret.clear();lobby_epoch.clear();registrations.clear();}
 }
 live_tick();
 if(!job.valid()&&!queue.empty()){Request q=queue.front();queue.pop_front();job=std::async(std::launch::async,http,base,q.tag,q.route,q.body,q.token);}
 return ready;
}
extern "C" long kfx_hub_result_status(){return last_status;}
extern "C" const char *kfx_hub_result_tag(){return last_tag.c_str();}
extern "C" const char *kfx_hub_result(){return last_body.c_str();}
extern "C" int kfx_hub_busy(){return job.valid()||!queue.empty();}
extern "C" const char *kfx_hub_account_name(){return local_profile?"":name.c_str();}
extern "C" void kfx_hub_remember_session(int remember){remember_next_login=remember!=0;}
extern "C" int kfx_hub_session_remembered(){return remember_session&&!local_profile&&!name.empty();}
extern "C" void kfx_hub_use_local_profile(){local_profile=true;identity_ticket.clear();match_id.clear();}
extern "C" int kfx_hub_forget_cached_profile(const char *id)
{
    if(!id||account_id!=id)return 1;
    if(!folder.empty()&&unlink((folder+"/community-session.cfg").c_str())!=0&&errno!=ENOENT)
        return 0;
    account.clear();
    name.clear();
    account_id.clear();
    remember_session=false;
    kfx_hub_use_local_profile();
    return 1;
}
extern "C" const char *kfx_hub_cached_id(){return account_id.c_str();}
extern "C" const char *kfx_hub_cached_name(){return name.c_str();}
extern "C" int kfx_hub_probe_picker(){
    if(account.empty()||queue.size()>=8)return 0;
    queue.push_back({"picker-me","/api/me","",account});
    return 1;
}
extern "C" int kfx_hub_resume_account(const char *id){
    if(!id||account_id!=id||account.empty())return 0;
    local_profile=false;
    return 1;
}
extern "C" int kfx_hub_has_session(){return !local_profile&&!account.empty();}
extern "C" const char *kfx_hub_account_id(){return local_profile?"":account_id.c_str();}
extern "C" void kfx_hub_sign_out(){kfx_hub_request("logout","/api/logout","{}",1);account.clear();name.clear();account_id.clear();identity_ticket.clear();save_token();}
extern "C" void kfx_hub_client_relay(const char *,int,const char *){registrations.clear();}
extern "C" int kfx_hub_relay_registration(int,char *,int,int *,char *,int){return 0;}
extern "C" void kfx_hub_outcome(const char *result){if(local_profile||account.empty()||match_id.empty())return;std::string body="{\"result\":"+quote(result?result:"disconnected")+"}";kfx_hub_request("result",("/api/matches/"+match_id+"/result").c_str(),body.c_str(),1);match_id.clear();}

extern "C" void kfx_hub_set_login_ticket(const char *ticket){login_ticket=ticket?ticket:"";}
extern "C" const char *kfx_hub_login_ticket(){return login_ticket.c_str();}
extern "C" int kfx_hub_requires_login_ticket(){return getenv("KFX_PUBLIC_DEDICATED")!=nullptr;}
extern "C" int kfx_hub_admit_peer(int peer,unsigned occupied,const char *ticket)
{
    if(peer<1||peer>=4||server_id.empty()||server_secret.empty()||strlen(ticket)!=43||
       strspn(ticket,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=43)return 0;
    const std::string body="{\"peer\":"+std::to_string(peer)+",\"occupied\":"+std::to_string(occupied)+",\"ticket\":"+quote(ticket)+"}";
    const Reply reply=http(base,"admit","/api/servers/"+server_id+"/admit",body,server_secret);
    if(reply.status!=200)return 0;
    const Document result(reply.body);
    const std::string user=field(&result.root,"user");
    if(user.empty())return 0;
    identities[peer]=user;
    lobby_epoch=field(&result.root,"lobby_epoch");
    return 1;
}

extern "C" const char *kfx_hub_identity_claim(){return identity_ticket.c_str();}
extern "C" void kfx_hub_receive_identity(int peer,const char *ticket){if(kfx_hub_requires_login_ticket()||peer<1||peer>3||server_id.empty()||!ticket||strlen(ticket)>100)return;std::string body="{\"peer\":"+std::to_string(peer)+",\"ticket\":"+quote(ticket)+"}";kfx_hub_request(("identity:"+std::to_string(peer)).c_str(),("/api/servers/"+server_id+"/identity").c_str(),body.c_str(),2);}
extern "C" const char *kfx_hub_peer_account(int peer){return peer>=0&&peer<4?identities[peer].c_str():"";}
// Spectator streams append only authenticated service data to a private replay file.
namespace {
std::string live_stream,live_file;size_t live_offset=0,live_target=0;bool live_finished=false,live_ready=false,live_launched=false;Uint64 live_next=0;
bool decode64(const std::string &s,std::string &out){static const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";unsigned value=0;int bits=0;bool padding=false;for(unsigned char c:s){if(c=='='){padding=true;continue;}size_t pos=alphabet.find(c);if(pos==std::string::npos||padding)return false;value=(value<<6)|(unsigned)pos;bits+=6;if(bits>=8){bits-=8;out+=(char)((value>>bits)&255);}}return out.size()<=32768;}
void live_reply(const VALUE *root){if(live_stream.empty())return;const VALUE *offset=value_dict_get(root,"offset"),*size=value_dict_get(root,"size"),*ended=value_dict_get(root,"finished");if(!offset||!size||value_uint32(offset)!=live_offset)return;std::string data;if(!decode64(field(root,"data"),data)||live_offset+data.size()>16*1024*1024)return;
 if(!live_target)live_target=value_uint32(size);
 FILE *f=fopen(live_file.c_str(),live_offset?"ab":"wb");if(!f)return;restrict_user_file(f);bool ok=fwrite(data.data(),1,data.size(),f)==data.size();if(fclose(f))ok=false;if(!ok)return;live_offset+=data.size();live_finished=ended&&value_bool(ended)&&live_offset>=value_uint32(size);if(live_offset>=live_target&&live_target>0&&!live_launched)live_ready=true;live_next=SDL_GetTicks()+(live_offset<value_uint32(size)?0:300);
}
void live_tick(){if(live_stream.empty()||live_finished||job.valid()||!queue.empty()||SDL_GetTicks()<live_next)return;std::string route="/api/streams/"+live_stream+"?offset="+std::to_string(live_offset);kfx_hub_request("stream",route.c_str(),nullptr,0);live_next=SDL_GetTicks()+300;}
}
extern "C" void kfx_hub_spectator_start(const char *id){live_stream=id?id:"";live_file=folder+"/community-spectator.pck";live_offset=live_target=0;live_finished=live_ready=live_launched=false;live_next=0;}
extern "C" const char *kfx_hub_spectator_file(){return live_file.c_str();}
extern "C" int kfx_hub_spectator_ready(){if(!live_ready)return 0;live_ready=false;live_launched=true;return 1;}
extern "C" int kfx_hub_spectator_finished(){return live_finished;}
extern "C" int kfx_hub_spectator_active(){return !live_stream.empty();}
extern "C" void kfx_hub_spectator_stop(){live_stream.clear();live_finished=false;live_launched=false;}

extern "C" int kfx_hub_probe(const char *endpoint){if(!endpoint||strlen(endpoint)>150||queue.size()>=8)return 0;queue.push_back({std::string("ping:")+endpoint,"","",""});return 1;}

namespace {std::array<int,4> terminal={},departed_early={};}
extern "C" void kfx_hub_new_match(){terminal.fill(0);departed_early.fill(0);}
extern "C" void kfx_hub_record_terminal(int peer,int outcome){if(peer>=0&&peer<4&&!terminal[peer]&&(outcome==1||outcome==2))terminal[peer]=outcome;}
extern "C" void kfx_hub_record_departure(int peer){if(peer>=0&&peer<4&&!terminal[peer])departed_early[peer]=1;}
extern "C" int kfx_hub_terminal_outcome(int peer){return peer>=0&&peer<4?terminal[peer]:0;}
extern "C" int kfx_hub_terminal_verified(int peer){return peer>=0&&peer<4&&terminal[peer]&&!departed_early[peer];}

extern "C" int kfx_hub_peer_departed(int peer){return peer>=0&&peer<4&&departed_early[peer];}
