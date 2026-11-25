// Account-scoped friends, party chat and server prompts.
struct PartyPlayer {
    std::string id, name, status, readiness, join;
    bool online = false;
    int unread = 0;
};
struct PartyView {
    std::string account, id, owner, server, server_name, prompt;
    BrowserServer target;
    std::string target_revision, target_blocker;
    std::vector<PartyPlayer> friends, members, invites;
    std::vector<std::string> chat, direct_chat;
    int unread=0, party_unread=0;
};
PartyView party;
std::string party_notice, party_message, party_join_server, shown_party_prompt, direct_friend, direct_name;
std::map<int, PartyPlayer> party_targets;
int party_tab = 0, party_offset = 0, party_chat_offset = 0;
Uint64 party_poll_at = 0, party_last_input = 0, social_toast_until = 0;
bool mute_social_in_match=false;
std::future<std::pair<bool,std::string>> party_scan;
std::string party_scan_key;
Uint64 party_check_at=0;
std::string joining_prompt, pending_party_progress;
bool party_join_connecting=false;
std::string party_match_room,party_match_address,party_match_result;
std::array<bool,MAX_NET_USERS> party_match_departed{};
bool party_match_active=false,party_rematch_pending=false;

void party_match_tick()
{
    const bool playing=network_is_active()&&game.game_kind==GKind_MultiGame&&get_gameturn()>0;
    if(playing&&party.id==party_match_room&&!party_match_room.empty()){
        party_match_active=true;
        for(int i=0;i<MAX_NET_USERS;++i)
            party_match_departed[i]|=kfx_hub_peer_departed(i)!=0;
        const auto result=get_my_player()->victory_state;
        if(result==VicS_WonLevel)party_match_result="Victory";
        if(result==VicS_LostLevel)party_match_result="Defeat";
    }else if(party_match_active&&frontend_menu_state==FeSt_MAIN_MENU){
        party_match_active=false;
        if(party.id!=party_match_room){party_match_result.clear();return;}
        if(party_match_result.empty())party_match_result="Match ended without a local result";
        if(!kfx_controller_guide_visible){party_tab=5;open_panel(38);}
    }
}

void party_join_result(const std::string &result)
{
    if(joining_prompt.empty())return;
    pending_party_progress=result;
    party_notice=result;
    party_join_connecting=false;
}

bool party_all_ready()
{
    return !party.target_revision.empty() && party.target_blocker.empty() && !party.members.empty() &&
        std::all_of(party.members.begin(),party.members.end(),[](const PartyPlayer &member){
            return member.readiness=="Ready";
        });
}
int social_unread(){return party.unread+static_cast<int>(party.invites.size());}
std::string social_prefs_path(){return profiles_base()+"/social-mute.cfg";}

void party_notification_draw()
{
    if(!social_unread()||SDL_GetTicks()>=social_toast_until||!winfont)return;
    if(mute_social_in_match&&(game.game_kind==GKind_LocalGame||
       (network_is_active()&&frontend_menu_state==FeSt_INITIAL)))return;
    const int width=RendererScreenWidth();
    const int panel_width=std::min(360,width/2);
    const int left=width-panel_width-12;
    const int flags=RendererGetDrawFlags();
    LbDrawBox(left,12,panel_width,40,0);
    LbTextSetFont(winfont);
    LbTextSetWindow(left+6,20,panel_width-12,24);
    RendererSetDrawFlags(Lb_TEXT_HALIGN_CENTER);
    const std::string summary="Social updates: "+std::to_string(social_unread())+"  (F8)";
    const int units=std::max(12,std::min(24,18*16/std::max(1,(int)LbTextLineHeight())));
    LbTextDrawResized(0,0,units,summary.c_str());
    RendererSetDrawFlags(flags);
}

bool party_owner()
{
    return !party.account.empty() && party.owner == party.account;
}

void note_party_input()
{
    party_last_input=SDL_GetTicks();
}

const char *party_activity()
{
    if(game.game_kind==GKind_LocalGame||(network_is_active()&&frontend_menu_state==FeSt_INITIAL))
        return "playing";
    if(network_is_active()&&frontend_menu_state==FeSt_NET_START)
        return "lobby";
    return SDL_GetTicks()-party_last_input>=120000?"idle":"online";
}

const char *party_status_label(const PartyPlayer &player)
{
    if(!player.online||player.status=="offline")return "Offline";
    if(player.status=="lobby")return "In lobby";
    if(player.status=="playing")return "Playing";
    if(player.status=="idle")return "Idle";
    return "Online";
}

bool party_request(const char *action, const char *field = nullptr, const std::string &value = {}, const std::string &message = {})
{
    const std::string account = kfx_hub_account_id();
    if (account.empty()) {
        party_notice = "Sign in to a cloud account to use Friends and Party.";
        return false;
    }
    std::string body = "{\"action\":" + hub_quote(action);
    if (field) body += std::string(",\"") + field + "\":" + hub_quote(value);
    if (!message.empty()) body += ",\"message\":" + hub_quote(message);
    if(strcmp(action,"state")==0){
        body += ",\"activity\":" + hub_quote(party_activity());
        if(kfx_controller_guide_visible&&page==38){
            if(party_tab==1)body+=",\"reading\":\"party\"";
            if(party_tab==4)body+=",\"reading\":\"friend\"";
        }
    }
    body += "}";
    const auto tag = std::string("party:") + account + ":" + action;
    if (kfx_hub_request(tag.c_str(), "/api/party", body.c_str(), 1)) {
        party_poll_at = SDL_GetTicks() + 4000;
        return true;
    }
    party_notice = "Please try again in a moment.";
    return false;
}

std::vector<PartyPlayer> party_players(const VALUE &root, const char *key)
{
    std::vector<PartyPlayer> result;
    const VALUE *rows = value_dict_get(&root, key);
    if (!rows || value_type(rows) != VALUE_ARRAY) return result;
    for (size_t i = 0; i < value_array_size(rows); ++i) {
        const VALUE *row = value_array_get(rows, i);
        const VALUE *online = value_dict_get(row, "online");
        result.push_back({hub_field(row, "id"), hub_field(row, "name"), hub_field(row, "status"), hub_field(row, "readiness"), hub_field(row,"join"),
            online && value_type(online) == VALUE_BOOL && value_bool(online), hub_integer(row,"unread")});
    }
    return result;
}

void party_reply(const std::string &tag, const VALUE &root, const std::string &error)
{
    const std::string prefix = std::string("party:") + kfx_hub_account_id() + ":";
    if (tag.rfind(prefix, 0) != 0 || party.account != kfx_hub_account_id()) return;
    const bool poll = tag == prefix + "state";
    if (!error.empty() || kfx_hub_result_status() != 200) {
        if (poll) party_poll_at = SDL_GetTicks() + 15000;
        if (!poll || page == 38) party_notice = error.empty() ? "Party service unavailable." : error;
        return;
    }
    const int previous_unread=social_unread();
    party.id = hub_field(&root, "party");
    party.owner = hub_field(&root, "owner");
    party.friends = party_players(root, "friends");
    party.members = party_players(root, "members");
    party.invites = party_players(root, "invites");
    party.unread=std::max(0,hub_integer(&root,"unread"));
    party.party_unread=std::max(0,hub_integer(&root,"party_unread"));
    if(social_unread()>previous_unread&&(!mute_social_in_match||strcmp(party_activity(),"playing")!=0))
        social_toast_until=SDL_GetTicks()+5000;
    if (hub_field(&root, "conversation") == direct_friend) {
        party.direct_chat.clear();
        const VALUE *direct = value_dict_get(&root, "friend_messages");
        if (direct && value_type(direct) == VALUE_ARRAY) {
            for (size_t i = 0; i < value_array_size(direct); ++i) {
                const VALUE *row = value_array_get(direct, i);
                party.direct_chat.push_back(hub_field(row, "name") + ": " + hub_field(row, "body"));
            }
        }
    }
    party.chat.clear();
    const VALUE *messages = value_dict_get(&root, "messages");
    if (messages && value_type(messages) == VALUE_ARRAY) {
        for (size_t i = 0; i < value_array_size(messages); ++i) {
            const VALUE *row = value_array_get(messages, i);
            party.chat.push_back(hub_field(row, "name") + ": " + hub_field(row, "body"));
        }
    }
    const VALUE *prompt = value_dict_get(&root, "prompt");
    if (prompt && value_type(prompt) != VALUE_DICT) prompt = nullptr;
    party.server = hub_field(prompt, "id");
    party.server_name = hub_field(prompt, "name");
    party.prompt = hub_field(prompt, "idempotency_key");
    if(party.prompt!=joining_prompt){
        party_join_server.clear();
        joining_prompt.clear();
        pending_party_progress.clear();
        party_join_connecting=false;
    }
    const VALUE *target = value_dict_get(&root,"target");
    if (target && value_type(target)!=VALUE_DICT) target=nullptr;
    const auto revision=hub_field(target,"revision");
    if(party.target_revision!=revision)party_check_at=0;
    party.target_revision=revision;
    party.target_blocker=hub_field(target,"blocker");
    const VALUE *listing=target?value_dict_get(target,"server"):nullptr;
    party.target=listing&&value_type(listing)==VALUE_DICT?browser_server_from_json(listing):BrowserServer{};
    party.target.id=hub_field(target,"id");
    if (!poll && tag != prefix + "readiness" && tag != prefix + "join-progress") {
        party_notice = tag == prefix + "prompt" ? "Join prompt sent to the party." :
            tag == prefix + "invite" ? "Party invitation sent." : "";
        if (tag == prefix + "accept") { party_tab = 1; party_offset = 0; }
    }
    if (!party.prompt.empty() && party.prompt != shown_party_prompt && !party_owner() &&
        !kfx_controller_guide_visible && !network_is_active() && frontend_menu_state == FeSt_MAIN_MENU) {
        shown_party_prompt = party.prompt;
        party_tab = 1;
        party_offset = 0;
        open_panel(38);
    }
}

// Recheck while the selection is live so a disconnected member's readiness expires.
bool party_check_target()
{
    const auto scan_key=party.account+"|"+party.target_revision+"|"+keeper_runtime_directory;
    std::pair<bool,std::string> scanned;
    bool scanned_current=false;
    if(party_scan.valid()){
        if(party_scan.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        scanned=party_scan.get();
        scanned_current=party_scan_key==scan_key;
    }
    if(party.target_revision.empty()||!party.target_blocker.empty()||!party.prompt.empty()||
        SDL_GetTicks()<party_check_at)return false;
    std::string result="Ready";
    if(network_is_active()||game.game_kind==GKind_LocalGame)result="Already in a game or lobby";
    else if(!supported_server_version(party.target))result="Unsupported client version";
    else {
        const auto missing=missing_server_assets();
        if(!missing.empty())result="Missing "+missing;
        else if(!party.target.content.empty()){
            if(!scanned_current){
                const std::string root=keeper_runtime_directory,map=party.target.map;
                party_scan_key=scan_key;
                party_scan=std::async(std::launch::async,[root,map](){
                    char fingerprint[256]={};
                    const bool ok=kfx_content_fingerprint(root.c_str(),map.c_str(),fingerprint,sizeof(fingerprint));
                    return std::make_pair(ok,std::string(fingerprint));
                });
                return false;
            }
            if(!scanned.first)result=scanned.second.empty()?"Could not check local content":scanned.second;
            else if(scanned.second!=party.target.content)result="Campaign, map or mods differ";
        }
    }
    if(!party_request("readiness","revision",party.target_revision,short_text(result,145)))return false;
    party_check_at=SDL_GetTicks()+8000;
    // Readiness reports do not substitute for presence polls or conversation refreshes.
    party_poll_at=SDL_GetTicks();
    return true;
}

void party_tick()
{
    const std::string account = kfx_hub_account_id();
    if (party.account != account) {
        party = {};
        party.account = account;
        party_notice.clear();
        party_message.clear();
        direct_friend.clear();
        direct_name.clear();
        shown_party_prompt.clear();
        party_join_server.clear();
        joining_prompt.clear();
        pending_party_progress.clear();
        party_join_connecting=false;
        party_poll_at = 0;
        party_last_input = SDL_GetTicks();
        social_toast_until = 0;
        mute_social_in_match=access(social_prefs_path().c_str(),F_OK)==0;
        party_offset = 0;
        party_check_at = 0;
        party_match_room.clear();party_match_result.clear();party_match_active=false;party_rematch_pending=false;
    }
    party_match_tick();
    if (!account.empty() && !kfx_hub_busy() && !pending_party_progress.empty()){
        if(party_request("join-progress","prompt",joining_prompt,short_text(pending_party_progress,145)))
            pending_party_progress.clear();
        return;
    }
    if (!account.empty() && !kfx_hub_busy() && party_check_target())return;
    if (!account.empty() && !kfx_hub_busy() && SDL_GetTicks() >= party_poll_at)
        party_request("state",party_tab==4?"user":nullptr,direct_friend);
}

bool party_action(int action)
{
    if (action == 2120) {
        party_message.clear();
        party_tab = party.id==party_match_room&&!party_match_result.empty()&&!party_match_active?5:0;
        party_offset = 0;
        party_poll_at = 0;
        open_panel(38);
        return true;
    }
    if (action == 2121 && page == 21 && party_owner() && !browser_servers.empty()) {
        const auto &server = browser_servers[browser_index];
        if (!server.id.empty()) {
            if(party.target.id!=server.id||!party.target_blocker.empty())
                party_request("select-server","server",server.id);
            party_tab=1;
            party_offset=0;
            open_panel(38);
        }
        return true;
    }
    if (page != 38) return false;
    if(action==2136){
        if(party_owner()&&party.id==party_match_room&&kfx_hub_request("servers","/api/servers",nullptr,0)){
            party_rematch_pending=true;
            party_notice="Looking for the next lobby...";
        }
        return true;
    }
    if(action==2137){browser_tab=0;open_panel(20);browser_refresh();return true;}
    if(action==2135){
        if(party_owner()&&party_all_ready()&&party.prompt.empty())
            party_request("prompt","server",party.target.id);
        return true;
    }
    if(action==2134){
        const auto file=social_prefs_path();
        const bool ok=mute_social_in_match?unlink(file.c_str())==0:write_small_file(file,"1\n");
        if(ok){mute_social_in_match=!mute_social_in_match;social_toast_until=0;}
        else party_notice="Could not save the notification setting.";
        return true;
    }
    if (action >= 2122 && action <= 2124) {
        party_message.clear();
        party_tab = action - 2122;
        party_poll_at=0;
        party_offset = party_chat_offset = selected = 0;
        if(editing==19){editing=0;SDL_StopTextInput(SDL_GetKeyboardFocus());}
    } else if (action == 2125) {
        if (party_tab==4 || !party.id.empty()) start_edit(19);
    } else if (action == 2126) {
        if (!party_message.empty()) {
            const bool sent = party_tab == 4
                ? party_request("friend-message", "user", direct_friend, party_message)
                : party_request("message", "message", party_message);
            if (sent) {party_message.clear();party_chat_offset=0;}
        }
    } else if (action == 2127) {
        party_request("leave");
    } else if (action == 2128) {
        if (network_is_active() || game.game_kind == GKind_LocalGame)
            party_notice = "Return to the main menu before joining.";
        else if (!party.server.empty()&&!party_join_connecting) {
            if(party_request("join-progress","prompt",party.prompt,"Joining")){
                joining_prompt=party.prompt;
                party_join_connecting=true;
                party_notice="Joining "+party.server_name+"...";
                if(kfx_hub_request("servers","/api/servers",nullptr,0))
                    party_join_server=party.server;
                else party_join_result("Server request queue is full. Try again.");
            }
        }
    } else if (action == 2129) {
        party_offset = std::max(0, party_offset - 1);
    } else if (action == 2130) {
        ++party_offset;
    } else if (action >= 2160 && action < 2164 && (party_tab == 0 || party_tab == 4)) {
        auto friend_action = party_targets.find(action);
        if (friend_action != party_targets.end()) {
            direct_friend = friend_action->second.id;
            direct_name = friend_action->second.name;
            party.direct_chat.clear();
            party_message.clear();
            party_tab = 4;
            party_chat_offset = selected = 0;
            party_request("state", "user", direct_friend);
        }
    } else if (action >= 2140 && action < 2144) {
        const auto target = party_targets.find(action);
        if (target == party_targets.end()) return true;
        if (party_tab == 0 || party_tab == 4) party_request("invite", "user", target->second.id);
        else if (party_tab == 2) party_request("accept", "party", target->second.id);
        else if (party_tab == 3) party_request("kick", "user", target->second.id);
    } else if (action >= 2150 && action < 2154 && party_tab == 2) {
        const auto target = party_targets.find(action);
        if (target != party_targets.end()) party_request("decline", "party", target->second.id);
    } else if ((action == 2132 || action == 2133) && (party_tab == 1 || party_tab == 4)) {
        party_chat_offset = std::max(0, party_chat_offset + (action == 2132 ? 3 : -3));
    } else if (action == 2131) {
        party_tab = 3;
        party_offset = selected = 0;
    } else return false;
    return true;
}

void draw_party_chat(const std::vector<std::string> &messages, int y, int count)
{
    std::vector<std::string> lines;
    for (const auto &message : messages) {
        size_t offset = 0;
        while (offset < message.size()) {
            size_t length = std::min<size_t>(39,message.size()-offset);
            if(offset+length<message.size()){
                const size_t space=message.rfind(' ',offset+length);
                if(space!=std::string::npos&&space>offset)length=space-offset;
                else while((static_cast<unsigned char>(message[offset+length])&0xc0)==0x80)--length;
            }
            lines.push_back(message.substr(offset,length));
            offset+=length;
            while(offset<message.size()&&message[offset]==' ')++offset;
        }
    }
    const int latest=std::max(0,static_cast<int>(lines.size())-count);
    party_chat_offset=std::clamp(party_chat_offset,0,latest);
    const int first=latest-party_chat_offset;
    for(int i=0;i<count&&first+i<static_cast<int>(lines.size());++i)
        small_line(275,y+i*25,lines[first+i],335);
}

void party_draw()
{
    if (page != 38) return;
    party_targets.clear();
    auto control = [](int x, int y, int width, const std::string &label, int action, int height=34) {
        button(x, y, width, label, action, static_cast<int>(hits.size()) == selected, height);
    };
    const int friend_unread=std::max(0,party.unread-party.party_unread);
    control(24, 86, 184, friend_unread?"Friends ("+std::to_string(friend_unread)+")":"Friends", 2122);
    control(228, 86, 184, party.party_unread?"Party ("+std::to_string(party.party_unread)+")":"Party", 2123);
    control(432, 86, 184, "Invites (" + std::to_string(party.invites.size()) + ")", 2124);
    if (party.account.empty()) {
        centered(210, "Sign in with a cloud account to play together.", 21);
        return;
    }

    if(party_tab==5&&party.id==party_match_room){
        centered(154,party_match_result,24);
        const auto count=std::count(party_match_departed.begin(),party_match_departed.end(),true);
        small_line(96,216,std::to_string(count)+(count==1?" disconnect recorded":" disconnects recorded"),450);
        if(party_owner()){
            control(48,285,260,"Play Again",2136,40);
            control(329,285,263,"Choose Another Map",2137,40);
        }else small_line(96,278,"Your party leader can choose the next lobby.",450);
        if(!party_notice.empty())small_line(48,388,party_notice,544);
        return;
    }
    const auto self=std::find_if(party.members.begin(),party.members.end(),[](const PartyPlayer &p){return p.id==party.account;});
    const bool can_join=!party.prompt.empty()&&!network_is_active()&&!party_join_connecting;
    const bool can_retry=can_join&&self!=party.members.end()&&!self->join.empty()&&self->join!="Invited";
    const bool showing_friends=party_tab==0||party_tab==4;
    const bool showing_invites=party_tab==2;
    const auto &rows=showing_friends?party.friends:showing_invites?party.invites:party.members;
    frame(24,128,229,260);
    frame(263,128,353,260);

    const int last_page=rows.empty()?0:static_cast<int>((rows.size()-1)/4);
    party_offset=std::clamp(party_offset,0,last_page);
    for(int i=0;i<4&&party_offset*4+i<static_cast<int>(rows.size());++i){
        const auto &player=rows[party_offset*4+i];
        const int y=141+i*50;
        const bool current=showing_friends&&party_tab==4&&player.id==direct_friend;
        const std::string name=short_text(player.name,24);
        if(showing_friends){
            party_targets[2160+i]=player;
            control(34,y,207,(current?"> "+name:name)+(player.unread?" ("+std::to_string(player.unread)+")":""),2160+i,27);
            const auto tint=player.online?std::array<Uint8,3>{125,185,100}:std::array<Uint8,3>{117,111,105};
            rect(42,y+33,5,5,tint[0],tint[1],tint[2]);
            small_line(53,y+28,party_status_label(player),180);
        }else{
            small_line(38,y+6,name+(player.id==party.owner?" / Owner":""),195);
            if(!showing_invites&&!player.readiness.empty()){
                const bool retry_row=party_tab==1&&can_retry&&player.id==party.account;
                small_line(38,y+27,player.join.empty()?player.readiness:player.join,retry_row?125:202);
                if(retry_row)control(166,y+21,76,"Retry",2128,25);
            }
            if(showing_invites){
                party_targets[2140+i]=player;
                party_targets[2150+i]=player;
                control(279,y,147,"Accept "+short_text(name,10),2140+i);
                control(441,y,155,"Decline",2150+i);
            }else if(party_tab==3&&party_owner()&&player.id!=party.owner){
                party_targets[2140+i]=player;
                control(279,y,236,"Remove "+short_text(name,16),2140+i);
            }
        }
    }
    if(rows.empty())small_line(38,150,showing_friends?"No accepted friends":showing_invites?"No pending invites":"No party members",197);
    if(party_offset>0)control(34,345,98,"Previous",2129);
    if(party_offset<last_page)control(144,345,98,"Next",2130);

    if(party_tab==4){
        small_line(276,145,"Chat with "+direct_name,226);
        control(513,137,91,"Older",2132);
        draw_party_chat(party.direct_chat,177,5);
        control(275,303,243,party_message.empty()?"Write a message":short_text(party_message,25),2125);
        control(530,303,74,"Send",2126);
        control(513,348,91,"Newer",2133);
        const auto friend_row=std::find_if(rows.begin(),rows.end(),[](const PartyPlayer &player){return player.id==direct_friend;});
        if(friend_row!=rows.end()&&friend_row->online&&(party.id.empty()||party_owner())){
            party_targets[2140]=*friend_row;
            control(275,348,221,"Invite to Party",2140);
        }
    }else if(party_tab==0){
        small_line(276,150,"Choose a friend to open chat.",324);
        if(!party.friends.empty())small_line(276,181,"Online friends can be invited to a party.",324);
    }else if(party_tab==1){
        if(party.id.empty())small_line(276,150,"Invite a friend to start a party.",324);
        else{
            small_line(276,145,party.target.name.empty()
                ? (party_owner()?"Party chat / You lead":"Party chat") : party.target.name,226);
            control(513,137,91,"Older",2132);
            draw_party_chat(party.chat,175,4);
            control(275,276,243,party_message.empty()?"Write a party message":short_text(party_message,25),2125);
            control(530,276,74,"Send",2126);
            if(party_notice.rfind("Cloud session expired",0)==0)
                control(275,310,329,"Sign In Again",2113);
            else if(party_notice.rfind("Lobby is full",0)==0&&party_owner())
                control(275,310,329,"Choose Another Server",2137);
            else if(can_join&&!can_retry)
                control(275,310,329,"Join "+short_text(party.server_name,25),2128);
            else if(party.prompt.empty()&&!party.target_revision.empty()){
                if(party_owner()&&party_all_ready())control(275,310,329,"Send Join Prompt",2135);
                else small_line(276,320,party.target_blocker.empty()
                    ? "Checking for "+party.target.name : party.target_blocker,320);
            }
            control(275,348,124,party_owner()?"Members":"Roster",2131);
            control(405,348,99,party_owner()?"Disband":"Leave",2127);
            control(513,348,91,"Newer",2133);
        }
    }else if(showing_invites&&rows.empty()){
        small_line(276,150,"Party invitations will appear here.",324);
    }
    if(!party_notice.empty())small_line(24,397,party_notice,310);
    control(367,394,249,mute_social_in_match?"Match notifications: Muted":"Mute during matches",2134,27);
}
