#include "../../src/macos_readiness.cpp"
#include <cassert>
#include <fstream>
extern "C" {
NetState netstate={}; GameCampaign campaign={}; CampaignsList mp_mappacks_list={},mappacks_list={};
char keeper_runtime_directory[152]={};int frontend_menu_state=FeSt_NET_START;
static ModsConfig fixture_mods={};static std::string fixture_path;static std::vector<std::string> messages;static int starts=0,shown=-1;
const ModsConfig *get_loaded_mods_conf(){return &fixture_mods;}
char *prepare_file_path(short,const char*){return fixture_path.data();}
short get_current_campaign_fgroup(){return FGrp_Campgn;}
TbBool is_campaign_in_list(const char*,CampaignsList*){return false;}
uint8_t prepare_campaign_file_name(const char*,char *name,int size){snprintf(name,size,"fixture.cfg");return CampgnT_Default;}
TbBool change_campaign(uint8_t,const char *name){snprintf(campaign.fname,sizeof(campaign.fname),"%s",name);return true;}
TbBool network_is_host(){return netstate.my_id==0;}
TbBool network_is_active(){return true;}
void frontnet_send_campaign_change_message(const char*){}
void send_network_chat_message(NetUserId,const char *text){messages.push_back(text);}
void process_frontend_chat_message(NetUserId,const char*){++starts;}
void kfx_options_show_page(int p){shown=p;}
void kfx_options_hide(){}
}
int main(){
 char dir[]="/tmp/kfx-ready-XXXXXX";assert(mkdtemp(dir));fs::path root(dir);fs::create_directory(root/"levels");fs::create_directories(root/"mods/test");fixture_path=(root/"fixture.cfg").string();std::ofstream(fixture_path)<<"campaign";std::ofstream(root/"levels/map00001.slb")<<"map";std::ofstream(root/"mods/test/config.cfg")<<"mod";
 fs::create_directory(root/"fxdata");
 for(const char *archive:required_sprite_zips)std::ofstream(root/"fxdata"/archive)<<"fixture";
 ReadyInput input;input.base=root;input.campaign_file=fixture_path;input.levels="levels";input.level=1;input.mods={"0:test"};auto stop=std::make_shared<std::atomic<bool>>(false);auto original=scan_input(input,stop);assert(original.complete&&original.map.size()==64);assert(scan_input(input,stop).map==original.map);
 fs::remove(root/"fxdata"/required_sprite_zips[0]);
 assert(scan_input(input,stop).category=="sprites");
 std::ofstream(root/"fxdata"/required_sprite_zips[0])<<"fixture";
 std::ofstream(root/"levels/map00001.slb")<<"changed";assert(scan_input(input,stop).map!=original.map);std::ofstream(root/"mods/test/config.cfg")<<"changed";assert(scan_input(input,stop).mods!=original.mods);fs::create_symlink(root/"fixture.cfg",root/"mods/test/link");assert(!scan_input(input,stop).complete);fs::remove(root/"mods/test/link");stop->store(true);assert(!scan_input(input,stop).complete);
 std::string camp;int level;assert(parse_start("fixture:1",camp,level));assert(!parse_start("../../bad:1",camp,level));assert(!parse_start("fixture:0",camp,level));assert(!parse_start("fixture:99999999999999999999",camp,level));
 netstate.my_id=0;for(int i=0;i<2;i++){netstate.users[i].progress=USER_LOGGEDIN;snprintf(netstate.users[i].name,32,"Player%d",i);}nonce="test";reports[0]=original;received[0]=true;
 std::string reply="KFXE_REP2|test|"+original.version+"|"+original.map+"|"+original.mods+"|1";
 assert(kfx_readiness_message(1,2,reply.c_str())&&!received[2]);assert(kfx_readiness_message(1,1,reply.c_str())&&matched());reports[1].map=std::string(64,'0');assert(!matched());describe();assert(report_text.find("Map differs")!=std::string::npos);reports[1]=original;reports[1].mods=std::string(64,'0');assert(!matched());reports[1]=original;reports[1].version="old";assert(!matched());received[1]=false;assert(!matched());
 pending_start="fixture:1";pending=true;roster=roster_signature();started=SDL_GetTicks();kfx_readiness_tick();assert(starts==0);reports[1]=original;received[1]=true;kfx_readiness_tick();assert(starts==1&&!pending);
 pending_start="fixture:1";pending=true;kfx_readiness_cancel();kfx_readiness_override();assert(starts==1);pending_start="fixture:1";kfx_readiness_override();assert(starts==2);
 pending_start="fixture:1";pending=true;roster=roster_signature();netstate.users[1].progress=USER_UNUSED;kfx_readiness_tick();assert(starts==2&&!pending&&pending_start.empty()&&shown==19);
 snprintf(keeper_runtime_directory,sizeof(keeper_runtime_directory),"%s",dir);snprintf(campaign.levels_location,sizeof(campaign.levels_location),"levels");netstate.users[1].progress=USER_LOGGEDIN;
 assert(kfx_readiness_submit_start("fixture:1")==1&&pending&&work.valid());
 for(int n=0;n<500&&!received[0];n++){kfx_readiness_tick();SDL_Delay(2);}assert(received[0]&&reports[0].complete&&starts==2);
 std::string current="KFXE_REP2|"+nonce+"|"+reports[0].version+"|"+reports[0].map+"|"+reports[0].mods+"|1";kfx_readiness_message(1,1,current.c_str());kfx_readiness_tick();assert(starts==3&&!pending);
 fs::remove_all(root);puts("PASS: real content hashing, map/mod mismatch, symlink/cancellation, spoofed replies, guarded start, explicit override and roster changes");
}

extern "C" TbBool kfx_lobby_ready_all(void){return true;}
