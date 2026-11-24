// Save preflight, restart-safe content profiles and non-network replay controls.
std::string new_profile,save_requirements;int requirements_state=0,profile_index=0;
std::vector<std::string> content_profiles;
std::string profiles_base(){const char *base=getenv("KFX_BASE_USER_DATA_DIR");return base&&*base?base:PlatformManager_GetUserPrefDir();}
bool profile_name_valid(const std::string &name){return !name.empty()&&name.size()<=32&&std::all_of(name.begin(),name.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
void scan_profiles(){
 content_profiles={"default"};std::string folder=profiles_base()+"/ContentProfiles";DIR *directory=opendir(folder.c_str());if(directory){for(dirent *entry;(entry=readdir(directory));){std::string name=entry->d_name;if(name=="default"||!profile_name_valid(name))continue;struct stat info;if(lstat((folder+"/"+name).c_str(),&info)==0&&S_ISDIR(info.st_mode))content_profiles.push_back(name);}closedir(directory);}std::sort(content_profiles.begin()+1,content_profiles.end());profile_index=std::clamp(profile_index,0,(int)content_profiles.size()-1);
}
bool profile_storage_safe(){struct stat info;std::string root=profiles_base()+"/ContentProfiles";return lstat(root.c_str(),&info)!=0?errno==ENOENT:S_ISDIR(info.st_mode);}
bool create_profile(){
 if(!profile_storage_safe()){notice="ContentProfiles must be a regular directory.";return false;}
 if(!profile_name_valid(new_profile)||new_profile=="default"){notice="Use 1-32 letters, numbers, underscores or hyphens.";return false;}
 std::string folder=profiles_base()+"/ContentProfiles/"+new_profile;
 struct stat info;if(lstat(folder.c_str(),&info)==0){notice="That profile already exists. Choose it from the list.";return false;}
 if((folder+"/GameData").size()>=sizeof(keeper_runtime_directory)){notice="The profile path is too long for this engine.";return false;}
 if(!SDL_CreateDirectory((folder+"/EnhancedData").c_str())||!SDL_CreateDirectory((folder+"/save").c_str())){notice="Could not create a separate profile.";return false;}
 scan_profiles();for(int i=0;i<(int)content_profiles.size();i++)if(content_profiles[i]==new_profile)profile_index=i;notice="Profile created. Add content in its Mod Folder, then choose Use on Next Launch.";return true;
}
void inspect_save(){char buffer[1024];requirements_state=kfx_save_check_requirements(selected_save().path.c_str(),buffer,sizeof(buffer));save_requirements=buffer;page=29;selected=requirements_state>=0?1:0;focus_action=-1;}
std::mutex replay_selection_lock;std::string replay_selection;bool replay_dialog_open=false;
void replay_file_selected(void *,const char *const *files,int){std::lock_guard<std::mutex> lock(replay_selection_lock);replay_selection=files&&files[0]?files[0]:"";replay_dialog_open=false;}
void play_tools_tick(){
 std::string file;{std::lock_guard<std::mutex> lock(replay_selection_lock);file.swap(replay_selection);}if(file.empty())return;
 if(network_is_active()||game.game_kind!=GKind_Unset){notice="Return to the main menu before opening another replay.";return;}
 if(file.size()>=sizeof(game.packet_fname)){notice="Replay path is too long. Copy it to a shorter local path.";return;}
 char report[512];if(!kfx_replay_validate_file(file.c_str(),report,sizeof(report))){notice=report;return;}
 close_panel();kfx_replay_request_explicit();snprintf(game.packet_fname,sizeof(game.packet_fname),"%s",file.c_str());game.packet_load_enable=true;game.packet_load_initialized=false;frontend_set_state(FeSt_PACKET_DEMO);
}
bool play_tools_action(int action){
 if(action==1200){page=27;selected=0;focus_action=-1;scan_profiles();notice.clear();return true;}
 if(action==1201||action==1202){if(!content_profiles.empty())profile_index=(profile_index+(action==1201?-1:1)+(int)content_profiles.size())%content_profiles.size();return true;}
 if(action>=1210&&action<=1212){int choice=(profile_index/3)*3+action-1210;if(choice<(int)content_profiles.size())profile_index=choice;return true;}
 if(action==1203){new_profile.clear();start_edit(11);notice="Name the new profile; Enter creates it. No existing files are changed.";return true;}
 if(action==1204){if(!profile_storage_safe()){notice="ContentProfiles must be a regular directory.";return true;}if(!content_profiles.empty()){std::string folder=content_profiles[profile_index]=="default"?profiles_base():profiles_base()+"/ContentProfiles/"+content_profiles[profile_index];struct stat info;if(lstat((folder+"/GameData").c_str(),&info)==0&&!S_ISDIR(info.st_mode)){notice="GameData must be a regular directory.";return true;}unlink((folder+"/GameData/.asset-version").c_str());}if(!content_profiles.empty())notice=write_small_file(profiles_base()+"/content-profile.cfg",content_profiles[profile_index]+"\n")?"Next launch: "+content_profiles[profile_index]+". Quit and reopen the app to switch safely.":"Could not save the next profile.";return true;}
 if(action==1205){if(!profile_storage_safe()){notice="ContentProfiles must be a regular directory.";return true;}if(content_profiles.empty())return true;std::string folder=content_profiles[profile_index]=="default"?profiles_base():profiles_base()+"/ContentProfiles/"+content_profiles[profile_index];folder+="/EnhancedData";SDL_CreateDirectory(folder.c_str());open_folder(folder);return true;}
 if(action==1206){profile_index=0;return play_tools_action(1204);}
 if(action==1306){if(network_is_active()||game.game_kind!=GKind_Unset){notice="Return to the main menu before opening another replay.";return true;}bool show=false;{std::lock_guard<std::mutex> lock(replay_selection_lock);if(!replay_dialog_open){replay_dialog_open=true;show=true;}}if(show){static const SDL_DialogFileFilter filters[]={{"KeeperFX replay","pck"}};SDL_ShowOpenFileDialog(replay_file_selected,nullptr,SDL_GetKeyboardFocus(),filters,1,nullptr,false);}return true;}
 if(action==1300){page=28;selected=0;focus_action=-1;notice.clear();return true;}
 if(action>=1301&&action<=1305){const int commands[]={0,-1,1,2,3};notice=kfx_replay_control(commands[action-1301])?"Replay controls updated.":"Controls apply only to completed local replays.";return true;}
 if(action==1400){if(!saves.empty())inspect_save();return true;}
 if(action==1402){notice=SDL_SetClipboardText(save_requirements.c_str())?"Requirements copied.":"Clipboard unavailable.";return true;}
 if(action==1401){if(requirements_state>=0&&!saves.empty())load_selected(true);return true;}
 return false;
}
void wrapped_tools_text(int x,int y,const std::string &value,int max_lines){std::istringstream input(value);std::string word,line;int lines=0;while(input>>word){if(line.size()+word.size()>44){text(x,y+lines++*29,line,20,556);line.clear();if(lines>=max_lines)return;}if(!line.empty())line+=' ';line+=word;}if(!line.empty()&&lines<max_lines)text(x,y+lines*29,line,20,556);}
void play_tools_draw(){
 if(page==27){const char *active=getenv("KFX_CONTENT_PROFILE");small_line(24,83,std::string("Current: ")+(active?active:"default"),592);int offset=profile_index/3*3;for(int i=0;i<3&&offset+i<(int)content_profiles.size();i++)button(24,114+i*46,592,content_profiles[offset+i],1210+i,profile_index==offset+i,38);
  button(24,257,184,"Previous",1201,selected==0,34);button(228,257,184,"Next",1202,selected==1,34);button(432,257,184,"Default",1206,selected==5,34);
  button(24,305,592,editing==11?"New: "+new_profile+"|":"Create Separate Profile",1203,selected==2||editing==11,38);
  button(24,354,284,"Use Next Launch",1204,selected==3,36);button(332,354,284,"Open Mod Folder",1205,selected==4,36);
 }else if(page==28){wrapped_tools_text(42,98,kfx_replay_status(),4);small_line(24,216,"F10 / Start: controls   Space: pause   [ ]: speed   B: mark",592);button(24,254,184,"Pause / Play",1301,selected==0,38);button(228,254,184,"Slower",1302,selected==1,38);button(432,254,184,"Faster",1303,selected==2,38);button(24,313,284,"Add Bookmark",1304,selected==3,38);button(332,313,284,"Next Bookmark",1305,selected==4,38);button(134,363,372,"Open Local Replay...",1306,selected==5,34);
 }else if(page==29){wrapped_tools_text(42,100,save_requirements,6);button(134,292,372,"Copy Requirements",1402,selected==0,36);if(requirements_state>=0)button(64,343,512,"Preserve Copy and Load",1401,selected==1,42);}
 if(page>=27&&page<=29&&!notice.empty())status_line(notice);
}
