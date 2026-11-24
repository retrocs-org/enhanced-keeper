// Native in-game libraries, recovery, setup and personal-data tools.
struct LocalReplay {std::string file,title,campaign,date;int level=0,players=0;unsigned long turns=0;bool favourite=false;};
std::vector<LocalReplay> replay_library;int replay_index=0,recovery_index=0;std::string replay_title;
bool reset_confirm=false,recovery_noticed=false;Uint64 healthy_after=0,capture_until=0;
SDL_Process *personal_process=nullptr;std::string personal_job;
std::vector<double> frame_samples;std::vector<std::string> capture_rows;Uint64 sample_previous=0;
std::string profile_file(const char *name){return data_folder()+"/"+name;}
void scan_replays(){
 replay_library.clear();namespace fs=std::filesystem;
 try{for(const fs::path &folder:{fs::path(data_folder())/"recordings",fs::path(keeper_runtime_directory)}){
  if(fs::is_symlink(folder)||!fs::is_directory(folder))continue;
  for(const auto &entry:fs::directory_iterator(folder)){
   if(replay_library.size()>=256)break;if(entry.is_symlink()||!entry.is_regular_file()||entry.path().extension()!=".pck")continue;
   LocalReplay item;item.file=entry.path().string();item.title=entry.path().stem().string();item.favourite=fs::is_regular_file(item.file+".favourite");
   FILE *f=fopen(item.file.c_str(),"rb");if(!f)continue;FileChunkHeader head={};CatalogueEntry info={};
   if(fread(&head,sizeof(head),1,f)==1&&head.id==SGC_InfoBlock&&head.len==sizeof(info)&&fread(&info,sizeof(info),1,f)==1){info.campaign_name[sizeof(info.campaign_name)-1]=0;item.campaign=info.campaign_name;item.level=info.level_num;}
   for(int chunk=0;chunk<16;chunk++){if(fread(&head,sizeof(head),1,f)!=1)break;if(head.id==SGC_PacketData){struct stat st;if(head.len==0&&fstat(fileno(f),&st)==0)item.turns=(st.st_size-ftell(f))/(PACKETS_COUNT*sizeof(Packet)+sizeof(TbBigChecksum));break;}if(head.id==SGC_PacketHeader&&head.len==sizeof(PacketSaveHead)){PacketSaveHead ph={};if(fread(&ph,sizeof(ph),1,f)!=1)break;item.players=__builtin_popcount((unsigned)ph.players_exist);}else if(head.len>512u*1024*1024||fseek(f,head.len,SEEK_CUR))break;}
   fclose(f);std::ifstream alias(item.file+".title");std::string title;if(std::getline(alias,title)&&!title.empty())item.title=title.substr(0,63);
   struct stat st;if(lstat(item.file.c_str(),&st)==0){char date[40];struct tm tm;localtime_r(&st.st_mtime,&tm);strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&tm);item.date=date;}replay_library.push_back(item);
  }}std::sort(replay_library.begin(),replay_library.end(),[](const LocalReplay &a,const LocalReplay &b){return a.favourite!=b.favourite?a.favourite>b.favourite:a.title<b.title;});
 }catch(...){notice="Some recording folders could not be read.";}
 replay_index=std::clamp(replay_index,0,std::max(0,(int)replay_library.size()-1));
}
bool run_personal_tool(const char *command,const char *argument=nullptr){
 if(personal_process){notice="A personal-data operation is still running.";return false;}
 const char *resources=getenv("KFX_APP_RESOURCES");if(!resources){notice="Use the packaged app for import and export.";return false;}
 std::string tool=std::string(resources)+"/../MacOS/KeeperFXE";const char *args[]={tool.c_str(),command,argument,nullptr};
 personal_process=SDL_CreateProcess(args,false);personal_job=command;notice=personal_process?"Working; keep the game open until completion.":"The data tool could not start.";return personal_process!=nullptr;
}
std::string replay_export_destination,replay_export_source,import_selection;
void import_folder_selected(void *,const char *const *files,int){std::lock_guard<std::mutex> lock(replay_selection_lock);import_selection=files&&files[0]?files[0]:"";}
void export_replay(void *,const char *const *files,int){
 // SDL delivers this callback off-thread; queue the destination only.
 std::lock_guard<std::mutex> lock(replay_selection_lock);replay_export_destination=files&&files[0]?files[0]:"";
}
void library_rename(){if(replay_library.empty()||replay_title.empty()){notice="Enter a replay title.";return;}notice=write_small_file(replay_library[replay_index].file+".title",replay_title+"\n")?"Replay title saved.":"Replay title could not be saved.";scan_replays();editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());}
void open_enhancement(int target){page=target;selected=0;focus_action=-1;notice.clear();if(target==30)scan_replays();if(target==35){scan_saves();recovery_index=0;}}
int enhancement_count(){return page==30?6:page==31?5:page==32?5:page==33?5:page==34?3:page==35?3:0;}
void enhancement_confirm(){const int library[]={1501,1502,1503,1504,1505,1506},tools[]={1511,1512,1513,1514,1515},setup[]={1521,1522,1523,1524,1525},comfort[]={1531,1532,1533,1534,1535},perf[]={1541,1542,1543},recover[]={1551,1552,1553};const int *commands=page==30?library:page==31?tools:page==32?setup:page==33?comfort:page==34?perf:recover;activate(commands[selected]);}
bool enhancement_action(int action){
 if(action==1500||action==1510||action==1520||action==1530||action==1540||action==1550){open_enhancement(action==1500?30:action==1510?31:action==1520?32:action==1530?33:action==1540?34:35);return true;}
 if(action==1501||action==1502){if(!replay_library.empty())replay_index=(replay_index+(action==1501?-1:1)+(int)replay_library.size())%replay_library.size();return true;}
 if(action>=1503&&action<=1506){if(replay_library.empty()){notice="Put .pck files in your profile's recordings folder.";return true;}auto &r=replay_library[replay_index];
  if(action==1503){std::lock_guard<std::mutex> lock(replay_selection_lock);replay_selection=r.file;}
  if(action==1504){replay_title=r.title;start_edit(12);}
  if(action==1505){if(r.favourite)unlink((r.file+".favourite").c_str());else write_small_file(r.file+".favourite","Favourite\n");scan_replays();}
  if(action==1506){replay_export_source=r.file;static const SDL_DialogFileFilter filter[]={{"KeeperFX replay","pck"}};SDL_ShowSaveFileDialog(export_replay,nullptr,SDL_GetKeyboardFocus(),filter,1,nullptr);}
  return true;
 }
 if(action==1511){run_personal_tool("--export-personal");return true;}
 if(action==1512){SDL_CreateDirectory((profiles_base()+"/Exports").c_str());open_folder(profiles_base()+"/Exports");return true;}
 if(action==1513){if(access(profile_file("reset-profile-next.cfg").c_str(),F_OK)==0){notice=unlink(profile_file("reset-profile-next.cfg").c_str())==0?"Scheduled reset cancelled.":"Could not cancel the scheduled reset.";return true;}reset_confirm=!reset_confirm;notice=reset_confirm?"Confirm Reset to move this profile's files to ProfileBackups on restart.":"Reset cancelled.";return true;}
 if(action==1514){if(!reset_confirm){notice="Choose Reset Profile first, then Confirm Reset.";return true;}notice=write_small_file(profile_file("reset-profile-next.cfg"),"reset\n")?"Reset scheduled. Quit and reopen. Files will be kept in ProfileBackups.":"Could not schedule reset.";reset_confirm=false;return true;}
 if(action==1515){open_folder("/Applications");notice="Quit the game, then move Keeper-Enhanced.app to Trash. Personal data stays in its data folder.";return true;}
 if(action==1521){open_panel(5);return true;}
 if(action==1522){open_panel(26);return true;}
 if(action==1523){SDL_ShowOpenFolderDialog(import_folder_selected,nullptr,SDL_GetKeyboardFocus(),nullptr,false);return true;}
 if(action==1524){if(write_small_file(profile_file("setup-complete.cfg"),"1\n")){close_panel();notice="Setup completed.";}else notice="Could not save setup status.";return true;}
 if(action==1525){SDL_CreateDirectory((data_folder()+"/EnhancedData/music").c_str());open_folder(data_folder()+"/EnhancedData/music");return true;}
 if(action>=1531&&action<=1535){if(action==1531)kfx_caption_scale=kfx_caption_scale>=2?1:kfx_caption_scale+.25f;if(action==1532)kfx_reduce_flashes=!kfx_reduce_flashes;if(action==1533)kfx_high_contrast=!kfx_high_contrast;if(action==1534)env_number("KFX_CAMERA_RESPONSE",kfx_camera_response()>=3?1:kfx_camera_response()+.5);if(action==1535)kfx_tooltip_scale=kfx_tooltip_scale>=1.5?1:kfx_tooltip_scale+.25;persist();return true;}
 if(action==1541){performance_overlay=!performance_overlay;persist();return true;}
 if(action==1542){frame_samples.clear();capture_rows.clear();sample_previous=0;capture_until=SDL_GetTicks()+15000;notice="Capturing 15 seconds. Close the menu and play normally.";return true;}
 if(action==1543){open_folder(path(""));return true;}
 if(action==1551||action==1552){int count=0;for(auto &s:all_saves)if(s.backup)count++;if(count)recovery_index=(recovery_index+(action==1551?-1:1)+count)%count;return true;}
 if(action==1553){int i=0;for(const auto &entry:all_saves)if(entry.backup&&i++==recovery_index){for(int n=0;n<(int)saves.size();n++)if(saves[n].slot==entry.slot){save_index=n;backup_choice=entry.backup;inspect_save();return true;}}notice="No recoverable backup is selected.";return true;}
 return false;
}
void enhancement_tick(){
 if(!recovery_noticed){recovery_noticed=true;if(access(profile_file("display-recovered.txt").c_str(),F_OK)==0){save_toast="Safe windowed display restored. See Display Options.";save_toast_until=SDL_GetTicks()+10000;unlink(profile_file("display-recovered.txt").c_str());}}
 if(!healthy_after)healthy_after=SDL_GetTicks()+3000;
 if(SDL_GetTicks()>=healthy_after&&!display_confirmation&&!pending_display){unlink(profile_file("display-startup.pending").c_str());healthy_after=UINT64_MAX;}
 if(personal_process){int code=0;if(SDL_WaitProcess(personal_process,false,&code)){SDL_DestroyProcess(personal_process);personal_process=nullptr;notice=code==0?(personal_job=="--import"?"Original data imported. Restart to prepare it safely.":"Export completed in the Exports folder."):"Data operation failed. Existing files were retained.";}}
 std::string destination,import;{std::lock_guard<std::mutex> lock(replay_selection_lock);destination.swap(replay_export_destination);import.swap(import_selection);}
 if(!destination.empty())notice=copy_exclusive(replay_export_source,destination)?"Replay exported.":"Export failed; choose a new filename. Existing files were kept.";
 if(!import.empty())run_personal_tool("--import",import.c_str());
}
unsigned long performance_ping(){if(!network_is_active())return 0;if(!network_is_host())return GetPing(SERVER_ID);unsigned long result=0;for(int i=1;i<MAX_NET_USERS;i++)result=std::max(result,GetPing(i));return result;}
void enhancement_sample(){
 Uint64 now=SDL_GetTicks();if(sample_previous&&now>sample_previous&&(performance_overlay||capture_until||page==34)){frame_samples.push_back(now-sample_previous);if(frame_samples.size()>4096)frame_samples.erase(frame_samples.begin());if(capture_until&&capture_rows.size()<4096){std::ostringstream row;row<<(now-sample_previous)<<","<<frametime_measurements.frametime_current[Frametime_Logic]<<","<<frametime_measurements.frametime_current[Frametime_Draw]<<","<<performance_ping();capture_rows.push_back(row.str());}}sample_previous=now;
 if(capture_until&&now>=capture_until){capture_until=0;std::ostringstream report;report<<"frame_ms,logic_ms,draw_ms,ping_ms\n";for(auto &value:capture_rows)report<<value<<"\n";notice=write_small_file(path("performance-capture.csv"),report.str())?"Performance capture saved.":"Performance capture could not be written.";}
}
void enhancement_draw(){
 if(page==30){if(replay_library.empty())wrapped_tools_text(42,105,"No local recordings. Put .pck recordings in the profile recordings folder, or use Open Local Replay.",4);else{const auto &r=replay_library[replay_index];centered(100,(r.favourite?"* ":"")+(editing==12?replay_title+"|":r.title),24);small_line(24,145,r.campaign+" / Map "+std::to_string(r.level),592);small_line(24,204,r.date,592);small_line(24,178,std::to_string(r.turns)+" turns / "+std::to_string(r.players)+" players / "+std::to_string(replay_index+1)+" of "+std::to_string(replay_library.size()),592);}const char *labels[]={"Previous","Next","Play Replay","Rename","Favourite","Export Replay"};for(int i=0;i<6;i++)button(i%2?332:24,232+(i/2)*54,284,labels[i],1501+i,selected==i,38);
 }else if(page==31){small_line(24,92,"Export excludes imported originals and account tokens.",592);const char *labels[]={"Export Personal Data","Open Exports Folder",access(profile_file("reset-profile-next.cfg").c_str(),F_OK)==0?"Cancel Scheduled Reset":reset_confirm?"Cancel Reset":"Reset Profile on Restart","Confirm Reset","Show App in Finder"};for(int i=0;i<5;i++)button(64,132+i*51,512,labels[i],1511+i,selected==i,38);
 }else if(page==32){int music=0;try{for(const auto &entry:std::filesystem::directory_iterator(std::string(keeper_runtime_directory)+"/music"))if(entry.is_regular_file())music++;}catch(...){}small_line(24,88,"Music files detected: "+std::to_string(music)+". Changes apply after restart.",592);const char *labels[]={"Display and Fullscreen","Test and Calibrate Controller","Import Original Game Data","Finish Setup","Open Music Folder"};for(int i=0;i<5;i++)button(64,127+i*51,512,labels[i],1521+i,selected==i,38);
 }else if(page==33){const std::string labels[]={"Caption size: "+std::to_string((int)(100*kfx_caption_scale))+"%",std::string("Reduced damage flashes: ")+(kfx_reduce_flashes?"On":"Off"),std::string("Contrast and focus marker: ")+(kfx_high_contrast?"On":"Off"),"Stick response curve: "+std::to_string(kfx_camera_response()).substr(0,3),"Tooltip size: "+std::to_string((int)(100*kfx_tooltip_scale))+"%"};for(int i=0;i<5;i++)button(50,108+i*57,540,labels[i],1531+i,selected==i,40);
 }else if(page==34){double ms=frame_samples.empty()?0:frame_samples.back();small_line(24,93,"Frame: "+std::to_string(ms).substr(0,5)+" ms / "+(ms>0?std::to_string((int)(1000/ms)):"?")+" FPS",592);int x=24;for(int i=std::max(0,(int)frame_samples.size()-148);i<(int)frame_samples.size();i++){int h=std::min(96,(int)frame_samples[i]);rect(x,230-h,3,h,224,200,140);x+=4;}button(64,257,512,performance_overlay?"Overlay: On":"Overlay: Off",1541,selected==0,38);button(64,309,512,capture_until?"Capturing...":"Capture 15 Seconds",1542,selected==1,38);button(64,361,512,"Open Capture Folder",1543,selected==2,38);
 }else if(page==35){int index=0,count=0;for(const Save &entry:all_saves)if(entry.backup){if(index++==recovery_index){centered(96,short_text(entry.title,36),24);small_line(24,140,entry.campaign+" / Map "+std::to_string(entry.level),592);small_line(24,176,entry.date+" / Backup "+std::to_string(entry.backup),592);SDL_Surface *image=IMG_Load((entry.path+".png").c_str());if(image){SDL_Rect to={215,215,210,100};SDL_BlitSurfaceScaled(image,nullptr,canvas,&to,SDL_SCALEMODE_LINEAR);SDL_DestroySurface(image);}}count++;}if(!count)centered(160,"No rotating save backups found.",22);button(24,330,284,"Previous Backup",1551,selected==0,38);button(332,330,284,"Next Backup",1552,selected==1,38);button(64,380,512,"Check and Restore to Free Slot",1553,selected==2,34);}
 if(page>=30&&!notice.empty())status_line(notice);
}

void performance_draw(){
 if(!performance_overlay||!winfont||frame_samples.empty())return;
 int flags=RendererGetDrawFlags(),width=std::min(360,(int)RendererScreenWidth()-16);if(width<120)return;
 LbDrawBox(8,8,width,100,0);LbTextSetFont(winfont);LbTextSetWindow(14,12,width-12,48);RendererSetDrawFlags(0);
 std::ostringstream line;double ms=frame_samples.back();line<<(ms>0?(int)(1000/ms):0)<<" FPS | "<<(int)ms<<" ms | logic "<<(int)frametime_measurements.frametime_current[Frametime_Logic]<<" ms | ping "<<performance_ping()<<" ms";
 LbTextDrawResized(0,0,16,line.str().c_str());int x=14;for(int i=std::max(0,(int)frame_samples.size()-(width-12)/3);i<(int)frame_samples.size();i++){int h=std::min(42,(int)frame_samples[i]);LbDrawBox(x,100-h,2,h,15);x+=3;}RendererSetDrawFlags(flags);
}

void recovery_status_draw(){
 const char *status=kfx_network_recovery_status();if(!status[0]||!winfont)return;
 int width=RendererScreenWidth(),flags=RendererGetDrawFlags();LbDrawBox(width/8,112,width*3/4,44,0);LbTextSetFont(winfont);LbTextSetWindow(width/8+8,118,width*3/4-16,32);RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);LbTextDrawResized(0,0,20,status);RendererSetDrawFlags(flags);
}
