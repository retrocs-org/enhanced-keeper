#include "../../src/macos_game_options.cpp"
#include <cassert>
#include <fstream>
#include "../../src/macos_window_placement.h"
static int menu_click_count=0;
extern "C" {
void do_sound_menu_click(){menu_click_count++;}
int GetRemoteUserCount(){return 0;}
TbBool network_is_host(){return true;}
struct NetState netstate={};
struct GameCampaign campaign={};
LevelNumber get_loaded_level_number(){return 1;}
struct TbNetworkSessionNameEntry lan_sessions[LAN_SESSIONS_MAX]={};int lan_session_count=0;
void lan_refresh_sessions(){}
void send_network_chat_message(NetUserId,const char*){}
void frontnet_session_create(struct GuiButton*){}
struct Game game={};
struct GameSettings settings={};
struct StartupParameters start_params={};
struct DisplayStruct lbDisplay={};
unsigned char lbKeyOn[KC_LIST_END]={};
TbKeyCode lbInkey=KC_UNASSIGNED;
unsigned char last_used_input_device=ID_Keyboard_Mouse;
int frontend_menu_state=FeSt_MAIN_MENU;
int32_t fps_limit_main=0,fps_limit_secondary=0;
int debug_display_frametime=0;
struct FrametimeMeasurements frametime_measurements={};
unsigned long GetPing(int){return 0;}
const char *kfx_network_recovery_status(void){return "";}
GameTurn get_gameturn(){return 0;}
int kfx_network_retry_now(){return 0;}
TbBool load_game_save_catalogue(){return true;}
TbBool load_settings(){return true;}
struct TbSpriteSheet *frontend_font[FRONTEND_FONTS_COUNT]={},*winfont=nullptr;
unsigned long key_modifiers=0;
TbControllerButtons controller_button_state=0;
short is_key_pressed(TbKeyCode key,TbKeyMods){return lbKeyOn[key];}
void clear_key_pressed(long key){lbKeyOn[key]=0;}
static std::string test_folder,art_folder,render_file;
const char *PlatformManager_GetUserPrefDir(void){return test_folder.c_str();}
char keeper_runtime_directory[152]={};
uint16_t enet_port=5556;
static bool online=false;
static long loaded_slot=-1;
static int save_settings_count=0;
char *prepare_file_path(short group,const char *file){static std::string result;result=(group==FGrp_FxData?art_folder:test_folder)+"/"+file;return result.data();}
TbBool network_is_active(void){return online;}
void clear_controller_input(void){}
TbBool controller_connected(void){return true;}
static int test_device_model=0;
const char *controller_device_name(void){return "Fixture Xbox";}
int controller_device_model(void){return test_device_model;}
unsigned int controller_active_id(void){return 42;}
int controller_device_count(void){return 2;}
unsigned int controller_device_id(int i){return i>=0&&i<2?42+i:0;}
const char *controller_name_for_id(unsigned int id){return id==42?"Fixture Xbox":"Fixture DualSense";}
TbBool controller_select(unsigned int id){return id==42||id==43;}
void controller_prefer_guid(const char*){}
void controller_guid_for_id(unsigned int,char *buffer,int size){snprintf(buffer,size,"00000000000000000000000000000001");}
void controller_suppress_until_neutral(void){}
static PlayerInfo fixture_player={};
unsigned char my_player_number=0;
PlayerInfo *get_player_f(PlayerNumber,const char*){return &fixture_player;}
void set_menu_mode(long){}
TbBool save_catalogue_slot_disable(unsigned int){return true;}
void kfx_readiness_tick(void){}
void kfx_readiness_check(void){}
void kfx_readiness_override(void){}
void kfx_readiness_cancel(void){}
const char *kfx_readiness_report(void){return "Fixture lobby readiness";}

ConfigInfo net_config_info={};
char net_player_name[20]={},tmp_net_player_name[24]={};
void net_load_config_file(void){strcpy(net_config_info.net_player_name,"Fixture");}
void frontnet_session_set_player_name(GuiButton*){snprintf(net_player_name,sizeof(net_player_name),"%s",tmp_net_player_name);}
void gui_switch_video_mode(GuiButton*);
TbBool controller_owns_event(unsigned int id){return id==42;}
void controller_rumble(long){}
void keyboardControl(unsigned int,TbKeyCode,TbKeyMods,int){}
void mouseControl(unsigned int,TbPoint*){}
short save_settings(void){save_settings_count++;return 1;}
TbBool load_game(long slot){loaded_slot=slot;return true;}
void reset_autosave_timer(void){}
unsigned char frontend_palette[768]={};
struct TbSpriteSheet *load_spritesheet(const char*,const char*){return nullptr;}
long LbFileLengthRnc(const char*){return 0;}
long LbFileLoadAt(const char*,void*){return 0;}
const struct TbSprite *get_sprite(const struct TbSpriteSheet*,long){return nullptr;}
const struct TbSprite *get_frontend_sprite(short){return nullptr;}
TbResult RendererPaletteGet(unsigned char *palette){memset(palette,0,768);return 1;}
unsigned short RendererGetDrawFlags(void){return 0;}
TbResult LbDrawBox(long,long,unsigned long,unsigned long,TbPixel){return 1;}
uint32_t network_lobby_ping=0;
struct TbNetworkSessionNameEntry *net_session[SESSION_ENTRIES_COUNT]={};
long net_number_of_sessions=0,net_session_index_active=-1;
const char *matchmaking_error_text(void){return "";}
const char *matchmaking_last_error(void){return "";}
short setup_network_service(enum FrontendNetService){return 0;}
void frontnet_service_setup(void){}
void frontnet_session_setup(void){}
void frontnet_session_join(struct GuiButton*){}
TbError LbNetwork_Stop(void){return 0;}

void LbGrabMouseCheck(long){}
void LbSetMouseGrab(TbBool){}
void frontend_draw_button(struct GuiButton*,unsigned short,const char*,unsigned int){}
TbBool MinimalResolutionSetup=true;
static TbScreenMode active_mode=2;
static bool fail_display=false;
TbScreenMode LbScreenActiveMode(void){return active_mode;}
TbScreenModeInfo *LbScreenGetModeInfo(TbScreenMode){static TbScreenModeInfo info={};info.Width=1280;info.Height=800;info.VideoFlags=Lb_VF_WINDOWED;return &info;}
void set_macos_hud_scale(int){}
static unsigned long registered_fullscreen_flags;
TbScreenMode LbRegisterVideoMode(const char *, TbScreenCoord, TbScreenCoord, unsigned short, unsigned long flags){
    registered_fullscreen_flags=flags;
    return 3;
}
TbScreenMode LbRegisterVideoModeString(const char *mode){return !strcmp(mode,"DESKTOP")?3:2;}
static TbScreenMode configured_frontend=3,configured_movies=2;
TbScreenMode get_frontend_vidmode(void){return configured_frontend;}
TbScreenMode get_movies_vidmode(void){return configured_movies;}
TbScreenMode get_game_vidmode(uint){return active_mode;}
void set_movies_vidmode(TbScreenMode mode){configured_movies=mode;}
void set_frontend_vidmode(TbScreenMode mode){configured_frontend=mode;}
void set_game_vidmode(uint,TbScreenMode){}
TbScreenMode setup_screen_mode_minimal(TbScreenMode mode){if(fail_display)return 0;active_mode=mode;return mode;}
TbScreenMode setup_screen_mode(TbScreenMode mode,TbBool){return setup_screen_mode_minimal(mode);}
void init_gui(void){}
void reinit_all_menus(void){}

FrontendMenuState frontend_set_state(FrontendMenuState state){frontend_menu_state=state;return state;}
RendererType RendererGetActiveType(void){return RENDERER_SOFTWARE;}
const char *log_file_name="keeperfx.log";
extern "C" { const char *kfx_join_failure_reason=""; }
TbScreenCoord RendererScreenWidth(void){return 960;}
TbScreenCoord RendererScreenHeight(void){return 720;}
TbBool RendererPresentImage(const RendererPresentImageDesc *desc){SDL_Surface *surface=SDL_CreateSurfaceFrom(desc->src_w,desc->src_h,SDL_PIXELFORMAT_RGBA32,(void*)desc->src,desc->src_pitch);assert(surface);assert(IMG_SavePNG(surface,render_file.c_str()));SDL_DestroySurface(surface);return 1;}
void RendererSetDrawFlags(unsigned short){}
TbBool LbTextSetFont(const struct TbSpriteSheet*){return 1;}
int LbTextSetWindow(int,int,int,int){return 1;}
int LbTextLineHeight(void){return 20;}
TbBool LbTextDrawResized(int,int,int,const char*){return 1;}
}
static void key(SDL_Keycode code){SDL_Event event={};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;assert(kfx_options_event(&event));}
static void pad(Uint8 button){SDL_Event event={};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=42;event.gbutton.button=button;assert(kfx_options_event(&event));}
extern "C" TbControllerButtons controller_raw_buttons(void){return 0;}
extern "C" float controller_raw_axis(int){return 0;}
int main(){
 char folder[]="/tmp/kfx-ingame-options-XXXXXX";assert(mkdtemp(folder));test_folder=folder;setenv("KFX_USER_DATA_DIR",test_folder.c_str(),1);snprintf(keeper_runtime_directory,sizeof(keeper_runtime_directory),"%s/GameData",folder);SDL_CreateDirectory(keeper_runtime_directory);art_folder=getenv("KFX_TEST_ART")?getenv("KFX_TEST_ART"):"macos/packaging/controllers";
 kfx_options_initialize();assert(!actions.empty());assert(configured_movies==configured_frontend);
 frontend_menu_state=FeSt_MAIN_MENU;game.game_kind=GKind_Unset;enhancement_tick();assert(!kfx_controller_guide_visible);
 game.game_kind=GKind_LocalGame;frontend_menu_state=FeSt_INITIAL;kfx_options_open(nullptr);assert(kfx_controller_guide_visible);assert(game.operation_flags&GOF_Paused);
 for(int i=0;i<30;i++){page=i;render_file="/tmp/kfx-options-page-"+std::to_string(i)+".png";kfx_options_draw();}
 page=26;kfx_controller_guide_visible=1;pad(SDL_GAMEPAD_BUTTON_EAST);assert(page==26);calibration_owner=42;calibration_peak=.08f;calibration_until=SDL_GetTicks()?SDL_GetTicks():1;SDL_Delay(2);feature_tick();assert(!calibration_until);assert(std::abs(kfx_controller_deadzone()-.11f)<.001f);
 calibration_peak=.8f;calibration_until=SDL_GetTicks();feature_tick();assert(std::abs(kfx_controller_deadzone()-.11f)<.001f);assert(notice.find("movement")!=std::string::npos);
 calibration_owner=999;calibration_until=SDL_GetTicks()+100;feature_tick();assert(!calibration_until);
 page=0;float before=kfx_controller_deadzone();change(1,1);assert(kfx_controller_deadzone()>before);close_panel();assert(!(game.operation_flags&GOF_Paused));
 game.packet_load_enable=1;game.packet_fopened=1;open_panel(28);assert(!(game.operation_flags&GOF_Paused));close_panel();game.packet_load_enable=0;game.packet_fopened=0;
 online=true;kfx_options_open(nullptr);assert(!(game.operation_flags&GOF_Paused));close_panel();online=false;
 game.operation_flags|=GOF_Paused;kfx_options_open(nullptr);close_panel();assert(game.operation_flags&GOF_Paused);game.operation_flags=0;
 open_panel(3);int before_click=menu_click_count;activate(402);assert(menu_click_count==before_click+1);pad(SDL_GAMEPAD_BUTTON_EAST);assert(capturing&&captured==CBtn_B);pad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);key(SDLK_RETURN);assert(settings.kbkeys[0].controller_buttons==(CBtn_B|CBtn_LEFTSHOULDER));assert(save_settings_count==1);
 preset=0;page=1;change(0,1);assert(fps_limit_main==30);change(1,1);assert(debug_display_frametime==1);close_panel();
 preset=0;debug_display_frametime=0;kfx_options_initialize();assert(preset==1&&debug_display_frametime==1);
 FILE *file=fopen((test_folder+"/fx1g0005.sav.backup1").c_str(),"wb");assert(file);FileChunkHeader header={sizeof(CatalogueEntry),SGC_InfoBlock,1};CatalogueEntry entry={};strcpy(entry.textname,"Backup test");strcpy(entry.campaign_name,"Test campaign");entry.level_num=7;fwrite(&header,sizeof(header),1,file);fwrite(&entry,sizeof(entry),1,file);fclose(file);
 scan_saves();assert(saves.size()==1&&saves[0].backup==1&&saves[0].level==7);open_panel(2);render_file="/tmp/kfx-options-page-2.png";kfx_options_draw();load_selected();assert(page==29&&loaded_slot==-1);render_file="/tmp/kfx-save-requirements.png";kfx_options_draw();activate(1401);assert(loaded_slot==0);assert(access((test_folder+"/fx1g0000.sav").c_str(),F_OK)==0);assert(access((test_folder+"/fx1g0005.sav.backup1").c_str(),F_OK)==0);
 assert(!kfx_allow_cursor_grab(1,0,0));assert(kfx_allow_cursor_grab(1,1,0));assert(!kfx_allow_cursor_grab(1,1,1));
 kfx_window_cursor_lock=1;assert(kfx_allow_cursor_grab(1,0,0));assert(!kfx_allow_cursor_grab(0,0,0));assert(!kfx_allow_cursor_grab(1,0,1));kfx_window_cursor_lock=0;
 const auto initial_movies=configured_movies,initial_frontend=configured_frontend;
 assert(!kfx_options_set_display(0,9999,9999));assert(kfx_options_set_display(0,800,600));kfx_options_apply_display();assert(display_confirmation&&!pending_display);assert(saved_display_mode==-1);assert(configured_movies==active_mode);
 display_deadline=0;kfx_options_apply_display();assert(!display_confirmation&&saved_display_mode==-1);assert(resolution_index==2);assert(configured_movies==initial_movies&&configured_frontend==initial_frontend);
 assert(kfx_options_set_display(0,800,600));kfx_options_apply_display();assert(kfx_options_confirm_display(1));assert(saved_display_mode==0&&saved_resolution==0);
 assert(kfx_options_set_display(1,0,0));
 hud_scale=80;kfx_options_apply_display();
 assert(display_confirmation&&active_hud_scale==80&&(registered_fullscreen_flags&Lb_VF_NATIVE_PIXELS));
 display_deadline=0;kfx_options_apply_display();
 assert(!display_confirmation&&active_hud_scale==100&&saved_hud_scale==100);
 assert(kfx_options_set_display(1,1920,1080));
 kfx_options_apply_display();
 assert(display_confirmation&&(registered_fullscreen_flags&Lb_VF_FIXED_RENDER));
 assert(kfx_options_confirm_display(0));kfx_options_apply_display();
 display_mode=1;activate(500);fail_display=true;kfx_options_apply_display();assert(saved_display_mode==0);fail_display=false;
 saved_display_mode=-1;kfx_options_initialize();assert(saved_display_mode==0&&resolution_index==0);assert(configured_movies==configured_frontend);
 // The timeline preserves a snapshot when pinning a rotating backup.
 game.game_kind=GKind_Unset;frontend_menu_state=FeSt_MAIN_MENU;
 open_panel(2);scan_saves();activate(312);assert(save_timeline);
 auto recovery=std::find_if(saves.begin(),saves.end(),[](const Save &row){return row.slot==5&&row.backup==1;});
 assert(recovery!=saves.end());save_index=(int)(recovery-saves.begin());
 const auto source=recovery->path;
 activate(1008);
 const auto copy=selected_save().path;
 assert(copy!=source&&access(source.c_str(),F_OK)==0&&access((copy+".protected").c_str(),F_OK)==0);
 unlink((copy+".protected").c_str());unlink(copy.c_str());scan_saves();activate(312);assert(!save_timeline);
 open_panel(16);activate(899);assert(kfx_title_motion_preference()==1);
 activate(899);assert(kfx_title_motion_preference()==2);
 title_motion_mode=0;kfx_options_initialize();assert(kfx_title_motion_preference()==2);
 activate(899);assert(kfx_title_motion_preference()==0);
 // Search/filter and backup grouping keep one row per save slot.
 scan_saves();assert(saves.size()==2);save_search="does not exist";filter_saves();assert(saves.empty());save_search="Backup";filter_saves();assert(!saves.empty());save_search.clear();save_filter=2;filter_saves();
 save_filter=0;filter_saves();
 Save automatic=saves.front();automatic.slot=8;automatic.path=test_folder+"/fx1g0008.sav";automatic.title="Automatic save 1";automatic.automatic=true;automatic.backup=0;all_saves.push_back(automatic);save_filter=2;filter_saves();assert(saves.size()==1&&saves[0].slot==8);save_filter=1;filter_saves();for(const Save &entry:saves)assert(entry.title.find("Automatic save ")!=0);save_filter=0;filter_saves();
 std::string endpoint;assert(valid_endpoint("192.168.1.2",endpoint)&&endpoint=="192.168.1.2:5556");assert(valid_endpoint("127.0.0.1:1234",endpoint));assert(!valid_endpoint("127.0.0.1:99999",endpoint));assert(!valid_endpoint("127.0.0.1:abc",endpoint));assert(!valid_endpoint("1.2.3.999",endpoint));
 assert(save_aux_button(0)!=kfx_menu_confirm_button()&&save_aux_button(1)!=kfx_menu_back_button());
 assert(normalized_version("1.4.0")=="1.4.0.0");assert(normalized_version("1.4.x").empty());
 for(int confirm=0;confirm<4;confirm++)for(int back=0;back<4;back++)if(confirm!=back){env_number("KFX_MENU_CONFIRM_BUTTON",face_buttons[confirm]);env_number("KFX_MENU_BACK_BUTTON",face_buttons[back]);assert(save_aux_button(0)!=save_aux_button(1));for(int aux=0;aux<2;aux++)assert(save_aux_button(aux)!=kfx_menu_confirm_button()&&save_aux_button(aux)!=kfx_menu_back_button());}env_number("KFX_MENU_CONFIRM_BUTTON",1);env_number("KFX_MENU_BACK_BUTTON",2);
 open_panel(0);selected=0;pad(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(selected==1&&repeat_button==SDL_GAMEPAD_BUTTON_DPAD_DOWN);repeat_at=0;render_file="/tmp/kfx-options-controller-repeat.png";kfx_options_draw();assert(selected==2);
 SDL_Event unrelated={};unrelated.type=SDL_EVENT_GAMEPAD_BUTTON_UP;unrelated.gbutton.which=999;unrelated.gbutton.button=SDL_GAMEPAD_BUTTON_DPAD_DOWN;kfx_options_event(&unrelated);assert(repeat_button==SDL_GAMEPAD_BUTTON_DPAD_DOWN);
 SDL_Event release={};release.type=SDL_EVENT_GAMEPAD_BUTTON_UP;release.gbutton.which=42;release.gbutton.button=SDL_GAMEPAD_BUTTON_DPAD_DOWN;kfx_options_event(&release);assert(repeat_button==255);
 open_panel(3);direction_control(1);assert(focus_action==403);confirm_control();assert(settings.kbkeys[0].controller_buttons==0);
 open_panel(7);change(0,1);change(1,1);change(2,1);change(4,1);assert(kfx_text_scale>1&&kfx_cursor_scale>1&&!kfx_edge_scroll&&kfx_mentor_captions);
 kfx_caption_speech(SMsg_HeartUnderAttack);assert(!caption.empty()&&caption_until>SDL_GetTicks());
 render_file="/tmp/kfx-options-access-large.png";kfx_options_draw();close_panel();
 kfx_text_scale=1;kfx_cursor_scale=1;kfx_edge_scroll=1;kfx_mentor_captions=0;kfx_options_initialize();assert(kfx_text_scale>1&&kfx_cursor_scale>1&&!kfx_edge_scroll&&kfx_mentor_captions);
 open_panel(5);display_mode=1;activate(500);kfx_options_apply_display();assert(display_confirmation);render_file="/tmp/kfx-options-confirm-display.png";kfx_options_draw();key(SDLK_ESCAPE);assert(rollback_display);kfx_options_apply_display();assert(!display_confirmation&&!rollback_display);
 close_panel();
 std::string helper=test_folder+"/connection-check";FILE *script=fopen(helper.c_str(),"w");assert(script);fputs("#!/bin/sh\nprintf 'Lobby service: fixture connected\\nLocal UDP port 5556: available.\\n'\n",script);fclose(script);assert(chmod(helper.c_str(),0700)==0);setenv("KFX_APP_RESOURCES",test_folder.c_str(),1);
 start_diagnostics();assert(diagnostic_process);for(int i=0;i<100&&diagnostic_process;i++){process_network();SDL_Delay(10);}assert(!diagnostic_process);assert(network_report.find("fixture connected")!=std::string::npos);
 game.game_kind=GKind_Unset;join_ip="127.0.0.1:5556";pending_join=true;process_network();assert(!pending_join&&notice.find("could not start")!=std::string::npos);close_panel();
 open_panel(5);display_mode=1;activate(500);kfx_options_apply_display();assert(display_confirmation);close_panel();assert(rollback_display);kfx_options_apply_display();assert(!rollback_display);
 assert(kfx_options_set_display(0,800,600));kfx_options_apply_display();assert(display_confirmation);int confirmed=saved_resolution;display_deadline=0;assert(!kfx_options_confirm_display(1));assert(rollback_display&&saved_resolution==confirmed);kfx_options_apply_display();assert(!display_confirmation);
 // Section chooser and nested pages preserve Back navigation without closing the panel.
 open_panel(8);assert(page==8);direction_control(1);assert(selected==4);navigate(1);assert(selected==5);confirm_control();assert(page==4);confirm_control();assert(guide_more);confirm_control();assert(guide_section==2);confirm_control();assert(guide_section==0&&!guide_more);activate(901);assert(page==8&&kfx_controller_guide_visible);activate(901);assert(!kfx_controller_guide_visible);
 open_panel(2);confirm_control();assert(page==12);activate(901);assert(page==2);open_panel(6);activate(606);assert(page==10);activate(901);assert(page==6);
 // The cursor is composited above the exclusive menu, including letterboxed windows.
 SDL_Surface *pointer=SDL_CreateSurface(4,4,SDL_PIXELFORMAT_RGBA32);assert(pointer);SDL_FillSurfaceRect(pointer,nullptr,SDL_MapSurfaceRGB(pointer,255,0,255));
 for(const auto &dims:std::vector<std::pair<int,int>>{{1280,960},{1280,800},{800,1000}}){rect(0,0,WIDTH,HEIGHT,0,0,0);draw_menu_cursor(pointer,dims.first/2.0f,dims.second/2.0f,dims.first,dims.second);const Uint8 *pixel=(const Uint8*)canvas->pixels+(HEIGHT/2)*canvas->pitch+(WIDTH/2)*4;assert(pixel[0]==255&&pixel[1]==0&&pixel[2]==255);}
 SDL_DestroySurface(pointer);
 // Use the legacy graphics callback, ensuring resolution opens the real display panel.
 gui_switch_video_mode(nullptr);assert(page==5&&kfx_controller_guide_visible);
 // Original eight-frame highlight animation runs at the same 100ms cadence.
 const int animation[]={0,1,2,3,4,3,2,1,0};for(int i=0;i<9;i++)assert(button_animation_frame(i*100)==animation[i]);
 SDL_Point point;assert(menu_point(640,400,1280,800,point)&&point.x==320&&point.y==240);assert(!menu_point(0,0,1280,800,point));
 for(const auto &dimensions:std::vector<std::pair<int,int>>{{800,600},{1920,1080},{1512,982}}){
     const int w=dimensions.first,h=dimensions.second;
     SDL_Point logical{},retina{};
     assert(menu_point(w*.5f,h*.5f,w,h,logical));
     assert(menu_point(w,h,2*w,2*h,retina));
     assert(std::abs(logical.x-retina.x)<=1&&std::abs(logical.y-retina.y)<=1);
 }
 assert(options_density(1280,960)==2);
 assert(ensure_options_canvas(2)&&canvas->w==1280&&canvas->h==960);
 const auto physical=canvas_rect({20,30,40,50});
 assert(physical.x==40&&physical.y==60&&physical.w==80&&physical.h==100);
 assert(ensure_options_canvas(1));
 // Controller model follows the active hardware in Auto, and explicit label choices survive.
 label_model=-1;test_device_model=1;open_panel(0);kfx_options_draw();assert(kfx_controller_guide_ps5==1);test_device_model=0;kfx_options_draw();assert(!kfx_controller_guide_ps5);label_model=1;kfx_options_draw();assert(kfx_controller_guide_ps5);label_model=-1;
 open_panel(8);selected=10;confirm_control();assert(page==13);selected=3;confirm_control();assert(page==14);activate(901);assert(page==13);
 // Remember a validated host, reject invalid edits, and preserve the previous on-disk value.
 open_panel(11);start_edit(3);append_input("192.168.1.44:6000");finish_edit();assert(!editing&&join_ip=="192.168.1.44:6000");join_ip.clear();load_network_profile();assert(join_ip=="192.168.1.44:6000");start_edit(3);append_input("not an address");finish_edit();assert(editing==3);editing=0;load_network_profile();assert(join_ip=="192.168.1.44:6000");
 open_panel(6);activate(611);append_input("Keeper Tester");finish_edit();assert(!strcmp(net_player_name,"Keeper Tester"));
 // Feedback is independent of mentor captions; failure leaves a useful recovery explanation.
 kfx_mentor_captions=0;kfx_save_feedback(1,1,nullptr);assert(last_autosave&&save_toast=="Automatic save complete");kfx_save_feedback(0,0,"Cannot open the save file.");assert(save_error=="Cannot open the save file."&&save_toast_until>SDL_GetTicks());
 assert(file_url("/tmp/Some Folder/#")=="file:///tmp/Some%20Folder/%23");
 open_panel(9);focus_action=600;direction_control(1);assert(selected==1&&focus_action==-1);
 assert(diagnostic_report().find("Fixture Xbox")!=std::string::npos&&diagnostic_report().find(VER_STRING)!=std::string::npos);
 // A monitor removed from the left cannot strand a window offscreen; oversized windows fit.
 SDL_Rect fitted=kfx_clamp_window({-1800,100,1280,800},{0,30,1440,870});assert(fitted.x==0&&fitted.y==100&&fitted.w==1280);fitted=kfx_clamp_window({900,900,2560,1600},{0,30,1440,870});assert(fitted.w<=1440&&fitted.h<=870&&fitted.x>=0&&fitted.y>=30&&fitted.y+fitted.h<=900);
 // Exercise the actual game binding matcher and defaults for controller camera chords.
 const int camera_keys[]={Gkey_ZoomIn,Gkey_ZoomOut,Gkey_RotateCW,Gkey_RotateCCW};const TbControllerButtons directions[]={CBtn_DPAD_UP,CBtn_DPAD_DOWN,CBtn_DPAD_LEFT,CBtn_DPAD_RIGHT};
 for(int i=0;i<4;i++){int key=camera_keys[i];settings.kbkeys[key].controller_buttons=game_key_settings[key].default_controller_buttons;assert(settings.kbkeys[key].controller_buttons==(CBtn_A|directions[i]));controller_button_state=directions[i];assert(!is_game_key_pressed(key,false,false));controller_button_state=CBtn_A|directions[i];assert(is_game_key_pressed(key,false,false));}
 settings.kbkeys[Gkey_ZoomIn].controller_buttons=CBtn_LEFTSHOULDER|CBtn_DPAD_UP;controller_button_state=CBtn_LEFTSHOULDER|CBtn_DPAD_UP;assert(is_game_key_pressed(Gkey_ZoomIn,false,false));controller_button_state=CBtn_A|CBtn_DPAD_UP;assert(!is_game_key_pressed(Gkey_ZoomIn,false,false));
 // New tools preserve game bytes while editing labels and protecting/trashing groups.
 save_filter=0;save_search.clear();scan_saves();assert(!saves.empty());save_index=0;std::string base=save_base(selected_save());std::string bytes_before;{FILE *f=fopen(base.c_str(),"rb");if(f){char bytes[4096];size_t n;while((n=fread(bytes,1,sizeof(bytes),f)))bytes_before.append(bytes,n);fclose(f);}}
 if(!bytes_before.empty()){open_panel(17);activate(1000);append_input("Favourite keep");finish_edit();assert(selected_save().title=="Favourite keep");activate(1001);assert(access((base+".protected").c_str(),F_OK)==0);assert(!trash_save_group(base));activate(1001);assert(access((base+".protected").c_str(),F_OK)!=0);assert(trash_save_group(base));assert(access(base.c_str(),F_OK)!=0);scan_saves();}
 apply_access_preset(3);assert(kfx_camera_pan_scale==.5f&&!kfx_edge_scroll&&kfx_mentor_captions);apply_access_preset(1);assert(kfx_text_scale==1.5f&&kfx_cursor_scale==1.75f);apply_access_preset(0);assert(kfx_camera_pan_scale==1&&kfx_text_scale==1);
 assert(wheel_sector(0,-1)==0&&wheel_sector(1,0)==2&&wheel_sector(0,1)==4&&wheel_sector(-1,0)==6&&wheel_sector(0,0)==-1);
 fixture_player.view_type=PVT_DungeonTop;frontend_menu_state=FeSt_INITIAL;game.game_kind=GKind_LocalGame;game.operation_flags=0;close_panel();quick_wheel_enabled=true;settings.kbkeys[Gkey_MouseRight].controller_buttons=0;
 SDL_Event wheel={};wheel.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;wheel.gbutton.which=42;wheel.gbutton.button=SDL_GAMEPAD_BUTTON_RIGHT_STICK;assert(kfx_options_event(&wheel)&&wheel_open&&!(game.operation_flags&GOF_Paused));render_file="/tmp/kfx-options-quick-wheel.png";kfx_options_draw();wheel_choice=2;wheel.type=SDL_EVENT_GAMEPAD_BUTTON_UP;assert(kfx_options_event(&wheel)&&!wheel_open);feature_tick();assert(kfx_quick_game_key(Gkey_SwitchToMap));assert(!kfx_quick_game_key(Gkey_SwitchToMap));
 open_panel(0);selected=5;confirm_control();assert(page==15);activate(901);assert(page==0);open_panel(7);selected=5;confirm_control();assert(page==16);activate(901);assert(page==7);
 {VALUE status;const char *payload=kfx_options_community_status();assert(json_dom_parse(payload,strlen(payload),nullptr,0,&status,nullptr)==0);assert(value_type(&status)==VALUE_DICT);value_fini(&status);}
 assert(valid_endpoint("localhost:5556",endpoint)&&endpoint=="localhost:5556");assert(valid_endpoint("[::1]:5556",endpoint));assert(!valid_endpoint("bad host:5556",endpoint));assert(!valid_endpoint("127.0.0.1:",endpoint));assert(normalized_version(VER_STRING)=="1.4.0.0");
 BrowserServer alpha;alpha.id="fixture-id";alpha.name="Alpha";alpha.address="127.0.0.1:5556";alpha.engine="1.4.0.0";alpha.kind="listen";BrowserServer beta=alpha;beta.name="Beta";beta.address="127.0.0.1:5557";beta.engine="9.9.9.9";public_servers={alpha,beta};browser_pings[alpha.address]=1;browser_pings[beta.address]=2;browser_tab=0;browser_search.clear();browser_compatible=true;browser_filter();assert(browser_servers.size()==1);browser_compatible=false;browser_search="Beta";browser_filter();assert(browser_servers.size()==1&&browser_servers[0].name=="Beta");browser_search.clear();browser_filter();browser_index=0;favourite_servers.clear();browser_action(2042);std::vector<BrowserServer> remembered;browser_load("servers-favourites.cfg",remembered);assert(remembered.size()==1&&remembered[0].id=="fixture-id");server_password="old password";browser_action(2010);assert(page==21&&server_password.empty());open_panel(20);selected=10;confirm_control();assert(editing==6);editing=0;selected=11;confirm_control();assert(browser_sort==1);close_panel();
 mouse_navigation=false;focus_action=-1;focus_animation[2500]=123;button(1,1,100,"A",2500,true);button(110,1,100,"B",2501,true);assert(focus_animation[2500]==123&&focus_animation.count(2501));
 kfx_hub_new_match();kfx_hub_record_terminal(1,1);kfx_hub_record_departure(1);assert(kfx_hub_terminal_verified(1));kfx_hub_record_departure(2);kfx_hub_record_terminal(2,2);assert(!kfx_hub_terminal_verified(2));
 std::string isolated=test_folder+"/profile-tests";SDL_CreateDirectory(isolated.c_str());setenv("KFX_BASE_USER_DATA_DIR",isolated.c_str(),1);open_panel(27);activate(1200);assert(content_profiles.size()==1);new_profile="test-mod";assert(create_profile());assert(access((isolated+"/ContentProfiles/test-mod/EnhancedData").c_str(),F_OK)==0);activate(1204);std::ifstream chosen(isolated+"/content-profile.cfg");std::string name;std::getline(chosen,name);assert(name=="test-mod");new_profile="../outside";assert(!create_profile());render_file="/tmp/kfx-content-profiles.png";kfx_options_draw();unsetenv("KFX_BASE_USER_DATA_DIR");
 // Startup keeps every local profile and a cached cloud account reachable.
 const std::string startup_test=test_folder+"/startup-profiles";
 setenv("KFX_BASE_USER_DATA_DIR",startup_test.c_str(),1);
 const std::string cloud_id(32,'a');
 std::filesystem::create_directories(startup_test+"/CloudProfiles/"+cloud_id);
 assert(write_small_file(startup_test+"/CloudProfiles/"+cloud_id+"/profile-name","OfflineKeeper\n"));
 assert(write_small_file(startup_test+"/CloudProfiles/"+cloud_id+"/profile-summary","2026-11-25\nClassic Campaign\n3\n"));
 for(int i=1;i<=5;++i)std::filesystem::create_directories(startup_test+"/ContentProfiles/Local"+std::to_string(i));
 game.game_kind=GKind_Unset;
 choose_startup_profile();
 assert(page==37&&startup_login&&startup_profiles.size()==6);
 activate(901);assert(page==37&&kfx_controller_guide_visible);
 activate(2203);assert(startup_profile_offset==4);
 assert(startup_profiles[5].date=="2026-11-25"&&startup_profiles[5].campaign=="Classic Campaign");
 assert(startup_profiles[5].avatar==3);
 activate(2211);
 assert(!startup_login&&!kfx_controller_guide_visible);
 assert(std::string(getenv("KFX_PLAYER_PROFILE_DIR"))==startup_test+"/CloudProfiles/"+cloud_id);
 open_social_panel(7);
 assert(page==38&&party_tab==7);
 const auto previous_menu=frontend_menu_state;
 frontend_menu_state=FeSt_MAIN_MENU;
 open_social_panel(0);
 assert(social_drawer&&party_tab==0);
 const auto drawer=options_window_view(1280,800);
 SDL_Point drawer_point;
 assert(drawer.x>0&&drawer.y>0);
 assert(menu_point(drawer.x+drawer.w/2,drawer.y+drawer.h/2,1280,800,drawer_point));
 assert(std::abs(drawer_point.x-WIDTH/2)<=1&&std::abs(drawer_point.y-HEIGHT/2)<=1);
 assert(!menu_point(drawer.x-1,drawer.y,1280,800,drawer_point));
 close_panel();
 assert(!social_drawer);
 frontend_menu_state=previous_menu;
 close_panel();
 kfx_options_show_page(40);
 assert(page==37&&startup_profile_offset==4&&selected==1);
 assert(!getenv("KFX_PLAYER_PROFILE_DIR")&&active_profile_name.empty()&&!kfx_hub_has_session());
 activate(2202);
 activate(2210);
 assert(!startup_login&&active_profile_name=="Local1");
 const std::string saved_profile=startup_test+"/ContentProfiles/Local1";
 assert(write_small_file(saved_profile+"/save/preserved.txt","profile save"));
 kfx_options_show_page(41);
 assert(page==41);
 kfx_options_draw();
 assert(hits.size()==2&&hits[0].action==2263&&hits[1].action==2204);
 const auto menu_before_quit=frontend_menu_state;
 activate(2204);
 assert(frontend_menu_state==FeSt_QUIT_GAME&&!kfx_controller_guide_visible);
 frontend_menu_state=menu_before_quit;
 kfx_options_show_page(41);
 activate(2263);
 assert(page==37&&active_profile_name.empty());
 const int local_row=selected;
 activate(2250+local_row);
 activate(901);
 assert(std::filesystem::is_directory(saved_profile));
 activate(2250+selected);
 activate(2230);
 assert(!std::filesystem::exists(saved_profile)&&!getenv("KFX_PLAYER_PROFILE_DIR"));
 int archived_saves=0;
 for(const auto &entry:std::filesystem::recursive_directory_iterator(startup_test+"/RemovedProfiles"))
     if(entry.path().filename()=="preserved.txt")++archived_saves;
 assert(archived_saves==1);
 activate(2203);
 assert(startup_profiles[4].id==cloud_id);
 activate(2250);
 assert(profile_picker_dialog==ProfilePickerDialog::Delete);
 activate(2230);
 assert(!std::filesystem::exists(startup_test+"/CloudProfiles/"+cloud_id));
 assert(startup_profiles.size()==4);
 const auto menu_before_removal=frontend_menu_state;
 activate(2204);
 assert(frontend_menu_state==FeSt_QUIT_GAME&&!kfx_controller_guide_visible);
 frontend_menu_state=menu_before_removal;
 choose_startup_profile();
 setenv("KFX_CONTENT_PROFILE","Local2",1);
 setenv("KFX_APP_RESOURCES",startup_test.c_str(),1);
 activate(2250);
 activate(2230);
 assert(frontend_menu_state==FeSt_QUIT_GAME);
 std::ifstream pending_removal(startup_test+"/remove-runtime-profile.pending");
 std::string queued_profile;
 pending_removal>>queued_profile;
 assert(queued_profile=="Local2"&&std::filesystem::is_directory(startup_test+"/ContentProfiles/Local2"));
 frontend_menu_state=menu_before_removal;
 unsetenv("KFX_CONTENT_PROFILE");
 unsetenv("KFX_APP_RESOURCES");
 close_panel();
 startup_login=false;
 unsetenv("KFX_PLAYER_PROFILE_DIR");
 unsetenv("KFX_BASE_USER_DATA_DIR");
 // Replay labels/favourites are sidecars; the recorded bytes stay unchanged.
 SDL_CreateDirectory((test_folder+"/recordings").c_str());std::string replay_file=test_folder+"/recordings/library.pck";{std::ofstream out(replay_file,std::ios::binary);CatalogueEntry e={};strcpy(e.campaign_name,"Library Campaign");e.level_num=52;FileChunkHeader h={sizeof(e),SGC_InfoBlock,1};out.write((char*)&h,sizeof(h));out.write((char*)&e,sizeof(e));PacketSaveHead ph={};ph.players_exist=3;h={sizeof(ph),SGC_PacketHeader,1};out.write((char*)&h,sizeof(h));out.write((char*)&ph,sizeof(ph));h={0,SGC_PacketData,1};out.write((char*)&h,sizeof(h));out<<"packet bytes";}
 open_enhancement(30);assert(replay_library.size()==1&&replay_library[0].players==2&&replay_library[0].level==52);activate(1504);replay_title="A Favourite Replay";finish_edit();assert(replay_library[0].title=="A Favourite Replay");activate(1505);assert(replay_library[0].favourite);assert(std::filesystem::file_size(replay_file)>sizeof(CatalogueEntry));
 open_enhancement(31);activate(1513);activate(1514);assert(access(profile_file("reset-profile-next.cfg").c_str(),F_OK)==0);activate(1513);assert(access(profile_file("reset-profile-next.cfg").c_str(),F_OK)!=0);
 // New sections keep controller focus and expose reversible operations.
 for(int number=30;number<=35;number++){open_panel(number);selected=0;navigate(1);assert(selected==1);render_file="/tmp/kfx-enhancement-page-"+std::to_string(number)+".png";kfx_options_draw();activate(901);assert(page<30);}
 open_panel(7);selected=6;confirm_control();assert(page==33);float old_caption=kfx_caption_scale;activate(1531);assert(kfx_caption_scale!=old_caption);activate(1532);assert(kfx_reduce_flashes);activate(1533);assert(kfx_high_contrast);activate(1534);assert(kfx_camera_response()!=2);persist();kfx_caption_scale=1;kfx_reduce_flashes=0;kfx_options_initialize();assert(kfx_caption_scale!=old_caption&&kfx_reduce_flashes);
 open_panel(1);selected=4;confirm_control();assert(page==34);activate(1542);assert(capture_until>SDL_GetTicks());enhancement_sample();SDL_Delay(3);enhancement_sample();capture_until=1;enhancement_sample();assert(access(path("performance-capture.csv").c_str(),R_OK)==0);
 open_panel(8);selected=8;direction_control(-1);assert(selected==11);direction_control(1);assert(selected==8);
 open_panel(0);selected=0;pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(repeat_button==SDL_GAMEPAD_BUTTON_DPAD_RIGHT);repeat_at=0;kfx_options_draw();assert(repeat_at>SDL_GetTicks());repeat_button=255;
 kfx_options_recover_display();kfx_options_apply_display();assert(!display_confirmation&&saved_display_mode==0&&saved_resolution==0);
 // Stale windowed movie config must not override saved fullscreen, or vice versa.
 saved_display_mode=1;saved_resolution=2;persist();configured_frontend=2;configured_movies=2;
 kfx_options_initialize();assert(configured_movies==3&&configured_frontend==3);
 saved_display_mode=0;persist();configured_frontend=3;configured_movies=3;
 kfx_options_initialize();assert(configured_movies==2&&configured_frontend==2);
 {std::ofstream log(test_folder+"/keeperfx.log");log<<"Warning: synthetic audio warning\n";}
 const auto report=diagnostic_report();
 assert(report.find("Renderer: Software")!=std::string::npos);
 assert(report.find("synthetic audio warning")!=std::string::npos);
 puts("PASS: in-game pages, persistent options, pause ownership, multiplayer continues, physical-button chord remapping, legacy-save metadata and nondestructive backup restoration");
}
