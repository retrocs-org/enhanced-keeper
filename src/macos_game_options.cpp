// In-game Mac settings. No launcher UI is involved in normal play.
#include "pre_inc.h"
#include "macos_game_options.h"
#include "macos_controls.h"
#include "macos_save_compat.h"
#include "macos_replay.h"
#include "kfx/platform/PlatformManager.h"
#include "bflib_joyst.h"
#include "bflib_inputctrl.h"
#include "bflib_planar.h"
#include "bflib_keybrd.h"
#include "bflib_mouse.h"
#include "bflib_video.h"
#include "bflib_fileio.h"
#include "bflib_dernc.h"
#include "bflib_sprfnt.h"
#include "bflib_guibtns.h"
#include "bflib_datetm.h"
#include "front_input.h"
#include "frontend.h"
#include "config_settings.h"
#include "keeper_release.h"
#include "game_legacy.h"
#include "game_saves.h"
#include "keeperfx.hpp"
#include "vidmode.h"
#include "vidfade.h"
#include "bflib_sprite.h"
#include "bflib_vidraw.h"
#include "custom_sprites.h"
#include "sprites.h"
#include "gui_draw.h"
#include "gui_soundmsgs.h"
#include "net_matchmaking.h"
#include "net_lobby.h"
#include "net_game.h"
#include "front_network.h"
#include "frontmenu_net.h"
#include "ver_defs.h"
#include "macos_readiness.h"
#include "net_hub.h"
#include "config_campaigns.h"
#include "net_exchange_common.h"
#include "bflib_enet.h"
#include "net_lan.h"
#include "net_main.h"
#include "json-dom.h"
#include "player_data.h"
#include "gui_frontmenu.h"
#include <arpa/inet.h>
#include <sstream>
#include <cctype>
#include "gui_frontbtns.h"
#include "kfx/renderer/RendererManager.h"
#include <SDL3/SDL.h>
#ifndef KFX_DEDICATED
#include <SDL3_image/SDL_image.h>
#endif
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <future>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <ctime>
#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif
#include "post_inc.h"

#if defined(__APPLE__) && !defined(KFX_DEDICATED)
namespace {
constexpr int WIDTH=640,HEIGHT=480;
int button_animation_frame(Uint64 elapsed);
int page=0,selected=0,save_index=0,action_index=0;
bool owns_pause=false,capturing=false,performance_overlay=false,emergency_display=false;
TbControllerButtons captured=0;
std::string notice;
SDL_Surface *canvas=nullptr,*guides[2]={},*preview=nullptr;
std::string preview_path;
std::map<std::string,SDL_Surface*> labels;
struct Hit {SDL_Rect rect;int action;};
std::vector<Hit> hits;
struct Save {std::string path,title,campaign,date;long slot,level,seconds;int backup;time_t modified;bool automatic=false;bool favourite=false;};
std::string save_base(const Save &entry);
std::string read_save_alias(const std::string &base);
bool automatic_save(const Save &entry){return entry.automatic;}
std::vector<Save> saves;
std::vector<int> actions;
const int caps[]={60,30,120,0};
int preset=0;
int display_mode=-1,resolution_index=2,saved_display_mode=-1,saved_resolution=2;
bool pending_display=false;
bool display_confirmation=false,rollback_display=false;
Uint64 display_deadline=0;
TbScreenMode rollback_mode=0;
int rollback_display_choice=0,rollback_resolution=2;
bool rollback_minimal=false;
std::string save_search,peer_version,join_ip="127.0.0.1:5556",network_report;
bool preview_large=false;
bool guide_more=false,edit_selected=false;
int guide_section=0,device_offset=0;
bool quick_wheel_enabled=true,wheel_open=false;
int wheel_choice=-1,quick_pending=-1,quick_key=-1;
float wheel_x=0,wheel_y=0;
unsigned int wheel_owner=0;
Uint64 quick_key_until=0;
std::string save_alias;
bool mouse_navigation=false;
int hovered_action=-1;
std::map<int,Uint64> focus_animation;
int label_model=-1;
std::string network_name,save_toast,save_error;
Uint64 save_toast_until=0;
time_t last_autosave=0;
int report_offset=0,help_parent=2;
bool capture_preview=false;
int editing=0,save_filter=0,backup_choice=0,focus_action=-1;
std::vector<Save> all_saves;
SDL_Process *diagnostic_process=nullptr;
Uint8 repeat_button=255;
Uint64 repeat_at=0;
std::string caption;
Uint64 caption_until=0;
bool pending_join=false;
std::string pending_login_ticket;
std::map<const unsigned char*,SDL_Surface*> skin_cache;
TbSpriteSheet *options_font=nullptr,*options_accent_font=nullptr,*options_buttons=nullptr;
SDL_Surface *options_background=nullptr;
unsigned char options_palette[768]={};
bool skin_attempted=false;
void ensure_skin(){
    if(skin_attempted)return;skin_attempted=true;
    options_font=load_font("ldata/frontft3.dat","ldata/frontft3.tab");
    options_accent_font=load_font("ldata/frontft2.dat","ldata/frontft2.tab");
    options_buttons=load_spritesheet("ldata/frontbit.dat","ldata/frontbit.tab");
    size_t size=0;void *data=SDL_LoadFile("ldata/front.pal",&size);if(data&&size>=sizeof(options_palette))memcpy(options_palette,data,sizeof(options_palette));else RendererPaletteGet(options_palette);SDL_free(data);
    long length=LbFileLengthRnc("ldata/front.raw");
    if(length==640*480){std::vector<unsigned char> pixels((size_t)length);if(LbFileLoadAt("ldata/front.raw",pixels.data())==length){
        options_background=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_RGBA32);
        if(options_background)for(int y=0;y<480;y++)for(int x=0;x<640;x++){int index=pixels[y*640+x];Uint8 *pixel=(Uint8*)options_background->pixels+y*options_background->pitch+x*4;pixel[0]=options_palette[index*3]*4;pixel[1]=options_palette[index*3+1]*4;pixel[2]=options_palette[index*3+2]*4;pixel[3]=255;}
    }}
}


const int resolutions[][2]={{800,600},{1024,640},{1280,800},{1440,900},{1920,1080},{1920,1200},{2560,1440},{2560,1600}};
constexpr int resolution_count=sizeof(resolutions)/sizeof(resolutions[0]);
TbScreenMode chosen_mode(int mode,int resolution){
    if(mode==1)return LbRegisterVideoModeString("DESKTOP");
    char name[32];snprintf(name,sizeof(name),"%dx%dw32",resolutions[resolution][0],resolutions[resolution][1]);return LbRegisterVideoModeString(name);
}
void set_modes(TbScreenMode mode){set_movies_vidmode(mode);set_frontend_vidmode(mode);for(int i=0;i<MAX_GAME_VIDMODE_COUNT;i++)set_game_vidmode(i,mode);}
void infer_display(){
    if(display_mode>=0)return;
    const TbScreenModeInfo *info=LbScreenGetModeInfo(LbScreenActiveMode());
    display_mode=info && !(info->VideoFlags & Lb_VF_WINDOWED)?1:0;
    if(info)for(int i=0;i<resolution_count;i++)if(info->Width==resolutions[i][0]&&info->Height==resolutions[i][1])resolution_index=i;
}
const char *face_names[2][4]={{"A","B","X","Y"},{"Cross","Circle","Square","Triangle"}};
const TbControllerButtons face_buttons[]={CBtn_A,CBtn_B,CBtn_X,CBtn_Y};
std::string path(const char *name){return prepare_file_path(FGrp_Save,name);}
void env_number(const char *key,double value){char text[48];snprintf(text,sizeof(text),"%.6g",value);setenv(key,text,1);}
void persist(){
    std::string dest=path("macos-options.cfg"),temp=dest+".pending";
    FILE *file=fopen(temp.c_str(),"w");if(!file){notice="Could not write options. Check your save-folder permissions.";return;}
    fprintf(file,"version=1\npreset=%d\noverlay=%d\nenhanced=%d\ncursor=%g\ndeadzone=%g\nrumble=%g\nconfirm=%llu\nback=%llu\nautosave=%s\n",preset,debug_display_frametime!=0,start_params.enhanced_mode!=0,kfx_controller_cursor_speed(),kfx_controller_deadzone(),kfx_controller_rumble_strength(),(unsigned long long)kfx_menu_confirm_button(),(unsigned long long)kfx_menu_back_button(),getenv("KFX_AUTOSAVE_SECONDS")?getenv("KFX_AUTOSAVE_SECONDS"):"300");
    fprintf(file,"display_mode=%d\nresolution=%d\nwindow_cursor_lock=%d\n",saved_display_mode,saved_resolution,kfx_window_cursor_lock);
    fprintf(file,"caption_scale=%g\nreduced_flashes=%d\nhigh_contrast=%d\ntooltip_scale=%g\ncamera_response=%g\nperformance_overlay=%d\n",kfx_caption_scale,kfx_reduce_flashes,kfx_high_contrast,kfx_tooltip_scale,kfx_camera_response(),performance_overlay);
    fprintf(file,"label_model=%d\nquick_wheel=%d\ncamera_pan_scale=%g\n",label_model,quick_wheel_enabled,kfx_camera_pan_scale);
    fprintf(file,"text_scale=%g\ncursor_scale=%g\nedge_scroll=%d\nedge_border=%d\nmentor_captions=%d\ncaption_seconds=%d\n",kfx_text_scale,kfx_cursor_scale,kfx_edge_scroll,kfx_edge_border,kfx_mentor_captions,kfx_caption_seconds);
    bool ok=fflush(file)==0 && fsync(fileno(file))==0;if(fclose(file)!=0)ok=false;
    if(!ok || rename(temp.c_str(),dest.c_str())!=0){unlink(temp.c_str());notice="Options could not be saved.";}else notice="Options saved.";
}
void close_panel(){
    if(display_confirmation){display_confirmation=false;rollback_display=true;}
    editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());repeat_button=255;focus_action=-1;
    kfx_controller_guide_visible=0;capturing=false;clear_controller_input();
    if(owns_pause){game.operation_flags &= ~GOF_Paused;owns_pause=false;}
    lbInkey=KC_UNASSIGNED;
    LbGrabMouseCheck(MG_OnFocusGained);
}
void open_panel(int new_page){
    if(!kfx_controller_guide_visible){
        owns_pause=game.game_kind==GKind_LocalGame && !game.packet_load_enable && !network_is_active() && !(game.operation_flags&GOF_Paused);
        if(owns_pause)game.operation_flags|=GOF_Paused;
        clear_controller_input();
        for(int key=0;key<KC_LIST_END;key++)if(lbKeyOn[key])keyboardControl(KActn_KEYUP,key,0,0);
        TbPoint point={lbDisplay.MMouseX,lbDisplay.MMouseY};mouseControl(MActn_LBUTTONUP,&point);mouseControl(MActn_RBUTTONUP,&point);
    }
    focus_animation.clear();for(auto &entry:labels)SDL_DestroySurface(entry.second);labels.clear();for(auto &entry:skin_cache)SDL_DestroySurface(entry.second);skin_cache.clear();
    kfx_controller_guide_visible=1;page=new_page;selected=0;capturing=false;notice.clear();infer_display();editing=0;repeat_button=255;focus_action=-1;
    LbSetMouseGrab(false);
}
void rect(int x,int y,int w,int h,Uint8 r,Uint8 g,Uint8 b){SDL_Rect area={x,y,w,h};SDL_FillSurfaceRect(canvas,&area,SDL_MapSurfaceRGB(canvas,r,g,b));}
SDL_Surface *native_sprite(const TbSprite *sprite){
    if(!sprite||!sprite->Data||!sprite->SWidth||!sprite->SHeight)return nullptr;
    auto cached=skin_cache.find(sprite->Data);if(cached!=skin_cache.end())return cached->second;
    std::vector<unsigned char> pixels(sprite->SWidth*sprite->SHeight);LbSpriteDecode(pixels.data(),sprite->SWidth,sprite->Data,sprite->SWidth,sprite->SHeight);
    const unsigned char *palette=options_palette;
    SDL_Surface *image=SDL_CreateSurface(sprite->SWidth,sprite->SHeight,SDL_PIXELFORMAT_RGBA32);if(!image)return nullptr;
    for(int y=0;y<image->h;y++)for(int x=0;x<image->w;x++){int index=pixels[y*image->w+x];Uint8 *pixel=(Uint8*)image->pixels+y*image->pitch+x*4;pixel[0]=palette[index*3]*4;pixel[1]=palette[index*3+1]*4;pixel[2]=palette[index*3+2]*4;pixel[3]=index?255:0;}
    SDL_SetSurfaceBlendMode(image,SDL_BLENDMODE_BLEND);skin_cache[sprite->Data]=image;return image;
}
SDL_Surface *native_label(const std::string &value,int size,bool accent){
    ensure_skin();TbSpriteSheet *chosen=accent?options_accent_font:options_font;TbSpriteSheet *font=chosen?chosen:(frontend_menu_state!=FeSt_INITIAL&&frontend_font[1]?frontend_font[1]:winfont);
    if(!font||value.find('@')!=std::string::npos||std::any_of(value.begin(),value.end(),[](unsigned char c){return c>=128;}))return nullptr;
    int height=1;for(unsigned char c=32;c<127;c++){const TbSprite *glyph=get_sprite(font,c-31);if(glyph)height=std::max(height,(int)glyph->SHeight);}
    int width=0;for(unsigned char c:value){const TbSprite *glyph=c>=32&&c<127?get_sprite(font,c-31):nullptr;width+=glyph?glyph->SWidth+1:height/2;}
    SDL_Surface *image=SDL_CreateSurface(std::max(1,width),height,SDL_PIXELFORMAT_RGBA32);if(!image)return nullptr;
    int x=0;for(unsigned char c:value){const TbSprite *glyph=c>=32&&c<127?get_sprite(font,c-31):nullptr;SDL_Surface *source=native_sprite(glyph);if(source){SDL_Rect to={x,0,source->w,source->h};SDL_BlitSurface(source,nullptr,image,&to);}x+=glyph?glyph->SWidth+1:height/2;}
    SDL_Surface *scaled=SDL_CreateSurface(std::max(1,width*size/height),size+3,SDL_PIXELFORMAT_RGBA32);if(scaled){SDL_BlitSurfaceScaled(image,nullptr,scaled,nullptr,SDL_SCALEMODE_NEAREST);SDL_SetSurfaceBlendMode(scaled,SDL_BLENDMODE_BLEND);}SDL_DestroySurface(image);return scaled;
}
SDL_Surface *label(const std::string &text,int size,bool accent=false){
    std::string key=std::to_string((uintptr_t)(frontend_menu_state!=FeSt_INITIAL?frontend_font[1]:winfont))+":"+std::to_string(size)+(accent?":red:":":cream:")+text;auto found=labels.find(key);if(found!=labels.end())return found->second;
    if(labels.size()>512){for(auto &entry:labels)SDL_DestroySurface(entry.second);labels.clear();}
    if(SDL_Surface *image=native_label(text,size,accent)){labels[key]=image;return image;}
#ifdef __APPLE__
    CFStringRef string=CFStringCreateWithBytes(nullptr,(const UInt8*)text.data(),text.size(),kCFStringEncodingUTF8,false);
    if(!string)return nullptr;
    CTFontRef font=CTFontCreateWithName(CFSTR("Menlo"),size,nullptr);
    CGFloat components[]={0.94,0.87,0.71,1};CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();CGColorRef color=CGColorCreate(space,components);
    const void *keys[]={kCTFontAttributeName,kCTForegroundColorAttributeName};const void *values[]={font,color};
    CFDictionaryRef attributes=CFDictionaryCreate(nullptr,keys,values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef attributed=CFAttributedStringCreate(nullptr,string,attributes);CTLineRef line=CTLineCreateWithAttributedString(attributed);
    int width=std::min(1600,std::max(1,(int)ceil(CTLineGetTypographicBounds(line,nullptr,nullptr,nullptr))+4)),height=size+12;
    SDL_Surface *surface=SDL_CreateSurface(width,height,SDL_PIXELFORMAT_RGBA32);
    if(surface){
        CGContextRef context=CGBitmapContextCreate(surface->pixels,width,height,8,surface->pitch,space,(CGBitmapInfo)kCGImageAlphaPremultipliedLast|(CGBitmapInfo)kCGBitmapByteOrder32Big);
        if(context){CGContextSetTextPosition(context,1,7);CTLineDraw(line,context);CGContextRelease(context);}else{SDL_DestroySurface(surface);surface=nullptr;}
        if(surface)SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_BLEND);
    }
    CFRelease(line);CFRelease(attributed);CFRelease(attributes);CFRelease(font);CFRelease(string);CGColorRelease(color);CGColorSpaceRelease(space);
    labels[key]=surface;return surface;
#else
    (void)text;(void)size;return nullptr;
#endif
}
std::string short_text(const std::string &value,size_t max){if(value.size()<=max)return value;size_t length=max>3?max-3:0;while(length>0&&(((unsigned char)value[length]&0xc0)==0x80))length--;return value.substr(0,length)+"...";}
void text(int x,int y,const std::string &value,int size=22,int max_width=0,bool accent=false){
    if(!max_width)max_width=WIDTH-x-20;int wanted=std::max(18,(int)round(size*kfx_text_scale));std::string shown=value;SDL_Surface *image=label(shown,wanted,accent);
    // Never make the type smaller to fit a field. Long labels are shortened.
    while(image && image->w>max_width && shown.size()>4){shown=short_text(value,shown.size()-1);image=label(shown,wanted,accent);}
    if(image){SDL_Rect to={x,y,image->w,image->h};SDL_BlitSurface(image,nullptr,canvas,&to);}
}
void centered(int y,const std::string &value,int size=22,bool accent=false){SDL_Surface *image=label(value,std::max(18,(int)round(size*kfx_text_scale)),accent);if(image){SDL_Rect to={(WIDTH-image->w)/2,y,image->w,image->h};SDL_BlitSurface(image,nullptr,canvas,&to);}}
void frame(int x,int y,int w,int h,bool active=false){
    ensure_skin();rect(x,y,w,h,105,77,38);rect(x+2,y+2,w-4,h-4,23,17,9);rect(x+4,y+4,w-8,h-8,42,29,15);
    SDL_Surface *tile=options_buttons?native_sprite(get_sprite(options_buttons,GFS_hugearea_thc_tx1_mc)):nullptr;
    if(tile&&w>12&&h>12){SDL_Rect saved,inner={x+5,y+5,w-10,h-10};SDL_GetSurfaceClipRect(canvas,&saved);SDL_SetSurfaceClipRect(canvas,&inner);SDL_SetSurfaceColorMod(tile,170,170,170);for(int py=y+5;py<y+h-5;py+=32)for(int px=x+5;px<x+w-5;px+=64){SDL_Rect to={px,py,64,32};SDL_BlitSurfaceScaled(tile,nullptr,canvas,&to,SDL_SCALEMODE_NEAREST);}SDL_SetSurfaceColorMod(tile,255,255,255);SDL_SetSurfaceClipRect(canvas,&saved);}
    if(active){rect(x,y,w,2,231,176,74);rect(x,y+h-2,w,2,231,176,74);}
}
void button(int x,int y,int w,const std::string &title,int action,bool focus=false,int height=40){
    ensure_skin();bool active=action>=0&&(mouse_navigation?action==hovered_action:(focus_action>=0?action==focus_action:focus));
    int sprite_index=GFS_hugebutton_a05l;
    if(active){auto start=focus_animation.emplace(action,SDL_GetTicks()).first;sprite_index=GFS_hugebutton_a01l+3*button_animation_frame(SDL_GetTicks()-start->second);}else focus_animation.erase(action);
    SDL_Surface *left=options_buttons?native_sprite(get_sprite(options_buttons,sprite_index)):nullptr,*middle=options_buttons?native_sprite(get_sprite(options_buttons,sprite_index+1)):nullptr,*right=options_buttons?native_sprite(get_sprite(options_buttons,sprite_index+2)):nullptr;
    if(left&&middle&&right){int cap=std::min(w/3,left->w*height/left->h);SDL_Rect l={x,y,cap,height},m={x+cap,y,w-2*cap,height},r={x+w-cap,y,cap,height};SDL_BlitSurfaceScaled(left,nullptr,canvas,&l,SDL_SCALEMODE_NEAREST);SDL_BlitSurfaceScaled(middle,nullptr,canvas,&m,SDL_SCALEMODE_NEAREST);SDL_BlitSurfaceScaled(right,nullptr,canvas,&r,SDL_SCALEMODE_NEAREST);}else frame(x,y,w,height,active);
    int size=std::max(20,std::min(height-4,(int)round(24*kfx_text_scale)));std::string shown=title;SDL_Surface *image=label(shown,size,!active&&!kfx_high_contrast&&title!="+"&&title!="-");
    while(image&&image->w>w-24&&shown.size()>4){shown=short_text(title,shown.size()-1);image=label(shown,size,!active&&!kfx_high_contrast);}
    if(image){SDL_Rect to={x+(w-image->w)/2,y+(height-image->h)/2,image->w,image->h};SDL_BlitSurface(image,nullptr,canvas,&to);}
    if(active&&kfx_high_contrast){rect(x+7,y+height/2-4,8,8,255,255,224);rect(x,y,w,2,255,255,224);rect(x,y+height-2,w,2,255,255,224);}
    hits.push_back({{x,y,w,height},action});
}
void title(const std::string &name){button(134,24,372,name,-10,false,48);if(!hits.empty())hits.pop_back();}
void background(){ensure_skin();if(options_background)SDL_BlitSurface(options_background,nullptr,canvas,nullptr);else rect(0,0,WIDTH,HEIGHT,35,25,14);}
void draw_menu_cursor(SDL_Surface *image,float mx,float my,int w,int h){
    if(!image||w<=0||h<=0)return;
    double scale=std::min(w/(double)WIDTH,h/(double)HEIGHT);
    int x=(int)((mx-(w-WIDTH*scale)/2)/scale),y=(int)((my-(h-HEIGHT*scale)/2)/scale);
    int height=(int)(32*kfx_cursor_scale);SDL_Rect to={x,y,image->w*height/image->h,height};
    SDL_BlitSurfaceScaled(image,nullptr,canvas,&to,SDL_SCALEMODE_NEAREST);
}
void menu_cursor(){
    SDL_Window *window=SDL_GetMouseFocus();if(!window)return;
    if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)){SDL_ShowCursor();return;}
    const TbSprite *sprite=options_buttons?get_sprite(options_buttons,GFS_cursor_horny):nullptr;
    SDL_Surface *image=native_sprite(sprite);if(!image){SDL_ShowCursor();return;}
    int w=0,h=0;SDL_GetWindowSize(window,&w,&h);float mx=0,my=0;SDL_GetMouseState(&mx,&my);
    if(w<=0||h<=0)return;
    double scale=std::min(w/(double)WIDTH,h/(double)HEIGHT);
    float x=(mx-(w-WIDTH*scale)/2)/scale,y=(my-(h-HEIGHT*scale)/2)/scale;
    if(x<0||y<0||x>=WIDTH||y>=HEIGHT){SDL_ShowCursor();return;}
    draw_menu_cursor(image,mx,my,w,h);SDL_HideCursor();
}
void filter_saves(){
    saves.clear();std::string query=save_search;std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return std::tolower(c);});
    for(const Save &entry:all_saves){if(entry.backup)continue;bool automatic=automatic_save(entry);if((save_filter==1&&automatic)||(save_filter==2&&!automatic)||(save_filter==3&&!entry.favourite))continue;
        std::string haystack=entry.title+" "+entry.campaign+" "+std::to_string(entry.level);std::transform(haystack.begin(),haystack.end(),haystack.begin(),[](unsigned char c){return std::tolower(c);});if(haystack.find(query)!=std::string::npos)saves.push_back(entry);
    }
    // Orphaned backup groups remain recoverable if the current save was removed.
    for(const Save &entry:all_saves)if(entry.backup){bool automatic=automatic_save(entry);if((save_filter==1&&automatic)||(save_filter==2&&!automatic)||(save_filter==3&&!entry.favourite))continue;std::string haystack=entry.title+" "+entry.campaign+" "+std::to_string(entry.level);std::transform(haystack.begin(),haystack.end(),haystack.begin(),[](unsigned char c){return std::tolower(c);});if(haystack.find(query)==std::string::npos)continue;bool grouped=false;for(const Save &current:all_saves)if(!current.backup&&current.slot==entry.slot)grouped=true;if(!grouped){bool duplicate=false;for(const Save &current:saves)if(current.slot==entry.slot)duplicate=true;if(!duplicate)saves.push_back(entry);}}
    save_index=std::clamp(save_index,0,std::max(0,(int)saves.size()-1));backup_choice=0;
}
Save selected_save(){if(saves.empty())return {};Save current=saves[save_index];if(backup_choice)for(const Save &entry:all_saves)if(entry.slot==current.slot&&entry.backup==backup_choice)return entry;return current;}
void scan_preserved_saves(){
 namespace fs=std::filesystem;int number=4;
 for(const char *directory:{"CompatibilityBackups","Trash"})try{fs::path folder=path(directory);if(fs::is_symlink(folder)||!fs::is_directory(folder))continue;
  for(const auto &item:fs::directory_iterator(folder)){if(number>=260)break;if(item.is_symlink()||!item.is_regular_file())continue;std::string name=item.path().filename().string();long slot=-1;int used=0;if(sscanf(name.c_str(),"fx1g%4ld.sav%n",&slot,&used)!=1||used!=12||slot<0||slot>=SAVE_SLOTS_LIMIT)continue;
   FILE *stream=fopen(item.path().c_str(),"rb");if(!stream)continue;FileChunkHeader header={};CatalogueEntry info={};bool ok=fread(&header,sizeof(header),1,stream)==1&&header.id==SGC_InfoBlock&&header.len==sizeof(info)&&fread(&info,sizeof(info),1,stream)==1;fclose(stream);if(!ok)continue;
   info.textname[sizeof(info.textname)-1]=0;info.campaign_name[sizeof(info.campaign_name)-1]=0;struct stat st;if(lstat(item.path().c_str(),&st))continue;char date[48];struct tm tm;localtime_r(&st.st_mtime,&tm);strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&tm);
   all_saves.push_back({item.path().string(),info.textname,info.campaign_name,date,slot,(long)info.level_num,-1,number++,st.st_mtime});
  }
 }catch(...){}
}
void scan_saves(){
    all_saves.clear();saves.clear();preview_path.clear();std::string folder=path("");DIR *dir=opendir(folder.c_str());if(!dir){notice="Could not read the save folder.";return;}
    for(dirent *entry;(entry=readdir(dir));){
        const char *name=entry->d_name;long slot=-1;int used=0;
        if(sscanf(name,"fx1g%4ld.sav%n",&slot,&used)!=1 || used!=12 || slot<0 || slot>=10000)continue;
        int backup=0;if(name[used]){int count=0;if(sscanf(name+used,".backup%d%n",&backup,&count)!=1 || backup<1 || backup>3 || name[used+count])continue;}
        std::string file=folder+"/"+name;struct stat statbuf;if(stat(file.c_str(),&statbuf)!=0 || !S_ISREG(statbuf.st_mode))continue;
        FILE *stream=fopen(file.c_str(),"rb");if(!stream)continue;FileChunkHeader header={};CatalogueEntry info={};
        bool valid=fread(&header,sizeof(header),1,stream)==1 && header.id==SGC_InfoBlock && header.len==sizeof(info) && fread(&info,sizeof(info),1,stream)==1;fclose(stream);if(!valid)continue;
        info.textname[sizeof(info.textname)-1]=0;info.campaign_name[sizeof(info.campaign_name)-1]=0;
        char date[64];struct tm tm;localtime_r(&statbuf.st_mtime,&tm);strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&tm);
        long seconds=-1;size_t size=0;char *meta=(char*)SDL_LoadFile((file+".json").c_str(),&size);
        if(meta){const char *field=strstr(meta,"\"levelSeconds\":");if(field)sscanf(field,"\"levelSeconds\":%ld",&seconds);SDL_free(meta);}
        Save saved_entry={file,info.textname,info.campaign_name,date,slot,(long)info.level_num,seconds,backup,statbuf.st_mtime};saved_entry.automatic=saved_entry.title.find("Automatic save ")==0;char base_name[40];snprintf(base_name,sizeof(base_name),"fx1g%04ld.sav",slot);std::string base=path(base_name),alias=read_save_alias(base);saved_entry.favourite=access((base+".protected").c_str(),F_OK)==0;if(!alias.empty())saved_entry.title=alias;all_saves.push_back(saved_entry);
    }
    closedir(dir);scan_preserved_saves();std::sort(all_saves.begin(),all_saves.end(),[](const Save &a,const Save &b){return a.modified!=b.modified?a.modified>b.modified:a.path<b.path;});
    filter_saves();
}
bool copy_exclusive(const std::string &source,const std::string &dest){
    FILE *input=fopen(source.c_str(),"rb");if(!input)return false;
    int fd=open(dest.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){fclose(input);return false;}
    FILE *output=fdopen(fd,"wb");if(!output){close(fd);fclose(input);unlink(dest.c_str());return false;}
    bool ok=true;char buffer[16384];size_t count;
    while((count=fread(buffer,1,sizeof(buffer),input)))if(fwrite(buffer,1,count,output)!=count){ok=false;break;}
    if(ferror(input)||fflush(output)!=0||fsync(fd)!=0)ok=false;fclose(input);if(fclose(output)!=0)ok=false;
    if(!ok)unlink(dest.c_str());return ok;
}
bool has_named_player_profile(){
    const char *folder=getenv("KFX_PLAYER_PROFILE_DIR");
    return folder&&*folder;
}
void import_legacy_saves(){
    if(!has_named_player_profile()||network_is_active()||game.game_kind!=GKind_Unset){
        notice="Open Saved Games from the main menu with a named profile.";
        return;
    }
    namespace fs=std::filesystem;
    const char *base=getenv("KFX_BASE_USER_DATA_DIR");
    const fs::path source=fs::path(base&&*base?base:PlatformManager_GetUserPrefDir())/"save";
    const fs::path destination=path("");
    int copied=0, skipped=0, failed=0;
    const char *members[]={"",".png",".json",".requirements",".label",".protected",
        ".backup1",".backup1.png",".backup1.json",".backup1.requirements",
        ".backup2",".backup2.png",".backup2.json",".backup2.requirements",
        ".backup3",".backup3.png",".backup3.json",".backup3.requirements"};
    std::error_code error;
    if(fs::is_symlink(fs::symlink_status(source,error))||!fs::is_directory(source,error)){
        notice="No legacy save folder found.";
        return;
    }
    for(const auto &entry:fs::directory_iterator(source,error)){
        const std::string name=entry.path().filename().string();
        if(name.size()!=12||name.compare(0,4,"fx1g")||name.compare(8,4,".sav")||
           name.find_first_not_of("0123456789",4)!=8||!fs::is_regular_file(entry.symlink_status(error)))continue;
        std::vector<std::pair<fs::path,fs::path>> files;
        bool occupied=false;
        for(const char *suffix:members){
            const auto from=source/(name+suffix), to=destination/(name+suffix);
            if(fs::symlink_status(to,error).type()!=fs::file_type::not_found){
                occupied=true;
                break;
            }
            if(fs::is_regular_file(fs::symlink_status(from,error)))files.emplace_back(from,to);
        }
        if(occupied){++skipped;continue;}
        size_t done=0;
        for(;done<files.size();++done)
            if(!copy_exclusive(files[done].first.string(),files[done].second.string()))break;
        if(done==files.size()){++copied;continue;}
        while(done>0){--done;fs::remove(files[done].second,error);}
        ++failed;
    }
    load_game_save_catalogue();
    scan_saves();
    notice="Imported "+std::to_string(copied)+" saves. Occupied slots: "+std::to_string(skipped)+". Failed: "+std::to_string(failed)+".";
}

void inspect_save();
void load_selected(bool checked=false){
    if(network_is_active()){notice="Saved games cannot be loaded during multiplayer.";return;}
    if(saves.empty())return;Save entry=selected_save();long slot=entry.slot;
    if(!checked){inspect_save();return;}
    if(!kfx_save_preserve_before_load(entry.path.c_str())){notice="Could not preserve the save. Check disk space and permissions.";return;}
    if(entry.backup){
        slot=-1;
        for(int i=0;i<SAVE_SLOTS_LIMIT;i++){char name[32];snprintf(name,sizeof(name),"fx1g%04d.sav",i);std::string target=path(name);
            if(access(target.c_str(),F_OK)==0)continue;
            if(!copy_exclusive(entry.path,target)){notice="Backup restore failed. Existing saves are unchanged.";return;}
            for(const char *suffix:{".png",".json",".requirements"})if(access((entry.path+suffix).c_str(),R_OK)==0)copy_exclusive(entry.path+suffix,target+suffix);
            if(access((save_base(entry)+".label").c_str(),R_OK)==0)copy_exclusive(save_base(entry)+".label",target+".label");
            slot=i;break;
        }
        if(slot<0){notice="No free save slot is available.";return;}
    }
    bool in_game=game.game_kind==GKind_LocalGame && frontend_menu_state==FeSt_INITIAL;
    close_panel();
    if(in_game){if(!load_game(slot)){open_panel(2);notice="Could not load this saved game.";}}
    else {game.save_game_slot=slot;frontend_set_state(FeSt_LOAD_GAME);}
}
TbControllerButtons pad_button(Uint8 button){
    const TbControllerButtons map[]={CBtn_A,CBtn_B,CBtn_X,CBtn_Y,CBtn_BACK,0,CBtn_START,CBtn_LEFTSTICK,CBtn_RIGHTSTICK,CBtn_LEFTSHOULDER,CBtn_RIGHTSHOULDER,CBtn_DPAD_UP,CBtn_DPAD_DOWN,CBtn_DPAD_LEFT,CBtn_DPAD_RIGHT,CBtn_MISC1,CBtn_PADDLE1,CBtn_PADDLE2,CBtn_PADDLE3,CBtn_PADDLE4,CBtn_TOUCHPAD};
    return button<sizeof(map)/sizeof(map[0])?map[button]:0;
}
TbControllerButtons save_aux_button(int index){
    const TbControllerButtons order[]={CBtn_X,CBtn_Y,CBtn_A,CBtn_B};int count=0;
    for(TbControllerButtons value:order)if(value!=kfx_menu_confirm_button()&&value!=kfx_menu_back_button()){if(count++==index)return value;}
    return CBtn_NONE;
}
std::string binding_name(TbControllerButtons mask){
    if(!mask)return "Unassigned";std::string result;
    const char *names[]={"A","B","X","Y","Back","Start","L3","R3","LB","RB","D-pad up","D-pad down","D-pad left","D-pad right","Misc","Paddle 1","Paddle 2","Paddle 3","Paddle 4","Touchpad","LT","RT","LS up","LS down","LS left","LS right","RS up","RS down","RS left","RS right"};
    for(int i=0;i<30;i++)if(mask&((TbControllerButtons)1<<i)){if(!result.empty())result+=" + ";std::string name=names[i];if(kfx_controller_guide_ps5){if(i<4)name=face_names[1][i];if(i==8)name="L1";if(i==9)name="R1";if(i==20)name="L2";if(i==21)name="R2";if(i==5)name="Options";}result+=name;}
    return result;
}
void start_diagnostics();
void start_edit(int field);
std::string save_base(const Save &entry);
bool write_small_file(const std::string&,const std::string&);
void refresh_save(const std::string&);
void update_hover();
bool feature_action(int action);
bool feature_event(const SDL_Event *event);
void feature_draw();
bool browser_action(int);
void browser_draw();
void browser_tick();
void browser_initialize();
void party_draw();
bool party_owner();
extern std::string party_message;
int cloud_account_options_count();
void browser_filter();
void browser_refresh();
void community_request();
extern std::string browser_search,hub_edit,host_title,host_address,host_map,server_password;
extern int browser_tab;
void feature_init();
void feature_tick();
void wheel_draw();
bool polish_action(int action);
void save_feedback_draw();
std::string cloud_name,cloud_email,cloud_password,cloud_code,cloud_ticket,cloud_confirmation;
int cloud_avatar=1;
bool cloud_create=false, choosing_local_profile=false, remember_cloud=false;
bool startup_login=false;
int local_profile_count();
void choose_startup_profile();

std::string *cloud_input(int field){
    switch(field){
    case 14:return &cloud_name;
    case 15:return &cloud_email;
    case 16:return &cloud_password;
    case 17:return &cloud_code;
    case 18:return &cloud_confirmation;
    default:return nullptr;
    }
}
void finish_edit();
void load_network_profile();
std::string autosave_status();
std::string profile_file(const char*);
bool enhancement_action(int);void enhancement_draw();void enhancement_tick();void enhancement_sample();void performance_draw();void recovery_status_draw();int enhancement_count();void enhancement_confirm();void library_rename();extern std::string replay_title;
bool play_tools_action(int);void play_tools_draw();void scan_profiles();bool create_profile();void play_tools_tick();extern std::string new_profile;extern int requirements_state;
void change(int row,int direction){
    if(page==0){
        if(row==0)env_number("KFX_CONTROLLER_CURSOR_SPEED",std::clamp(kfx_controller_cursor_speed()+direction*.25,.25,3.0));
        if(row==1)env_number("KFX_CONTROLLER_DEADZONE",std::clamp(kfx_controller_deadzone()+direction*.05,.05,.5));
        if(row==2){env_number("KFX_CONTROLLER_RUMBLE",std::clamp(kfx_controller_rumble_strength()+direction*.1,0.0,1.0));last_used_input_device=ID_Controller;controller_rumble(180);}
        if(row==3||row==4){uint64_t old=row==3?kfx_menu_confirm_button():kfx_menu_back_button(),other=row==3?kfx_menu_back_button():kfx_menu_confirm_button();int index=0;for(int i=0;i<4;i++)if(old==(uint64_t)face_buttons[i])index=i;
            do{index=(index+direction+4)%4;}while((uint64_t)face_buttons[index]==other);env_number(row==3?"KFX_MENU_CONFIRM_BUTTON":"KFX_MENU_BACK_BUTTON",face_buttons[index]);}
        if(row==5){label_model=(label_model+1+direction+3)%3-1;if(label_model>=0)kfx_controller_guide_ps5=label_model;}
    }else if(page==1){
        if(row==0){preset=(preset+direction+4)%4;fps_limit_main=fps_limit_secondary=caps[preset];start_params.num_fps_draw_main=start_params.num_fps_draw_secondary=caps[preset];}
        if(row==1)debug_display_frametime=debug_display_frametime?0:1;
        if(row==2)start_params.enhanced_mode=!start_params.enhanced_mode;
        if(row==3){const int times[]={0,120,300,600};int current=getenv("KFX_AUTOSAVE_SECONDS")?atoi(getenv("KFX_AUTOSAVE_SECONDS")):300,index=2;for(int i=0;i<4;i++)if(current==times[i])index=i;env_number("KFX_AUTOSAVE_SECONDS",times[(index+direction+4)%4]);reset_autosave_timer();}
    }else if(page==7){
        if(row==0)kfx_text_scale=std::clamp(kfx_text_scale+direction*.25f,1.0f,1.5f);
        if(row==1)kfx_cursor_scale=std::clamp(kfx_cursor_scale+direction*.25f,1.0f,2.0f);
        if(row==2)kfx_edge_scroll=!kfx_edge_scroll;
        if(row==3)kfx_edge_border=std::clamp(kfx_edge_border+direction*4,4,24);
        if(row==4)kfx_mentor_captions=!kfx_mentor_captions;
        if(row==5)kfx_caption_seconds=std::clamp(kfx_caption_seconds+direction,3,12);
    }else if(page==5){
        if(row==0)display_mode=(display_mode+1)%2;
        if(row==1)resolution_index=(resolution_index+direction+resolution_count)%resolution_count;
        if(row==2){kfx_window_cursor_lock=!kfx_window_cursor_lock;LbSetMouseGrab(false);}
    }
    persist();
    if(page==5 && row<2)notice="Choose Apply Display to use and save these display settings.";
}
void activate(int action){
    if(action>=0)do_sound_menu_click();
    if(enhancement_action(action)||play_tools_action(action)||browser_action(action)||feature_action(action)||polish_action(action))return;
    focus_action=action;
    if(action==901&&page==36){
        cloud_password.clear();cloud_confirmation.clear();cloud_ticket.clear();cloud_code.clear();
        if(cloud_create){cloud_create=false;open_panel(36);}
        else if(!startup_login)open_panel(22);
        return;
    }
    if(action==901&&page==38){close_panel();return;}
    if(action==901&&page==27&&choosing_local_profile){choosing_local_profile=false;open_panel(36);return;}
    if(action==901&&(page==11||page==20)&&frontend_menu_state==FeSt_NET_SERVICE){close_panel();return;}
    if(action==901){if(page==8)close_panel();else{int previous=page;if(previous==19)kfx_readiness_cancel();page=previous==30?28:previous==31||previous==32?13:previous==33?7:previous==34?1:previous==35?2:previous==29?12:previous>=27?8:previous==26?15:previous>=21&&previous<=24?20:previous==25?22:previous==20?6:previous==15?0:previous==16?7:previous==17?12:previous==18?17:previous==19?6:previous==14?help_parent:previous==12?2:(previous>=9&&previous<=11?6:8);selected=0;
        if(page==8){const int order[]={5,0,1,2,3,4,6,7,13};for(int i=0;i<9;i++)if(order[i]==previous)selected=i;
            if(previous==27)selected=8;else if(previous==28)selected=9;else if(previous==13)selected=10;}
        else if(page==6)selected=previous==9?1:previous==10?2:3;else if(page==13)selected=3;
        editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());focus_action=-1;}return;}
    if(action>=100 && action<=107){page=action-100;selected=0;capturing=false;editing=0;preview_large=false;repeat_button=255;focus_action=-1;SDL_StopTextInput(SDL_GetKeyboardFocus());notice.clear();if(page==2)scan_saves();return;}
    if(action==501){if(!display_confirmation)return;if(SDL_GetTicks()>=display_deadline){display_confirmation=false;rollback_display=true;notice="Confirmation expired. Restoring the previous display.";return;}display_confirmation=false;saved_display_mode=display_mode;saved_resolution=resolution_index;persist();unlink(profile_file("display-startup.pending").c_str());notice="Display settings kept.";return;}
    if(action==502){display_confirmation=false;rollback_display=true;notice="Restoring previous display...";return;}
    if(action==307){preview_large=true;return;}
    if(action==308){preview_large=false;return;}
    if(action==304){start_edit(1);return;}
    if(action==305){save_filter=(save_filter+1)%4;save_index=0;filter_saves();return;}
    if(action==306){save_search.clear();save_index=0;filter_saves();return;}
    if(action>=320&&action<324&&!saves.empty()){save_index=std::min((int)saves.size()-1,(save_index/4)*4+action-320);backup_choice=0;page=12;return;}
    if(action>=340&&action<=343){backup_choice=action-340;return;}
    if(action==600){start_diagnostics();page=9;report_offset=0;return;}
    if(action==606){page=10;selected=0;return;}
    if(action==607){page=11;selected=0;return;}
    if(action==309){if(!saves.empty()){page=12;selected=0;}return;}
    if(action==705){guide_section=(guide_section+1)%3;guide_more=guide_section!=0;return;}
    if(action==601){start_edit(2);return;}
    if(action==602){start_edit(3);return;}
    if(action==603){if(network_is_active()||game.game_kind==GKind_LocalGame){notice="Return to the main menu before joining another game.";return;}pending_join=true;return;}
    if(action==500){if(display_confirmation)return;pending_display=true;notice="Applying display settings...";return;}
    if(action==900){close_panel();return;}
    if(action>=200 && action<212){int row=(action-200)/2;change(row,(action%2)?1:-1);return;}
    if(action==300||action==301){backup_choice=0;save_index=std::clamp(save_index+(action==300?-1:1),0,std::max(0,(int)saves.size()-1));return;}
    if(action==310&&page==2){import_legacy_saves();return;}
    if(action==302){load_selected();return;}
    if(action==303){scan_saves();return;}
    if(action==400||action==401){if(!actions.empty())action_index=(action_index+(action==400?-1:1)+(int)actions.size())%actions.size();capturing=false;return;}
    if(action==402){capturing=true;captured=0;notice="Press a button, stick direction or chord. Enter / Start saves; Esc cancels.";return;}
    if(action==403 && !actions.empty()){settings.kbkeys[actions[action_index]].controller_buttons=CBtn_NONE;notice=save_settings()?"Binding cleared; keyboard controls kept.":"Could not save this binding. Check save-folder permissions.";capturing=false;}
    if(action==404 && !actions.empty()){settings.kbkeys[actions[action_index]].controller_buttons=game_key_settings[actions[action_index]].default_controller_buttons;notice=save_settings()?"Default controller binding restored.":"Could not save this binding. Check save-folder permissions.";capturing=false;}
    if(action==405 && capturing && !actions.empty()){settings.kbkeys[actions[action_index]].controller_buttons=captured;capturing=false;notice=save_settings()?"Controller binding saved; keyboard controls kept.":"Could not save this binding. Check save-folder permissions.";}
}
void row(int number,const std::string &name,const std::string &value){int y=96+number*50;
    text(42,y+8,name,22,278);text(332,y+8,value,22,182);button(524,y,40,"-",200+number*2,false,40);button(572,y,40,"+",201+number*2,selected==number,40);
}
void start_edit(int field){editing=field;edit_selected=true;repeat_button=255;SDL_Window *window=SDL_GetKeyboardFocus();if(window)SDL_StartTextInput(window);notice=field==1?"Type to search saved games. Enter finishes; Esc returns.":field==2?"Enter the other player's four-part game version.":"Enter the host's IPv4 address and optional UDP port.";}
void start_diagnostics(){
    if(diagnostic_process){notice="A connection check is already running.";return;}
    const char *resources=getenv("KFX_APP_RESOURCES");std::string executable=resources?std::string(resources)+"/connection-check":std::string(SDL_GetBasePath()?SDL_GetBasePath():"")+"../connection-check";
    std::string port=std::to_string(enet_port);const char *args[]={executable.c_str(),"--service",kfx_hub_url(),"--endpoint",host_address.c_str(),"--port",port.c_str(),"--hosting",network_is_host()?"1":"0",nullptr};diagnostic_process=SDL_CreateProcess(args,true);network_report.clear();
    notice=diagnostic_process?"Checking lobby reachability and the local UDP port...":"Connection helper unavailable. Use the packaged Mac app for this check.";
}
bool valid_endpoint(const std::string &input,std::string &normalized){
    if(input.empty()||input.size()>120)return false;std::string host=input,port_text;bool v6=host.front()=='[';int port=5556;
    if(v6){size_t end=host.find(']');if(end==std::string::npos)return false;std::string addr=host.substr(1,end-1);struct in6_addr address;if(inet_pton(AF_INET6,addr.c_str(),&address)!=1)return false;if(end+1<host.size()){if(host[end+1]!=':')return false;port_text=host.substr(end+2);}host=host.substr(0,end+1);}
    else{size_t colon=host.find(':');if(colon!=std::string::npos){port_text=host.substr(colon+1);host.resize(colon);}if(host.empty()||host.front()=='.'||host.back()=='.'||host.front()=='-'||host.back()=='-')return false;bool numeric=true;for(unsigned char c:host){if(!std::isalnum(c)&&c!='-'&&c!='.')return false;if(!std::isdigit(c)&&c!='.')numeric=false;}struct in_addr address;if(numeric&&inet_pton(AF_INET,host.c_str(),&address)!=1)return false;}
    if(input.back()==':')return false;if(!port_text.empty()){if(port_text.size()>5||!std::all_of(port_text.begin(),port_text.end(),[](unsigned char c){return std::isdigit(c);}))return false;port=atoi(port_text.c_str());}if(port<1||port>65535)return false;normalized=host+":"+std::to_string(port);return true;
}
std::string normalized_version(const std::string &value){
    std::string clean=value;while(!clean.empty()&&std::isspace((unsigned char)clean.back()))clean.pop_back();size_t suffix=clean.find(' ');if(suffix!=std::string::npos)clean.resize(suffix);
    if(clean.empty()||clean.front()=='.'||clean.back()=='.')return {};
    std::istringstream stream(clean);std::string part,result;int count=0;
    while(std::getline(stream,part,'.')){if(part.empty()||part.size()>5||!std::all_of(part.begin(),part.end(),[](unsigned char c){return std::isdigit(c);}))return {};if(count++)result+='.';result+=std::to_string(atoi(part.c_str()));}
    if(count==3)result+=".0";else if(count!=4)return {};return result;
}
void backup_step(int direction);
void navigate(int direction){
    mouse_navigation=false;focus_action=-1;
    if(page==38){if(!hits.empty()){const int count=static_cast<int>(hits.size());selected=(std::min(selected,count-1)+direction+count)%count;}return;}
    if(page==8){selected=(selected+direction+12)%12;return;}
    if(page==36){int count=cloud_ticket.empty()?5:2;selected=(selected+direction+count)%count;return;}
    if(page>=30){int count=enhancement_count();selected=(selected+direction+count)%count;return;}
    if(page==27&&choosing_local_profile){
        int count=std::min(4,local_profile_count()+1);selected=(selected+direction+count)%count;return;
    }
    if(page>=27&&page<=29){int count=page==27?6:page==28?7:requirements_state>=0?2:1;selected=(selected+direction+count)%count;return;}
    if(page==9){report_offset=std::max(0,report_offset+direction);return;}
    if(page==10)return;
    if(page==26){selected=(selected+direction+2)%2;return;}
    if(page>=20){int count=page==20?14:page==21?(party_owner()&&browser_tab!=1?6:5):page==22?cloud_account_options_count():page==23?5:page==24?5:2;selected=(selected+direction+count)%count;return;}
    if(page>=15){int count=page==15?8:page==16?6:page==17?4:page==18?2:3;selected=(selected+direction+count)%count;return;}
    if(page==13){selected=(selected+direction+6)%6;return;}
    if(page==14){selected=(selected+direction+2)%2;return;}
    if(page==11){selected=(selected+direction+2)%2;return;}
    if(page==12){backup_step(direction);return;}
    if(page==2){save_index=std::clamp(save_index+direction,0,std::max(0,(int)saves.size()-1));backup_choice=0;}
    else if(page==3){if(!actions.empty())action_index=(action_index+direction+(int)actions.size())%actions.size();}
    else {int count=page==7?7:page==0?6:page==6?6:page==1?5:4;selected=(selected+direction+count)%count;}
}
void switch_tab(int direction){if(page==8){selected=(selected+direction+12)%12;return;}if(page>=9&&page<=11)page=6;else if(page==12)page=2;else if(page==14)page=help_parent;const int order[]={5,0,1,2,3,4,6,7,13};int index=0;for(int i=0;i<9;i++)if(order[i]==page)index=i;int next=order[(index+direction+9)%9];activate(next==13?800:100+next);}

void backup_step(int direction){
    if(saves.empty())return;
    for(int attempt=0;attempt<4;attempt++){backup_choice=(backup_choice+direction+4)%4;if(!backup_choice)return;for(const Save &entry:all_saves)if(entry.slot==saves[save_index].slot&&entry.backup==backup_choice)return;}
}
void confirm_control(){
    mouse_navigation=false;
    if(page==38){if(!hits.empty())activate(hits[std::min(selected,static_cast<int>(hits.size())-1)].action);return;}
    if(page==8){const int sections[]={5,0,1,2,3,4,6,7};const int extra[]={1200,1300,800,901};activate(selected<8?100+sections[selected]:extra[selected-8]);return;}
    if(page>=30&&page<=35){enhancement_confirm();return;}
    if(page==27&&choosing_local_profile){const int count=local_profile_count();
        activate(selected<count?1210+selected:1203);return;}
    if(page==27){const int choices[]={1201,1202,1203,1204,1205,1206};activate(choices[selected]);return;}
    if(page==28){activate(selected==6?1500:selected==5?1306:1301+selected);return;}
    if(page==29){activate(selected?1401:1402);return;}
    if(page==36){
        const int signup[]={2101,2102,2103,2108,2105};
        const int signin[]={2101,2103,2106,2105,2111};
        activate(cloud_ticket.empty()?(cloud_create?signup[selected]:signin[selected]):selected?2105:2107);
        return;
    }
    if(page==1&&selected==4){activate(1540);return;}
    if(page==7&&selected==6){activate(1530);return;}
    if((page==0||page==7)&&selected==5)activate(page==0?840:841);
    else if(page==0||page==1||page==7)change(selected,1);
    else if(page==5){if(selected==3)activate(500);else change(selected,1);}
    else if(page==2)activate(saves.empty()&&has_named_player_profile()?310:309);
    else if(page==12)activate(302);
    else if(page==9)activate(selected?815:600);
    else if(page==10)activate(601);
    else if(page==11)activate(selected?603:602);
    else if(page==3)activate(focus_action>=402&&focus_action<=405?focus_action:402);
    else if(page==6){const int choices[]={611,600,606,607,844,2000};activate(choices[selected]);}
    else if(page==4)activate(705);
    else if(page==13){const int commands[]={810,811,813,814,1510,1520};activate(commands[selected]);}
    else if(page==14)activate(selected?812:811);
    else if(page==15){const int actions[]={860,861,862,863,864,895,894,896};activate(actions[selected]);}
    else if(page==26)activate(selected?898:897);
    else if(page==16)activate(selected<4?880+selected:selected==4?891:893);
    else if(page==17){const int actions[]={1000,1001,1003,1004};activate(actions[selected]);}
    else if(page==18)activate(selected?1006:1005);
    else if(page==19){const int actions[]={1100,1102,1101};activate(actions[selected]);}
    else if(page==20){const int actions[]={2010,2011,2012,2030,2031,2020,2022,2023,2024,2025,2021,2027,2026,2120};activate(actions[selected]);}
    else if(page==21){const int actions[]={2044,2040,2042,2043,2045,2121};activate(actions[selected]);}
    else if(page==22){const int actions[]={2050,2051,2052,2053,2113,2114,2115};activate(actions[cloud_account_options_count()==2?selected+5:selected]);}
    else if(page==23){const int actions[]={2060,2061,2063,2064,2062};activate(actions[selected]);}
    else if(page==24){const int actions[]={2080,2081,2083,2082,2084};activate(actions[selected]);}
    else if(page==25)activate(2070+selected);
}
void direction_control(int direction){
    mouse_navigation=false;if(page!=3)focus_action=-1;
    if(page>=30){navigate(direction);return;}
    if(page==20){activate(direction<0?2030:2031);return;}
    if(page==9){selected=selected?0:1;return;}
    if(page==8){if(selected<8)selected=(selected+direction*4+8)%8;else selected=8+(selected-8+direction+4)%4;return;}
    if(page==2){save_filter=(save_filter+direction+4)%4;save_index=0;filter_saves();}
    else if(page==12)backup_step(direction);
    else if(page==3){int current=focus_action>=402&&focus_action<=405?focus_action-402:0;focus_action=402+(current+direction+4)%4;}
    else if(page==4){label_model=kfx_controller_guide_ps5?0:1;kfx_controller_guide_ps5=label_model;}
    else if((page==0||page==7)&&selected<5)change(selected,direction);
    else if(page==1||(page==5&&selected<3))change(selected,direction);
    else if(page==16&&selected>=4)activate(selected==4?(direction<0?890:891):(direction<0?892:893));
}
void append_input(std::string value){
    if(editing==19){
        if(edit_selected){party_message.clear();edit_selected=false;}
        value.erase(std::remove_if(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}),value.end());
        size_t length=std::min(value.size(),220-party_message.size());
        while(length>0&&length<value.size()&&(static_cast<unsigned char>(value[length])&0xc0)==0x80)--length;
        party_message.append(value,0,length);
        return;
    }
    if(auto *input=cloud_input(editing)){
        if(edit_selected){input->clear();edit_selected=false;}
        const size_t limit=editing==14?(cloud_create?24:254):editing==15?254:(editing==16||editing==18)?128:6;
        for(unsigned char c:value)if(c>=32&&c<127&&input->size()<limit)input->push_back(c);
        return;
    }
    std::string *target=editing==1?&save_search:editing==2?&peer_version:editing==4?&network_name:editing==5?&save_alias:editing==6?&browser_search:editing==7?&host_title:editing==8?&host_address:editing==9?&hub_edit:editing==10?&server_password:editing==13?&host_map:editing==12?&replay_title:editing==11?&new_profile:&join_ip;size_t limit=editing==1?64:editing==2?24:editing==4?19:editing==5?63:editing==6?64:editing==7?64:editing==8?120:editing==9?240:editing==10?31:editing==13?88:editing==12?63:editing==11?32:120;
    if(edit_selected){target->clear();edit_selected=false;}
    std::string clean;for(unsigned char c:value)if(c>=32&&c!=127&&(editing!=4||c<128))clean.push_back(c);
    size_t length=std::min(clean.size(),limit-target->size());while(length>0&&length<clean.size()&&(((unsigned char)clean[length]&0xc0)==0x80))length--;target->append(clean.data(),length);
    if(editing==1){save_index=0;filter_saves();}if(editing==6)browser_filter();
}
void delete_input(){
    if(editing==19){
        if(edit_selected)party_message.clear();
        else if(!party_message.empty()){
            size_t end=party_message.size()-1;
            while(end>0&&(static_cast<unsigned char>(party_message[end])&0xc0)==0x80)--end;
            party_message.resize(end);
        }
        edit_selected=false;return;
    }
    if(auto *input=cloud_input(editing)){
        if(edit_selected){input->clear();edit_selected=false;}
        else if(!input->empty())input->pop_back();
        return;
    }
    std::string *target=editing==1?&save_search:editing==2?&peer_version:editing==4?&network_name:editing==5?&save_alias:editing==6?&browser_search:editing==7?&host_title:editing==8?&host_address:editing==9?&hub_edit:editing==10?&server_password:editing==13?&host_map:editing==12?&replay_title:editing==11?&new_profile:&join_ip;
    if(edit_selected){target->clear();edit_selected=false;}
    else if(!target->empty()){size_t index=target->size()-1;while(index>0&&(((unsigned char)(*target)[index]&0xc0)==0x80))index--;target->erase(index);}
    if(editing==1){save_index=0;filter_saves();}if(editing==6)browser_filter();
}
#include "macos_options_polish.inc"
#include "macos_play_tools.inc"
#include "macos_enhancements.inc"
#include "macos_feature_tools.inc"
#include "macos_server_browser.inc"

void captions_draw(){
    if(!kfx_mentor_captions||caption.empty()||SDL_GetTicks()>=caption_until||!winfont)return;
    int width=RendererScreenWidth();int saved_flags=RendererGetDrawFlags();
    int left=width/4,height=(int)(76*kfx_caption_scale);LbDrawBox(left,12,width-left-12,height,0);LbTextSetFont(winfont);LbTextSetWindow(left+12,18,width-left-36,height-8);RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);
    int units=std::max(16,(int)(24*kfx_caption_scale*16/std::max(1,(int)LbTextLineHeight())));LbTextDrawResized(0,0,units,caption.c_str());RendererSetDrawFlags(saved_flags);
}
}

int kfx_quick_game_key(long key){if(quick_key>=0&&SDL_GetTicks()>quick_key_until)quick_key=-1;if(quick_key!=key)return 0;quick_key=-1;return 1;}
void kfx_save_feedback(int success,int automatic,const char *reason){
    save_toast=success?(automatic?"Automatic save complete":"Game saved"):"Save failed: Saving Help";
    save_toast_until=SDL_GetTicks()+(success?4000:10000);
    if(success){if(automatic)last_autosave=time(nullptr);}else save_error=reason?reason:"Could not write the saved game.";
}
void kfx_save_display_label(long slot,const char *fallback,char *buffer,unsigned int size){if(!buffer||!size)return;char name[40];snprintf(name,sizeof(name),"fx1g%04ld.sav",slot);std::string alias=slot>=0&&slot<SAVE_SLOTS_LIMIT?read_save_alias(path(name)):"";snprintf(buffer,size,"%s",alias.empty()?(fallback?fallback:""):alias.c_str());}
int kfx_options_select_replay(const char *file){if(!file||!*file||strlen(file)>=sizeof(game.packet_fname)||network_is_active()||game.game_kind!=GKind_Unset)return 0;std::lock_guard<std::mutex> lock(replay_selection_lock);replay_selection=file;return 1;}
int kfx_options_community_action(int action){
    if((action<2000||action>2084)&&action!=2120&&action!=2121&&action!=2128)return 0;
    if(!kfx_controller_guide_visible)open_panel(20);
    return browser_action(action);
}
const char *kfx_options_community_status(void){static std::string output;output="{\"page\":"+std::to_string(page)+",\"servers\":"+std::to_string(browser_servers.size())+",\"party_members\":"+std::to_string(party.members.size())+",\"party_prompt\":"+hub_quote(party.server_name)+",\"status\":"+hub_quote(hub_message)+",\"account\":"+hub_quote(kfx_hub_account_name())+",\"busy\":"+(kfx_hub_busy()?"true":"false")+"}";return output.c_str();}
void kfx_options_initialize(void){
    // Movies follow the configured menu mode, then any saved in-game override.
    set_movies_vidmode(get_frontend_vidmode());
    feature_init();load_network_profile();browser_initialize();
    for(int i=0;i<4;i++)if(start_params.num_fps_draw_main==caps[i])preset=i;
    actions.clear();for(int i=0;i<GAME_KEYS_COUNT;i++)if(game_key_settings[i].binding_menu_visibility!=BMV_Hidden && game_key_settings[i].binding_menu_visibility!=BMV_KeyMouseOnly)actions.push_back(i);
    FILE *file=fopen(path("macos-options.cfg").c_str(),"r");if(!file)return;
    char line[160],key[40];double value;
    while(fgets(line,sizeof(line),file))if(sscanf(line,"%39[^=]=%lf",key,&value)==2 && std::isfinite(value)){
        if(!strcmp(key,"caption_scale")&&value>=1&&value<=2)kfx_caption_scale=value;
        else if(!strcmp(key,"tooltip_scale")&&value>=1&&value<=1.5)kfx_tooltip_scale=value;
        else if(!strcmp(key,"reduced_flashes"))kfx_reduce_flashes=value==1;
        else if(!strcmp(key,"high_contrast"))kfx_high_contrast=value==1;
        else if(!strcmp(key,"performance_overlay"))performance_overlay=value==1;
        else if(!strcmp(key,"camera_response")&&value>=1&&value<=3)env_number("KFX_CAMERA_RESPONSE",value);
        else if(!strcmp(key,"quick_wheel"))quick_wheel_enabled=value==1;
        else if(!strcmp(key,"camera_pan_scale")&&value>=.25&&value<=1)kfx_camera_pan_scale=value;
        else if(!strcmp(key,"label_model")&&value>=-1&&value<=1&&value==(int)value)label_model=value;
        else if(!strcmp(key,"preset") && value>=0&&value<=3 && value==(int)value){preset=value;start_params.num_fps_draw_main=start_params.num_fps_draw_secondary=caps[preset];}
        else if(!strcmp(key,"overlay"))debug_display_frametime=value==1;
        else if(!strcmp(key,"enhanced"))start_params.enhanced_mode=value==1;
        else if(!strcmp(key,"display_mode") && (value==0||value==1))saved_display_mode=value;
        else if(!strcmp(key,"resolution") && value>=0&&value<resolution_count && value==(int)value)saved_resolution=value;
        else if(!strcmp(key,"text_scale")&&value>=1&&value<=1.5)kfx_text_scale=value;
        else if(!strcmp(key,"cursor_scale")&&value>=1&&value<=2)kfx_cursor_scale=value;
        else if(!strcmp(key,"edge_scroll"))kfx_edge_scroll=value==1;
        else if(!strcmp(key,"edge_border")&&value>=4&&value<=24)kfx_edge_border=value;
        else if(!strcmp(key,"mentor_captions"))kfx_mentor_captions=value==1;
        else if(!strcmp(key,"caption_seconds")&&value>=3&&value<=12)kfx_caption_seconds=value;
        else if(!strcmp(key,"window_cursor_lock"))kfx_window_cursor_lock=value==1;
        else if(!strcmp(key,"cursor")&&value>=.25&&value<=3)env_number("KFX_CONTROLLER_CURSOR_SPEED",value);
        else if(!strcmp(key,"deadzone")&&value>=.05&&value<=.5)env_number("KFX_CONTROLLER_DEADZONE",value);
        else if(!strcmp(key,"rumble")&&value>=0&&value<=1)env_number("KFX_CONTROLLER_RUMBLE",value);
        else if((!strcmp(key,"confirm")||!strcmp(key,"back"))&&(value==1||value==2||value==4||value==8))env_number(!strcmp(key,"confirm")?"KFX_MENU_CONFIRM_BUTTON":"KFX_MENU_BACK_BUTTON",value);
        else if(!strcmp(key,"autosave")&&(value==0||value==120||value==300||value==600))env_number("KFX_AUTOSAVE_SECONDS",value);
    }
    fclose(file);
    if(saved_display_mode>=0){display_mode=saved_display_mode;resolution_index=saved_resolution;set_modes(chosen_mode(display_mode,resolution_index));}
}
void process_network(){
    if(diagnostic_process){
        char buffer[2048];SDL_IOStream *output=SDL_GetProcessOutput(diagnostic_process);size_t count=output?SDL_ReadIO(output,buffer,sizeof(buffer)):0;
        if(count&&network_report.size()<16384)network_report.append(buffer,std::min(count,16384-network_report.size()));
        int code;if(SDL_WaitProcess(diagnostic_process,false,&code)){
            while(output&&(count=SDL_ReadIO(output,buffer,sizeof(buffer)))>0)if(network_report.size()<16384)network_report.append(buffer,std::min(count,16384-network_report.size()));
            SDL_DestroyProcess(diagnostic_process);diagnostic_process=nullptr;notice=code==0?"Connection test finished. See service and direct-hosting results below.":"Connection check finished with an error. See the report below.";
        }
    }
    if(pending_join){
        pending_join=false;
        std::string login_ticket;
        login_ticket.swap(pending_login_ticket);
        std::string endpoint;
        if(!valid_endpoint(join_ip,endpoint)){notice="Enter a hostname, IPv4 or bracketed IPv6 address and UDP port.";return;}
        if(network_is_active()||game.game_kind==GKind_LocalGame){notice="Return to the main menu before joining another game.";return;}
        persist_endpoint(endpoint);
        close_panel();
        LbNetwork_Stop();
        if(!setup_network_service(FrontendNetSvc_LAN)){open_panel(6);notice="The local network service could not start. Check the log and UDP port.";return;}
        frontnet_service_setup();frontnet_session_setup();
        if(kfx_hub_account_name()[0])snprintf(net_player_name,sizeof(net_player_name),"%s",kfx_hub_account_name());
        snprintf(netstate.password,sizeof(netstate.password),"%s",server_password.c_str());
        static TbNetworkSessionNameEntry direct={};memset(&direct,0,sizeof(direct));direct.in_use=1;direct.joinable=1;
        snprintf(direct.text,sizeof(direct.text),"%s",endpoint.c_str());snprintf(direct.join_address,sizeof(direct.join_address),"LAN:%s",endpoint.c_str());net_session[0]=&direct;net_number_of_sessions=1;net_session_index_active=0;
        kfx_hub_set_login_ticket(login_ticket.c_str());
        frontnet_session_join(nullptr);
        kfx_hub_set_login_ticket(nullptr);
        if(frontend_menu_state!=FeSt_NET_START){if(browser_join_failed())return;open_panel(6);notice="Could not join. Check the host address, UDP port, firewall and matching game version.";}
    }
}
void kfx_caption_speech(int sample){
    if(!kfx_mentor_captions||sample<=0||sample>=SMsg_MAX)return;
    const char *description=nullptr;
    auto describe=[](int id)->const char*{switch(id){
#include "macos_speech_captions.inc"
    default:return "Mentor announcement";}};
    description=describe(sample);
    // Event descriptions are independent of the audio volume.
    if(description){caption=description;caption_until=SDL_GetTicks()+(Uint64)kfx_caption_seconds*1000;}
}

void kfx_options_apply_display(void){
    enhancement_tick();play_tools_tick();feature_tick();browser_tick();process_network();
    if(display_confirmation && SDL_GetTicks()>=display_deadline){display_confirmation=false;rollback_display=true;}
    if(rollback_display){
        rollback_display=false;display_confirmation=false;
        if(rollback_minimal)setup_screen_mode_minimal(rollback_mode);else setup_screen_mode(rollback_mode,false);
        set_modes(rollback_mode);display_mode=rollback_display_choice;resolution_index=rollback_resolution;
        if(rollback_minimal){FrontendMenuState state=(FrontendMenuState)frontend_menu_state;init_gui();frontend_set_state(state);}else reinit_all_menus();
        unlink(profile_file("display-startup.pending").c_str());notice="Previous display settings restored.";LbSetMouseGrab(false);return;
    }
    if(!pending_display)return;pending_display=false;
    write_small_file(profile_file("display-startup.pending"),"display-change\n");
    TbScreenMode previous=LbScreenActiveMode(),mode=chosen_mode(display_mode,resolution_index);
    bool minimal=MinimalResolutionSetup;
    rollback_mode=previous;rollback_minimal=minimal;const TbScreenModeInfo *old_info=LbScreenGetModeInfo(previous);rollback_display_choice=old_info&&!(old_info->VideoFlags&Lb_VF_WINDOWED)?1:0;rollback_resolution=saved_resolution;
    if(old_info)for(int i=0;i<resolution_count;i++)if(old_info->Width==resolutions[i][0]&&old_info->Height==resolutions[i][1])rollback_resolution=i;
    TbScreenMode result=minimal?setup_screen_mode_minimal(mode):setup_screen_mode(mode,false);
    if(result==mode){
        set_modes(mode);display_confirmation=true;display_deadline=SDL_GetTicks()+15000;if(minimal){FrontendMenuState state=(FrontendMenuState)frontend_menu_state;init_gui();frontend_set_state(state);}else reinit_all_menus();notice="Keep these display settings? Automatic rollback in 15 seconds.";
    }else{
        if(minimal)setup_screen_mode_minimal(previous);else setup_screen_mode(previous,false);
        display_mode=-1;infer_display();notice="This display mode is unavailable. Previous display restored.";
    }
    if(emergency_display){emergency_display=false;if(result==mode){display_confirmation=false;saved_display_mode=0;saved_resolution=0;persist();unlink(profile_file("display-startup.pending").c_str());notice="Monitor changed. Safe windowed mode restored.";}}
    LbSetMouseGrab(false);
}
void kfx_options_recover_display(void){display_confirmation=false;rollback_display=false;display_mode=0;resolution_index=0;emergency_display=true;pending_display=true;}
int kfx_options_confirm_display(int keep){if(!display_confirmation)return 0;if(keep&&SDL_GetTicks()>=display_deadline){activate(502);return 0;}activate(keep?501:502);return 1;}
int kfx_options_set_display(int fullscreen,int width,int height){
    if(display_confirmation||rollback_display||pending_display)return 0;
    if(fullscreen!=0 && fullscreen!=1)return 0;
    int index=-1;for(int i=0;i<resolution_count;i++)if(resolutions[i][0]==width&&resolutions[i][1]==height)index=i;
    if(index<0)return 0;
    display_mode=fullscreen;resolution_index=index;pending_display=true;return 1;
}
void kfx_options_display(struct GuiButton *button){(void)button;open_panel(5);}
void kfx_options_hide(void){if(kfx_controller_guide_visible)close_panel();}
void kfx_options_open(struct GuiButton *button){(void)button;open_panel(8);}
void kfx_options_preview(int number){capture_preview=true;kfx_options_show_page(number);}
void kfx_options_show_page(int number){if(number==37){choose_startup_profile();return;}open_panel(std::clamp(number,0,36));if(page==2||page==12||page==17||page==18)scan_saves();if(page==20){browser_filter();browser_refresh();}if(page==25)hub_edit=kfx_hub_url();if(page==24)community_request();}
void kfx_options_button(struct GuiButton *button){
    if(frontend_menu_state==FeSt_FEOPTIONS)frontend_draw_button(button,1,"More Options",Lb_TEXT_HALIGN_CENTER);
    else{
        LbTextSetFont(winfont);LbTextSetWindow(button->scr_pos_x,button->scr_pos_y,button->width,button->height);RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);
        int units=button->height*14/std::max(1,(int)LbTextLineHeight());LbTextDrawResized(0,0,units,"More Options");
    }
}
bool kfx_options_event(const SDL_Event *event){
    if(kfx_replay_available()){
        if(event->type==SDL_EVENT_KEY_DOWN&&!event->key.repeat){
            if(event->key.key==SDLK_F10){if(kfx_controller_guide_visible&&page==28)close_panel();else open_panel(28);return true;}
            if(!kfx_controller_guide_visible){if(event->key.key==SDLK_SPACE){kfx_replay_control(0);return true;}if(event->key.key==SDLK_LEFTBRACKET){kfx_replay_control(-1);return true;}if(event->key.key==SDLK_RIGHTBRACKET){kfx_replay_control(1);return true;}if(event->key.key==SDLK_B){kfx_replay_control(2);return true;}}
        }
        if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&controller_owns_event(event->gbutton.which)&&event->gbutton.button==SDL_GAMEPAD_BUTTON_START&&(!kfx_controller_guide_visible||page==28)){if(kfx_controller_guide_visible)close_panel();else open_panel(28);return true;}
    }
    if(feature_event(event))return true;
    if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST || (event->type==SDL_EVENT_GAMEPAD_REMOVED && controller_owns_event(event->gdevice.which)))repeat_button=255;
    if(event->type==SDL_EVENT_GAMEPAD_BUTTON_UP && controller_owns_event(event->gbutton.which) && event->gbutton.button==repeat_button)repeat_button=255;
    if(event->type==SDL_EVENT_KEY_DOWN && event->key.key==SDLK_F8 && !event->key.repeat && !editing){
        if(kfx_controller_guide_visible&&page==38)close_panel();else activate(2120);
        return true;
    }
    if(event->type==SDL_EVENT_KEY_DOWN && event->key.key==SDLK_F9 && !event->key.repeat && !(event->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_ALT|SDL_KMOD_GUI))){if(kfx_controller_guide_visible)close_panel();else open_panel(4);return true;}
    if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN && controller_owns_event(event->gbutton.which) && event->gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_STICK && !capturing && !editing){
        bool assigned=false;for(int i=0;i<GAME_KEYS_COUNT;i++)if(get_game_key_controller_buttons(i)&CBtn_RIGHTSTICK)assigned=true;
        if(!assigned){if(kfx_controller_guide_visible)close_panel();else open_panel(4);return true;}
    }
    if(!kfx_controller_guide_visible)return false;
    if(event->type==SDL_EVENT_KEY_DOWN && event->key.key==SDLK_Q && (event->key.mod&SDL_KMOD_GUI))return false;
    if(event->type==SDL_EVENT_MOUSE_MOTION){mouse_navigation=true;return true;}
    if(event->type==SDL_EVENT_KEY_DOWN||event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN)mouse_navigation=false;
    if(event->type==SDL_EVENT_TEXT_INPUT){if(editing)append_input(event->text.text);return true;}
    if(event->type==SDL_EVENT_KEY_DOWN){
        SDL_Keycode key=event->key.key;
        if(key==SDLK_RETURN && event->key.repeat)return true;
        if(preview_large){if(key==SDLK_ESCAPE||key==SDLK_RETURN)preview_large=false;return true;}
        if(display_confirmation){if(key==SDLK_RETURN)activate(501);else if(key==SDLK_ESCAPE)activate(502);return true;}
        if(key==SDLK_ESCAPE){if(capturing){capturing=false;notice="Remapping cancelled.";}else if(editing){editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());notice.clear();}else activate(901);return true;}
        if(editing){if(key==SDLK_A&&(event->key.mod&(SDL_KMOD_GUI|SDL_KMOD_CTRL))){edit_selected=true;return true;}if(key==SDLK_BACKSPACE)delete_input();else if(key==SDLK_RETURN){finish_edit();}else if(key==SDLK_V&&(event->key.mod&(SDL_KMOD_GUI|SDL_KMOD_CTRL))){char *value=SDL_GetClipboardText();if(value){append_input(value);SDL_free(value);}}return true;}
        if(capturing){if(key==SDLK_RETURN)activate(405);return true;}
        if(key==SDLK_TAB){switch_tab(event->key.mod&SDL_KMOD_SHIFT?-1:1);return true;}
        if(key==SDLK_UP||key==SDLK_DOWN){navigate(key==SDLK_UP?-1:1);return true;}
        if(key==SDLK_LEFT||key==SDLK_RIGHT){direction_control(key==SDLK_LEFT?-1:1);return true;}
        if(key==SDLK_RETURN){confirm_control();return true;}
        if(page==12 && key==SDLK_M){activate(842);return true;}
        if(page==2 && key==SDLK_H){activate(814);return true;}
        if(page==20 && key==SDLK_F&&(event->key.mod&(SDL_KMOD_GUI|SDL_KMOD_CTRL))){start_edit(6);return true;}
        if(page==2 && key==SDLK_I){activate(310);return true;}
        if(page==2 && key==SDLK_F && (event->key.mod&(SDL_KMOD_GUI|SDL_KMOD_CTRL))){start_edit(1);return true;}
        return true;
    }
    if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN && controller_owns_event(event->gbutton.which)){
        Uint8 button=event->gbutton.button;TbControllerButtons binding=pad_button(button);
        if(preview_large){if(binding==kfx_menu_back_button()||binding==kfx_menu_confirm_button())preview_large=false;return true;}
        if(display_confirmation){if(binding==kfx_menu_confirm_button())activate(501);else if(binding==kfx_menu_back_button())activate(502);return true;}
        if(capturing){if(button==SDL_GAMEPAD_BUTTON_START && captured)activate(405);else captured|=binding;return true;}
        if(binding==kfx_menu_back_button()){if(editing){editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());notice.clear();}else activate(901);return true;}
        if(editing)return true;
        if(button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER||button==SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER){switch_tab(button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER?-1:1);return true;}
        if(button==SDL_GAMEPAD_BUTTON_DPAD_UP||button==SDL_GAMEPAD_BUTTON_DPAD_DOWN){navigate(button==SDL_GAMEPAD_BUTTON_DPAD_UP?-1:1);repeat_button=button;repeat_at=SDL_GetTicks()+350;return true;}
        if(button==SDL_GAMEPAD_BUTTON_DPAD_LEFT||button==SDL_GAMEPAD_BUTTON_DPAD_RIGHT){direction_control(button==SDL_GAMEPAD_BUTTON_DPAD_LEFT?-1:1);repeat_button=button;repeat_at=SDL_GetTicks()+350;return true;}
        if(page==12 && (button==SDL_GAMEPAD_BUTTON_START||binding==save_aux_button(0))){activate(842);return true;}
        if(page==2 && button==SDL_GAMEPAD_BUTTON_START){activate(814);return true;}
        if(page==12 && binding==save_aux_button(1)){activate(307);return true;}
        if(page==2 && binding==save_aux_button(1)){activate(305);return true;}
        if(page==2 && binding==save_aux_button(0)){start_edit(1);return true;}
        if(binding==kfx_menu_confirm_button()){confirm_control();return true;}
        return true;
    }
    if(event->type==SDL_EVENT_GAMEPAD_AXIS_MOTION){
        if(capturing && controller_owns_event(event->gaxis.which)){
            int axis=event->gaxis.axis,value=event->gaxis.value;
            if(axis>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER){if(value>8000)captured|=axis==SDL_GAMEPAD_AXIS_LEFT_TRIGGER?CBtn_L2:CBtn_R2;}
            else if(axis>=0&&axis<4&&abs(value)>16000){const TbControllerButtons directions[4][2]={{CBtn_LS_LEFT,CBtn_LS_RIGHT},{CBtn_LS_UP,CBtn_LS_DOWN},{CBtn_RS_LEFT,CBtn_RS_RIGHT},{CBtn_RS_UP,CBtn_RS_DOWN}};captured|=directions[axis][value>0];}
        }
        return true;
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN){
        if(event->button.button==SDL_BUTTON_LEFT){SDL_Window *window=SDL_GetMouseFocus();int w=0,h=0;if(window)SDL_GetWindowSize(window,&w,&h);if(w>0&&h>0){double scale=std::min(w/(double)WIDTH,h/(double)HEIGHT);SDL_Point point={(int)((event->button.x-(w-WIDTH*scale)/2)/scale),(int)((event->button.y-(h-HEIGHT*scale)/2)/scale)};for(const Hit &hit:hits)if(SDL_PointInRect(&point,&hit.rect)){activate(hit.action);break;}}}return true;
    }
    if(event->type==SDL_EVENT_MOUSE_WHEEL){if(!display_confirmation&&!editing&&(page==2||page==3||page==9||page==12))navigate(event->wheel.y>0?-1:1);return true;}
    return false;
}
void kfx_options_draw(void){
    enhancement_sample();
    if(wheel_open){wheel_draw();return;}
    if(!kfx_controller_guide_visible){captions_draw();save_feedback_draw();performance_draw();recovery_status_draw();return;}
    if(repeat_button!=255&&!editing&&!capturing&&!display_confirmation&&SDL_GetTicks()>=repeat_at){if(repeat_button==SDL_GAMEPAD_BUTTON_DPAD_LEFT||repeat_button==SDL_GAMEPAD_BUTTON_DPAD_RIGHT)direction_control(repeat_button==SDL_GAMEPAD_BUTTON_DPAD_LEFT?-1:1);else navigate(repeat_button==SDL_GAMEPAD_BUTTON_DPAD_UP?-1:1);repeat_at=SDL_GetTicks()+90;}
    if(!canvas)canvas=SDL_CreateSurface(WIDTH,HEIGHT,SDL_PIXELFORMAT_RGBA32);if(!canvas)return;
    if(label_model<0){int model=controller_device_model();if(model>=0)kfx_controller_guide_ps5=model;}else kfx_controller_guide_ps5=label_model;
    update_hover();hits.clear();background();
    const char *names[]={"Controller Options","Performance","Saved Games","Remap Controller","Controller Guide","Display Options","Network Options","Accessibility","More Options","Connection Test","Game Versions","Join by IP","Saved Game","About & Diagnostics","Saving Help","Connected Controllers","Accessibility Presets","Manage Saved Game","Delete Saved Game","Multiplayer Readiness","Server Browser","Server Details","Community Account","Host Public Game","Community","Community Service","Controller Test","Content Profiles","Replay Controls","Save Requirements","Replay Library","Personal Data Tools","Setup Guide","Comfort and Readability","Performance Capture","Save Recovery","Cloud Profile","Choose Profile"};
    title(page==38?"Friends and Party":page==36?(cloud_create?"Create Cloud Account":"Sign In"):page==27&&choosing_local_profile?"Local Profiles":names[std::clamp(page,0,37)]);
    if(page==8){
        const char *sections[]={"Display","Controller","Performance","Saved Games","Remap","Controller Guide","Network","Accessibility"};const int pages[]={5,0,1,2,3,4,6,7};
        for(int i=0;i<8;i++)button(i<4?50:338,104+(i%4)*72,252,sections[i],100+pages[i],selected==i,46);
        button(24,382,184,"Content",1200,selected==8,34);button(228,382,184,"Replays",1300,selected==9,34);button(432,382,184,"About",800,selected==10,34);
        button(134,426,372,"Return to Options",901,selected==11,42);
    }else if(page==5){
        frame(24,90,592,166);row(0,"Display mode",display_mode==1?"Fullscreen":"Windowed");row(1,"Window size",std::to_string(resolutions[resolution_index][0])+" x "+std::to_string(resolutions[resolution_index][1]));row(2,"Lock mouse in window",kfx_window_cursor_lock?"On":"Off");
        button(134,282,372,"Apply Display",500,selected==3,46);
        centered(345,"Fullscreen uses desktop resolution.",20);centered(375,"Keep changes within 15 seconds.",20);
    }else if(page==0){
        frame(24,90,592,256);char value[64];snprintf(value,sizeof(value),"%.2fx",kfx_controller_cursor_speed());row(0,"Pointer speed",value);snprintf(value,sizeof(value),"%.0f%%",kfx_controller_deadzone()*100);row(1,"Stick dead zone",value);snprintf(value,sizeof(value),"%.0f%%",kfx_controller_rumble_strength()*100);row(2,"Vibration",value);row(3,"Menu confirm",binding_name(kfx_menu_confirm_button()));row(4,"Menu back",binding_name(kfx_menu_back_button()));button(134,350,372,"Select Controller",840,selected==5,40);
    }else if(page==1){
        frame(24,90,592,214);const char *short_presets[]={"60 FPS","30 FPS","120 FPS","Unlimited"};row(0,"Frame rate",short_presets[preset]);row(1,"Frame timing display",debug_display_frametime?"On":"Off");row(2,"Presentation",start_params.enhanced_mode?"Enhanced":"Original");int seconds=getenv("KFX_AUTOSAVE_SECONDS")?atoi(getenv("KFX_AUTOSAVE_SECONDS")):300;row(3,"Automatic saves",seconds?std::to_string(seconds/60)+" minutes":"Off");button(64,338,512,"Performance Graph and Capture",1540,selected==4,40);
    }else if(page==7){
        frame(24,90,592,256);row(0,"Menu / caption text",std::to_string((int)round(kfx_text_scale*100))+"%");row(1,"Cursor / hand size",std::to_string((int)round(kfx_cursor_scale*100))+"%");row(2,"Edge scrolling",kfx_edge_scroll?"On":"Off");row(3,"Scroll border",std::to_string(kfx_edge_border)+" pixels");row(4,"Mentor captions",kfx_mentor_captions?"On":"Off");button(24,350,284,"Presets",841,selected==5,40);button(332,350,284,"Comfort / Text",1530,selected==6,40);
    }else if(page==2){
        small_line(24,78,autosave_status(),592);
        button(24,106,386,std::string("Search: ")+save_search+(editing==1?"|":""),304,editing==1,40);const char *filters[]={"All saves","Manual","Automatic","Favourites"};button(426,106,190,filters[save_filter],305,false,40);
        if(saves.empty())centered(212,"No matching saved games.",24);
        int offset=(save_index/4)*4;for(int i=0;i<4&&offset+i<(int)saves.size();i++)button(24,156+i*50,592,(saves[offset+i].favourite?"* ":"")+saves[offset+i].title,320+i,offset+i==save_index,42);
        button(24,366,160,"Previous",300);button(456,366,160,"Next",301);if(has_named_player_profile()){
            button(179,366,113,"Recovery",1550);
            button(299,366,149,"Import Old Saves",310);
        }else button(216,366,208,"Recovery",1550);
    }else if(page==12){
        if(saves.empty()){page=2;}else{
            Save entry=selected_save();centered(83,short_text(entry.title,36),24);
            if(preview_path!=entry.path){SDL_DestroySurface(preview);preview=IMG_Load((entry.path+".png").c_str());preview_path=entry.path;}
            frame(24,120,426,194);if(preview){double scale=std::min(414.0/preview->w,182.0/preview->h);SDL_Rect to={30+(414-(int)(preview->w*scale))/2,126+(182-(int)(preview->h*scale))/2,(int)(preview->w*scale),(int)(preview->h*scale)};SDL_BlitSurfaceScaled(preview,nullptr,canvas,&to,SDL_SCALEMODE_LINEAR);}else text(62,196,"No preview available",22,350);
            text(472,132,"Slot "+std::to_string(entry.slot),22,144);button(472,176,144,"Enlarge",307,false,36);button(472,222,144,"Check",1400,false,36);button(472,268,144,"Manage",842,false,36);text(472,306,entry.backup?"Backup "+std::to_string(entry.backup):automatic_save(entry)?"Automatic":"Manual",20,144);
            text(24,323,entry.campaign+" - Level "+std::to_string(entry.level),20,426);text(24,350,entry.date+"  |  "+(entry.seconds>=0?std::to_string(entry.seconds/60)+" min":"Time unknown"),18,592);
            const Save &group=saves[save_index];if(!group.backup)button(24,376,140,"Current",340,backup_choice==0,36);
            for(int number=1;number<=3;number++){bool exists=false;for(const Save &older:all_saves)if(older.slot==group.slot&&older.backup==number)exists=true;if(exists)button(24+number*148,376,140,"Backup "+std::to_string(number),340+number,entry.backup==number,36);}
            button(24,426,284,entry.backup?"Restore and Load":"Load Saved Game",302,false,42);button(332,426,284,"Return to Saves",901,false,42);
        }
    }else if(page==3){
        if(!actions.empty()){
            centered(96,short_text(game_key_settings[actions[action_index]].toml_name,34),24);button(24,142,180,"Previous",400);button(436,142,180,"Next",401);centered(198,short_text(binding_name(capturing?captured:settings.kbkeys[actions[action_index]].controller_buttons),43),22);
            button(134,244,372,capturing?"Recording...":"Record Binding",402,focus_action<0||focus_action==402,44);button(50,304,252,"Unassign",403);button(338,304,252,"Reset Action",404);button(134,360,372,"Save Recording",405,false,40);
        }
    }else if(page==4){
        if(!guide_more){
            int model=kfx_controller_guide_ps5?1:0;if(!guides[model])guides[model]=IMG_Load(prepare_file_path(FGrp_FxData,model?"controller-ps5-body.png":"controller-xbox-body.png"));
            if(guides[model]){SDL_Surface *image=guides[model];SDL_Rect crop={0,0,image->w,image->h};double scale=std::min(412.0/crop.w,220.0/crop.h);SDL_Rect to={(WIDTH-(int)(crop.w*scale))/2,88,(int)(crop.w*scale),(int)(crop.h*scale)};SDL_BlitSurfaceScaled(image,&crop,canvas,&to,SDL_SCALEMODE_LINEAR);}
            text(24,314,"Left stick: camera",22,288);text(332,314,"Right stick: pointer",22,284);text(24,348,kfx_controller_guide_ps5?"R2: select":"RT: select",22,288);text(332,348,kfx_controller_guide_ps5?"L2: drop":"LT: drop",22,284);
        }else if(guide_section==2){
            frame(24,90,592,278);
            const char *names[]={"Zoom in","Zoom out","Rotate left","Rotate right"};const int keys[]={Gkey_ZoomIn,Gkey_ZoomOut,Gkey_RotateCW,Gkey_RotateCCW};
            for(int i=0;i<4;i++){text(42,105+i*52,names[i],22,190);text(250,105+i*52,binding_name(get_game_key_controller_buttons(keys[i])),22,350);}
            small_line(42,336,"Hold all buttons shown in each combination.",548);
        }else{
            frame(24,90,592,278);const char *left[]={"Menu confirm","Menu back","Battle","Dungeon heart","Map","Pause menu"};const TbControllerButtons binds[]={kfx_menu_confirm_button(),kfx_menu_back_button(),get_game_key_controller_buttons(Gkey_ZoomToFight),get_game_key_controller_buttons(Gkey_ZoomRoomHeart),get_game_key_controller_buttons(Gkey_SwitchToMap),get_game_key_controller_buttons(Gkey_PauseMenu)};for(int i=0;i<6;i++){text(42,100+i*43,left[i],22,330);text(390,100+i*43,binding_name(binds[i]),22,210);}
        }
        button(134,382,372,guide_section==0?"More Controls":guide_section==1?"Camera Controls":"Controller Picture",705,false,36);
    }else if(page==6){
        if(editing!=4){if(!net_player_name[0])net_load_config_file();network_name=net_player_name[0]?net_player_name:net_config_info.net_player_name;}
        button(95,98,450,"Player: "+network_name+(editing==4?"|":""),611,selected==0||editing==4,44);
        button(134,156,372,"Test Connection",600,selected==1,44);button(134,212,372,"Compare Versions",606,selected==2,44);button(134,268,372,"Join by IP Address",607,selected==3,44);button(134,324,372,"Readiness Check",844,selected==4,44);
        button(134,378,372,"Server Browser / Accounts",2000,selected==5,32);
    }else if(page==9){
        frame(24,90,592,244);std::vector<std::string> lines;std::istringstream input(network_report);std::string line;
        int font=std::max(18,(int)round(22*kfx_text_scale)),line_height=font+8,visible=220/line_height;
        while(std::getline(input,line)){std::istringstream words(line);std::string word,current;while(words>>word){std::string next=current.empty()?word:current+" "+word;SDL_Surface *image=label(next,font);if(image&&image->w>548&&!current.empty()){lines.push_back(current);current=word;}else current=next;}lines.push_back(current);}
        if(lines.empty())lines.push_back(diagnostic_process?"Checking connection...":"Choose Retry to check the connection.");report_offset=std::clamp(report_offset,0,std::max(0,(int)lines.size()-visible));for(int i=0;i<visible&&report_offset+i<(int)lines.size();i++)text(42,103+i*line_height,lines[report_offset+i],22,556);
        button(24,342,284,diagnostic_process?"Checking...":"Retry",600,selected==0,44);button(332,342,284,"Copy Report",815,selected==1,44);
    }else if(page==10){
        centered(104,std::string("Your version: ")+VER_STRING,26);button(95,160,450,"Friend: "+peer_version+(editing==2?"|":""),601,true,46);
        frame(50,235,540,100);std::string peer=normalized_version(peer_version),ours=normalized_version(VER_STRING);centered(258,peer_version.empty()?"Enter your friend's version.":peer.empty()?"Use a version such as 1.4.0.0.":peer==ours?"Game versions match.":"Game versions are different.",24);centered(358,"Compare selected mods as well.",22);
    }else if(page==11){
        centered(106,"Hostname / IP address and UDP port",24);button(64,163,512,join_ip+(editing==3?"|":""),602,selected==0||editing==3,46);button(134,250,372,"Join Game",603,selected==1,46);centered(330,"Example: 192.168.1.20:5556",22);centered(368,"Enter the address supplied by the host.",20);
    }
    polish_draw_page();feature_draw();browser_draw();play_tools_draw();enhancement_draw();party_draw();
    if(page!=8&&page!=12&&(page!=36||cloud_create||!startup_login))button(134,426,372,page==38?"Close":page==36?(cloud_create?"Return to Sign In":"Return to Account"):(page==27&&choosing_local_profile)?"Return to Sign In":page==29?"Return to Saved Game":page==26?"Return to Devices":page==25?"Return to Account":page>=21&&page<=24?"Return to Browser":page==20?(frontend_menu_state==FeSt_NET_SERVICE?"Return to Multiplayer":"Return to Network"):page==15?"Return to Controller":page==16?"Return to Accessibility":page==17?"Return to Save":page==18?"Return to Management":page==19?"Return to Network":page==14?(help_parent==13?"Return to About":"Return to Saves"):page>=9&&page<=11?"Return to Network":"Return to Sections",901,false,42);
    if(!notice.empty()&&page<20&&page!=8&&page!=12&&page!=4&&page!=16&&page!=19)status_line(notice);
    if(preview_large&&page==12&&!saves.empty()){
        hits.clear();background();frame(18,82,604,328);if(preview){double scale=std::min(592.0/preview->w,316.0/preview->h);SDL_Rect to={24+(592-(int)(preview->w*scale))/2,88+(316-(int)(preview->h*scale))/2,(int)(preview->w*scale),(int)(preview->h*scale)};SDL_BlitSurfaceScaled(preview,nullptr,canvas,&to,SDL_SCALEMODE_LINEAR);}else centered(225,"No preview available",26);button(134,426,372,"Return to Saved Game",308,false,42);
    }
    if(display_confirmation){
        hits.clear();frame(48,142,544,230,true);centered(164,"Keep these display settings?",28);Uint64 now=SDL_GetTicks();int seconds=(int)((display_deadline>now?display_deadline-now:0)+999)/1000;centered(218,"Restoring in "+std::to_string(seconds)+" seconds.",24);button(68,302,236,"Keep",501,true,46);button(336,302,236,"Revert",502,false,46);
    }
    menu_cursor();
    if(capture_preview){capture_preview=false;IMG_SavePNG(canvas,path("options-preview.png").c_str());}
    int w=RendererScreenWidth(),h=RendererScreenHeight();double scale=std::min(w/(double)WIDTH,h/(double)HEIGHT);
    RendererPresentImageDesc desc={};desc.dst_w=WIDTH*scale;desc.dst_h=HEIGHT*scale;desc.dst_x=(w-desc.dst_w)/2;desc.dst_y=(h-desc.dst_h)/2;desc.src=(const unsigned char*)canvas->pixels;desc.src_pitch=canvas->pitch;desc.src_w=WIDTH;desc.src_h=HEIGHT;desc.format=PRESENT_FORMAT_RGBA8;desc.kind=PRESENT_KIND_OPAQUE;desc.filter=PRESENT_FILTER_NEAREST;desc.exclusive=1;RendererPresentImage(&desc);
}

#else
void kfx_options_recover_display(void){}
int kfx_options_select_replay(const char*){return 0;}
int kfx_options_community_action(int){return 0;}
const char *kfx_options_community_status(void){return "{}";}
void kfx_save_display_label(long,const char *fallback,char *buffer,unsigned int size){if(buffer&&size)snprintf(buffer,size,"%s",fallback?fallback:"");}
void kfx_options_initialize(void){}
void kfx_options_apply_display(void){}
int kfx_quick_game_key(long){return 0;}
int kfx_options_set_display(int,int,int){return 0;}
int kfx_options_confirm_display(int){return 0;}
void kfx_options_hide(void){}
void kfx_options_open(struct GuiButton*){}
void kfx_options_show_page(int){}
void kfx_options_preview(int){}
void kfx_options_button(struct GuiButton*){}
void kfx_options_draw(void){}
void kfx_caption_speech(int){}
void kfx_save_feedback(int,int,const char*){}
void kfx_options_display(struct GuiButton*){}
bool kfx_options_event(const SDL_Event*){return false;}
#endif
