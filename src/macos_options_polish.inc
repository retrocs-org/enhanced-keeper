// Integrated feedback and diagnostics; shares the original-menu canvas and input ownership.
bool menu_point(float mx,float my,int w,int h,SDL_Point &point){
    if(w<=0||h<=0)return false;double scale=std::min(w/(double)WIDTH,h/(double)HEIGHT);
    point={(int)((mx-(w-WIDTH*scale)/2)/scale),(int)((my-(h-HEIGHT*scale)/2)/scale)};
    return point.x>=0&&point.y>=0&&point.x<WIDTH&&point.y<HEIGHT;
}
int button_animation_frame(Uint64 elapsed){static const int frames[]={0,1,2,3,4,3,2,1};return frames[(elapsed/100)&7];}
void update_hover(){
    int previous=hovered_action;hovered_action=-1;if(!mouse_navigation)return;SDL_Window *window=SDL_GetMouseFocus();if(!window)return;
    int w=0,h=0;SDL_GetWindowSize(window,&w,&h);float x=0,y=0;SDL_GetMouseState(&x,&y);SDL_Point point;
    if(!menu_point(x,y,w,h,point))return;
    for(const Hit &hit:hits)if(SDL_PointInRect(&point,&hit.rect)){hovered_action=hit.action;break;}
    if(hovered_action!=previous)focus_animation.erase(hovered_action);
}
std::string data_folder(){std::string folder=path("");while(!folder.empty()&&folder.back()=='/')folder.pop_back();return folder.substr(0,folder.find_last_of('/'));}
std::string file_url(const std::string &folder){
    std::string result="file://";const char hex[]="0123456789ABCDEF";
    for(unsigned char c:folder){if(std::isalnum(c)||c=='/'||c=='-'||c=='_'||c=='.'||c=='~')result+=c;else{result+='%';result+=hex[c>>4];result+=hex[c&15];}}return result;
}
void open_folder(const std::string &folder){notice=SDL_OpenURL(file_url(folder).c_str())?"Folder opened in Finder.":"Could not open Finder. See the data folder in the report.";}
std::string build_field(const char *key){
    const char *resources=getenv("KFX_APP_RESOURCES");if(!resources)return "development";
    size_t length=0;void *bytes=SDL_LoadFile((std::string(resources)+"/build-info.json").c_str(),&length);if(!bytes)return "development";
    std::string data((char*)bytes,length);SDL_free(bytes);std::string field=std::string("\"")+key+"\"";size_t pos=data.find(field);
    if(pos==std::string::npos)return "development";pos=data.find(':',pos+field.size());if(pos==std::string::npos)return "development";pos=data.find('"',pos+1);if(pos==std::string::npos)return "development";size_t end=data.find('"',pos+1);return end==std::string::npos?"development":data.substr(pos+1,end-pos-1);
}
std::string diagnostic_report(){
    std::string last_issue;size_t size=0;void *bytes=SDL_LoadFile(path("network-diagnostics.json").c_str(),&size);if(bytes){last_issue.assign((char*)bytes,std::min(size,(size_t)2048));SDL_free(bytes);}

    return "KeeperFX-E "+build_field("appVersion")+" (build "+build_field("appBuild")+")\nEngine: "+VER_STRING+"\nPlatform: "+SDL_GetPlatform()+"\nSDL: "+std::to_string(SDL_MAJOR_VERSION)+"."+std::to_string(SDL_MINOR_VERSION)+"."+std::to_string(SDL_MICRO_VERSION)+"\nController: "+controller_device_name()+"\nButton labels: "+(kfx_controller_guide_ps5?"DualSense":"Xbox")+"\nDisplay: "+std::to_string(RendererScreenWidth())+" x "+std::to_string(RendererScreenHeight())+"\nMultiplayer: "+(network_is_active()?"connected":"not connected")+"\nHost address: "+join_ip+"\nData folder: "+data_folder()+"\n\nLast network issue: "+last_issue+"\n"+network_report;
}
void copy_report(){notice=SDL_SetClipboardText(diagnostic_report().c_str())?"Report copied. Paste it into your message.":"Clipboard unavailable. Try again with the game focused.";}
bool persist_endpoint(const std::string &endpoint){
    std::string destination=path("network-address.cfg"),temporary=destination+".pending";FILE *file=fopen(temporary.c_str(),"w");if(!file)return false;
    bool ok=fprintf(file,"%s\n",endpoint.c_str())>0;ok=fflush(file)==0&&fsync(fileno(file))==0&&ok;if(fclose(file)!=0)ok=false;
    if(ok&&rename(temporary.c_str(),destination.c_str())==0)return true;unlink(temporary.c_str());return false;
}
void load_network_profile(){
    FILE *file=fopen(path("network-address.cfg").c_str(),"r");if(!file)return;char line[80];std::string endpoint;
    if(fgets(line,sizeof(line),file)){std::string input=line;while(!input.empty()&&(input.back()=='\r'||input.back()=='\n'))input.pop_back();if(valid_endpoint(input,endpoint))join_ip=endpoint;}fclose(file);
}
void finish_edit(){
    if(editing==11){if(!create_profile())return;editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());return;}
    if(editing>=6){editing=0;browser_filter();SDL_StopTextInput(SDL_GetKeyboardFocus());return;}
    if(editing==5){if(saves.empty()||save_alias.empty()){notice="Enter a name for the saved game.";return;}std::string base=save_base(selected_save());if(!write_small_file(base+".label",save_alias)){notice="Could not rename this saved game.";return;}refresh_save(base);}
    if(editing==3){std::string endpoint;if(!valid_endpoint(join_ip,endpoint)){notice="Use an IPv4 address and a port between 1 and 65535.";return;}join_ip=endpoint;if(!persist_endpoint(endpoint)){notice="Host address could not be remembered. Check save-folder permissions.";return;}}
    if(editing==4){if(network_name.empty()||network_name.find_first_not_of(' ')==std::string::npos){notice="Enter a player name.";return;}snprintf(tmp_net_player_name,sizeof(tmp_net_player_name),"%s",network_name.c_str());frontnet_session_set_player_name(nullptr);}
    editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());notice="Changes saved.";
}
bool polish_action(int action){
    if(action==800){page=13;selected=0;focus_action=-1;notice.clear();return true;}
    if(action==810){open_folder(data_folder());return true;}
    if(action==811){open_folder(path(""));return true;}
    if(action==812){page=2;scan_saves();selected=0;focus_action=-1;return true;}
    if(action==813||action==815){copy_report();return true;}
    if(action==814){help_parent=page==13?13:2;page=14;selected=0;focus_action=-1;notice.clear();return true;}
    if(action==611){if(!net_player_name[0])net_load_config_file();network_name=net_player_name[0]?net_player_name:net_config_info.net_player_name;start_edit(4);notice="Type a player name. Enter saves it.";return true;}
    return false;
}
std::string autosave_status(){
    for(const Save &entry:all_saves)if(automatic_save(entry))last_autosave=std::max(last_autosave,entry.modified);
    if(!last_autosave)return "No automatic save yet";struct tm local={};localtime_r(&last_autosave,&local);char value[64];strftime(value,sizeof(value),"Last autosave: %d %b %H:%M",&local);return value;
}
void save_feedback_draw(){
    if(save_toast.empty()||SDL_GetTicks()>=save_toast_until||!winfont)return;
    int width=RendererScreenWidth(),flags=RendererGetDrawFlags();int left=width/4;
    LbDrawBox(left,96,width-left-12,48,0);LbTextSetFont(winfont);LbTextSetWindow(left+10,101,width-left-32,42);RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);
    int units=std::max(16,(int)(22*kfx_text_scale*16/std::max(1,(int)LbTextLineHeight())));LbTextDrawResized(0,0,units,save_toast.c_str());RendererSetDrawFlags(flags);
}
void small_line(int x,int y,const std::string &value,int width){
    int size=std::min(20,std::max(18,(int)round(18*kfx_text_scale)));std::string shown=value;SDL_Surface *image=label(shown,size);
    while(image&&image->w>width&&shown.size()>4){shown=short_text(value,shown.size()-1);image=label(shown,size);}if(image){SDL_Rect to={x,y,image->w,image->h};SDL_BlitSurface(image,nullptr,canvas,&to);}
}
void status_line(const std::string &value){small_line(24,401,value,592);}
void polish_draw_page(){
    if(page==13){
        frame(24,90,592,116);text(42,100,"KeeperFX-E "+build_field("appVersion"),24,556);text(42,137,"Build "+build_field("appBuild")+" / Engine "+VER_STRING,20,556);text(42,173,controller_device_name(),20,556);
        button(134,214,372,"Open Logs Folder",810,selected==0,40);button(134,262,372,"Open Save Folder",811,selected==1,40);button(134,310,372,"Copy Diagnostics",813,selected==2,40);button(134,358,372,"Saving Help",814,selected==3,40);
    }else if(page==14){
        frame(24,90,592,214);text(42,102,save_error.empty()?"Save recovery":save_error,20,548);text(42,146,"1. Check free disk space.",22,548);text(42,184,"2. Check save-folder permissions.",22,548);text(42,222,"3. Open a save and choose a backup.",22,548);text(42,266,"Restoring keeps the original backup.",20,548);
        button(134,312,372,"Open Save Folder",811,selected==0,40);button(134,360,372,"Browse Saved Games",812,selected==1,40);
    }
}
