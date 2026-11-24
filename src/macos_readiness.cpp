// Optional Enhanced lobby preflight carried in existing reliable chat frames.
#include "pre_inc.h"
#include "macos_readiness.h"
#include "macos_game_options.h"
#include "config.h"
#include "game_legacy.h"
#include "config_campaigns.h"
#include "config_mods.h"
#include "front_network.h"
#include "frontend.h"
#include "net_game.h"
#include "net_main.h"
#include "net_exchange_common.h"
#include "game_merge.h"
#include "ver_defs.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <future>
#include <chrono>
#include <atomic>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cctype>
#include "post_inc.h"
#if defined(__APPLE__) || defined(KFX_DEDICATED)
#include "kfx/sha256.h"
namespace {
namespace fs=std::filesystem;
struct ReadyDigest{std::string version,map,mods,error;bool complete=false;};
struct ReadyInput{std::string base,campaign_file,levels;std::vector<std::string> mods;int level=0;};
std::array<ReadyDigest,MAX_NET_USERS> reports;
std::array<bool,MAX_NET_USERS> received={};
std::future<ReadyDigest> work;
std::shared_ptr<std::atomic<bool>> cancelled;
struct ReadyShutdown{~ReadyShutdown(){if(cancelled)cancelled->store(true);}} ready_shutdown;
std::string roster;
std::string nonce,work_nonce,pending_start,target_campaign,report_text="Join a lobby. The host checks the selected map before starting.";
int target_level=0;bool pending=false,announced=false;
Uint64 started=0;
std::string engine_version(){return std::to_string(VER_MAJOR)+"."+std::to_string(VER_MINOR)+"."+std::to_string(VER_RELEASE)+"."+std::to_string(VER_BUILD)+"+hub1";}
std::string short_campaign(std::string value){value=fs::path(value).filename().string();auto dot=value.rfind('.');if(dot!=std::string::npos)value.resize(dot);return value;}
bool parse_start(const std::string &message,std::string &camp,int &level){size_t pos=message.find(':');if(pos==std::string::npos)pos=message.find(' ');if(pos==std::string::npos||pos<1||pos>63)return false;camp=message.substr(0,pos);std::string component=camp;for(const char *prefix:{"campgns/","levels/","multiplayer/"})if(component.rfind(prefix,0)==0){component.erase(0,strlen(prefix));break;}if(component.empty())return false;for(unsigned char c:component)if(!std::isalnum(c)&&c!='_'&&c!='-'&&c!='.')return false;std::string number=message.substr(pos+1);while(!number.empty()&&number.front()==' ')number.erase(0,1);while(!number.empty()&&number.back()==' ')number.pop_back();if(number.empty()||number.size()>7||!std::all_of(number.begin(),number.end(),[](unsigned char c){return std::isdigit(c);}))return false;level=std::stoi(number);return level>0&&level<=99999;}
std::string hex_digest(KfxSha256Context &ctx){unsigned char bytes[32];KfxSha256Final(bytes,&ctx);const char chars[]="0123456789abcdef";std::string value;for(unsigned char c:bytes){value+=chars[c>>4];value+=chars[c&15];}return value;}
void hash_text(KfxSha256Context &ctx,const std::string &value){KfxSha256Update(&ctx,value.data(),(size_t)value.size());const char zero=0;KfxSha256Update(&ctx,&zero,1);}
ReadyInput snapshot_input(int level){ReadyInput input;input.base=keeper_runtime_directory;input.level=level;input.levels=campaign.levels_location;short group=get_current_campaign_fgroup();if(group==FGrp_None)group=FGrp_Campgn;input.campaign_file=prepare_file_path(group,campaign.fname);const ModsConfig *mods=get_loaded_mods_conf();const ModConfigItem *lists[]={mods->after_base_item,mods->after_campaign_item,mods->after_map_item};int counts[]={mods->after_base_cnt,mods->after_campaign_cnt,mods->after_map_cnt};for(int stage=0;stage<3;stage++)for(int i=0;i<std::clamp(counts[stage],0,MOD_ITEM_MAX);i++)input.mods.push_back(std::to_string(stage)+":"+lists[stage][i].name);return input;}
ReadyDigest scan_input(ReadyInput input,std::shared_ptr<std::atomic<bool>> stop){
    ReadyDigest result;result.version=engine_version();KfxSha256Context mapctx,modctx;KfxSha256Init(&mapctx);KfxSha256Init(&modctx);uint64_t total=0;size_t files=0;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
    auto file=[&](KfxSha256Context &ctx,const fs::path &path,const std::string &name){
        if(stop->load()||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Content scan cancelled or timed out.");
        if(fs::is_symlink(path)||!fs::is_regular_file(path))throw std::runtime_error("Content contains a link or a missing file.");
        auto size=fs::file_size(path);auto modified=fs::last_write_time(path);if(++files>16000||(total+=size)>512ull*1024*1024)throw std::runtime_error("Content scan exceeds its size limit.");hash_text(ctx,name);FILE *stream=fopen(path.c_str(),"rb");if(!stream)throw std::runtime_error("A selected content file cannot be read.");
        unsigned char bytes[32768];size_t count;bool stopped=false;while((count=fread(bytes,1,sizeof(bytes),stream))>0){KfxSha256Update(&ctx,bytes,(size_t)count);if(stop->load()||std::chrono::steady_clock::now()>deadline){stopped=true;break;}}bool bad=ferror(stream)||stopped;fclose(stream);if(bad||fs::file_size(path)!=size||fs::last_write_time(path)!=modified)throw std::runtime_error("Content changed or could not be read. Retry the check.");
    };
    try{
        if(input.level<=0||input.level>99999)throw std::runtime_error("Choose a map before checking readiness.");
        if(!fs::exists(input.campaign_file))throw std::runtime_error("The campaign configuration is missing.");file(mapctx,input.campaign_file,"campaign.cfg");
        fs::path leveldir=fs::path(input.levels).is_absolute()?fs::path(input.levels):fs::path(input.base)/input.levels;char prefix[32];snprintf(prefix,sizeof(prefix),"map%05d.",input.level);std::vector<fs::path> maps;
        for(const auto &entry:fs::directory_iterator(leveldir)){if(maps.size()>16000||stop->load()||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Map scan cancelled or exceeds its limit.");if(entry.path().filename().string().rfind(prefix,0)==0)maps.push_back(entry.path());}std::sort(maps.begin(),maps.end());if(maps.empty())throw std::runtime_error("The selected map files are missing.");for(const auto &path:maps)file(mapctx,path,path.filename().string());
        for(const std::string &mod:input.mods){hash_text(modctx,mod);fs::path root=fs::path(input.base)/"mods"/mod.substr(mod.find(':')+1);if(!fs::is_directory(root))throw std::runtime_error("A selected mod is missing.");std::vector<fs::path> paths;for(const auto &entry:fs::recursive_directory_iterator(root)){if(paths.size()>16000||stop->load()||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Mod scan cancelled or exceeds its limit.");if(entry.is_symlink())throw std::runtime_error("A selected mod contains a link.");if(entry.is_regular_file()&&entry.path().filename()!=".DS_Store")paths.push_back(entry.path());}std::sort(paths.begin(),paths.end());for(const auto &path:paths)file(modctx,path,fs::relative(path,root).generic_string());}
        result.map=hex_digest(mapctx);result.mods=hex_digest(modctx);result.complete=true;
    }catch(const std::exception &error){result.error=error.what();}
    return result;
}
std::string roster_signature(){std::string value;for(int i=0;i<MAX_NET_USERS;i++)value+=std::to_string(netstate.users[i].progress)+":"+std::string(netstate.users[i].name,strnlen(netstate.users[i].name,sizeof(netstate.users[i].name)))+";";return value;}
void start_work(int level){if(cancelled)cancelled->store(true);if(work.valid())return;cancelled=std::make_shared<std::atomic<bool>>(false);work_nonce=nonce;ReadyInput input=snapshot_input(level);work=std::async(std::launch::async,scan_input,std::move(input),cancelled);}
void send(const std::string &message){if(message.size()<500)send_network_chat_message(netstate.my_id,message.c_str());}
void describe(){
    report_text="Map: "+target_campaign+" / "+std::to_string(target_level)+"\n";bool all=true;int count=0;
    for(int i=0;i<MAX_NET_USERS;i++)if(netstate.users[i].progress>=USER_LOGGEDIN){++count;std::string name(netstate.users[i].name,strnlen(netstate.users[i].name,sizeof(netstate.users[i].name)));if(name.size()>12){size_t length=12;while(length&&((unsigned char)name[length]&0xc0)==0x80)--length;name.resize(length);}std::string status;
        if(!received[i]){status=SDL_GetTicks()-started<4500?"Checking...":"Unchecked: no reply";all=false;}
        else if(!reports[i].complete){status="Scan failed: check files";all=false;}
        else if(!received[netstate.my_id]){status="Waiting for local scan";all=false;}
        else if(reports[i].version!=reports[netstate.my_id].version){status="Engine differs";all=false;}
        else if(reports[i].map!=reports[netstate.my_id].map){status="Map differs";all=false;}
        else if(reports[i].mods!=reports[netstate.my_id].mods){status="Mods differ";all=false;}
        else status="Ready";
        report_text+=name+": "+status+"\n";
    }
    if(received[netstate.my_id]&&!reports[netstate.my_id].error.empty())report_text+=reports[netstate.my_id].error+"\n";
    report_text+=count<2?"Waiting for another player.":all?"All checked peers match.":"Match versions, map files and mods; retry.";
}
bool matched(){if(netstate.my_id<0||netstate.my_id>=MAX_NET_USERS||!received[netstate.my_id]||!reports[netstate.my_id].complete)return false;int count=0;for(int i=0;i<MAX_NET_USERS;i++)if(netstate.users[i].progress>=USER_LOGGEDIN){count++;if(!received[i]||!reports[i].complete||reports[i].version!=reports[netstate.my_id].version||reports[i].map!=reports[netstate.my_id].map||reports[i].mods!=reports[netstate.my_id].mods)return false;}return count>=2;}
void begin(const std::string &camp,int level){nonce=std::to_string(SDL_GetPerformanceCounter());target_campaign=camp;target_level=level;received.fill(false);started=SDL_GetTicks();roster=roster_signature();announced=false;send("KFXE_REQ2|"+nonce+"|"+std::to_string(level)+"|"+camp);start_work(level);}
void finish_start(){std::string message=pending_start;pending=false;pending_start.clear();if(message.empty())return;kfx_options_hide();send_network_chat_message(netstate.my_id,message.c_str());process_frontend_chat_message(netstate.my_id,message.c_str());}
}
extern "C" int kfx_readiness_message(int source,int sender,const char *message){
    if(!message||strncmp(message,"KFXE_",5))return 0;
    if(sender<0||sender>=MAX_NET_USERS||source<0||source>=MAX_NET_USERS)return 1;
    if(netstate.my_id==SERVER_ID&&source!=SERVER_ID&&source!=sender)return 1;
    if(netstate.my_id!=SERVER_ID&&source!=SERVER_ID)return 1;
    if(strlen(message)>500)return 1;std::vector<std::string> fields;std::istringstream input(message);std::string value;while(std::getline(input,value,'|'))fields.push_back(value);
    if(fields.size()==4&&fields[0]=="KFXE_REQ2"&&sender==SERVER_ID&&netstate.my_id!=SERVER_ID){std::string camp;int level;if(!parse_start(fields[3]+":"+fields[2],camp,level)||fields[1].empty()||fields[1].size()>24)return 1;nonce=fields[1];target_campaign=camp;target_level=level;started=SDL_GetTicks();received.fill(false);start_work(level);}
    if(fields.size()==6&&fields[0]=="KFXE_REP2"&&fields[1]==nonce){auto validhash=[](const std::string &s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isxdigit(c);});};bool complete=fields[5]=="1";if(fields[2].size()>32||(complete&&(!validhash(fields[3])||!validhash(fields[4]))))return 1;reports[sender]={fields[2],fields[3],fields[4],"",complete};received[sender]=true;describe();}
    return 1;
}
extern "C" int kfx_readiness_submit_start(const char *text){
    if(!network_is_host()||!text)return 0;std::string camp;int level;if(!parse_start(text,camp,level))return 0;
    char filename[DISKPATH_SIZE];uint8_t pack=prepare_campaign_file_name(camp.c_str(),filename,sizeof(filename));if(pack==CampgnT_Default&&is_campaign_in_list(filename,&mp_mappacks_list))pack=CampgnT_MultiplayerMappack;
    if(!change_campaign(pack,filename)||strcasecmp(campaign.fname,filename)){report_text="The selected campaign cannot be loaded.";kfx_options_show_page(19);return 1;}
    if(work.valid()){report_text="A content scan is still finishing. Retry Start shortly.";kfx_options_show_page(19);return 1;}
    frontnet_send_campaign_change_message(campaign.fname);pending_start=text;pending=true;begin(short_campaign(camp),level);return 1;
}
extern "C" void kfx_readiness_tick(void){
    if(!network_is_active()||frontend_menu_state!=FeSt_NET_START){if(cancelled)cancelled->store(true);pending=false;pending_start.clear();nonce.clear();}
    if(work.valid()&&work.wait_for(std::chrono::seconds(0))==std::future_status::ready){ReadyDigest result=work.get();if(!nonce.empty()&&work_nonce==nonce&&netstate.my_id>=0&&netstate.my_id<MAX_NET_USERS){reports[netstate.my_id]=result;received[netstate.my_id]=true;send("KFXE_REP2|"+nonce+"|"+result.version+"|"+(result.complete?result.map:"-")+"|"+(result.complete?result.mods:"-")+"|"+(result.complete?"1":"0"));describe();}}
    if(!nonce.empty()&&!work.valid()&&netstate.my_id>=0&&netstate.my_id<MAX_NET_USERS&&!received[netstate.my_id])start_work(target_level);
    if(pending&&roster_signature()!=roster){pending=false;pending_start.clear();nonce.clear();if(cancelled)cancelled->store(true);report_text="Lobby players changed. Choose a map and Start again.";kfx_options_show_page(19);}
    if(pending){describe();if(matched()){finish_start();return;}if(SDL_GetTicks()-started>=4500&&!announced){announced=true;kfx_options_show_page(19);}}
}
extern "C" void kfx_readiness_check(void){if(!network_is_active()){report_text="Join a lobby before checking readiness.";return;}if(!network_is_host()){report_text="The host must choose a map and start the check.";return;}if(pending_start.empty()){report_text="Choose a map in the lobby, then press Start.\nThe check runs before the start message is sent.";return;}if(work.valid()){report_text="A content scan is still running.";return;}pending=true;frontnet_send_campaign_change_message(campaign.fname);begin(target_campaign,target_level);}
extern "C" void kfx_readiness_override(void){if(!network_is_host()||pending_start.empty()){report_text="There is no pending host start to override.";return;}finish_start();}
extern "C" void kfx_readiness_cancel(void){pending=false;pending_start.clear();}
extern "C" const char *kfx_readiness_report(void){if(!network_is_active())return "Join a lobby before checking readiness.";if(frontend_menu_state!=FeSt_NET_START)return "Readiness checks run in the pre-game lobby.";return report_text.c_str();}
#else
extern "C" int kfx_readiness_message(int,int,const char*){return 0;}
extern "C" int kfx_readiness_submit_start(const char*){return 0;}
extern "C" void kfx_readiness_tick(void){}
extern "C" void kfx_readiness_check(void){}
extern "C" void kfx_readiness_override(void){}
extern "C" void kfx_readiness_cancel(void){}
extern "C" const char *kfx_readiness_report(void){return "Readiness checking is unavailable in this build.";}
#endif
