#include "../../src/macos_save_compat.cpp"
#include "../../src/macos_replay.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
extern "C" {
char keeper_runtime_directory[152]={};
struct GameCampaign campaign={};
struct Game game={};
static bool online=false,live=false;
TbBool network_is_active(void){return online;}
int kfx_hub_spectator_active(void){return live;}
short get_current_campaign_fgroup(void){return FGrp_Campgn;}
char *prepare_file_path(short group,const char *name){static std::string output;output=std::string(keeper_runtime_directory)+(group==FGrp_Save?"/save/":group==FGrp_Campgn?"/campgns/":"/")+name;return output.data();}
}
static void write_save(const std::filesystem::path &name,int major=VER_MAJOR){CatalogueEntry entry={};entry.game_ver_major=major;entry.game_ver_minor=VER_MINOR;entry.game_ver_release=VER_RELEASE;entry.game_ver_build=VER_BUILD;entry.level_num=1;FileChunkHeader header={sizeof(entry),SGC_InfoBlock,1};std::ofstream out(name,std::ios::binary);out.write((char*)&header,sizeof(header));out.write((char*)&entry,sizeof(entry));out<<"state";}
int main(){
 namespace fs=std::filesystem;char directory[]="/tmp/keeperfx-play-tools-XXXXXX";assert(mkdtemp(directory));fs::path root=directory;snprintf(keeper_runtime_directory,sizeof(keeper_runtime_directory),"%s",directory);
 for(const char *folder:{"save","campgns","levels","fxdata"})fs::create_directories(root/folder);
 strcpy(campaign.fname,"test.cfg");strcpy(campaign.levels_location,"levels");std::ofstream(root/"campgns/test.cfg")<<"campaign";std::ofstream(root/"levels/map00001.slb")<<"map";std::ofstream(root/"fxdata/rules.cfg")<<"rules";
 fs::path file=root/"save/fx1g0000.sav";write_save(file);char report[1024];assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))==0);assert(kfx_save_preserve_before_load(file.c_str()));assert(kfx_save_preserve_before_load(file.c_str()));assert(std::distance(fs::directory_iterator(root/"save/CompatibilityBackups"),fs::directory_iterator())==1);
 CatalogueEntry entry={};entry.level_num=1;kfx_save_write_requirements(file.c_str(),&entry);assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))==1);
 std::ofstream(root/"levels/map00001.slb")<<"different";assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))<0);std::ofstream(root/"levels/map00001.slb")<<"map";
 fs::create_directories(root/"mods");std::ofstream(root/"mods/added.cfg")<<"new mod";assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))<0);fs::remove(root/"mods/added.cfg");assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))==1);
 std::ofstream(root/"levels/map00001.lua")<<"new script";assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))<0);fs::remove(root/"levels/map00001.lua");
 fs::create_symlink(root/"fxdata/rules.cfg",root/"mods/link");assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))<0);fs::remove(root/"mods/link");write_save(file,VER_MAJOR+1);assert(kfx_save_check_requirements(file.c_str(),report,sizeof(report))<0);write_save(file);
 assert(!kfx_replay_validate_file(file.c_str(),report,sizeof(report)));
 std::ofstream(root/"save/replay.pck")<<"replay fixture";game.packet_load_enable=1;game.packet_fopened=1;game.turns_stored=1000;kfx_replay_begin((root/"save/replay.pck").c_str());assert(kfx_replay_available()&&kfx_replay_speed()==1);kfx_replay_control(1);assert(kfx_replay_speed()==2);kfx_replay_control(1);kfx_replay_control(1);assert(kfx_replay_speed()==4);
 game.pckt_gameturn=40;kfx_replay_control(2);game.pckt_gameturn=10;kfx_replay_control(3);assert(!kfx_replay_should_hold());game.pckt_gameturn=40;assert(kfx_replay_should_hold());kfx_replay_control(0);assert(!kfx_replay_should_hold());kfx_replay_begin((root/"save/replay.pck").c_str());game.pckt_gameturn=1;kfx_replay_control(3);game.pckt_gameturn=40;assert(kfx_replay_should_hold());online=true;assert(!kfx_replay_available()&&kfx_replay_speed()==1&&!kfx_replay_control(0));online=false;live=true;assert(!kfx_replay_available());live=false;game.mode_flags|=MFlg_IsDemoMode;assert(!kfx_replay_available());kfx_replay_request_explicit();kfx_replay_begin((root/"save/replay.pck").c_str());assert(kfx_replay_available());
 fs::remove_all(root);puts("PASS: save version/content preflight, immutable backups, added/changed/link rejection, replay speed/pause/bookmarks and multiplayer isolation");
}
