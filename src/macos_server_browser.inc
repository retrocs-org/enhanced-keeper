// Integrated community browser. Service I/O is asynchronous; LAN/direct play is independent.
struct HubDocument{VALUE root;HubDocument(const std::string &s){value_init_null(&root);json_dom_parse(s.data(),s.size(),nullptr,0,&root,nullptr);}~HubDocument(){value_fini(&root);}};
std::string hub_field(const VALUE *root,const char *key){const VALUE *v=root?value_dict_get(root,key):nullptr;return v&&value_type(v)==VALUE_STRING?value_string(v):"";}
int hub_integer(const VALUE *root,const char *key){const VALUE *v=root?value_dict_get(root,key):nullptr;return v&&(value_type(v)==VALUE_INT32||value_type(v)==VALUE_UINT32)?value_int32(v):0;}
std::string hub_quote(const std::string &s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';if(c>=32)out+=c;}return out+'"';}
struct BrowserServer{std::string id,name,address,engine,map,kind,region,content,status,ai_style;int players=0,capacity=4;bool password=false,voting=false;std::vector<std::string> clients;};
BrowserServer browser_server_from_json(const VALUE *r)
{
    BrowserServer s;
    s.id=hub_field(r,"id");
    s.name=hub_field(r,"name");
    s.address=hub_field(r,"endpoint");
    s.engine=hub_field(r,"engine");
    s.map=hub_field(r,"map");
    s.content=hub_field(r,"mods");
    if(s.content.size()!=64||!std::all_of(s.content.begin(),s.content.end(),
        [](unsigned char c){return std::isxdigit(c);}))s.content.clear();
    s.kind=hub_field(r,"kind");
    s.status=hub_field(r,"status");
    s.ai_style=hub_field(r,"ai_style");
    s.region=hub_field(r,"region");
    s.players=hub_integer(r,"players");
    s.capacity=hub_integer(r,"capacity");
    s.password=hub_integer(r,"password")!=0;
    s.voting=hub_integer(r,"voting")!=0;
    const VALUE *clients=value_dict_get(r,"client_versions");
    if(clients&&value_type(clients)==VALUE_ARRAY){
        for(size_t index=0;index<std::min<size_t>(16,value_array_size(clients));++index){
            const VALUE *client=value_array_get(clients,index);
            if(client&&value_type(client)==VALUE_STRING)s.clients.emplace_back(value_string(client));
        }
    }
    return s;
}
bool dedicated_listing(const BrowserServer &server){return server.kind=="dedicated"||server.kind=="community";}
bool supported_server_version(const BrowserServer &server){
 if(!server.clients.empty())return std::find(server.clients.begin(),server.clients.end(),KFX_APP_VERSION)!=server.clients.end();
 return server.kind!="listen"||server.engine.empty()||normalized_version(server.engine)==normalized_version(VER_STRING);
}
std::string missing_server_assets(){
 std::string missing;
 int count=0;
 for(const char *archive:required_sprite_zips){
  if(access(prepare_file_path(FGrp_FxData,archive),R_OK)==0)continue;
  if(count++==0)missing=archive;
 }
 if(count>1)missing+=" and "+std::to_string(count-1)+" more";
 return missing;
}
std::vector<BrowserServer> browser_servers,public_servers,favourite_servers,recent_servers;
std::map<std::string,int> browser_pings;int browser_sort=0;
int browser_tab=0,browser_index=0,community_tab=0,community_offset=0;bool browser_compatible=false;
std::string browser_search,hub_edit,host_title="My Dungeon",host_address="127.0.0.1:5556",host_map,device_code,pair_code,server_password,hub_message="Guest play is available.",browser_join_content,community_text="Choose a community section.";Uint64 pair_at=0,pair_deadline=0;
std::string browser_store(const BrowserServer &s){return s.address+"\t"+s.name+"\t"+s.engine+"\t"+s.id+"\n";}
bool browser_save(const char *file,const std::vector<BrowserServer> &rows){std::string body;for(size_t i=0;i<std::min(rows.size(),(size_t)32);i++)body+=browser_store(rows[i]);return write_small_file(path(file),body);}
void browser_load(const char *file,std::vector<BrowserServer> &rows){FILE *f=fopen(path(file).c_str(),"r");if(!f)return;rows.clear();char line[512];while(rows.size()<32&&fgets(line,sizeof(line),f)){std::string s=line;size_t a=s.find('\t'),b=s.find('\t',a+1);if(a==std::string::npos||b==std::string::npos)continue;BrowserServer r;r.address=s.substr(0,a);r.name=s.substr(a+1,b-a-1);size_t extra=s.find('\t',b+1);r.engine=s.substr(b+1,extra==std::string::npos?std::string::npos:extra-b-1);if(extra!=std::string::npos){r.id=s.substr(extra+1);while(!r.id.empty()&&(r.id.back()=='\n'||r.id.back()=='\r'))r.id.pop_back();}while(!r.engine.empty()&&(r.engine.back()=='\n'||r.engine.back()=='\r'))r.engine.pop_back();r.kind="saved";rows.push_back(r);}fclose(f);}
void browser_filter(bool preserve_selection){
 const auto previous=preserve_selection&&browser_index>=0&&browser_index<(int)browser_servers.size()
    ? browser_servers[browser_index] : BrowserServer{};

 browser_servers=browser_tab==0?public_servers:browser_tab==2?favourite_servers:browser_tab==3?recent_servers:std::vector<BrowserServer>{};
 if(browser_tab==1){lan_refresh_sessions();for(int i=0;i<lan_session_count&&i<LAN_SESSIONS_MAX;i++){BrowserServer s;s.name=lan_sessions[i].text;s.address=lan_sessions[i].join_address;if(s.address.rfind("LAN:",0)==0)s.address.erase(0,4);s.kind="LAN";browser_servers.push_back(s);}}
 std::string q=browser_search;std::transform(q.begin(),q.end(),q.begin(),[](unsigned char c){return std::tolower(c);});browser_servers.erase(std::remove_if(browser_servers.begin(),browser_servers.end(),[&](const BrowserServer &s){std::string text=s.name+" "+s.map+" "+s.region;std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::tolower(c);});return text.find(q)==std::string::npos||(browser_compatible&&!supported_server_version(s));}),browser_servers.end());std::sort(browser_servers.begin(),browser_servers.end(),[](const BrowserServer &a,const BrowserServer &b){if(browser_sort==1){int x=browser_pings.count(a.address)&&browser_pings[a.address]>=0?browser_pings[a.address]:999999,y=browser_pings.count(b.address)&&browser_pings[b.address]>=0?browser_pings[b.address]:999999;if(x!=y)return x<y;}else if(browser_sort==2&&a.players!=b.players)return a.players>b.players;else if(browser_sort==3&&a.map!=b.map)return a.map<b.map;return a.name<b.name;});browser_index=std::clamp(browser_index,0,std::max(0,(int)browser_servers.size()-1));
 if(!previous.address.empty()){
    const auto match=std::find_if(browser_servers.begin(),browser_servers.end(),[&](const BrowserServer &server){
        return previous.id.empty()?server.address==previous.address:server.id==previous.id;
    });
    if(match!=browser_servers.end())browser_index=static_cast<int>(match-browser_servers.begin());
    else if(page==21){page=20;selected=0;}
 }
 for(int i=(browser_index/3)*3;i<std::min((int)browser_servers.size(),(browser_index/3)*3+3);i++)if(!browser_pings.count(browser_servers[i].address)&&kfx_hub_probe(browser_servers[i].address.c_str()))browser_pings[browser_servers[i].address]=-2;
}
void browser_refresh(){if(browser_tab==0||browser_tab==2||browser_tab==3){if(!kfx_hub_request("servers","/api/servers",nullptr,0))hub_message="A community request is still running.";else hub_message="Refreshing directory...";}else{browser_filter(true);hub_message=browser_servers.empty()?"No servers found in this tab.":"Server list updated.";}}
void browser_initialize(){kfx_hub_initialize(path("").c_str());browser_load("servers-favourites.cfg",favourite_servers);browser_load("servers-recent.cfg",recent_servers);size_t length=0;char *url=(char*)SDL_LoadFile(path("community-url.cfg").c_str(),&length);if(url){std::string value(url,length);SDL_free(url);kfx_hub_set_url(value.c_str());}}
std::string settings_account, synced_settings, uploading_settings, incoming_settings;
int settings_revision=-1, incoming_revision=-1;
Uint64 next_settings_sync=0;
std::string settings_status="Checking cloud settings";
bool settings_conflict=false;
int cloud_account_options_count(){return settings_conflict?2:5;}
std::string read_profile_file(const char *filename){
 size_t length=0;
 char *data=static_cast<char *>(SDL_LoadFile(path(filename).c_str(),&length));
 if(!data)return {};
 std::string result;
 if(length<=16384)result.assign(data,length);
 SDL_free(data);
 return result;
}
std::string quote_settings(const std::string &value){
 std::string escaped;
 for(char ch:value){
  if(ch=='\n')escaped+="\\n";
  else if(ch=='\r')escaped+="\\r";
  else if(ch=='\t')escaped+="\\t";
  else {if(ch=='\\'||ch=='"')escaped+='\\';escaped+=ch;}
 }
 return '"'+escaped+'"';
}
bool record_synced_settings(int revision,const std::string &snapshot){
 if(!write_small_file(path("cloud-settings-baseline.toml"),snapshot)||
    !write_small_file(path("cloud-settings-revision.cfg"),std::to_string(revision)))return false;
 settings_revision=revision;
 synced_settings=snapshot;
 return true;
}
bool apply_cloud_settings(){
 const auto current=read_profile_file("settings.toml");
 if(incoming_settings!=current){
  if(!current.empty()&&!write_small_file(path("settings-before-cloud-sync.toml"),current))return false;
  if(!write_small_file(path("settings.toml"),incoming_settings))return false;
  load_settings();
 }
 return record_synced_settings(incoming_revision,incoming_settings);
}
void sync_cloud_settings(){
 if(settings_account.empty()||settings_account!=kfx_hub_account_id()||settings_conflict)return;
 if(incoming_revision>=0){
  const std::string local=read_profile_file("settings.toml");
  const std::string baseline=read_profile_file("cloud-settings-baseline.toml");
  const std::string cached_revision=read_profile_file("cloud-settings-revision.cfg");
  const bool cloud_changed=cached_revision!=std::to_string(incoming_revision);
  const bool local_changed=!cached_revision.empty()&&!local.empty()&&baseline!=local;
  if(cloud_changed&&!incoming_settings.empty()&&local!=incoming_settings){
   if(local_changed){
    settings_conflict=true;
    if(page==22)selected=0;
    settings_status="Sync conflict";
    return;
   }
   if(game.game_kind!=GKind_Unset||network_is_active())return;
   if(!apply_cloud_settings()){settings_status="Could not save cloud settings";return;}
  }else if(!record_synced_settings(incoming_revision,incoming_settings)){
   settings_status="Could not save sync status";
   return;
  }
  incoming_revision=-1;
  settings_status="Settings synced";
 }
 if(kfx_hub_busy()||SDL_GetTicks()<next_settings_sync)return;
 const std::string current=read_profile_file("settings.toml");
 if(settings_revision<0||current.empty()||current==synced_settings){
  next_settings_sync=SDL_GetTicks()+15000;
  kfx_hub_request("settings-read","/api/cloud/settings",nullptr,1);
  return;
 }
 next_settings_sync=SDL_GetTicks()+5000;
 const std::string body="{\"revision\":"+std::to_string(settings_revision)+",\"settings\":"+quote_settings(current)+"}";
 if(kfx_hub_request("settings-write","/api/cloud/settings",body.c_str(),1)){
  uploading_settings=current;
  settings_status="Syncing settings";
 }
}
bool cloud_profile_id(const std::string &id)
{
    return id.size()==32&&std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isxdigit(c);});
}
struct StartupProfile {
    std::string id,name;
    bool cloud;
    std::string date,campaign;
    int avatar=0;
};
std::string active_profile_name;
StartupProfile active_startup_profile{};
bool profile_game_recorded=false;
enum class PickerSession { Offline, NeedsSignIn, Checking, SignedIn };
PickerSession picker_session=PickerSession::Offline;
std::string picker_session_id;


std::vector<StartupProfile> startup_profiles;
int startup_profile_offset=0;
enum class ProfilePickerDialog { None, Delete };
ProfilePickerDialog profile_picker_dialog=ProfilePickerDialog::None;
StartupProfile managed_picker_profile{};
bool picked_startup_cloud=false;

std::string profile_folder(const StartupProfile &profile)
{
    return profiles_base()+(profile.cloud?"/CloudProfiles/":"/ContentProfiles/")+profile.id;
}

std::string read_profile_label(const std::string &folder,const std::string &fallback)
{
    const auto file=folder+"/profile-name";
    struct stat info{};
    if(lstat(file.c_str(),&info)!=0||!S_ISREG(info.st_mode)||info.st_size>64)return fallback;
    std::ifstream input(file);
    std::string name;
    if(!std::getline(input,name))return fallback;
    if(!name.empty()&&name.back()=='\r')name.pop_back();
    return profile_name_valid(name)?name:fallback;
}

void load_picker_metadata(StartupProfile &profile)
{
    const auto file=profile_folder(profile)+"/profile-summary";
    struct stat info{};
    if(lstat(file.c_str(),&info)!=0||!S_ISREG(info.st_mode)||info.st_size>1024)return;
    std::ifstream input(file);
    std::string line;
    if(std::getline(input,line)&&line.size()<=24)profile.date=line;
    if(std::getline(input,line)&&line.size()<=80)profile.campaign=line;
    if(input>>profile.avatar)profile.avatar=std::clamp(profile.avatar,0,15);
}

void remember_profile(const StartupProfile &profile,bool playing=false)
{
    active_profile_name=profile.name;
    if(!playing)profile_game_recorded=false;
    write_small_file(profiles_base()+"/last-player-profile",(profile.cloud?"cloud ":"local ")+profile.id+"\n");
    auto summary=profile;
    const time_t now=time(nullptr);
    struct tm local{};
    char date[24]={};
    if(localtime_r(&now,&local))strftime(date,sizeof(date),"%Y-%m-%d",&local);
    if(playing){
        summary.date=date;
        if(campaign.name[0])summary.campaign=campaign.name;
    }else if(!all_saves.empty()){
        if(summary.date.empty())summary.date=all_saves.front().date.substr(0,10);
        if(summary.campaign.empty())summary.campaign=all_saves.front().campaign;
    }
    summary.campaign.erase(std::remove_if(summary.campaign.begin(),summary.campaign.end(),
        [](unsigned char c){return c<32||c==127;}),summary.campaign.end());
    active_startup_profile=summary;
    write_small_file(profile_folder(profile)+"/profile-summary",
        summary.date+"\n"+short_text(summary.campaign,80)+"\n"+std::to_string(summary.avatar)+"\n");
}

void record_local_profile(const std::string &name)
{
    StartupProfile profile{name,read_profile_label(profiles_base()+"/ContentProfiles/"+name,name),false,{},{},0};
    load_picker_metadata(profile);
    remember_profile(profile);
    snprintf(net_player_name,sizeof(net_player_name),"%s",profile.name.c_str());
}

void scan_startup_profiles()
{
    startup_profiles.clear();
    scan_profiles();
    if(profile_storage_safe())for(const auto &profile:content_profiles)
        if(profile!="default")startup_profiles.push_back({
            profile,read_profile_label(profiles_base()+"/ContentProfiles/"+profile,profile),false,{},{},0});
    const auto root=profiles_base()+"/CloudProfiles";
    struct stat info{};
    if(lstat(root.c_str(),&info)==0&&S_ISDIR(info.st_mode)){
        if(DIR *directory=opendir(root.c_str())){
            while(auto *entry=readdir(directory)){
                const std::string id=entry->d_name,folder=root+"/"+id;
                if(!cloud_profile_id(id)||lstat(folder.c_str(),&info)!=0||!S_ISDIR(info.st_mode))continue;
                const auto name=id==kfx_hub_cached_id()&&kfx_hub_cached_name()[0]?kfx_hub_cached_name():
                    read_profile_label(folder,"Cloud account "+id.substr(0,8));
                startup_profiles.push_back({id,name,true,{},{},0});
            }
            closedir(directory);
        }
    }
    const std::string active=kfx_hub_cached_id();
    if(cloud_profile_id(active)&&kfx_hub_cached_name()[0]&&
       std::none_of(startup_profiles.begin(),startup_profiles.end(),[&](const StartupProfile &entry){
           return entry.cloud&&entry.id==active;
       }))startup_profiles.push_back({active,kfx_hub_cached_name(),true,{},{},0});
    for(auto &profile:startup_profiles)load_picker_metadata(profile);
    std::sort(startup_profiles.begin(),startup_profiles.end(),[](const StartupProfile &a,const StartupProfile &b){
        return std::tie(a.cloud,a.name,a.id)<std::tie(b.cloud,b.name,b.id);
    });
}

bool activate_cloud_storage(){
 const std::string id=kfx_hub_account_id();
 if(!cloud_profile_id(id))return false;
 const std::string root=profiles_base()+"/CloudProfiles";
 struct stat info;
 if(lstat(root.c_str(),&info)==0&&!S_ISDIR(info.st_mode))return false;
 if(!use_player_storage(root+"/"+id))return false;
 const std::string name=kfx_hub_account_name();
 if(profile_name_valid(name))write_small_file(root+"/"+id+"/profile-name",name+"\n");
 if(!name.empty())snprintf(net_player_name,sizeof(net_player_name),"%s",name.c_str());
 settings_account=id;
 settings_conflict=false;
 settings_status="Checking cloud settings";
 settings_revision=incoming_revision=-1;
 next_settings_sync=0;
 return true;
}
void choose_startup_profile(){
 startup_login=true;
 choosing_local_profile=false;
 cloud_ticket.clear();cloud_code.clear();cloud_password.clear();cloud_create=false;
 scan_startup_profiles();
 startup_profile_offset=0;
 profile_picker_dialog=ProfilePickerDialog::None;
 open_panel(37);
 std::ifstream previous(profiles_base()+"/last-player-profile");
 std::string kind,id;
 if(previous>>kind>>id){
     const auto row=std::find_if(startup_profiles.begin(),startup_profiles.end(),[&](const StartupProfile &entry){
         return entry.id==id&&entry.cloud==(kind=="cloud");
     });
     if(row!=startup_profiles.end()){
         const int index=static_cast<int>(row-startup_profiles.begin());
         startup_profile_offset=index/4*4;
         selected=index%4;
     }
 }
 picker_session_id=kfx_hub_cached_id();
 picker_session=cloud_profile_id(picker_session_id)&&kfx_hub_probe_picker()?
     PickerSession::Checking:PickerSession::NeedsSignIn;
}

void finish_profile_management(const std::string &message)
{
    profile_picker_dialog=ProfilePickerDialog::None;
    editing=0;
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    scan_startup_profiles();
    const auto row=std::find_if(startup_profiles.begin(),startup_profiles.end(),[](const StartupProfile &entry){
        return entry.id==managed_picker_profile.id&&entry.cloud==managed_picker_profile.cloud;
    });
    if(row!=startup_profiles.end()){
        const int index=static_cast<int>(row-startup_profiles.begin());
        startup_profile_offset=index/4*4;
        selected=index%4;
    }else{
        startup_profile_offset=std::min(startup_profile_offset,
            startup_profiles.empty()?0:((int)startup_profiles.size()-1)/4*4);
        selected=0;
    }
    focus_action=-1;
    notice=message;
}

void archive_picker_profile()
{
    const auto &profile=managed_picker_profile;
    const std::string source=profile_folder(profile);
    struct stat info{};
    if(!profile.cloud){
        const auto setting=profiles_base()+"/content-profile.cfg";
        std::ifstream current_setting(setting);
        std::string next_content;
        current_setting>>next_content;
        const char *content=getenv("KFX_CONTENT_PROFILE");
        const bool runtime_in_use=content&&profile.id==content;
        if((runtime_in_use||next_content==profile.id)&&!write_small_file(setting,"default\n")){
            notice="Could not reset the runtime profile.";
            return;
        }
        if(runtime_in_use){
            if(!getenv("KFX_APP_RESOURCES")||
               !write_small_file(profiles_base()+"/remove-runtime-profile.pending",profile.id+"\n")){
                notice="Could not schedule safe profile removal.";
                return;
            }
            close_panel();
            frontend_set_state(FeSt_QUIT_GAME);
            return;
        }
    }
    const auto root=std::filesystem::path(source).parent_path();
    if(lstat(root.c_str(),&info)==0&&!S_ISDIR(info.st_mode)){notice="Invalid profile folder.";return;}
    const int found=lstat(source.c_str(),&info);
    if(found!=0&&(errno!=ENOENT||!profile.cloud||profile.id!=kfx_hub_cached_id())){
        notice="Could not find the profile folder.";
        return;
    }
    std::string archive,slot;
    if(found==0){
        if(!S_ISDIR(info.st_mode)){notice="Invalid profile folder.";return;}
        const auto archive_root=profiles_base()+"/RemovedProfiles";
        if(!SDL_CreateDirectory(archive_root.c_str())||
           lstat(archive_root.c_str(),&info)!=0||!S_ISDIR(info.st_mode)){
            notice="Could not create the RemovedProfiles folder.";
            return;
        }
        slot=archive_root+"/"+(profile.cloud?"cloud-":"local-")+"XXXXXX";
        if(!mkdtemp(slot.data())){notice="Could not prepare the profile archive.";return;}
        archive=slot+"/"+profile.id;
        if(rename(source.c_str(),archive.c_str())!=0){
            rmdir(slot.c_str());
            notice="Could not move the profile to RemovedProfiles.";
            return;
        }
    }
    if(profile.cloud&&!kfx_hub_forget_cached_profile(profile.id.c_str())){
        if(!archive.empty()&&rename(archive.c_str(),source.c_str())!=0){
            finish_profile_management("Profile archived. The saved sign-in could not be cleared.");
            return;
        }
        if(!slot.empty())rmdir(slot.c_str());
        notice="Could not clear the saved cloud sign-in.";
        return;
    }
    if(active_startup_profile.id==profile.id&&active_startup_profile.cloud==profile.cloud){
        unsetenv("KFX_PLAYER_PROFILE_DIR");
        active_startup_profile={};
        active_profile_name.clear();
    }
    if(profile.cloud&&picker_session_id==profile.id){
        picker_session_id.clear();
        picker_session=PickerSession::Offline;
    }
    finish_profile_management(archive.empty()?"Cloud sign-in removed from this Mac.":
        "Profile moved to RemovedProfiles in the game data folder.");
}

bool startup_profile_action(int action)
{
    if(page!=37)return false;
    if(profile_picker_dialog!=ProfilePickerDialog::None){
        if(action==901||action==2231){finish_profile_management("");return true;}
        if(action==2230)archive_picker_profile();
        return true;
    }
    if(action>=2250&&action<=2253){
        const auto index=static_cast<size_t>(startup_profile_offset+action-2250);
        if(index>=startup_profiles.size())return true;
        managed_picker_profile=startup_profiles[index];
        notice.clear();
        editing=0;
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        profile_picker_dialog=ProfilePickerDialog::Delete;
        selected=0;focus_action=-1;
        return true;
    }
    if(action==2200){hub_message.clear();cloud_login_rejected=false;open_panel(36);start_edit(14);return true;}
    if(action==2201){choosing_local_profile=true;new_profile.clear();start_edit(11);return true;}
    if(action==2202||action==2203){
        startup_profile_offset=std::clamp(startup_profile_offset+(action==2202?-4:4),0,
            startup_profiles.empty()?0:((int)startup_profiles.size()-1)/4*4);
        selected=0;
        return true;
    }
    if(action>=2210&&action<=2213){
        const auto index=static_cast<size_t>(startup_profile_offset+action-2210);
        if(index>=startup_profiles.size())return true;
        const auto profile=startup_profiles[index];
        if(profile.cloud&&profile.id==picker_session_id&&picker_session==PickerSession::Checking){
            notice="Checking your cloud session. Please try again in a moment.";
            return true;
        }
        if(!profile.cloud)launch_local_profile(profile.id);
        else if(profile.id==picker_session_id&&picker_session==PickerSession::SignedIn&&kfx_hub_resume_account(profile.id.c_str())){
            if(!activate_cloud_storage()){
                kfx_hub_use_local_profile();
                notice="Could not open the cloud profile.";
                return true;
            }
            remember_profile(profile);
            startup_login=false;
            picked_startup_cloud=true;
            close_panel();
            if(kfx_hub_has_session())kfx_hub_request("me","/api/me",nullptr,1);
        }else{
            if(!use_player_storage(profiles_base()+"/CloudProfiles/"+profile.id)){
                notice="Could not open the cached cloud profile.";return true;
            }
            kfx_hub_use_local_profile();
            remember_profile(profile);
            snprintf(net_player_name,sizeof(net_player_name),"%s",profile.name.c_str());
            startup_login=false;
            close_panel();
        }
        return true;
    }
    return false;
}

void draw_profile_avatar(const StartupProfile &profile,SDL_Rect target)
{
    static SDL_Surface *avatars=nullptr;
    if(!avatars)if(const char *resources=getenv("KFX_APP_RESOURCES"))
        avatars=IMG_Load((std::string(resources)+"/avatar-presets.png").c_str());
    if(!avatars)return;
    unsigned avatar=profile.avatar;
    if(!avatar){
        avatar=1;
        for(unsigned char c:profile.id)avatar=(avatar*17+c)%15;
        ++avatar;
    }
    --avatar;
    const SDL_Rect source={(int)(avatar%5)*avatars->w/5,(int)(avatar/5)*avatars->h/3,
                           avatars->w/5,avatars->h/3};
    blit_canvas(avatars,&source,target,SDL_SCALEMODE_LINEAR);
}

void startup_profile_draw()
{
    if(page!=37)return;
    auto control=[](int x,int y,int width,const std::string &label,int action,int height=38){
        button(x,y,width,label,action,static_cast<int>(hits.size())==selected,height);
    };
    if(profile_picker_dialog!=ProfilePickerDialog::None){
        frame(54,107,532,245,true);
        centered(130,"Delete "+managed_picker_profile.name+"?",23);
        centered(184,managed_picker_profile.cloud?"Remove the saved sign-in and local copy.":"Move the profile, saves and settings to RemovedProfiles.",18);
        if(managed_picker_profile.cloud)centered(214,"Your cloud account stays available online.",18);
        control(90,269,216,"Cancel",2231,40);
        control(334,269,216,"Delete Profile",2230,40);
        if(!notice.empty())small_line(62,376,notice,516);
        return;
    }
    if(startup_profiles.empty())small_line(34,105,"Create a local profile or sign in to a cloud account.",572);
    const int rows=std::min(4,std::max(0,(int)startup_profiles.size()-startup_profile_offset));
    for(int i=0;i<rows;++i){
        const auto &profile=startup_profiles[startup_profile_offset+i];
        const int y=78+i*74;
        frame(34,y,70,70);
        draw_profile_avatar(profile,{37,y+3,64,64});
        control(116,y+17,356,profile.name,2210+i,36);
        std::string details=profile.date;
        if(!profile.campaign.empty()){
            if(!details.empty())details+=" / ";
            details+=profile.campaign;
        }
        if(!details.empty())small_line(121,y+51,details,484);
    }
    for(int i=0;i<rows;++i)
        control(484,95+i*74,122,"Delete",2250+i,36);
    if(startup_profile_offset>0)control(34,373,130,"Previous",2202,30);
    if(startup_profile_offset+4<(int)startup_profiles.size())control(476,373,130,"Next",2203,30);
    control(34,408,228,editing==11?new_profile+"|":"Create Local Profile",2201,34);
    control(274,408,228,"Sign In to Cloud",2200,34);
    control(514,408,92,"Quit",2204,34);
    if(!notice.empty())small_line(34,445,notice,572);
}
void community_request(){const char *routes[]={"/api/events","/api/friends","/api/matches","/api/challenges","/api/replays"};kfx_hub_request("community",routes[community_tab],nullptr,community_tab==1||community_tab==2?1:0);community_text="Loading...";community_offset=0;}
std::future<std::pair<bool,std::string>> content_scan;std::string content_key,content_value,content_error,content_join_id;bool content_host=false;int content_join_action=0;bool publish_after_scan=false;
void start_content_scan(const std::string &map,bool hosting){std::string key=std::string(keeper_runtime_directory)+"|"+map;if(content_scan.valid()||content_key==key)return;content_key=key;content_value.clear();content_error.clear();content_host=hosting;if(hosting)kfx_hub_content("");std::string root=keeper_runtime_directory;content_scan=std::async(std::launch::async,[root,map](){char out[256];bool ok=kfx_content_fingerprint(root.c_str(),map.c_str(),out,sizeof(out));return std::make_pair(ok,std::string(out));});}
#include "macos_party.inc"
#include "macos_cloud_saves.inc"

bool social_drawer_available()
{
    return !startup_login&&(frontend_menu_state==FeSt_MAIN_MENU||frontend_menu_state==FeSt_NET_START||
        frontend_menu_state==FeSt_NET_SERVICE);
}

void open_social_panel(int tab)
{
    party_action(tab==7?2181:2120);
    party_tab=tab;
    social_drawer=social_drawer_available();
}

SDL_Rect social_drawer_tab(int width,int height)
{
    const int w=std::clamp(width/8,55,115);
    const int h=std::clamp(height/20,20,45);
    return {width-w,height/4,w,h};
}

void social_drawer_tab_draw()
{
    if(!social_drawer_available()||!winfont)return;
    const auto tab=social_drawer_tab(RendererScreenWidth(),RendererScreenHeight());
    const int flags=RendererGetDrawFlags();
    RendererSetDrawFlags(0);
    LbDrawBox(tab.x,tab.y,tab.w,tab.h,0);
    LbTextSetFont(winfont);
    LbTextSetWindow(tab.x,tab.y+tab.h/5,tab.w,tab.h);
    RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);
    const auto unread=social_unread();
    const auto label=unread?"Social ("+std::to_string(unread)+")":std::string("Social");
    LbTextDrawResized(0,0,std::clamp(tab.h*10/std::max(1,(int)LbTextLineHeight()),8,24),label.c_str());
    RendererSetDrawFlags(flags);
}

bool social_drawer_event(const SDL_Event *event)
{
    if(!social_drawer_available())return false;
    const bool motion=event->type==SDL_EVENT_MOUSE_MOTION;
    if(!motion&&(event->type!=SDL_EVENT_MOUSE_BUTTON_DOWN||event->button.button!=SDL_BUTTON_LEFT))
        return false;
    auto *window=SDL_GetWindowFromID(motion?event->motion.windowID:event->button.windowID);
    int width=0,height=0;
    if(!window||!SDL_GetWindowSize(window,&width,&height)||width<=0||height<=0)return false;
    const SDL_Point point={static_cast<int>(motion?event->motion.x:event->button.x),
                           static_cast<int>(motion?event->motion.y:event->button.y)};
    if(kfx_controller_guide_visible){
        if(!social_drawer||page!=38)return false;
        const auto view=options_window_view(width,height);
        if(!SDL_PointInRect(&point,&view)&&!editing){
            close_panel();
            return !motion;
        }
        return false;
    }
    const auto game_view=game_window_view(width,height);
    const int rw=RendererScreenWidth(),rh=RendererScreenHeight();
    if(rw<=0||rh<=0)return false;
    auto tab=social_drawer_tab(rw,rh);
    tab.x=game_view.x+tab.x*game_view.w/rw;
    tab.y=game_view.y+tab.y*game_view.h/rh;
    tab.w=game_view.x+game_view.w-tab.x;
    tab.h=std::max(1,tab.h*game_view.h/rh);
    const int edge=game_view.x+game_view.w;
    const bool at_edge=motion&&point.x>=edge-12&&point.x<edge&&
        point.y>=game_view.y+game_view.h/8&&point.y<game_view.y+game_view.h*7/8;
    if(!SDL_PointInRect(&point,&tab)&&!at_edge)return false;
    if(motion&&event->motion.state)return false;
    open_social_panel(0);
    return true;
}

bool can_switch_profile_from_main_menu()
{
    return frontend_menu_state==FeSt_MAIN_MENU&&!startup_login&&!kfx_controller_guide_visible&&
        !active_profile_name.empty()&&!network_is_active()&&game.game_kind==GKind_Unset;
}

void switch_player_profile()
{
    const auto active_cloud=std::string(kfx_hub_account_id());
    if(!active_cloud.empty())kfx_hub_sign_out();
    kfx_hub_use_local_profile();
    unsetenv("KFX_PLAYER_PROFILE_DIR");
    active_startup_profile={};
    active_profile_name.clear();
    settings_account.clear();
    settings_conflict=false;
    picked_startup_cloud=false;
    choose_startup_profile();
}

void active_profile_draw()
{
    if(page!=41)return;
    frame(88,123,464,212);
    draw_profile_avatar(active_startup_profile,{112,146,88,88});
    text(222,154,active_profile_name,24,306);
    small_line(222,191,active_startup_profile.cloud?"Cloud profile":"Local profile",306);
    button(112,266,200,"Logout",2263,selected==0,38);
    button(328,266,200,"Quit Game",2204,selected==1,38);
}

bool active_profile_action(int action)
{
    if(action==2204&&(page==41||(page==37&&profile_picker_dialog==ProfilePickerDialog::None))){
        close_panel();
        frontend_set_state(FeSt_QUIT_GAME);
        return true;
    }
    if(page!=41)return false;
    if(action==2263){switch_player_profile();return true;}
    return false;
}

bool profile_switch_key_event(const SDL_Event *event)
{
    if(!can_switch_profile_from_main_menu()||event->type!=SDL_EVENT_KEY_DOWN||
       event->key.key!=SDLK_F6||event->key.repeat)return false;
    switch_player_profile();
    return true;
}

bool browser_join_blocked(const std::string &reason);
int rejoin_seconds=0;
void browser_tick(){
 if(frontend_menu_state==FeSt_MAIN_MENU)profile_game_recorded=false;
 if(!profile_game_recorded&&!active_startup_profile.id.empty()&&frontend_menu_state==FeSt_INITIAL&&
    game.game_kind!=GKind_Unset&&!game.packet_load_enable&&get_gameturn()>0){
    profile_game_recorded=true;
    remember_profile(active_startup_profile,true);
 }


 if(content_scan.valid()&&content_scan.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=content_scan.get();if(result.first)content_value=result.second;else content_error=result.second;if(content_host)kfx_hub_content(content_key.substr(content_key.find('|')+1).empty()?"":content_value.c_str());if(publish_after_scan){publish_after_scan=false;if(page==23)browser_action(2062);}}
 if(content_join_action&&!content_scan.valid()&&!kfx_hub_busy()){
    const int action=content_join_action;
    content_join_action=0;
    if((page==21||(page==38&&party_join_connecting))&&!browser_servers.empty()&&browser_servers[browser_index].id==content_join_id)browser_action(action);
    else if(party_join_connecting)party_join_result("Join cancelled");
 }

 static Uint64 identity_sent=0;if(network_is_active()&&!network_is_host()&&frontend_menu_state==FeSt_NET_START&&kfx_hub_identity_claim()[0]&&(identity_sent==0||SDL_GetTicks()-identity_sent>3000)){identity_sent=SDL_GetTicks();std::string claim=std::string("KFXH_ID1|")+kfx_hub_identity_claim();send_network_chat_message(netstate.my_id,claim.c_str());}
 bool hosting=network_is_active()&&network_is_host();int count=hosting?GetRemoteUserCount()+1:0;
 std::string current_map=hosting?host_map:"";if(hosting&&frontend_menu_state==FeSt_INITIAL){current_map=campaign.fname;if(current_map.size()>4&&current_map.substr(current_map.size()-4)==".cfg")current_map.resize(current_map.size()-4);current_map+=":"+std::to_string(get_loaded_level_number());}
 if(hosting)start_content_scan(current_map,true);else if(page==21&&!browser_servers.empty())start_content_scan(browser_servers[browser_index].map,false);
 if(kfx_hub_tick(hosting,count,hosting&&frontend_menu_state==FeSt_INITIAL,current_map.c_str())){
  std::string tag=kfx_hub_result_tag();HubDocument d(kfx_hub_result());std::string error=hub_field(&d.root,"error");if(tag.rfind("cloud-save:",0)==0)cloud_save_reply(tag,d.root,error);
  else if(tag.rfind("party:",0)==0)party_reply(tag,d.root,error);
  else if(tag=="settings-read"||tag=="settings-write"){
   if(settings_account==kfx_hub_account_id()){
    const long status=kfx_hub_result_status();
    if(status==200&&tag=="settings-read"){
     incoming_revision=hub_integer(&d.root,"revision");
     incoming_settings=hub_field(&d.root,"settings");
    }else if(status==200){
     settings_status=record_synced_settings(hub_integer(&d.root,"revision"),uploading_settings)?
        "Settings synced":"Could not save sync status";
    }else if(status==409){
     settings_revision=-1;
     next_settings_sync=0;
    }else{
     settings_status=status==401?"Sign in again to sync":status==0?"Offline - settings saved on this Mac":"Cloud settings unavailable";
     next_settings_sync=SDL_GetTicks()+15000;
    }
   }
  }
  else if(tag=="picker-me"){
   const auto status=kfx_hub_result_status();
   picker_session=status==200&&hub_field(&d.root,"id")==picker_session_id?PickerSession::SignedIn:
       status==401?PickerSession::NeedsSignIn:PickerSession::Offline;
   if(picker_session==PickerSession::SignedIn){
    for(auto &profile:startup_profiles)if(profile.cloud&&profile.id==picker_session_id){
        profile.avatar=std::clamp(hub_integer(&d.root,"avatar"),0,15);
    }
   }
  }
  else if(!error.empty()){
   hub_message=error;
   if(tag=="room-code"&&page==11)notice=error;
   if(tag=="join"){
    if(kfx_hub_result_status()==401)hub_message="Cloud session expired. Sign in again.";
    browser_join_error=true;
   }
   if((tag=="join"&&party_join_connecting)||(tag=="servers"&&!party_join_server.empty())){
    party_join_server.clear();
    party_join_result(hub_message);
    party_notice=hub_message;
    open_panel(38);
   }
   if(tag=="me"&&kfx_hub_session_remembered()){
    const long status=kfx_hub_result_status();
    if(status==401||status==0||status>=500){
     settings_status=status==401?"Cloud session expired. Sign in again.":"Account service unavailable.";
     if(!picked_startup_cloud&&frontend_menu_state==FeSt_MAIN_MENU&&!kfx_controller_guide_visible){
        open_panel(36);
        hub_message=settings_status+" You can use your cached profile.";
     }
    }
   }
   if(tag=="cloud-login"&&page==36){
    const long status=kfx_hub_result_status();
    cloud_login_rejected=status==401;
    if(cloud_login_rejected)hub_message="Username, email, or password incorrect.";
    else if(status==0)hub_message=error;
    else if(status>=500&&error.find("verification email")==std::string::npos)
        hub_message="Account service unavailable. Try again or play offline.";
    else hub_message=error;
   }
   if(tag=="servers"&&party_rematch_pending){party_rematch_pending=false;party_notice=error;}
   if(tag=="community")community_text=error;
  }
  else if(tag.rfind("ping:",0)==0){browser_pings[hub_field(&d.root,"address")]=hub_integer(&d.root,"ping_ms");}
  else if(tag=="servers"){public_servers.clear();const VALUE *list=value_dict_get(&d.root,"servers");for(size_t i=0;list&&i<std::min(value_array_size(list),(size_t)128);i++){const VALUE *r=value_array_get(list,i);      public_servers.push_back(browser_server_from_json(r));}for(auto *saved:{&favourite_servers,&recent_servers})for(auto &entry:*saved)for(const auto &fresh:public_servers)if(entry.address==fresh.address&&entry.name==fresh.name)entry=fresh;browser_filter(true);browser_join_error=false;
    if(party_rematch_pending){
     party_rematch_pending=false;
     const auto next=std::find_if(public_servers.begin(),public_servers.end(),[](const BrowserServer &server){
        return server.address==party_match_address&&server.map==party_match_map&&server.status=="lobby";
     });
     if(next!=public_servers.end()&&party_owner()&&party.id==party_match_room){
        party_request("select-server","server",next->id);
        party_tab=1;
     }else party_notice="No lobby for the previous map yet. Retry or choose another map.";
    }
    if(!party_join_server.empty()){
     auto requested=std::find_if(public_servers.begin(),public_servers.end(),[](const BrowserServer &s){return s.id==party_join_server;});
     party_join_server.clear();
     if(requested!=public_servers.end()&&page==38){
      browser_tab=0;browser_search.clear();browser_compatible=false;browser_filter();
      auto row=std::find_if(browser_servers.begin(),browser_servers.end(),[&](const BrowserServer &s){return s.id==requested->id;});
      if(row!=browser_servers.end()){
       browser_index=static_cast<int>(row-browser_servers.begin());
       server_password.clear();
       browser_action(2040);
       return;
      }
     }
     party_notice="The party server is no longer available.";
    party_join_result(party_notice);
    }
    hub_message=public_servers.empty()?"No public servers. LAN and direct play are available.":"Directory updated.";}
  else if(tag=="rejoin-lobby"&&page==20){
   const VALUE *row=value_dict_get(&d.root,"server");
   rejoin_seconds=hub_integer(&d.root,"seconds_left");
   if(!row||value_type(row)!=VALUE_DICT||rejoin_seconds<=0){
      hub_message="No lobby seat is currently held for this account.";
   }else{
      const auto server=browser_server_from_json(row);
      browser_filter();
      auto existing=std::find_if(browser_servers.begin(),browser_servers.end(),[&](const BrowserServer &item){return item.id==server.id;});
      if(existing==browser_servers.end()){browser_servers.push_back(server);browser_index=browser_servers.size()-1;}
      else{*existing=server;browser_index=existing-browser_servers.begin();}
      server_password.clear();browser_join_error=false;
      open_panel(21);
      hub_message="Seat held for up to "+std::to_string(rejoin_seconds)+" seconds. Choose Rejoin Lobby.";
   }
  }
  else if(tag=="room-code"){
   const VALUE *row=value_dict_get(&d.root,"server");
   if(!row||value_type(row)!=VALUE_DICT){notice="Room code did not return a lobby.";return;}
   browser_servers={browser_server_from_json(row)};
   browser_index=0;server_password.clear();
   hub_message="Private room ready. Choose Join Server.";
   open_panel(21);
  }
  else if(tag=="pair-start"){device_code=hub_field(&d.root,"device_code");pair_code=hub_field(&d.root,"user_code");pair_at=SDL_GetTicks()+3000;pair_deadline=SDL_GetTicks()+600000;hub_message="Approve this code in your browser.";SDL_OpenURL((std::string(kfx_hub_url())+"/index.php?route=%2Fdevice").c_str());}
  else if(tag=="pair-poll"&&!hub_field(&d.root,"token").empty()){device_code.clear();pair_code.clear();hub_message="Game connected to your account.";kfx_hub_request("me","/api/me",nullptr,1);}
  else if(tag=="cloud-signup"){cloud_ticket=hub_field(&d.root,"ticket");selected=0;hub_message="Enter the six-digit code sent to your email.";}
  else if(tag=="cloud-verify"||tag=="cloud-login"){
   const auto verification_ticket=hub_field(&d.root,"ticket");
   if(tag=="cloud-login"&&!verification_ticket.empty()){
    cloud_ticket=verification_ticket;
    cloud_code.clear();cloud_password.clear();
    hub_message="Verify your email with the new code we sent.";
    if(page==36){selected=0;start_edit(17);}
   }else if(!hub_field(&d.root,"token").empty()){
    cloud_ticket.clear();cloud_code.clear();cloud_password.clear();cloud_confirmation.clear();cloud_email.clear();
    hub_message="Signed in as "+std::string(kfx_hub_account_name())+".";
    if(page==36){if(activate_cloud_storage()){
      StartupProfile profile{kfx_hub_account_id(),kfx_hub_account_name(),true,{},{},std::clamp(hub_integer(&d.root,"avatar"),0,15)};
      remember_profile(profile);
      startup_login=false;
      close_panel();
    }else hub_message="Could not open the cloud profile on this Mac.";}
   }
  }
  else if(tag=="join"){
   const VALUE *required=value_dict_get(&d.root,"login_ticket_required");
   if(required&&value_type(required)==VALUE_BOOL&&value_bool(required)){
    pending_login_ticket=hub_field(&d.root,"identity_ticket");
    if(pending_login_ticket.empty()){browser_join_blocked("Sign in again to join this server.");return;}
   }
   join_ip=hub_field(&d.root,"endpoint");pending_join=true;
  }
  else if(tag=="spectate-start"){std::string id=hub_field(&d.root,"stream_id");if(normalized_version(hub_field(&d.root,"engine"))!=normalized_version(KFX_APP_VERSION)&&normalized_version(hub_field(&d.root,"engine"))!=normalized_version(VER_STRING)){hub_message="Spectator engine version differs.";return;}if(!id.empty()){kfx_hub_spectator_start(id.c_str());hub_message="Buffering a read-only spectator replay...";}}
  else if(tag=="publish")hub_message="Public lobby registered. Players can join directly.";
  else if(tag=="community"){community_text.clear();const char *keys[]={"events","friends","matches","challenges","replays"};const VALUE *rows=value_dict_get(&d.root,keys[community_tab]);for(size_t i=0;rows&&i<std::min(value_array_size(rows),(size_t)50);i++){const VALUE *r=value_array_get(rows,i);std::string title=hub_field(r,community_tab==0||community_tab==3?"title":community_tab==1?"name":community_tab==2?"map":"id");community_text+=title+"\n";std::string detail=hub_field(r,community_tab==0?"kind":community_tab==1?"status":community_tab==2?"verification":community_tab==3?"description":"sha");community_text+=detail+"\n\n";}if(community_text.empty())community_text="Nothing here yet. Create events and manage your profile on the website.";}
 }
 party_tick();
 sync_cloud_settings();
 if(kfx_hub_spectator_ready()){close_panel();snprintf(game.packet_fname,sizeof(game.packet_fname),"%s",kfx_hub_spectator_file());game.packet_load_enable=true;game.packet_load_initialized=false;frontend_set_state(FeSt_PACKET_DEMO);}
 if(!device_code.empty()&&SDL_GetTicks()>pair_deadline){device_code.clear();pair_code.clear();hub_message="Pairing expired. Choose Sign In again.";}
 if(!device_code.empty()&&SDL_GetTicks()>=pair_at&&!kfx_hub_busy()){pair_at=SDL_GetTicks()+3000;std::string body="{\"device_code\":"+hub_quote(device_code)+"}";kfx_hub_request("pair-poll","/api/device/poll",body.c_str(),0);}
}
bool browser_join_blocked(const std::string &reason)
{
    hub_message=reason;
    browser_join_error=true;
    if(party_join_connecting){
        party_join_result(reason);
        party_notice=reason;
        open_panel(38);
    }
    return true;
}

bool browser_action(int action){
 if(active_profile_action(action)||startup_profile_action(action)||party_action(action)||cloud_save_action(action))return true;
 if(action==2114||action==2115){
  if(!settings_conflict||settings_account!=kfx_hub_account_id())return true;
  if(game.game_kind!=GKind_Unset||network_is_active()){
   settings_status="Return to the main menu to resolve the sync conflict";
   return true;
  }
  const bool resolved=action==2115?apply_cloud_settings():record_synced_settings(incoming_revision,incoming_settings);
  if(!resolved){settings_status="Could not save settings";return true;}
  incoming_revision=-1;
  next_settings_sync=0;
  settings_conflict=false;
  selected=0;
  settings_status=action==2115?"Settings synced":"Syncing settings from this Mac";
  return true;
 }
 if(action==2111){start_local_profile_picker();return true;}
 if(action==2112&&cloud_login_rejected&&page==36){
  cloud_create=true;
  cloud_login_rejected=false;
  cloud_confirmation.clear();
  if(cloud_name.find('@')!=std::string::npos){cloud_email=cloud_name;cloud_name.clear();}
  open_panel(36);
  hub_message="Confirm your password to create an account.";
  return true;
 }
 if(action==2113){
  cloud_create=false;
  cloud_login_rejected=false;
  cloud_password.clear();cloud_code.clear();cloud_ticket.clear();
  hub_message.clear();open_panel(36);return true;
 }
 if(action>=2101&&action<=2103){start_edit(13+action-2100);return true;}
 if(action==2108){start_edit(18);return true;}
 if(action==2107){start_edit(17);return true;}
 if(action==2105){
  if(kfx_hub_busy()){hub_message="Please wait for the current request.";return true;}
  std::string body,tag,route;
  if(!cloud_ticket.empty()){
   if(cloud_code.size()!=6){hub_message="Enter your six-digit verification code.";return true;}
   tag="cloud-verify";route="/api/cloud/verify";
   body="{\"ticket\":"+hub_quote(cloud_ticket)+",\"code\":"+hub_quote(cloud_code)+"}";
  }else{
   if(cloud_name.empty()||cloud_password.empty()||(cloud_create&&(cloud_password.size()<12||cloud_email.empty()))){
    hub_message=cloud_create?"Enter your details and a password of at least 12 characters.":"Enter your username or email and password.";return true;
   }
   if(cloud_create&&cloud_password!=cloud_confirmation){
    hub_message="The passwords must match.";
    return true;
   }
   tag=cloud_create?"cloud-signup":"cloud-login";
   route=cloud_create?"/api/cloud/signup":"/api/cloud/login";
   body="{\"name\":"+hub_quote(cloud_name)+",\"password\":"+hub_quote(cloud_password);
   if(cloud_create)body+=",\"email\":"+hub_quote(cloud_email)+",\"avatar\":"+std::to_string(cloud_avatar);
   body+="}";
  }
  if(kfx_hub_request(tag.c_str(),route.c_str(),body.c_str(),0)){
   if(tag!="cloud-login"){cloud_password.clear();cloud_confirmation.clear();}
   hub_message="Contacting the account service...";
  }
  return true;
 }
 if(action==2116&&page==36){
  if(kfx_hub_session_remembered()&&activate_cloud_storage()){
   startup_login=false;
   cloud_password.clear();
   close_panel();
  }else hub_message="No cached cloud profile on this Mac.";
  return true;
 }
 if(action==2000){page=20;selected=0;browser_tab=0;browser_filter();browser_refresh();return true;}
 if(action>=2001&&action<=2004){browser_tab=action-2001;browser_index=0;browser_filter();browser_refresh();return true;}
 if(action>=2010&&action<2013){int index=(browser_index/3)*3+action-2010;if(index<(int)browser_servers.size()){browser_index=index;rejoin_seconds=0;server_password.clear();browser_join_error=false;page=21;selected=0;}return true;}
 if(action==2090){
  open_folder(keeper_runtime_directory);
  hub_message="Game Data folder opened in Finder.";
  return true;
 }
 if(action==2091){
  SDL_OpenURL((std::string(kfx_hub_url())+"/index.php?route=%2Fdownloads").c_str());
  return true;
 }
 if(action==2020){browser_refresh();return true;}
 if(action==2029&&page==20){
    if(!kfx_hub_has_session())hub_message="Sign in to check for a held lobby seat.";
    else if(kfx_hub_request("rejoin-lobby","/api/rejoin-lobby",nullptr,1))hub_message="Checking for a held lobby seat...";
    else hub_message="Wait for the current community request.";
    return true;
 }
 if(action==2028&&page==21){page=20;selected=0;browser_refresh();return true;}
 if(action==2021){start_edit(6);return true;}
 if(action==2022){page=23;selected=0;return true;}
 if(action==2023){page=22;selected=0;return true;}
 if(action==2024){page=24;selected=0;community_request();return true;}
 if(action==2025){browser_listing_join=false;if(network_is_active()||game.game_kind==GKind_LocalGame){hub_message="Return to the main menu first.";return true;}page=11;selected=0;return true;}
 if(action==2027){browser_sort=(browser_sort+1)%4;browser_filter();return true;}
 if(action==2026){browser_compatible=!browser_compatible;browser_filter();return true;}
 if(action==2030||action==2031){browser_index=std::clamp(browser_index+(action==2030?-3:3),0,std::max(0,(int)browser_servers.size()-1));browser_filter();return true;}
 if(action==2040){browser_join_error=false;if(browser_servers.empty())return true;const auto &s=browser_servers[browser_index];if(s.status=="maintenance")return browser_join_blocked("Server is draining for maintenance.");if(network_is_active()||game.game_kind==GKind_LocalGame){return browser_join_blocked("Return to the main menu before joining.");}if(!supported_server_version(s)){std::string requirement="Supported clients: ";
for(const auto &version:s.clients){if(requirement.size()>19)requirement+=", ";requirement+=version;}
return browser_join_blocked(s.clients.empty()?"Supported engine: "+s.engine:requirement);}
if(dedicated_listing(s)&&!kfx_hub_has_session()){return browser_join_blocked("Sign in with a cloud account to join.");}
const auto missing=missing_server_assets();
if(!missing.empty()){return browser_join_blocked("Missing "+missing);}if(!s.content.empty()){start_content_scan(s.map,false);if(content_scan.valid()){content_join_action=action;content_join_id=s.id;hub_message="Checking campaign, map, configuration and mods before joining...";return true;}if(!content_error.empty()){return browser_join_blocked(content_error);}if(content_value!=s.content){return browser_join_blocked("Content differs: campaign/map, configuration or mods. Use the host's content profile.");}}browser_join_id=s.id;browser_listing_join=true;browser_join_content=s.content.empty()?"":content_value;if(s.id.empty()){join_ip=s.address;pending_join=true;}else{std::string body="{\"content\":"+hub_quote(browser_join_content);if(s.kind=="listen")body+=",\"engine\":"+hub_quote(normalized_version(VER_STRING));body+="}";if(!kfx_hub_request("join",("/api/servers/"+s.id+"/join").c_str(),body.c_str(),1))return browser_join_blocked("Server request queue is full. Try again.");}recent_servers.erase(std::remove_if(recent_servers.begin(),recent_servers.end(),[&](const BrowserServer &r){return r.address==s.address;}),recent_servers.end());recent_servers.insert(recent_servers.begin(),s);if(recent_servers.size()>32)recent_servers.resize(32);browser_save("servers-recent.cfg",recent_servers);return true;}
 if(action==2044){start_edit(10);return true;}
 if(action==2045&&!browser_servers.empty()){if(network_is_active()||game.game_kind==GKind_LocalGame){hub_message="Return to the main menu before spectating.";return true;}kfx_hub_request("spectate-start",("/api/servers/"+browser_servers[browser_index].id+"/spectate").c_str(),nullptr,0);return true;}
 if(action==2042&&!browser_servers.empty()){BrowserServer s=browser_servers[browser_index];auto i=std::find_if(favourite_servers.begin(),favourite_servers.end(),[&](const BrowserServer &v){return v.address==s.address;});if(i==favourite_servers.end())favourite_servers.push_back(s);else favourite_servers.erase(i);hub_message=browser_save("servers-favourites.cfg",favourite_servers)?"Favourites updated.":"Could not save favourites. Check folder permissions.";browser_filter();return true;}
 if(action==2043&&!browser_servers.empty()){SDL_SetClipboardText(browser_servers[browser_index].address.c_str());hub_message="Server address copied.";return true;}
 if(action==2050){if(!kfx_hub_request("pair-start","/api/device/start","{}",0))hub_message="Wait for the current request.";return true;}
 if(action==2051){SDL_OpenURL((std::string(kfx_hub_url())+"/index.php?route=%2Fprofile").c_str());return true;}
 if(action==2052){device_code.clear();pair_code.clear();settings_conflict=false;kfx_hub_sign_out();hub_message="Signed out on this game.";return true;}
 if(action==2053){page=25;selected=0;hub_edit=kfx_hub_url();return true;}
 if(action==2064){start_edit(13);hub_message="Optional preview map, for example classic:52. Lobby readiness checks the final choice.";return true;}
 if(action==2063){start_edit(10);return true;}
 if(action==2060||action==2061){start_edit(action==2060?7:8);return true;}
 if(action==2062){
  std::string endpoint;if(!valid_endpoint(host_address,endpoint)){hub_message="Enter this host's reachable hostname and UDP port.";return true;}
  if(!network_is_active()){if(game.game_kind==GKind_LocalGame){hub_message="Return to the main menu before hosting.";return true;}close_panel();LbNetwork_Stop();if(!setup_network_service(FrontendNetSvc_LAN)){open_panel(23);hub_message="Could not create the local network host.";return true;}enet_port=atoi(endpoint.substr(endpoint.rfind(':')+1).c_str());frontnet_service_setup();frontnet_session_setup();snprintf(netstate.password,sizeof(netstate.password),"%s",server_password.c_str());frontnet_session_create(nullptr);open_panel(23);}
  if(!network_is_host()||frontend_menu_state!=FeSt_NET_START){hub_message="Publish a pre-game lobby you are hosting.";return true;}
  if(!host_map.empty()){start_content_scan(host_map,true);if(content_scan.valid()){publish_after_scan=true;hub_message="Checking the preview map and mods before publication...";return true;}if(!content_error.empty()){hub_message=content_error;return true;}}
  std::string body="{\"name\":"+hub_quote(host_title)+",\"endpoint\":"+hub_quote(endpoint)+",\"engine\":"+hub_quote(normalized_version(VER_STRING))+",\"map\":"+hub_quote(host_map)+",\"mods\":"+hub_quote(host_map.empty()?"":content_value)+",\"region\":\"Local\",\"password\":"+(netstate.password[0]?"true":"false")+"}";kfx_hub_request("publish","/api/servers/register",body.c_str(),1);return true;
 }
 if(action==2070){start_edit(9);return true;}
 if(action==2071){if(kfx_hub_set_url(hub_edit.c_str())){write_small_file(path("community-url.cfg"),hub_edit);hub_message="Community service address saved.";}else hub_message="Use HTTPS, or HTTP on localhost with a port.";return true;}
 if(action==2080||action==2081){community_tab=(community_tab+(action==2080?4:1))%5;community_request();return true;}
 if(action==2082){SDL_OpenURL((std::string(kfx_hub_url())+"/index.php?route=%2Fevents").c_str());return true;}
 if(action==2083||action==2084){community_offset=std::max(0,community_offset+(action==2083?-4:4));return true;}
 return false;
}
void browser_draw(){
 startup_profile_draw();
 active_profile_draw();
 if(page==36){
  if(!cloud_ticket.empty()){
   button(24,135,592,"Code: "+cloud_code+(editing==17?"|":""),2107,selected==0,42);
   button(24,230,592,"Verify Email",2105,selected==1,42);
   button(24,296,285,"Back to Sign In",2113,selected==2,38);
   button(331,296,285,"Use a Local Profile",2111,selected==3,38);
  }else{
   const std::string visible_name=cloud_name.size()>42?"..."+cloud_name.substr(cloud_name.size()-39):cloud_name;
   centered(86,cloud_create?"Username":"Username or email",20);
   button(104,114,432,visible_name+(editing==14?"|":""),2101,selected==0,38);
   if(cloud_create)button(104,159,432,cloud_email.empty()?"Email address":short_text(cloud_email,46),2102,selected==1,38);
   centered(cloud_create?200:163,"Password",20);
   const auto masked=[](const std::string &value){return std::string(std::min<size_t>(value.size(),24),'*');};
   button(104,cloud_create?225:190,432,masked(cloud_password)+(editing==16?"|":""),2103,selected==(cloud_create?2:1),38);
   if(cloud_create){
    button(104,274,432,"Confirm password "+masked(cloud_confirmation)+(editing==18?"|":""),2108,selected==3,38);
    button(104,325,432,"Send Verification Code",2105,selected==4,42);
   }else{
    if(cloud_login_rejected){
     button(104,253,204,"Try Again",2105,selected==2,43);
     button(320,253,216,"Create Account",2112,selected==3,43);
    }else button(104,253,432,"Sign In or Create Account",2105,selected==2,43);
    if(!startup_login){
        const bool cached=kfx_hub_session_remembered();
        const int local_index=cloud_login_rejected?4:3;
        button(104,347,cached?210:432,"Use a Local Profile",2111,selected==local_index,38);
        if(cached)button(326,347,210,"Use Cached Profile",2116,selected==local_index+1,38);
    }
   }
  }
  small_line(104,!cloud_create&&!startup_login?405:397,hub_message,432);return;
 }
 if(page==20){
  button(24,91,322,"Search: "+browser_search+(editing==6?"|":""),2021,editing==6||selected==10,34);
  const char *sorts[]={"By Name","By Ping","By Players","By Map"};
  button(360,91,120,sorts[browser_sort],2027,selected==11,34);
  button(496,91,120,browser_compatible?"Matching":"All versions",2026,selected==12,34);
  const int first=(browser_index/3)*3;
  for(int i=0;i<3&&first+i<(int)browser_servers.size();i++){
   const auto &server=browser_servers[first+i];
   const char *type=!server.ai_style.empty()?"Co-op":server.kind=="community"?"Dedicated":server.kind=="dedicated"?"Verified dedicated":"Player hosted";
   const std::string ping=browser_pings.count(server.address)&&browser_pings[server.address]>=0?
       std::to_string(browser_pings[server.address])+" ms":"Ping unavailable";
   button(24,138+i*65,592,server.name+"  "+std::to_string(server.players)+"/"+std::to_string(server.capacity),2010+i,selected==i,35);
   small_line(36,174+i*65,std::string(type)+"  /  "+(server.map.empty()?"Map pending":server.map)
       +"  /  "+(server.status=="maintenance"?"Maintenance":server.status=="playing"?"In progress":"Lobby")+(server.password?"  /  Password":""),568);
   small_line(36,188+i*65,server.engine+"  /  "+server.region+"  /  "+ping,568);
  }
  if(browser_servers.empty())centered(228,hub_message=="Refreshing directory..."?hub_message:"No public servers found.",22);
  button(24,341,108,"Prev",2030,selected==3,34);button(144,341,108,"Next",2031,selected==4,34);
  button(264,341,108,"Refresh",2020,selected==5,34);button(384,341,108,"Host",2022,selected==6,34);
  button(504,341,112,"Account",2023,selected==7,34);
  button(24,382,134,"Community",2024,selected==8,32);button(176,382,134,"Direct Connect",2025,selected==9,32);button(328,382,134,"Rejoin Lobby",2029,selected==14,32);button(480,382,136,social_unread()==0?"Friends and Party":"Social ("+std::to_string(social_unread())+")",2120,selected==13,32);
 }else if(page==21&&!browser_servers.empty()){const auto &s=browser_servers[browser_index];centered(88,s.name,24);small_line(24,126,s.address+" / "+s.engine,592);small_line(24,149,s.map+" / "+s.region+(s.password?" / Password required":""),592);
    const auto missing=missing_server_assets();
    const std::string version_status=!supported_server_version(s)?"Your client version is unsupported":
       s.clients.empty()&&dedicated_listing(s)?"Supported client versions not advertised":"Client version compatible";
    small_line(24,170,version_status,592);
    small_line(24,191,missing.empty()?"Required sprite archives present":"Missing "+missing,592);
    if(dedicated_listing(s))small_line(24,212,kfx_hub_has_session()?"Cloud sign-in present":"Cloud sign-in required",592);
    button(64,235,512,"Password: "+std::string(server_password.size(),'*'),2044,selected==0,32);button(64,272,512,browser_join_error?"Retry Join":rejoin_seconds?"Rejoin Lobby":"Join Server",2040,selected==1,38);button(64,315,250,"Toggle Favourite",2042,selected==2,38);button(323,315,139,"Copy Address",2043,selected==3,38);button(470,315,106,"Spectate",2045,selected==4,38);if(hub_message.rfind("Cloud session expired",0)==0||hub_message.rfind("Sign in with a cloud account",0)==0)button(64,368,512,"Sign In Again",2113,selected==5,38);else if(hub_message.rfind("Lobby is full",0)==0||hub_message.rfind("Match already started",0)==0)button(64,368,512,"Refresh Server List",2028,selected==5,38);else if(hub_message.rfind("Missing ",0)==0)button(64,368,512,"Open Game Data Folder",2090,selected==5,38);
    else if(hub_message.rfind("Supported ",0)==0)button(64,368,512,"Get Compatible Client",2091,selected==5,38);
    else if(browser_join_error)button(64,368,512,"Refresh Server List",2028,selected==5,38);
    else if(party_owner()&&!s.id.empty()){if(s.voting)button(326,368,250,"Vote on Map",2138,selected==5,38);button(64,368,s.voting?250:512,party.target.id==s.id&&party.target_blocker.empty()?"View Party Readiness":"Check Party Readiness",2121,selected==(s.voting?6:5),38);}
 }else if(page==22&&settings_conflict){
  centered(100,"Settings changed on both devices",22);
  small_line(60,158,settings_status,520);
  button(84,225,472,"Keep Settings on This Mac",2114,selected==0,42);
  button(84,285,472,"Use Cloud Settings",2115,selected==1,42);
 }else if(page==22){centered(94,kfx_hub_account_name()[0]?std::string("Signed in: ")+kfx_hub_account_name():"Optional community account",24);if(!pair_code.empty()){centered(137,"Pairing code: "+pair_code,28);small_line(24,180,"Approve this code on your own browser only.",592);}else small_line(24,148,kfx_hub_account_name()[0]?settings_status:"LAN and direct play do not require an account.",592);button(64,190,512,"Sign In",2113,selected==4,36);button(64,234,512,"Sign In with Browser",2050,selected==0,36);button(64,278,512,"Open Website / Profile",2051,selected==1,36);button(64,322,512,"Sign Out",2052,selected==2,36);button(64,366,512,"Community Service",2053,selected==3,36);
 }else if(page==23){button(24,98,592,"Name: "+host_title+(editing==7?"|":""),2060,selected==0,38);button(24,151,592,"Address: "+host_address+(editing==8?"|":""),2061,selected==1,38);button(24,204,592,"Password: "+std::string(server_password.size(),'*'),2063,selected==2,38);button(24,257,592,"Preview map: "+(host_map.empty()?"Unselected":host_map)+(editing==13?"|":""),2064,selected==3,38);small_line(24,308,"Example map: classic:52. Final map is checked in the lobby.",592);button(64,350,512,"Create / Publish Lobby",2062,selected==4,42);
 }else if(page==24){const char *names[]={"Events","Friends","Match History","Co-op Challenges","Replays"};button(24,90,108,"Prev",2080,selected==0,36);centered(99,names[community_tab],24);button(508,90,108,"Next",2081,selected==1,36);std::vector<std::string> lines;std::istringstream stream(community_text);std::string line;while(std::getline(stream,line))lines.push_back(line);int limit=5;community_offset=std::min(community_offset,std::max(0,(int)lines.size()-limit));for(int i=0;i<limit&&community_offset+i<(int)lines.size();i++)text(24,150+i*37,lines[community_offset+i],20,592);button(24,351,160,"Up",2083,selected==2,36);button(216,351,208,"Open Website",2082,selected==3,36);button(456,351,160,"Down",2084,selected==4,36);
 }else if(page==25){small_line(24,103,"Internet service URL (custom servers supported)",592);button(24,157,592,hub_edit+(editing==9?"|":""),2070,selected==0,48);small_line(24,231,"Remote services require HTTPS. Account tokens are",592);small_line(24,268,"cleared when you change the service address.",592);button(64,325,512,"Save Service Address",2071,selected==1,44);}
 if(page==21&&!browser_servers.empty()&&hub_message=="Directory updated."){const auto &s=browser_servers[browser_index];hub_message=s.content.empty()?"Content unverified; map readiness is checked in the lobby.":content_scan.valid()?"Checking local content...":!content_error.empty()?content_error:content_value==s.content?"Engine/content fingerprint matches.":"Content differs. Join blocked until profiles match.";}
 if(page==21&&!browser_servers.empty()&&!browser_servers[browser_index].ai_style.empty()&&
    (hub_message=="Directory updated."||hub_message.rfind("Content unverified",0)==0)){
    const auto &server=browser_servers[browser_index];
    small_line(24,402,"Humans vs "+server.ai_style+" AI / "+std::to_string(server.capacity-1)+" human seats",592);
 }else if(page>=20&&page<=25)small_line(24,402,hub_message,592);
}

void coop_lobby_draw()
{
    if(frontend_menu_state!=FeSt_NET_START||!network_is_active()||browser_join_id.empty()||!winfont)return;
    const auto server=std::find_if(public_servers.begin(),public_servers.end(),[](const BrowserServer &row){
        return row.id==browser_join_id;
    });
    if(server==public_servers.end()||!dedicated_listing(*server))return;
    const std::string summary=server->map+(server->ai_style.empty()?"":" / Humans vs "+server->ai_style+" AI");
    const int width=RendererScreenWidth();
    const int flags=RendererGetDrawFlags();
    LbTextSetFont(winfont);
    LbTextSetWindow(12,12,width-24,24);
    RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);
    const int units=std::max(12,std::min(24,18*16/std::max(1,(int)LbTextLineHeight())));
    LbTextDrawResized(0,0,units,summary.c_str());
    const std::string report=kfx_readiness_report();
    if(report.rfind("Map: ",0)==0){
        std::istringstream lines(report);
        std::string line,roster;
        std::getline(lines,line);
        for(int i=0;i<MAX_NET_USERS&&std::getline(lines,line)&&line.find(": ")!=std::string::npos;++i){
            if(!roster.empty())roster+="  /  ";
            roster+=line;
        }
        if(!roster.empty()){
            LbTextSetWindow(12,36,width-24,24);
            LbTextDrawResized(0,0,std::min(units,16),roster.c_str());
        }
    }
    RendererSetDrawFlags(flags);
}

bool browser_join_failed(){
 const char *reason=kfx_join_failure_reason[0]?kfx_join_failure_reason:"Connection failed. Retry from the server browser.";
 if(!party_join_connecting){open_panel(browser_listing_join?21:11);notice=reason;}
 return browser_join_blocked(reason);
}

bool browser_redeem_room_code()
{
    if(join_ip.size()<5||strncasecmp(join_ip.c_str(),"CODE ",5))return false;
    const std::string code=join_ip.substr(5);
    if(!kfx_hub_has_session()){notice="Sign in before joining a code room.";return true;}
    const auto body="{\"code\":"+hub_quote(code)+"}";
    notice=kfx_hub_request("room-code","/api/servers/redeem",body.c_str(),1)?
        "Checking room code...":"Try again once the current request finishes.";
    return true;
}
