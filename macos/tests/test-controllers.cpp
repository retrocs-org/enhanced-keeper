#include <SDL3/SDL.h>
#include "bflib_joyst.h"
#include "bflib_inputctrl.h"
#include "front_input.h"
#include "bflib_mouse.h"
#include "bflib_keybrd.h"
#include "game_legacy.h"
#include "frontend.h"
#include "macos_controls.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
extern "C" {
void JEvent(const SDL_Event *);
struct Game game={};
int frontend_menu_state=FeSt_INITIAL;
unsigned char lbKeyOn[KC_LIST_END]={};
void poll_controller_mouse_clicks(void);
static int mouse_down_count,mouse_up_count;
TbControllerButtons get_game_key_controller_buttons(long key){return key==Gkey_LeftClick?CBtn_R2:key==Gkey_RightClick?CBtn_L2:0;}
void mouseControl(unsigned int action,struct TbPoint *point){(void)point;if(action==MActn_LBUTTONDOWN)mouse_down_count++;if(action==MActn_LBUTTONUP)mouse_up_count++;}
GameTurn get_gameturn(void){return 0;}
unsigned char last_used_input_device = ID_Keyboard_Mouse;
int LbSyncLog(const char *fmt,...){va_list a;va_start(a,fmt);int r=vfprintf(stderr,fmt,a);va_end(a);return r;}
int LbWarnLog(const char *fmt,...){(void)fmt;return 0;}
int LbErrorLog(const char *fmt,...){(void)fmt;return 0;}
char *prepare_file_path(short,const char *){return (char *)"/dev/null";}
}
static void pump(){SDL_UpdateJoysticks();SDL_Event e;while(SDL_PollEvent(&e))JEvent(&e);}
static SDL_JoystickID attach(const char *name,Uint16 vendor,Uint16 product){
 SDL_VirtualJoystickDesc d;SDL_INIT_INTERFACE(&d);d.type=SDL_JOYSTICK_TYPE_GAMEPAD;d.vendor_id=vendor;d.product_id=product;d.naxes=SDL_GAMEPAD_AXIS_COUNT;d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;d.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;d.name=name;
 SDL_JoystickID id=SDL_AttachVirtualJoystick(&d);assert(id);pump();return id;
}
static void button(SDL_JoystickID id,Uint32 type,Uint8 button){SDL_Event e={};e.type=type;e.gbutton.which=id;e.gbutton.button=button;JEvent(&e);}
int main(){
 SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI,"0",SDL_HINT_OVERRIDE);
 SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS5,"0",SDL_HINT_OVERRIDE);
 SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_XBOX,"0",SDL_HINT_OVERRIDE);
 SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_MFI,"0",SDL_HINT_OVERRIDE);
 SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_IOKIT,"0",SDL_HINT_OVERRIDE);
 SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");init_controller_input();assert(!controller_connected());
 SDL_JoystickID sony=attach("DualSense virtual",0x054c,0x0ce6);assert(controller_connected());
 SDL_Joystick *raw=SDL_OpenJoystick(sony);assert(raw);
 button(sony,SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);assert(controller_button_state&CBtn_A);
 button(sony+999,SDL_EVENT_GAMEPAD_BUTTON_UP,SDL_GAMEPAD_BUTTON_SOUTH);assert(controller_button_state&CBtn_A);
 button(sony,SDL_EVENT_GAMEPAD_BUTTON_UP,SDL_GAMEPAD_BUTTON_SOUTH);assert(!(controller_button_state&CBtn_A));
 last_used_input_device=ID_Keyboard_Mouse;
 SDL_SetJoystickVirtualAxis(raw,SDL_GAMEPAD_AXIS_LEFTX,1000);pump();assert(std::abs(controller_raw_axis(SDL_GAMEPAD_AXIS_LEFTX)-1000.0f/32767)<.001f);assert(cbtn_axis_value(CBtn_LS_RIGHT)==0);assert(last_used_input_device==ID_Keyboard_Mouse);
 SDL_SetJoystickVirtualAxis(raw,SDL_GAMEPAD_AXIS_LEFTX,8100);pump();float near=cbtn_axis_value(CBtn_LS_RIGHT);assert(near>0 && near<0.02f);
 SDL_SetJoystickVirtualAxis(raw,SDL_GAMEPAD_AXIS_LEFTX,32767);pump();assert(cbtn_axis_value(CBtn_LS_RIGHT)>0.99f);
 SDL_SetJoystickVirtualAxis(raw,SDL_GAMEPAD_AXIS_LEFTX,-32768);pump();assert(cbtn_axis_value(CBtn_LS_LEFT)>0.99f);assert(!(controller_button_state&CBtn_LS_RIGHT));
 SDL_SetJoystickVirtualAxis(raw,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);pump();assert(controller_button_state&CBtn_R2);
 poll_controller_mouse_clicks();assert(mouse_down_count==1);
 SDL_SetJoystickVirtualAxis(raw,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);pump();assert(!(controller_button_state&CBtn_R2));poll_controller_mouse_clicks();assert(mouse_up_count==1);
 const SDL_GamepadButton inputs[]={SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_DPAD_UP};
 const TbControllerButtons outputs[]={CBtn_B,CBtn_X,CBtn_Y,CBtn_LEFTSHOULDER,CBtn_RIGHTSHOULDER,CBtn_BACK,CBtn_DPAD_UP};
 for(unsigned i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++){button(sony,SDL_EVENT_GAMEPAD_BUTTON_DOWN,inputs[i]);assert(controller_button_state&outputs[i]);button(sony,SDL_EVENT_GAMEPAD_BUTTON_UP,inputs[i]);assert(!(controller_button_state&outputs[i]));}
 SDL_JoystickID xbox=attach("Xbox virtual",0x045e,0x0b13);
 button(xbox,SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_NORTH);assert(!(controller_button_state&CBtn_Y));
 SDL_DetachVirtualJoystick(xbox);pump();assert(controller_connected());assert(controller_button_state&CBtn_LS_LEFT);
 xbox=attach("Xbox virtual",0x045e,0x0b13);
 SDL_CloseJoystick(raw);SDL_DetachVirtualJoystick(sony);pump();assert(controller_connected());assert(controller_button_state==0);
 button(xbox,SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_NORTH);assert(controller_button_state&CBtn_Y);
 clear_controller_input();assert(controller_button_state==0);
 button(xbox,SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_START);assert(controller_button_state&CBtn_START);
 SDL_Event trigger={};trigger.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;trigger.gaxis.which=xbox;trigger.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;trigger.gaxis.value=32767;JEvent(&trigger);poll_controller_mouse_clicks();assert(mouse_down_count==2);
 SDL_DetachVirtualJoystick(xbox);pump();assert(!controller_connected());assert(controller_button_state==0);poll_controller_mouse_clicks();assert(mouse_up_count==2);
 game.game_kind=GKind_Unset;frontend_menu_state=FeSt_MAIN_MENU;
 controller_button_state=CBtn_A;poll_controller_mouse_clicks();assert(mouse_down_count==3);
 game.game_kind=GKind_LocalGame;frontend_menu_state=FeSt_INITIAL;poll_controller_mouse_clicks();assert(mouse_up_count==3);
 game.game_kind=GKind_Unset;controller_button_state=CBtn_B;poll_controller_mouse_clicks();assert(lbKeyOn[KC_ESCAPE]);assert(!(controller_button_state&CBtn_B));
 controller_button_state=0;poll_controller_mouse_clicks();
 setenv("KFX_CONTROLLER_CURSOR_SPEED","2",1);assert(kfx_controller_cursor_speed()==2);setenv("KFX_CONTROLLER_CURSOR_SPEED","NaN",1);assert(kfx_controller_cursor_speed()==1);
 setenv("KFX_CONTROLLER_DEADZONE","0.4",1);assert(fabsf(kfx_controller_deadzone()-.4f)<.0001f);
 setenv("KFX_CONTROLLER_DEADZONE","nan",1);assert(fabsf(kfx_controller_deadzone()-8000.0f/32767)<.0001f);
 setenv("KFX_CONTROLLER_RUMBLE","0",1);assert(kfx_controller_rumble_strength()==0);
 setenv("KFX_CONTROLLER_RUMBLE","2",1);assert(kfx_controller_rumble_strength()==1);
 setenv("KFX_CONTROLLER_DEADZONE","0.4",1);init_controller_input();
 SDL_JoystickID adjusted=attach("Xbox adjustable",0x045e,0x0b13);SDL_Joystick *adjustedRaw=SDL_OpenJoystick(adjusted);
 assert(controller_device_model()==0);assert(strstr(controller_device_name(),"Xbox"));assert(controller_owns_event(adjusted));assert(!controller_owns_event(adjusted+999));
 SDL_SetJoystickVirtualAxis(adjustedRaw,SDL_GAMEPAD_AXIS_LEFTX,12000);pump();assert(cbtn_axis_value(CBtn_LS_RIGHT)==0);
 SDL_SetJoystickVirtualAxis(adjustedRaw,SDL_GAMEPAD_AXIS_LEFTX,20000);pump();assert(cbtn_axis_value(CBtn_LS_RIGHT)>0);
 kfx_controller_guide_visible=1;assert(cbtn_axis_value(CBtn_LS_RIGHT)==0);kfx_controller_guide_visible=0;
 SDL_JoystickID second=attach("DualSense selection",0x054c,0x0ce6);assert(controller_device_count()==2);assert(controller_select(second));assert(controller_active_id()==second&&controller_button_state==0);assert(strstr(controller_device_name(),"DualSense"));assert(!controller_select(999999));assert(controller_active_id()==second);
 button(adjusted,SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);assert(!(controller_button_state&CBtn_A));assert(controller_select(adjusted));assert(cbtn_axis_value(CBtn_LS_RIGHT)==0);SDL_SetJoystickVirtualAxis(adjustedRaw,SDL_GAMEPAD_AXIS_LEFTX,0);pump();assert(cbtn_axis_value(CBtn_LS_RIGHT)==0);SDL_SetJoystickVirtualAxis(adjustedRaw,SDL_GAMEPAD_AXIS_LEFTX,20000);pump();assert(cbtn_axis_value(CBtn_LS_RIGHT)>0);SDL_DetachVirtualJoystick(second);pump();assert(controller_active_id()==adjusted);
 SDL_CloseJoystick(adjustedRaw);SDL_DetachVirtualJoystick(adjusted);pump();
 SDL_Quit();puts("PASS: virtual DualSense/Xbox hot-plug, face/start mapping, active-pad filtering, dead zones, smooth/full stick range, failover, focus clear, disconnect cleanup");
}
