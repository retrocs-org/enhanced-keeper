#include "pre_inc.h"
#include "macos_replay.h"
#include "game_legacy.h"
#include "keeperfx.hpp"
#include "config.h"
#include "game_saves.h"
#include "version.h"
#include <filesystem>
#include "net_game.h"
#include "net_hub.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "post_inc.h"
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#endif
namespace {
bool requested_viewer=false,explicit_viewer=false;bool paused=false;int speed_index=1;const double speeds[]={.5,1,2,4};unsigned long pause_at=0;std::string marks_file,message;std::vector<unsigned long> marks;
}
extern "C" int kfx_replay_available(){return game.packet_load_enable&&game.packet_fopened&&!network_is_active()&&!kfx_hub_spectator_active()&&(!(game.mode_flags&MFlg_IsDemoMode)||explicit_viewer);}
extern "C" int kfx_replay_validate_file(const char *file,char *report,unsigned int capacity){
 auto result=[&](int code,const std::string &text){if(report&&capacity)snprintf(report,capacity,"%s",text.c_str());return code;};
 try{namespace fs=std::filesystem;if(fs::is_symlink(file)||!fs::is_regular_file(file))return result(0,"Choose a regular replay file.");auto length=fs::file_size(file);if(length>512ull*1024*1024)return result(0,"Replay exceeds the 512 MiB check limit.");std::ifstream in(file,std::ios::binary);uint64_t offset=0;bool info_found=false,packet_header=false;
  for(int i=0;i<64&&offset+sizeof(FileChunkHeader)<=length;i++){FileChunkHeader header={};in.read((char*)&header,sizeof(header));offset+=sizeof(header);if(!in||header.len>length-offset)return result(0,"Replay header is truncated.");
   if(header.id==SGC_InfoBlock){CatalogueEntry entry={};if(header.len!=sizeof(entry))return result(0,"Unsupported replay catalogue format.");in.read((char*)&entry,sizeof(entry));if(!in)return result(0,"Replay header is truncated.");if(entry.game_ver_major!=VER_MAJOR||entry.game_ver_minor!=VER_MINOR||entry.game_ver_release!=VER_RELEASE||entry.game_ver_build!=VER_BUILD)return result(0,"Replay requires engine "+std::to_string(entry.game_ver_major)+"."+std::to_string(entry.game_ver_minor)+"."+std::to_string(entry.game_ver_release)+"."+std::to_string(entry.game_ver_build)+". Use the matching engine.");info_found=true;offset+=header.len;continue;}
   if(header.id==SGC_PacketHeader){if(header.len!=sizeof(PacketSaveHead))return result(0,"Unsupported replay packet format.");packet_header=true;}
   if(header.id==SGC_GameOrig&&header.len!=sizeof(Game))return result(0,"Replay game-state format differs from this build.");
   if(header.id==SGC_PacketData)return result(info_found&&packet_header&&header.len==0?1:0,info_found&&packet_header&&header.len==0?"Replay engine and packet format match.":"Incomplete replay headers.");
   in.seekg(header.len,std::ios::cur);offset+=header.len;
  }return result(0,"No supported replay version header was found.");
 }catch(...){return result(0,"Replay could not be read.");}
}
extern "C" void kfx_replay_request_explicit(){requested_viewer=true;}
extern "C" void kfx_replay_begin(const char *file){
 explicit_viewer=requested_viewer;requested_viewer=false;
 paused=false;speed_index=1;pause_at=0;marks.clear();marks_file.clear();message.clear();
#ifdef __APPLE__
 FILE *input=fopen(file,"rb");if(!input)return;CC_SHA256_CTX context;CC_SHA256_Init(&context);char bytes[65536];size_t count,total=0;while((count=fread(bytes,1,sizeof(bytes),input))>0){total+=count;if(total>512u*1024*1024){fclose(input);message="Replay is too large for persistent bookmarks.";return;}CC_SHA256_Update(&context,bytes,(CC_LONG)count);}bool bad=ferror(input);fclose(input);if(bad)return;unsigned char digest[32];CC_SHA256_Final(digest,&context);const char *hex="0123456789abcdef";std::string id;for(unsigned char c:digest){id+=hex[c>>4];id+=hex[c&15];}
 std::string name="replay-"+id+".bookmarks";marks_file=prepare_file_path(FGrp_Save,name.c_str());std::ifstream in(marks_file);unsigned long turn;while(marks.size()<16&&in>>turn)if(turn>0&&turn<=(unsigned long)game.turns_stored)marks.push_back(turn);
#endif
}
extern "C" double kfx_replay_speed(){return kfx_replay_available()?speeds[speed_index]:1;}
extern "C" int kfx_replay_paused(){return kfx_replay_available()&&paused;}
extern "C" int kfx_replay_should_hold(){if(!kfx_replay_available())return 0;if(pause_at&&(unsigned long)game.pckt_gameturn>=pause_at){paused=true;pause_at=0;message="Paused at bookmark.";}return paused;}
extern "C" int kfx_replay_control(int action){
 if(!kfx_replay_available())return 0;
 if(action==0){paused=!paused;pause_at=0;}
 else if(action==-1||action==1)speed_index=std::clamp(speed_index+action,0,3);
 else if(action==2){if(marks_file.empty()){message="Persistent bookmarks are unavailable for this replay.";return 1;}unsigned long turn=game.pckt_gameturn;if(!turn){message="Play at least one turn before bookmarking.";return 1;}if(std::find(marks.begin(),marks.end(),turn)==marks.end()){if(marks.size()>=16){message="This replay already has 16 bookmarks.";return 1;}marks.push_back(turn);std::sort(marks.begin(),marks.end());}std::string pending=marks_file+".pending";std::ofstream out(pending);for(auto value:marks)out<<value<<"\n";out.close();if(!out||rename(pending.c_str(),marks_file.c_str()))message="Could not save bookmarks.";else message="Bookmark saved at turn "+std::to_string(turn)+".";}
 else if(action==3){auto next=std::upper_bound(marks.begin(),marks.end(),(unsigned long)game.pckt_gameturn);if(next==marks.end())message="No later bookmark. Playback does not seek backwards.";else{pause_at=*next;paused=false;message="Playing to turn "+std::to_string(pause_at)+". Every simulation turn is processed.";}}
 return 1;
}
extern "C" const char *kfx_replay_status(){static std::string output;if(!kfx_replay_available())output="Open a completed local replay to use playback controls. Live spectating and live multiplayer are unaffected.";else{std::ostringstream text;text<<(paused?"Paused":"Playing")<<" / "<<speeds[speed_index]<<"x / turn "<<game.pckt_gameturn<<" of "<<game.turns_stored<<"\nBookmarks:";for(auto turn:marks)text<<" "<<turn;text<<"\n"<<message;output=text.str();}return output.c_str();}
