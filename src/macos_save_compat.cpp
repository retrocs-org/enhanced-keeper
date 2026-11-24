#include "pre_inc.h"
#include "macos_save_compat.h"
#include "game_saves.h"
#include "config.h"
#include "config_campaigns.h"
#include "config_mods.h"
#include "version.h"
#include <filesystem>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include "post_inc.h"
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
namespace {
namespace fs=std::filesystem;
std::string version(){return std::to_string(VER_MAJOR)+"."+std::to_string(VER_MINOR)+"."+std::to_string(VER_RELEASE)+"."+std::to_string(VER_BUILD);}
std::string hash(const fs::path &path,uint64_t &budget){
 if(fs::is_symlink(path)||!fs::is_regular_file(path))throw std::runtime_error("A required file is missing or linked.");
 auto length=fs::file_size(path);auto modified=fs::last_write_time(path);if(length>512ull*1024*1024||budget+length>512ull*1024*1024)throw std::runtime_error("Content exceeds the check limit.");budget+=length;
 CC_SHA256_CTX context;CC_SHA256_Init(&context);std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read a required file.");char bytes[32768];auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 while(in){in.read(bytes,sizeof(bytes));auto count=in.gcount();if(count)CC_SHA256_Update(&context,bytes,(CC_LONG)count);if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Content check timed out.");}
 if(in.bad()||fs::file_size(path)!=length||fs::last_write_time(path)!=modified)throw std::runtime_error("Content changed during the check.");
 unsigned char out[32];CC_SHA256_Final(out,&context);const char *hex="0123456789abcdef";std::string result;for(unsigned char b:out){result+=hex[b>>4];result+=hex[b&15];}return result;
}
bool safe_relative(const fs::path &path){if(path.empty()||path.is_absolute())return false;for(const auto &part:path)if(part==".."||part==".")return false;std::string text=path.generic_string();for(unsigned char c:text)if(c<32||c>126)return false;return true;}
fs::path checked(const fs::path &root,const fs::path &relative){if(!safe_relative(relative))throw std::runtime_error("Invalid required content path.");fs::path path=root;for(const auto &part:relative){path/=part;if(fs::is_symlink(path))throw std::runtime_error("Linked content is unsupported.");}return path;}
void add_tree(std::set<std::string> &files,const fs::path &root,const fs::path &directory,bool configs=false){
 if(!fs::exists(directory))return;
 for(const auto &entry:fs::recursive_directory_iterator(directory)){
  if(files.size()>16000)throw std::runtime_error("Too many content files.");
  if(entry.is_symlink())throw std::runtime_error("Linked content is unsupported.");if(!entry.is_regular_file())continue;
  if(configs&&entry.path().extension()!=".cfg"&&entry.path().extension()!=".toml")continue;
  if(entry.path().filename()==".DS_Store")continue;auto relative=fs::relative(entry.path(),root);if(!safe_relative(relative))throw std::runtime_error("Content must be inside GameData.");files.insert(relative.generic_string());
 }
}
}
#endif
extern "C" void kfx_save_write_requirements(const char *destination,const CatalogueEntry *entry){
#ifdef __APPLE__
 std::string pending=std::string(destination)+".requirements.pending",output=std::string(destination)+".requirements";
 try{
  fs::path root=fs::absolute(keeper_runtime_directory);std::set<std::string> files;
  short group=get_current_campaign_fgroup();if(group==FGrp_None)group=FGrp_Campgn;
  fs::path descriptor=prepare_file_path(group,campaign.fname);auto campaign_relative=fs::relative(fs::absolute(descriptor),root);if(!safe_relative(campaign_relative)||!fs::is_regular_file(descriptor))throw std::runtime_error("Campaign content is unavailable.");files.insert(campaign_relative.generic_string());
  fs::path levels=fs::path(campaign.levels_location);if(!levels.is_absolute())levels=root/levels;auto level_relative=fs::relative(levels,root);if(!safe_relative(level_relative))throw std::runtime_error("Map must be inside GameData.");char prefix[32];snprintf(prefix,sizeof(prefix),"map%05d.",entry->level_num);
  if(fs::is_directory(levels))for(const auto &file:fs::directory_iterator(levels))if(file.path().filename().string().rfind(prefix,0)==0){auto name=fs::relative(file.path(),root);if(!safe_relative(name))throw std::runtime_error("Map must be inside GameData.");files.insert(name.generic_string());}
  add_tree(files,root,root/"fxdata",true);add_tree(files,root,root/"creatrs",true);add_tree(files,root,root/"mods");
  if(files.empty())throw std::runtime_error("Content is unavailable.");
  uint64_t budget=0;std::ostringstream text;text<<"KFXREQ2\nengine="<<version()<<"\ncampaign="<<campaign_relative.generic_string()<<"\nlevels="<<level_relative.generic_string()<<"\nmap="<<prefix<<"\n";
  for(const auto &name:files)text<<hash(checked(root,name),budget)<<" "<<name<<"\n";
  std::ofstream stream(pending,std::ios::binary);stream<<text.str();stream.close();if(!stream)throw std::runtime_error("Could not write requirements.");fs::rename(pending,output);
 }catch(...){std::error_code error;fs::remove(pending,error);/* Never attach stale requirements to a new save. */fs::remove(output,error);}
#else
 (void)destination;(void)entry;
#endif
}
extern "C" int kfx_save_check_requirements(const char *source,char *report,unsigned int size){
#ifdef __APPLE__
 auto result=[&](int code,const std::string &text){if(report&&size)snprintf(report,size,"%s",text.c_str());return code;};
 try{
  if(fs::is_symlink(source))return result(-1,"Linked saves cannot be loaded.");
  std::ifstream file(source,std::ios::binary);FileChunkHeader header={};CatalogueEntry entry={};file.read((char*)&header,sizeof(header));file.read((char*)&entry,sizeof(entry));
  if(!file||header.id!=SGC_InfoBlock||header.len!=sizeof(entry))return result(-1,"The saved-game header is damaged or unsupported.");
  std::string required=std::to_string(entry.game_ver_major)+"."+std::to_string(entry.game_ver_minor)+"."+std::to_string(entry.game_ver_release)+"."+std::to_string(entry.game_ver_build);
  if(required!="0.0.0.0"&&required!=version())return result(-1,"Requires engine "+required+". Current engine: "+version()+". Use the matching engine before loading.");
  fs::path requirements=std::string(source)+".requirements";
  if(!fs::exists(requirements))return result(0,"Engine: "+required+". Content requirements were not recorded for this save. A preserved copy will be made before loading.");
  if(fs::is_symlink(requirements)||fs::file_size(requirements)>4*1024*1024)return result(-1,"The save requirements file is invalid.");
  std::ifstream meta(requirements);std::string line;if(!std::getline(meta,line)||line!="KFXREQ2"||!std::getline(meta,line)||line!="engine="+version())return result(-1,"Unsupported save requirements version.");
  std::string descriptor,levels,prefix;
  if(!std::getline(meta,line)||line.rfind("campaign=",0))return result(-1,"Missing campaign requirement.");descriptor=line.substr(9);
  if(!std::getline(meta,line)||line.rfind("levels=",0))return result(-1,"Missing map requirement.");levels=line.substr(7);
  if(!std::getline(meta,line)||line.rfind("map=",0))return result(-1,"Missing map prefix.");prefix=line.substr(4);
  if(prefix.size()!=9||prefix.rfind("map",0)||prefix.back()!='.'||!std::all_of(prefix.begin()+3,prefix.end()-1,[](unsigned char c){return c>='0'&&c<='9';}))return result(-1,"Invalid map prefix.");
  size_t count=0;uint64_t budget=0;fs::path root=fs::absolute(keeper_runtime_directory);std::set<std::string> expected,recorded;checked(root,descriptor);auto map_directory=checked(root,levels);expected.insert(descriptor);
  add_tree(expected,root,root/"fxdata",true);add_tree(expected,root,root/"creatrs",true);add_tree(expected,root,root/"mods");
  if(fs::is_directory(map_directory))for(const auto &file:fs::directory_iterator(map_directory))if(file.path().filename().string().rfind(prefix,0)==0)expected.insert(fs::relative(file.path(),root).generic_string());

  while(std::getline(meta,line)){if(++count>16000||line.size()<66||line[64]!=' ')return result(-1,"Invalid content requirements.");std::string checksum=line.substr(0,64),name=line.substr(65);if(!std::all_of(checksum.begin(),checksum.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))return result(-1,"Invalid content checksum.");
   if(!recorded.insert(name).second)return result(-1,"Duplicate content requirement.");if(hash(checked(root,name),budget)!=checksum)return result(-1,"Required content differs: "+name+". Restore the matching campaign/mod profile first.");}
  if(!count)return result(-1,"No content requirements were recorded.");if(expected!=recorded)return result(-1,"Campaign, map or mod file list changed. Restore the matching content profile first.");return result(1,"Engine "+version()+" matches. Campaign, map, configuration and mod files match recorded checksums.");
 }catch(const std::exception &error){return result(-1,error.what());}
#else
 (void)source;if(report&&size)snprintf(report,size,"Compatibility checking is available in the macOS build.");return 0;
#endif
}
extern "C" int kfx_save_preserve_before_load(const char *source){
#ifdef __APPLE__
 try{uint64_t budget=0;std::string checksum=hash(source,budget);fs::path from=source,folder=from.parent_path()/"CompatibilityBackups";if(fs::is_symlink(folder))return 0;fs::create_directories(folder);fs::path backup=folder/(from.filename().string()+"."+checksum.substr(0,16));
  if(fs::exists(backup)){uint64_t verify_budget=0;return hash(backup,verify_budget)==checksum;}
  if(!fs::copy_file(from,backup,fs::copy_options::none))return 0;for(const char *suffix:{".requirements",".json",".png"})if(fs::is_regular_file(std::string(source)+suffix)&&!fs::is_symlink(std::string(source)+suffix))fs::copy_file(std::string(source)+suffix,backup.string()+suffix,fs::copy_options::none);return 1;
 }catch(...){return 0;}
#else
 (void)source;return 1;
#endif
}
