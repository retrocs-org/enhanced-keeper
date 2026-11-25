// Account-scoped friends, party chat and server prompts.
struct PartyPlayer {
    std::string id, name;
    bool online = false;
};
struct PartyView {
    std::string account, id, owner, server, server_name, prompt;
    std::vector<PartyPlayer> friends, members, invites;
    std::vector<std::string> chat, direct_chat;
};
PartyView party;
std::string party_notice, party_message, party_join_server, shown_party_prompt, direct_friend, direct_name;
std::map<int, PartyPlayer> party_targets;
int party_tab = 0, party_offset = 0, party_chat_offset = 0;
Uint64 party_poll_at = 0;

bool party_owner()
{
    return !party.account.empty() && party.owner == party.account;
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
        result.push_back({hub_field(row, "id"), hub_field(row, "name"),
            online && value_type(online) == VALUE_BOOL && value_bool(online)});
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
    party.id = hub_field(&root, "party");
    party.owner = hub_field(&root, "owner");
    party.friends = party_players(root, "friends");
    party.members = party_players(root, "members");
    party.invites = party_players(root, "invites");
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
    if (!poll) {
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
        party_poll_at = 0;
        party_offset = 0;
    }
    if (!account.empty() && !kfx_hub_busy() && SDL_GetTicks() >= party_poll_at)
        party_request("state",party_tab==4?"user":nullptr,direct_friend);
}

bool party_action(int action)
{
    if (action == 2120) {
        party_message.clear();
        party_tab = 0;
        party_offset = 0;
        party_poll_at = 0;
        open_panel(38);
        return true;
    }
    if (action == 2121 && page == 21 && party_owner() && !browser_servers.empty()) {
        const auto &server = browser_servers[browser_index];
        if (!server.id.empty()) party_request("prompt", "server", server.id);
        return true;
    }
    if (page != 38) return false;
    if (action >= 2122 && action <= 2124) {
        party_message.clear();
        party_tab = action - 2122;
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
        else if (!party.server.empty()) {
            if (kfx_hub_request("servers", "/api/servers", nullptr, 0)) {
                party_join_server = party.server;
                party_notice = "Finding the party server...";
            } else party_notice = "Please try again in a moment.";
        }
    } else if (action == 2129) {
        party_offset = std::max(0, party_offset - 1);
    } else if (action == 2130) {
        ++party_offset;
    } else if (action >= 2160 && action < 2164 && party_tab == 0) {
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
        if (party_tab == 0) party_request("invite", "user", target->second.id);
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
            size_t length = std::min<size_t>(58,message.size()-offset);
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
        small_line(24,y+i*25,lines[first+i],592);
}

void party_draw()
{
    if (page != 38) return;
    party_targets.clear();
    auto control = [](int x, int y, int width, const std::string &label, int action) {
        button(x, y, width, label, action, static_cast<int>(hits.size()) == selected, 34);
    };
    control(24, 86, 184, "Friends", 2122);
    control(228, 86, 184, "Party", 2123);
    control(432, 86, 184, "Invites (" + std::to_string(party.invites.size()) + ")", 2124);
    if (party.account.empty()) {
        centered(210, "Sign in with a cloud account to play together.", 21);
        return;
    }

    if (party_tab == 4) {
        small_line(24, 136, "Conversation with " + direct_name, 350);
        control(420, 127, 90, "Older", 2132);
        control(525, 127, 90, "Newer", 2133);
        draw_party_chat(party.direct_chat, 171, 5);
        control(24, 329, 455, party_message.empty() ? "Write a message" : short_text(party_message, 40), 2125);
        control(494, 329, 122, "Send", 2126);
    } else if (party_tab == 1) {
        if (party.id.empty()) centered(190, "Invite a friend to start a party.", 22);
        else {
            std::string roster = party_owner() ? "You lead the party" : "Party members";
            roster += "  /  " + std::to_string(party.members.size()) + "/4";
            small_line(24, 133, roster, 350);
            control(420, 125, 90, "Older", 2132);
            control(525, 125, 90, "Newer", 2133);
            draw_party_chat(party.chat, 166, 4);
            if (party.chat.empty()) small_line(24, 167, "Party chat is ready.", 592);
            control(24, 285, 455, party_message.empty() ? "Write a party message" : short_text(party_message, 40), 2125);
            control(494, 285, 122, "Send", 2126);
            control(24, 334, 185, "Members", 2131);
            control(224, 334, 185, party_owner() ? "Disband Party" : "Leave Party", 2127);
            if (!party.prompt.empty())
                control(24, 369, 592, "Join " + short_text(party.server_name, 35), 2128);
        }
    } else {
        const auto &rows = party_tab == 0 ? party.friends : party_tab == 2 ? party.invites : party.members;
        const int last_page = rows.empty() ? 0 : static_cast<int>((rows.size() - 1) / 4);
        party_offset = std::clamp(party_offset, 0, last_page);
        for (int i = 0; i < 4 && party_offset * 4 + i < static_cast<int>(rows.size()); ++i) {
            const auto &player = rows[party_offset * 4 + i];
            const int y = 137 + i * 45;
            const auto label = player.name + (party_tab == 0 ? (player.online ? "  /  Online" : "  /  Offline") :
                party_tab == 3 && player.id == party.owner ? "  /  Owner" : "");
            small_line(24, y + 7, label, 320);
            party_targets[2140+i] = player;
            if (party_tab == 2) {
                party_targets[2150+i] = player;
                control(350, y, 126, "Accept", 2140 + i);
                control(488, y, 128, "Decline", 2150 + i);
            } else if (party_tab == 0) {
                party_targets[2160+i] = player;
                control(350, y, 126, "Chat", 2160 + i);
                if ((party.id.empty() || party_owner()) && player.online)
                    control(488, y, 128, "Invite", 2140 + i);
            } else if (party_tab == 3 && party_owner() && player.id != party.owner) {
                control(444, y, 172, "Remove", 2140 + i);
            }
        }
        if (rows.empty()) centered(200, party_tab == 0 ? "Add friends from your web account." :
            party_tab == 2 ? "No pending invitations." : "No party members.", 21);
        if (party_offset > 0) control(24, 338, 152, "Previous", 2129);
        if (party_offset < last_page) control(464, 338, 152, "Next", 2130);
    }
    if (!party_notice.empty())
        small_line(24, party_tab == 1 && !party.prompt.empty() ? 405 : 397, party_notice, 592);
}
