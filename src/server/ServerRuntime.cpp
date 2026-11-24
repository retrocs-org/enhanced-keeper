#include "pre_inc.h"
#include "net_hub.h"
#include "net_game.h"
#include "game_legacy.h"
#include "game_merge.h"
#include "net_main.h"
#include "bflib_enet.h"
#include "frontend.h"
#include "config_campaigns.h"
#include "config.h"
#include "kfx/platform/PlatformManager.h"
#include "macos_readiness.h"
#include <string>
#include "post_inc.h"
// Directory heartbeats, identity replies and compatibility checks are simulation
// services. They must continue even though there are no UI frames to draw.
extern "C" void kfx_server_tick(void) {
    static bool initialized=false;
    if(!initialized){initialized=true;std::string folder=PlatformManager_GetUserPrefDir();folder+="/save";kfx_hub_initialize(folder.c_str());}
    bool hosting=network_is_active()&&network_is_host();
    bool playing=hosting&&frontend_menu_state==FeSt_INITIAL;
    std::string map;
    if(playing){map=campaign.fname;if(map.size()>4&&map.substr(map.size()-4)==".cfg")map.resize(map.size()-4);map+=":"+std::to_string(get_loaded_level_number());}
    kfx_hub_tick(hosting,hosting?GetRemoteUserCount()+1:0,playing,map.c_str());
    kfx_readiness_tick();
}
