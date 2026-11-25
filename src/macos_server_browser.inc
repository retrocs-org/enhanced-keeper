// Integrated community browser. Service I/O is asynchronous; LAN/direct play is independent.
struct HubDocument{VALUE root;HubDocument(const std::string &s){value_init_null(&root);json_dom_parse(s.data(),s.size(),nullptr,0,&root,nullptr);}~HubDocument(){value_fini(&root);}};
std::string hub_field(const VALUE *root,const char *key){const VALUE *v=root?value_dict_get(root,key):nullptr;return v&&value_type(v)==VALUE_STRING?value_string(v):"";}
int hub_integer(const VALUE *root,const char *key){const VALUE *v=root?value_dict_get(root,key):nullptr;return v&&(value_type(v)==VALUE_INT32||value_type(v)==VALUE_UINT32)?value_int32(v):0;}
std::string hub_quote(const std::string &s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';if(c>=32)out+=c;}return out+'"';}
struct BrowserServer{std::string id,name,address,engine,map,kind,region,content,status;int players=0,capacity=4;bool password=false;};
std::vector<BrowserServer> browser_servers,public_servers,favourite_servers,recent_servers;
std::map<std::string,int> browser_pings;int browser_sort=0;
int browser_tab=0,browser_index=0,community_tab=0,community_offset=0;bool browser_compatible=false;
std::string browser_search,hub_edit,host_title="My Dungeon",host_address="127.0.0.1:5556",host_map,device_code,pair_code,server_password,hub_message="Guest play is available.",browser_join_id,browser_join_content,community_text="Choose a community section.";Uint64 pair_at=0,pair_deadline=0;
std::string browser_store(const BrowserServer &s){return s.address+"\t"+s.name+"\t"+s.engine+"\t"+s.id+"\n";}
bool browser_save(const char *file,const std::vector<BrowserServer> &rows){std::string body;for(size_t i=0;i<std::min(rows.size(),(size_t)32);i++)body+=browser_store(rows[i]);return write_small_file(path(file),body);}
void browser_load(const char *file,std::vector<BrowserServer> &rows){FILE *f=fopen(path(file).c_str(),"r");if(!f)return;rows.clear();char line[512];while(rows.size()<32&&fgets(line,sizeof(line),f)){std::string s=line;size_t a=s.find('\t'),b=s.find('\t',a+1);if(a==std::string::npos||b==std::string::npos)continue;BrowserServer r;r.address=s.substr(0,a);r.name=s.substr(a+1,b-a-1);size_t extra=s.find('\t',b+1);r.engine=s.substr(b+1,extra==std::string::npos?std::string::npos:extra-b-1);if(extra!=std::string::npos){r.id=s.substr(extra+1);while(!r.id.empty()&&(r.id.back()=='\n'||r.id.back()=='\r'))r.id.pop_back();}while(!r.engine.empty()&&(r.engine.back()=='\n'||r.engine.back()=='\r'))r.engine.pop_back();r.kind="saved";rows.push_back(r);}fclose(f);}
void browser_filter(){
 browser_servers=browser_tab==0?public_servers:browser_tab==2?favourite_servers:browser_tab==3?recent_servers:std::vector<BrowserServer>{};
 if(browser_tab==1){lan_refresh_sessions();for(int i=0;i<lan_session_count&&i<LAN_SESSIONS_MAX;i++){BrowserServer s;s.name=lan_sessions[i].text;s.address=lan_sessions[i].join_address;if(s.address.rfind("LAN:",0)==0)s.address.erase(0,4);s.kind="LAN";browser_servers.push_back(s);}}
 std::string q=browser_search;std::transform(q.begin(),q.end(),q.begin(),[](unsigned char c){return std::tolower(c);});browser_servers.erase(std::remove_if(browser_servers.begin(),browser_servers.end(),[&](const BrowserServer &s){std::string text=s.name+" "+s.map+" "+s.region;std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::tolower(c);});return text.find(q)==std::string::npos||(browser_compatible&&!s.engine.empty()&&normalized_version(s.engine)!=normalized_version(VER_STRING));}),browser_servers.end());std::sort(browser_servers.begin(),browser_servers.end(),[](const BrowserServer &a,const BrowserServer &b){if(browser_sort==1){int x=browser_pings.count(a.address)&&browser_pings[a.address]>=0?browser_pings[a.address]:999999,y=browser_pings.count(b.address)&&browser_pings[b.address]>=0?browser_pings[b.address]:999999;if(x!=y)return x<y;}else if(browser_sort==2&&a.players!=b.players)return a.players>b.players;else if(browser_sort==3&&a.map!=b.map)return a.map<b.map;return a.name<b.name;});browser_index=std::clamp(browser_index,0,std::max(0,(int)browser_servers.size()-1));
 for(int i=(browser_index/3)*3;i<std::min((int)browser_servers.size(),(browser_index/3)*3+3);i++)if(!browser_pings.count(browser_servers[i].address)&&kfx_hub_probe(browser_servers[i].address.c_str()))browser_pings[browser_servers[i].address]=-2;
}
void browser_refresh(){if(browser_tab==0||browser_tab==2||browser_tab==3){if(!kfx_hub_request("servers","/api/servers",nullptr,0))hub_message="A community request is still running.";else hub_message="Refreshing directory...";}else{browser_filter();hub_message=browser_servers.empty()?"No servers found in this tab.":"Server list updated.";}}
void browser_initialize(){kfx_hub_initialize(path("").c_str());browser_load("servers-favourites.cfg",favourite_servers);browser_load("servers-recent.cfg",recent_servers);size_t length=0;char *url=(char*)SDL_LoadFile(path("community-url.cfg").c_str(),&length);if(url){std::string value(url,length);SDL_free(url);kfx_hub_set_url(value.c_str());}}
void community_request(){const char *routes[]={"/api/events","/api/friends","/api/matches","/api/challenges","/api/replays"};kfx_hub_request("community",routes[community_tab],nullptr,community_tab==1||community_tab==2?1:0);community_text="Loading...";community_offset=0;}
std::future<std::pair<bool,std::string>> content_scan;std::string content_key,content_value,content_error,content_join_id;bool content_host=false;int content_join_action=0;bool publish_after_scan=false;
void start_content_scan(const std::string &map,bool hosting){std::string key=std::string(keeper_runtime_directory)+"|"+map;if(content_scan.valid()||content_key==key)return;content_key=key;content_value.clear();content_error.clear();content_host=hosting;if(hosting)kfx_hub_content("");std::string root=keeper_runtime_directory;content_scan=std::async(std::launch::async,[root,map](){char out[256];bool ok=kfx_content_fingerprint(root.c_str(),map.c_str(),out,sizeof(out));return std::make_pair(ok,std::string(out));});}
void browser_tick(){
 if(content_scan.valid()&&content_scan.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=content_scan.get();if(result.first)content_value=result.second;else content_error=result.second;if(content_host)kfx_hub_content(content_key.substr(content_key.find('|')+1).empty()?"":content_value.c_str());if(publish_after_scan){publish_after_scan=false;if(page==23)browser_action(2062);}if(content_join_action){int action=content_join_action;content_join_action=0;if(page==21&&!browser_servers.empty()&&browser_servers[browser_index].id==content_join_id)browser_action(action);}}

 static Uint64 identity_sent=0;if(network_is_active()&&!network_is_host()&&frontend_menu_state==FeSt_NET_START&&kfx_hub_identity_claim()[0]&&(identity_sent==0||SDL_GetTicks()-identity_sent>3000)){identity_sent=SDL_GetTicks();std::string claim=std::string("KFXH_ID1|")+kfx_hub_identity_claim();send_network_chat_message(netstate.my_id,claim.c_str());}
 bool hosting=network_is_active()&&network_is_host();int count=hosting?GetRemoteUserCount()+1:0;
 std::string current_map=hosting?host_map:"";if(hosting&&frontend_menu_state==FeSt_INITIAL){current_map=campaign.fname;if(current_map.size()>4&&current_map.substr(current_map.size()-4)==".cfg")current_map.resize(current_map.size()-4);current_map+=":"+std::to_string(get_loaded_level_number());}
 if(hosting)start_content_scan(current_map,true);else if(page==21&&!browser_servers.empty())start_content_scan(browser_servers[browser_index].map,false);
 if(kfx_hub_tick(hosting,count,hosting&&frontend_menu_state==FeSt_INITIAL,current_map.c_str())){
  std::string tag=kfx_hub_result_tag();HubDocument d(kfx_hub_result());std::string error=hub_field(&d.root,"error");if(!error.empty()){hub_message=error;if(tag=="community")community_text=error;}
  else if(tag.rfind("ping:",0)==0){browser_pings[hub_field(&d.root,"address")]=hub_integer(&d.root,"ping_ms");}
  else if(tag=="servers"){public_servers.clear();const VALUE *list=value_dict_get(&d.root,"servers");for(size_t i=0;list&&i<std::min(value_array_size(list),(size_t)128);i++){const VALUE *r=value_array_get(list,i);BrowserServer s;s.id=hub_field(r,"id");s.name=hub_field(r,"name");s.address=hub_field(r,"endpoint");s.engine=hub_field(r,"engine");s.map=hub_field(r,"map");s.content=hub_field(r,"mods");if(s.content.size()!=64||!std::all_of(s.content.begin(),s.content.end(),[](unsigned char c){return std::isxdigit(c);}))s.content.clear();s.kind=hub_field(r,"kind");s.status=hub_field(r,"status");s.region=hub_field(r,"region");s.players=hub_integer(r,"players");s.capacity=hub_integer(r,"capacity");s.password=hub_integer(r,"password")!=0;public_servers.push_back(s);}for(auto *saved:{&favourite_servers,&recent_servers})for(auto &entry:*saved)for(const auto &fresh:public_servers)if(entry.address==fresh.address&&entry.name==fresh.name)entry=fresh;browser_filter();hub_message=public_servers.empty()?"No public servers. LAN and direct play are available.":"Directory updated.";}
  else if(tag=="pair-start"){device_code=hub_field(&d.root,"device_code");pair_code=hub_field(&d.root,"user_code");pair_at=SDL_GetTicks()+3000;pair_deadline=SDL_GetTicks()+600000;hub_message="Approve this code in your browser.";SDL_OpenURL((std::string(kfx_hub_url())+"/index.php?route=%2Fdevice").c_str());}
  else if(tag=="pair-poll"&&!hub_field(&d.root,"token").empty()){device_code.clear();pair_code.clear();hub_message="Game connected to your account.";kfx_hub_request("me","/api/me",nullptr,1);}
  else if(tag=="cloud-signup"){cloud_ticket=hub_field(&d.root,"ticket");selected=0;hub_message="Enter the six-digit code sent to your email.";}
  else if(tag=="cloud-verify"||tag=="cloud-login"){
   if(!hub_field(&d.root,"token").empty()){
    cloud_ticket.clear();cloud_code.clear();cloud_password.clear();cloud_email.clear();
    hub_message="Signed in as "+std::string(kfx_hub_account_name())+".";
    if(page==36)close_panel();
   }
  }
  else if(tag=="join"){join_ip=hub_field(&d.root,"endpoint");pending_join=true;}
  else if(tag=="spectate-start"){std::string id=hub_field(&d.root,"stream_id");if(normalized_version(hub_field(&d.root,"engine"))!=normalized_version(VER_STRING)){hub_message="Spectator engine version differs.";return;}if(!id.empty()){kfx_hub_spectator_start(id.c_str());hub_message="Buffering a read-only spectator replay...";}}
  else if(tag=="publish")hub_message="Public lobby registered. Players can join directly.";
  else if(tag=="community"){community_text.clear();const char *keys[]={"events","friends","matches","challenges","replays"};const VALUE *rows=value_dict_get(&d.root,keys[community_tab]);for(size_t i=0;rows&&i<std::min(value_array_size(rows),(size_t)50);i++){const VALUE *r=value_array_get(rows,i);std::string title=hub_field(r,community_tab==0||community_tab==3?"title":community_tab==1?"name":community_tab==2?"map":"id");community_text+=title+"\n";std::string detail=hub_field(r,community_tab==0?"kind":community_tab==1?"status":community_tab==2?"verification":community_tab==3?"description":"sha");community_text+=detail+"\n\n";}if(community_text.empty())community_text="Nothing here yet. Create events and manage your profile on the website.";}
 }
 if(kfx_hub_spectator_ready()){close_panel();snprintf(game.packet_fname,sizeof(game.packet_fname),"%s",kfx_hub_spectator_file());game.packet_load_enable=true;game.packet_load_initialized=false;frontend_set_state(FeSt_PACKET_DEMO);}
 if(!device_code.empty()&&SDL_GetTicks()>pair_deadline){device_code.clear();pair_code.clear();hub_message="Pairing expired. Choose Sign In again.";}
 if(!device_code.empty()&&SDL_GetTicks()>=pair_at&&!kfx_hub_busy()){pair_at=SDL_GetTicks()+3000;std::string body="{\"device_code\":"+hub_quote(device_code)+"}";kfx_hub_request("pair-poll","/api/device/poll",body.c_str(),0);}
}
bool browser_action(int action){
 if(action==2110){close_panel();return true;}
 if(action==2111){choosing_local_profile=true;return play_tools_action(1200);}
 if(action==2112||action==2113){
  cloud_create=action==2112;cloud_password.clear();cloud_code.clear();cloud_ticket.clear();
  hub_message.clear();open_panel(36);return true;
 }
 if(action>=2101&&action<=2103){start_edit(13+action-2100);return true;}
 if(action==2104){cloud_avatar=cloud_avatar%15+1;return true;}
 if(action==2107){start_edit(17);return true;}
 if(action==2105){
  if(kfx_hub_busy())return true;
  std::string body,tag,route;
  if(!cloud_ticket.empty()){
   if(cloud_code.size()!=6){hub_message="Enter your six-digit verification code.";return true;}
   tag="cloud-verify";route="/api/cloud/verify";
   body="{\"ticket\":"+hub_quote(cloud_ticket)+",\"code\":"+hub_quote(cloud_code)+"}";
  }else{
   if(cloud_name.empty()||cloud_password.size()<12||(cloud_create&&cloud_email.empty())){
    hub_message="Enter your account details and at least 12 password characters.";return true;
   }
   tag=cloud_create?"cloud-signup":"cloud-login";
   route=cloud_create?"/api/cloud/signup":"/api/cloud/login";
   body="{\"name\":"+hub_quote(cloud_name)+",\"password\":"+hub_quote(cloud_password);
   if(cloud_create)body+=",\"email\":"+hub_quote(cloud_email)+",\"avatar\":"+std::to_string(cloud_avatar);
   body+="}";
  }
  if(kfx_hub_request(tag.c_str(),route.c_str(),body.c_str(),0)){
   cloud_password.clear();hub_message="Contacting the account service...";
  }
  return true;
 }
 if(action==2000){page=20;selected=0;browser_tab=0;browser_filter();browser_refresh();return true;}
 if(action>=2001&&action<=2004){browser_tab=action-2001;browser_index=0;browser_filter();browser_refresh();return true;}
 if(action>=2010&&action<2013){int index=(browser_index/3)*3+action-2010;if(index<(int)browser_servers.size()){browser_index=index;server_password.clear();page=21;selected=0;}return true;}
 if(action==2020){browser_refresh();return true;}
 if(action==2021){start_edit(6);return true;}
 if(action==2022){page=23;selected=0;return true;}
 if(action==2023){page=22;selected=0;return true;}
 if(action==2024){page=24;selected=0;community_request();return true;}
 if(action==2025){if(network_is_active()||game.game_kind==GKind_LocalGame){hub_message="Return to the main menu first.";return true;}page=11;selected=0;return true;}
 if(action==2027){browser_sort=(browser_sort+1)%4;browser_filter();return true;}
 if(action==2026){browser_compatible=!browser_compatible;browser_filter();return true;}
 if(action==2030||action==2031){browser_index=std::clamp(browser_index+(action==2030?-3:3),0,std::max(0,(int)browser_servers.size()-1));browser_filter();return true;}
 if(action==2040){if(browser_servers.empty())return true;const auto &s=browser_servers[browser_index];if(network_is_active()||game.game_kind==GKind_LocalGame){hub_message="Return to the main menu before joining.";return true;}if(!s.engine.empty()&&normalized_version(s.engine)!=normalized_version(VER_STRING)){hub_message="Engine differs. Use the same game version before joining.";return true;}if(!s.content.empty()){start_content_scan(s.map,false);if(content_scan.valid()){content_join_action=action;content_join_id=s.id;hub_message="Checking campaign, map, configuration and mods before joining...";return true;}if(!content_error.empty()){hub_message=content_error;return true;}if(content_value!=s.content){hub_message="Content differs: campaign/map, configuration or mods. Use the host's content profile.";return true;}}browser_join_id=s.id;browser_join_content=s.content.empty()?"":content_value;if(s.id.empty()){join_ip=s.address;pending_join=true;}else{std::string body="{\"engine\":"+hub_quote(normalized_version(VER_STRING))+",\"content\":"+hub_quote(browser_join_content)+"}";kfx_hub_request("join",("/api/servers/"+s.id+"/join").c_str(),body.c_str(),1);}recent_servers.erase(std::remove_if(recent_servers.begin(),recent_servers.end(),[&](const BrowserServer &r){return r.address==s.address;}),recent_servers.end());recent_servers.insert(recent_servers.begin(),s);if(recent_servers.size()>32)recent_servers.resize(32);browser_save("servers-recent.cfg",recent_servers);return true;}
 if(action==2044){start_edit(10);return true;}
 if(action==2045&&!browser_servers.empty()){if(network_is_active()||game.game_kind==GKind_LocalGame){hub_message="Return to the main menu before spectating.";return true;}kfx_hub_request("spectate-start",("/api/servers/"+browser_servers[browser_index].id+"/spectate").c_str(),nullptr,0);return true;}
 if(action==2042&&!browser_servers.empty()){BrowserServer s=browser_servers[browser_index];auto i=std::find_if(favourite_servers.begin(),favourite_servers.end(),[&](const BrowserServer &v){return v.address==s.address;});if(i==favourite_servers.end())favourite_servers.push_back(s);else favourite_servers.erase(i);hub_message=browser_save("servers-favourites.cfg",favourite_servers)?"Favourites updated.":"Could not save favourites. Check folder permissions.";browser_filter();return true;}
 if(action==2043&&!browser_servers.empty()){SDL_SetClipboardText(browser_servers[browser_index].address.c_str());hub_message="Server address copied.";return true;}
 if(action==2050){if(!kfx_hub_request("pair-start","/api/device/start","{}",0))hub_message="Wait for the current request.";return true;}
 if(action==2051){SDL_OpenURL((std::string(kfx_hub_url())+"/index.php?route=%2Fprofile").c_str());return true;}
 if(action==2052){device_code.clear();pair_code.clear();kfx_hub_sign_out();hub_message="Signed out on this game.";return true;}
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
 if(page==37){
  const char *active=getenv("KFX_CONTENT_PROFILE");
  const std::string current=active?active:"default";
  button(40,108,560,"Play as "+std::string(kfx_hub_account_name()[0]?kfx_hub_account_name():current.c_str()),2110,selected==0,45);
  button(40,177,560,"Create or Change Local Profile",2111,selected==1,45);
  button(40,246,560,"Create Cloud Profile",2112,selected==2,45);
  button(40,315,560,"Sign In to Cloud Profile",2113,selected==3,45);
  return;
 }
 if(page==36){
  if(!cloud_ticket.empty()){
   button(24,135,592,"Code: "+cloud_code+(editing==17?"|":""),2107,selected==0,42);
   button(24,230,592,"Verify Email",2105,selected==1,42);
  }else{
   button(24,96,592,"Name: "+cloud_name+(editing==14?"|":""),2101,selected==0,40);
   if(cloud_create)button(24,148,592,"Email: "+cloud_email+(editing==15?"|":""),2102,selected==1,40);
   button(24,200,592,"Password: "+std::string(cloud_password.size(),'*')+(editing==16?"|":""),2103,selected==(cloud_create?2:1),40);
   if(cloud_create){
    static SDL_Surface *atlas=nullptr;
    if(!atlas){const char *resources=getenv("KFX_APP_RESOURCES");if(resources)atlas=IMG_Load((std::string(resources)+"/avatar-presets.png").c_str());}
    const int avatar=cloud_avatar-1;
    if(atlas){
     const int w=atlas->w/5,h=atlas->h/3;
     SDL_Rect from={(avatar%5)*w,(avatar/5)*h,w,h},to={24,254,68,68};
     SDL_BlitSurfaceScaled(atlas,&from,canvas,&to,SDL_SCALEMODE_LINEAR);
    }
    button(106,259,510,"Change Avatar  "+std::to_string(cloud_avatar)+"/15",2104,selected==3,48);
   }
   button(24,335,592,cloud_create?"Send Verification Code":"Sign In",2105,selected==(cloud_create?4:2),42);
  }
  small_line(24,397,hub_message,592);return;
 }
 if(page==20){
  button(24,91,322,"Search: "+browser_search+(editing==6?"|":""),2021,editing==6||selected==10,34);
  const char *sorts[]={"By Name","By Ping","By Players","By Map"};
  button(360,91,120,sorts[browser_sort],2027,selected==11,34);
  button(496,91,120,browser_compatible?"Matching":"All versions",2026,selected==12,34);
  const int first=(browser_index/3)*3;
  for(int i=0;i<3&&first+i<(int)browser_servers.size();i++){
   const auto &server=browser_servers[first+i];
   const char *type=server.kind=="community"?"Dedicated":server.kind=="dedicated"?"Verified dedicated":"Player hosted";
   const std::string ping=browser_pings.count(server.address)&&browser_pings[server.address]>=0?
       std::to_string(browser_pings[server.address])+" ms":"Ping unavailable";
   button(24,138+i*65,592,server.name+"  "+std::to_string(server.players)+"/"+std::to_string(server.capacity),2010+i,selected==i,35);
   small_line(36,174+i*65,std::string(type)+"  /  "+(server.map.empty()?"Map pending":server.map)
       +"  /  "+(server.status=="playing"?"In progress":"Lobby")+(server.password?"  /  Password":""),568);
   small_line(36,188+i*65,server.engine+"  /  "+server.region+"  /  "+ping,568);
  }
  if(browser_servers.empty())centered(228,hub_message=="Refreshing directory..."?hub_message:"No public servers found.",22);
  button(24,341,108,"Prev",2030,selected==3,34);button(144,341,108,"Next",2031,selected==4,34);
  button(264,341,108,"Refresh",2020,selected==5,34);button(384,341,108,"Host",2022,selected==6,34);
  button(504,341,112,"Account",2023,selected==7,34);
  button(24,382,284,"Community",2024,selected==8,32);button(332,382,284,"Direct Connect",2025,selected==9,32);
 }else if(page==21&&!browser_servers.empty()){const auto &s=browser_servers[browser_index];centered(88,s.name,24);small_line(24,126,s.address+" / "+s.engine,592);small_line(24,151,s.map+" / "+s.region+(s.password?" / Password required":""),592);button(64,180,512,"Password: "+std::string(server_password.size(),'*'),2044,selected==0,32);button(64,219,512,"Join Server",2040,selected==1,38);button(64,271,512,"Toggle Favourite",2042,selected==2,38);button(64,321,248,"Copy Address",2043,selected==3,38);button(328,321,248,"Spectate",2045,selected==4,38);
 }else if(page==22){centered(94,kfx_hub_account_name()[0]?std::string("Signed in: ")+kfx_hub_account_name():"Optional community account",24);if(!pair_code.empty()){centered(137,"Pairing code: "+pair_code,28);small_line(24,180,"Approve this code on your own browser only.",592);}else small_line(24,148,"LAN and direct play do not require an account.",592);button(64,211,512,"Sign In with Browser",2050,selected==0,40);button(64,261,512,"Open Website / Profile",2051,selected==1,40);button(64,311,512,"Sign Out",2052,selected==2,40);button(64,361,512,"Community Service",2053,selected==3,40);
 }else if(page==23){button(24,98,592,"Name: "+host_title+(editing==7?"|":""),2060,selected==0,38);button(24,151,592,"Address: "+host_address+(editing==8?"|":""),2061,selected==1,38);button(24,204,592,"Password: "+std::string(server_password.size(),'*'),2063,selected==2,38);button(24,257,592,"Preview map: "+(host_map.empty()?"Unselected":host_map)+(editing==13?"|":""),2064,selected==3,38);small_line(24,308,"Example map: classic:52. Final map is checked in the lobby.",592);button(64,350,512,"Create / Publish Lobby",2062,selected==4,42);
 }else if(page==24){const char *names[]={"Events","Friends","Match History","Co-op Challenges","Replays"};button(24,90,108,"Prev",2080,selected==0,36);centered(99,names[community_tab],24);button(508,90,108,"Next",2081,selected==1,36);std::vector<std::string> lines;std::istringstream stream(community_text);std::string line;while(std::getline(stream,line))lines.push_back(line);int limit=5;community_offset=std::min(community_offset,std::max(0,(int)lines.size()-limit));for(int i=0;i<limit&&community_offset+i<(int)lines.size();i++)text(24,150+i*37,lines[community_offset+i],20,592);button(24,351,160,"Up",2083,selected==2,36);button(216,351,208,"Open Website",2082,selected==3,36);button(456,351,160,"Down",2084,selected==4,36);
 }else if(page==25){small_line(24,103,"Internet service URL (custom servers supported)",592);button(24,157,592,hub_edit+(editing==9?"|":""),2070,selected==0,48);small_line(24,231,"Remote services require HTTPS. Account tokens are",592);small_line(24,268,"cleared when you change the service address.",592);button(64,325,512,"Save Service Address",2071,selected==1,44);}
 if(page==21&&!browser_servers.empty()&&hub_message=="Directory updated."){const auto &s=browser_servers[browser_index];hub_message=s.content.empty()?"Content unverified; map readiness is checked in the lobby.":content_scan.valid()?"Checking local content...":!content_error.empty()?content_error:content_value==s.content?"Engine/content fingerprint matches.":"Content differs. Join blocked until profiles match.";}
 if(page>=20&&page<=25)small_line(24,402,hub_message,592);
}

bool browser_join_failed(){hub_message="Connection failed. Check the server address, password and UDP firewall rules.";return false;}
