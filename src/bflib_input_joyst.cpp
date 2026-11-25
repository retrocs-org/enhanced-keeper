/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet or Dungeon Keeper.
/******************************************************************************/
/** @file bflib_input_joyst.cpp
 *     Input devices control and polling.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "bflib_joyst.h"
#include "macos_controls.h"
#include "bflib_inputctrl.h"
#include "bflib_keybrd.h"
#include "bflib_mouse.h"
#include "bflib_video.h"
#include "bflib_planar.h"
#include "config_keeperfx.h"
#include "config.h"
#include "config_settings.h"
#include "front_input.h"
#include "game_legacy.h"
#include "kjm_input.h"
#include "frontend.h"
#include <SDL3/SDL.h>
#include <cctype>
#include <cstring>
#include <cstdio>
#include "post_inc.h"

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

static SDL_Gamepad *controller = NULL;
static TbBool disconnect_pending;
static char preferred_guid[33]={};
static bool suppress_axes=false;
void controller_suppress_until_neutral(void){suppress_axes=true;}
TbBool take_controller_disconnect(void){TbBool result=disconnect_pending;disconnect_pending=false;return result;}
static int stick_deadzone = 8000;
static const int trigger_deadzone = 4000;

static TbControllerButtons internal_button_state = 0;
TbControllerButtons controller_button_state = 0;

static void open_controller(SDL_JoystickID id)
{
    if(getenv("KFX_HEADLESS"))return;
    if(controller){char guid[33];controller_guid_for_id(id,guid,sizeof(guid));if(preferred_guid[0]&&!strcmp(guid,preferred_guid)&&controller_active_id()!=id)controller_select(id);return;}
    controller = SDL_OpenGamepad(id);
    if (controller) {
        internal_button_state = 0;
        controller_button_state = 0;
        SDL_GamepadType type=SDL_GetGamepadType(controller);
        kfx_controller_guide_ps5=(type==SDL_GAMEPAD_TYPE_PS3 || type==SDL_GAMEPAD_TYPE_PS4 || type==SDL_GAMEPAD_TYPE_PS5);
        SYNCLOG("Gamepad connected: %s", SDL_GetGamepadName(controller));
    } else {
        WARNLOG("Could not open gamepad: %s", SDL_GetError());
    }
}

static void close_controller()
{
    internal_button_state = 0;
    controller_button_state = 0;
    if (controller) {
        SDL_CloseGamepad(controller);
        controller = NULL;
    }
}

static void open_next_controller()
{
    if (controller) return;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for(int i=0;ids&&i<count&&!controller&&preferred_guid[0];i++){char guid[33];controller_guid_for_id(ids[i],guid,sizeof(guid));if(!strcmp(guid,preferred_guid))open_controller(ids[i]);}
    for (int i = 0; ids && i < count && !controller; ++i)
        open_controller(ids[i]);
    SDL_free(ids);
}

TbBool controller_owns_event(unsigned int id){return controller && SDL_GetGamepadID(controller)==id;}

const char *controller_device_name(void){const char *name=controller?SDL_GetGamepadName(controller):nullptr;return name?name:"No controller connected";}
int controller_device_model(void){if(!controller)return -1;SDL_GamepadType type=SDL_GetGamepadType(controller);return type==SDL_GAMEPAD_TYPE_PS3||type==SDL_GAMEPAD_TYPE_PS4||type==SDL_GAMEPAD_TYPE_PS5;}

void controller_guid_for_id(unsigned int id,char *buffer,int size){SDL_GUIDToString(SDL_GetGamepadGUIDForID(id),buffer,size);}
void controller_prefer_guid(const char *guid){preferred_guid[0]=0;if(!guid||strlen(guid)!=32)return;for(int i=0;i<32;i++)if(!isxdigit((unsigned char)guid[i]))return;snprintf(preferred_guid,sizeof(preferred_guid),"%s",guid);if(controller){char current[33];controller_guid_for_id(controller_active_id(),current,sizeof(current));if(!strcmp(current,preferred_guid))return;}for(int i=0;i<controller_device_count();i++){unsigned int id=controller_device_id(i);char value[33];controller_guid_for_id(id,value,sizeof(value));if(!strcmp(value,preferred_guid)){controller_select(id);break;}}}
unsigned int controller_active_id(void){return controller?SDL_GetGamepadID(controller):0;}
int controller_device_count(void){int count=0;SDL_JoystickID *ids=SDL_GetGamepads(&count);SDL_free(ids);return count;}
unsigned int controller_device_id(int index){int count=0;SDL_JoystickID *ids=SDL_GetGamepads(&count);unsigned int id=ids&&index>=0&&index<count?ids[index]:0;SDL_free(ids);return id;}
const char *controller_name_for_id(unsigned int id){const char *name=SDL_GetGamepadNameForID(id);return name?name:"Disconnected controller";}
TbBool controller_select(unsigned int id){
    if(!id||!SDL_IsGamepad(id))return false;
    if(controller_active_id()==id)return true;
    SDL_Gamepad *next=SDL_OpenGamepad(id);if(!next)return false;
    disconnect_pending=true;close_controller();controller=next;suppress_axes=true;internal_button_state=controller_button_state=0;
    int model=controller_device_model();if(model>=0)kfx_controller_guide_ps5=model;return true;
}

void clear_controller_input()
{
    internal_button_state = 0;
    controller_button_state = 0;
}

static TbControllerButtons SDL_gamepadbutton_to_controllerbutton(SDL_GamepadButton button)
{
    switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:          return CBtn_A;
        case SDL_GAMEPAD_BUTTON_EAST:           return CBtn_B;
        case SDL_GAMEPAD_BUTTON_WEST:           return CBtn_X;
        case SDL_GAMEPAD_BUTTON_NORTH:          return CBtn_Y;
        case SDL_GAMEPAD_BUTTON_BACK:           return CBtn_BACK;
        case SDL_GAMEPAD_BUTTON_START:          return CBtn_START;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return CBtn_LEFTSTICK;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return CBtn_RIGHTSTICK;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return CBtn_LEFTSHOULDER;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return CBtn_RIGHTSHOULDER;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:        return CBtn_DPAD_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return CBtn_DPAD_DOWN;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return CBtn_DPAD_LEFT;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return CBtn_DPAD_RIGHT;
        case SDL_GAMEPAD_BUTTON_MISC1:          return CBtn_MISC1;
        case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:  return CBtn_PADDLE1;
        case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:   return CBtn_PADDLE2;
        case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:  return CBtn_PADDLE3;
        case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:   return CBtn_PADDLE4;
        case SDL_GAMEPAD_BUTTON_TOUCHPAD:       return CBtn_TOUCHPAD;
        default: break;
    }
    return CBtn_NONE;
}

static TbControllerButtons SDL_axis_to_controllerbutton(SDL_GamepadAxis axis, int8_t sign)
{
    switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return CBtn_L2;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return CBtn_R2;
        case SDL_GAMEPAD_AXIS_LEFTX:  return sign < 0 ? CBtn_LS_LEFT : CBtn_LS_RIGHT;
        case SDL_GAMEPAD_AXIS_LEFTY:  return sign < 0 ? CBtn_LS_UP   : CBtn_LS_DOWN;
        case SDL_GAMEPAD_AXIS_RIGHTX: return sign < 0 ? CBtn_RS_LEFT : CBtn_RS_RIGHT;
        case SDL_GAMEPAD_AXIS_RIGHTY: return sign < 0 ? CBtn_RS_UP   : CBtn_RS_DOWN;
        default: break;
    }
    return CBtn_NONE;
}

void JEvent(const SDL_Event *ev)
{
    SYNCDBG(10, "Starting");

    switch (ev->type)
    {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    {
        if (!controller || ev->gbutton.which != SDL_GetGamepadID(controller)) break;
        last_used_input_device = ID_Controller;
        SDL_GamepadButton button_val = (SDL_GamepadButton)ev->gbutton.button;
        TbControllerButtons controller_btn = SDL_gamepadbutton_to_controllerbutton(button_val);

        if (controller_btn != CBtn_NONE)
        {
            if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            {
                if (!(internal_button_state & controller_btn)) {
                    controller_button_state |= controller_btn;
                    internal_button_state |= controller_btn;
                }
            }
            else
            {
                controller_button_state &= ~controller_btn;
                internal_button_state &= ~controller_btn;
            }
        }
    }
    break;

    case SDL_EVENT_GAMEPAD_ADDED:
        open_controller(ev->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (controller && ev->gdevice.which == SDL_GetGamepadID(controller)) {
            disconnect_pending=true;
            close_controller();
            open_next_controller();
        }
        break;
    case SDL_EVENT_GAMEPAD_REMAPPED:
        if (controller && ev->gdevice.which == SDL_GetGamepadID(controller))
            clear_controller_input();
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            if (!controller || ev->gaxis.which != SDL_GetGamepadID(controller)) break;
            SDL_GamepadAxis axis = (SDL_GamepadAxis)ev->gaxis.axis;
            const bool trigger = axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            stick_deadzone=(int)(32767*kfx_controller_deadzone());
            const int deadzone = trigger ? trigger_deadzone : stick_deadzone;
            if (ev->gaxis.value > deadzone || (!trigger && ev->gaxis.value < -deadzone))
                last_used_input_device = ID_Controller;
            TbControllerButtons btn_pos = SDL_axis_to_controllerbutton(axis, 1);
            TbControllerButtons btn_neg = trigger ? static_cast<TbControllerButtons>(CBtn_NONE) : SDL_axis_to_controllerbutton(axis, -1);

            if (ev->gaxis.value > deadzone)
            {
                if (btn_neg != CBtn_NONE) {
                    controller_button_state &= ~btn_neg;
                    internal_button_state &= ~btn_neg;
                }
                if (btn_pos != CBtn_NONE && !(internal_button_state & btn_pos)) {
                    controller_button_state |= btn_pos;
                    internal_button_state |= btn_pos;
                }
            }
            else if (!trigger && ev->gaxis.value < -deadzone)
            {
                if (btn_pos != CBtn_NONE) {
                    controller_button_state &= ~btn_pos;
                    internal_button_state &= ~btn_pos;
                }
                if (btn_neg != CBtn_NONE && !(internal_button_state & btn_neg)) {
                    controller_button_state |= btn_neg;
                    internal_button_state |= btn_neg;
                }
            }
            else
            {
                if (btn_pos != CBtn_NONE) {
                    controller_button_state &= ~btn_pos;
                    internal_button_state &= ~btn_pos;
                }
                if (btn_neg != CBtn_NONE) {
                    controller_button_state &= ~btn_neg;
                    internal_button_state &= ~btn_neg;
                }
            }
        }
        break;
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
    default:
        break;
    }
}

void controller_rumble(long ms)
{
    if (controller != NULL && last_used_input_device == ID_Controller) {
        Uint16 strength=(Uint16)(65535*kfx_controller_rumble_strength());
        SDL_RumbleGamepad(controller, strength, strength, (Uint32)ms);
    }
}

void init_controller_input()
{
    if(getenv("KFX_HEADLESS"))return;
    stick_deadzone=(int)(32767*kfx_controller_deadzone());
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_MFI, "1");
    if (!(SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD) && !SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        ERRORLOG("SDL gamepad init: %s", SDL_GetError());
        return;
    }
    SDL_AddGamepadMappingsFromFile(prepare_file_path(FGrp_FxData, "gamecontrollerdb.txt"));

    // Built-in SDL HIDAPI/GCController mappings cover DualSense and Xbox over
    // supported USB/Bluetooth transports; the database adds third-party pads.
    open_next_controller();
}

TbControllerButtons controller_raw_buttons(void){TbControllerButtons result=0;if(controller)for(int i=0;i<SDL_GAMEPAD_BUTTON_COUNT;i++)if(SDL_GetGamepadButton(controller,(SDL_GamepadButton)i))result|=SDL_gamepadbutton_to_controllerbutton((SDL_GamepadButton)i);return result;}
float controller_raw_axis(int axis)
{
    if (!controller || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) return 0;
    return SDL_GetGamepadAxis(controller,(SDL_GamepadAxis)axis)/32767.0f;
}

float cbtn_axis_value(TbControllerButtons btn)
{
    if (controller == NULL || kfx_controller_guide_visible) {
        return 0.0f;
    }

    const float deadzone = 32767*kfx_controller_deadzone();
    if(suppress_axes){bool neutral=true;for(int i=0;i<4;i++)if(abs(SDL_GetGamepadAxis(controller,(SDL_GamepadAxis)i))>deadzone)neutral=false;if(!neutral)return 0;suppress_axes=false;}
    const float max_axis = 32767.0f;

    float value = 0.0f;

    auto sample_direction = [&](Sint16 axis_value, float sign) {
        float directional = ((float)axis_value) * sign;
        if (directional <= deadzone) {
            return 0.0f;
        }
        float normalized = (directional - deadzone) / (max_axis - deadzone);
        if (normalized > 1.0f) {
            normalized = 1.0f;
        }
        return normalized;
    };

    if (btn & (CBtn_LS_LEFT|CBtn_LS_RIGHT|CBtn_LS_UP|CBtn_LS_DOWN|CBtn_RS_LEFT|CBtn_RS_RIGHT|CBtn_RS_UP|CBtn_RS_DOWN))
    {
        const Sint16 left_x = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 left_y = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);
        const Sint16 right_x = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHTX);
        const Sint16 right_y = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHTY);

        if (btn & CBtn_LS_LEFT)  value = std::max(value, sample_direction(left_x,  -1.0f));
        if (btn & CBtn_LS_RIGHT) value = std::max(value, sample_direction(left_x,   1.0f));
        if (btn & CBtn_LS_UP)    value = std::max(value, sample_direction(left_y,  -1.0f));
        if (btn & CBtn_LS_DOWN)  value = std::max(value, sample_direction(left_y,   1.0f));
        if (btn & CBtn_RS_LEFT)  value = std::max(value, sample_direction(right_x, -1.0f));
        if (btn & CBtn_RS_RIGHT) value = std::max(value, sample_direction(right_x,  1.0f));
        if (btn & CBtn_RS_UP)    value = std::max(value, sample_direction(right_y, -1.0f));
        if (btn & CBtn_RS_DOWN)  value = std::max(value, sample_direction(right_y,  1.0f));

        if (value > 0.0f) {
            return value;
        }
    }

    if ((controller_button_state & btn) != 0)
        return 1.0;

    return 0.0;
}

TbBool controller_connected()
{
    return controller != NULL;
}
/******************************************************************************/
#ifdef __cplusplus
}
#endif
