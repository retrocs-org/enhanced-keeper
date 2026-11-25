// Calibration samples resting sticks only; a moving/disconnected pad is rejected.
Uint64 calibration_until=0;
float calibration_peak=0;
unsigned int calibration_owner=0;
// Save management, controller selection, accessibility presets and the quick wheel.
std::string save_base(const Save &entry){char name[40];snprintf(name,sizeof(name),"fx1g%04ld.sav",entry.slot);return path(name);}
std::string read_save_alias(const std::string &base){size_t length=0;char *bytes=(char*)SDL_LoadFile((base+".label").c_str(),&length);if(!bytes)return {};std::string result(bytes,std::min(length,(size_t)128));SDL_free(bytes);while(!result.empty()&&(result.back()=='\n'||result.back()=='\r'))result.pop_back();return result;}
bool write_small_file(const std::string &name,const std::string &value){std::string pending=name+".pending";FILE *file=fopen(pending.c_str(),"w");if(!file)return false;bool ok=fwrite(value.data(),1,value.size(),file)==value.size();ok=fflush(file)==0&&fsync(fileno(file))==0&&ok;if(fclose(file)!=0)ok=false;if(ok&&rename(pending.c_str(),name.c_str())==0)return true;unlink(pending.c_str());return false;}
void refresh_save(const std::string &base){scan_saves();for(int i=0;i<(int)saves.size();i++)if(save_base(saves[i])==base){save_index=i;break;}backup_choice=0;}
bool trash_save_group(const std::string &base){
    if(access((base+".protected").c_str(),F_OK)==0){notice="Unprotect this favourite before deleting it.";return false;}
    std::string folder=path("Trash");if(!SDL_CreateDirectory(folder.c_str())){notice="Could not create the recoverable Trash folder.";return false;}
    std::string stem=base.substr(base.find_last_of('/')+1)+"-"+std::to_string(SDL_GetPerformanceCounter());std::vector<std::pair<std::string,std::string>> moved;
    std::vector<std::string> sources={base,base+".label",base+".protected",base+".png",base+".json",base+".requirements"};for(int n=1;n<=3;n++)for(const char *suffix:{"",".png",".json",".requirements"})sources.push_back(base+".backup"+std::to_string(n)+suffix);
    for(const std::string &source:sources){if(access(source.c_str(),F_OK)!=0)continue;std::string dest=folder+"/"+stem+source.substr(base.size());if(access(dest.c_str(),F_OK)==0||rename(source.c_str(),dest.c_str())!=0){bool restored=true;for(auto i=moved.rbegin();i!=moved.rend();++i)if(rename(i->second.c_str(),i->first.c_str())!=0)restored=false;notice=restored?"Move failed; the save group was restored.":"Some files remain in Trash. Check the save folder.";return false;}moved.push_back({source,dest});}
    notice="Save and backups moved to the save folder's Trash.";return !moved.empty();
}
void apply_access_preset(int preset){
    if(preset==0){kfx_text_scale=1;kfx_cursor_scale=1;kfx_edge_scroll=1;kfx_mentor_captions=0;kfx_caption_seconds=5;kfx_camera_pan_scale=1;}
    if(preset==1){kfx_text_scale=1.5;kfx_cursor_scale=1.75;kfx_edge_scroll=1;kfx_mentor_captions=0;kfx_caption_seconds=8;kfx_camera_pan_scale=1;}
    if(preset==2){kfx_text_scale=1.25;kfx_cursor_scale=1.5;kfx_edge_scroll=0;kfx_mentor_captions=1;kfx_caption_seconds=10;kfx_camera_pan_scale=1;}
    if(preset==3){kfx_text_scale=1.25;kfx_cursor_scale=1.5;kfx_edge_scroll=0;kfx_mentor_captions=1;kfx_caption_seconds=10;kfx_camera_pan_scale=.5;}
    persist();notice=preset==3?"Slower panning and edge scrolling off.":"Preset applied. Individual settings remain adjustable.";
}
void feature_init(){size_t length=0;char *data=(char*)SDL_LoadFile(path("controller-guid.cfg").c_str(),&length);if(data){std::string guid(data,length);SDL_free(data);while(!guid.empty()&&(guid.back()=='\n'||guid.back()=='\r'))guid.pop_back();controller_prefer_guid(guid.c_str());}}
bool feature_action(int action){
    if(action==896){page=26;selected=0;focus_action=-1;notice="Move sticks and press buttons to test. Rest both sticks before calibrating.";return true;}
    if(action==897){if(!controller_connected()){notice="Connect a controller first.";return true;}calibration_owner=controller_active_id();calibration_peak=0;calibration_until=SDL_GetTicks()+2000;notice="Leave both sticks untouched for two seconds.";return true;}
    if(action==898){env_number("KFX_CONTROLLER_DEADZONE",8000.0/32767);persist();notice="Default stick dead zone restored.";return true;}
    if(action==840){page=15;selected=0;device_offset=0;focus_action=-1;return true;}
    if(action==841){page=16;selected=0;focus_action=-1;return true;}
    if(action==842&&!saves.empty()){page=17;selected=0;focus_action=-1;return true;}
    if(action>=860&&action<863){int index=device_offset+action-860;unsigned int id=controller_device_id(index);if(!controller_select(id)){notice="That controller disconnected. Choose an available device.";return true;}repeat_button=255;capturing=false;clear_controller_input();char guid[33];controller_guid_for_id(id,guid,sizeof(guid));controller_prefer_guid(guid);notice=write_small_file(path("controller-guid.cfg"),guid)?"Active controller selected and remembered.":"Controller selected; preference could not be saved.";return true;}
    if(action==863||action==864){int count=controller_device_count();device_offset=std::clamp(device_offset+(action==863?-3:3),0,std::max(0,((count-1)/3)*3));return true;}
    if(action==895){label_model=(label_model+2)%3-1;persist();return true;}
    if(action==894){quick_wheel_enabled=!quick_wheel_enabled;persist();return true;}
    if(action>=880&&action<=883){apply_access_preset(action-880);return true;}
    if(action==890||action==891){kfx_caption_seconds=std::clamp(kfx_caption_seconds+(action==890?-1:1),3,12);persist();return true;}
    if(action==892||action==893){kfx_camera_pan_scale=std::clamp(kfx_camera_pan_scale+(action==892?-.1f:.1f),.25f,1.0f);persist();return true;}
    if(action==1000&&!saves.empty()){save_alias=selected_save().title;start_edit(5);notice="Rename this save. Enter saves the display name.";return true;}
    if(action==1001&&!saves.empty()){std::string base=save_base(selected_save()),flag=base+".protected";bool on=access(flag.c_str(),F_OK)==0;bool ok=on?unlink(flag.c_str())==0:write_small_file(flag,"Protected favourite\n");refresh_save(base);notice=ok?(on?"Favourite unprotected.":"Favourite protected from overwrite and deletion."):"Could not update protection.";return true;}
    if(action==1003&&!saves.empty()){page=12;backup_step(1);preview_large=true;return true;}
    if(action==1004&&!saves.empty()){if(access((save_base(selected_save())+".protected").c_str(),F_OK)==0){notice="Unprotect this favourite before deleting it.";return true;}page=18;selected=0;focus_action=-1;return true;}
    if(action==1005&&page==18&&!saves.empty()){unsigned int slot=selected_save().slot;if(trash_save_group(save_base(selected_save()))){save_catalogue_slot_disable(slot);scan_saves();page=2;selected=0;focus_action=-1;}return true;}
    if(action==1006){page=17;selected=0;focus_action=-1;return true;}
    if(action==844){page=19;selected=0;focus_action=-1;return true;}
    if(action==1100){kfx_readiness_check();return true;}
    if(action==1101){kfx_readiness_override();return true;}
    if(action==1102){notice=SDL_SetClipboardText(kfx_readiness_report())?"Readiness report copied.":"Clipboard unavailable.";return true;}
    return false;
}
void feature_draw(){
    if(page==26){
        centered(90,controller_device_name(),22);
        const char *names[]={"Left X","Left Y","Right X","Right Y","Left trigger","Right trigger"};
        for(int i=0;i<6;i++){int y=128+i*30;char value[64];snprintf(value,sizeof(value),"%+.2f",controller_raw_axis(i));text(42,y,names[i],20,260);text(330,y,value,20,240);}
        text(42,315,"Buttons: "+(controller_raw_buttons()?binding_name(controller_raw_buttons()):std::string("None")),18,560);
        button(24,350,284,calibration_until?"Measuring...":"Calibrate sticks",897,selected==0,40);
        button(332,350,284,"Restore dead zone",898,selected==1,40);
    }else if(page==15){
        int count=controller_device_count();device_offset=std::min(device_offset,std::max(0,((count-1)/3)*3));
        if(!count)centered(160,"Connect a controller by USB or Bluetooth.",22);
        for(int i=0;i<3&&device_offset+i<count;i++){unsigned int id=controller_device_id(device_offset+i);button(24,98+i*52,592,std::string(id==controller_active_id()?"Using: ":"Select: ")+controller_name_for_id(id),860+i,selected==i,42);}
        button(24,258,160,"Previous",863,selected==3,40);button(456,258,160,"Next",864,selected==4,40);centered(268,std::to_string(count)+" connected",20);
        button(24,312,284,label_model<0?"Labels: Auto":label_model?"Labels: DualSense":"Labels: Xbox",895,selected==5,40);button(332,312,284,quick_wheel_enabled?"R3 wheel: On":"R3 wheel: Off",894,selected==6,40);
        button(24,366,592,"Test and calibrate controller",896,selected==7,40);
    }else if(page==16){
        const char *names[]={"Standard","Large Text and Cursor","Captions and Comfort","Reduced Camera Movement"};for(int i=0;i<4;i++)button(50,94+i*44,540,names[i],880+i,selected==i,36);
        const char *motion[]={"Title: Animated","Title: Still Artwork","Title: Skip"};
        button(50,275,540,motion[title_motion_mode],899,selected==4,38);
        text(24,337,"Caption duration",20,280);text(330,337,std::to_string(kfx_caption_seconds)+" sec",20,170);button(524,330,40,"-",890,false,36);button(572,330,40,"+",891,selected==5,36);
        text(24,387,"Camera pan speed",20,280);text(330,387,std::to_string((int)round(kfx_camera_pan_scale*100))+"%",20,170);button(524,380,40,"-",892,false,36);button(572,380,40,"+",893,selected==6,36);
    }else if(page==17&&!saves.empty()){
        Save entry=selected_save();centered(88,short_text(entry.title,38),24);std::string base=save_base(entry);bool protected_save=access((base+".protected").c_str(),F_OK)==0;
        button(64,136,512,editing==5?save_alias+"|":"Rename Save",1000,selected==0||editing==5,44);button(64,202,512,protected_save?"Favourite: Protected":"Protect as Favourite",1001,selected==1,44);button(64,268,512,"Preview Backups",1003,selected==2,44);button(64,334,512,protected_save?"Unprotect before deleting":"Move Save to Trash",1004,selected==3,44);
    }else if(page==18&&!saves.empty()){
        frame(24,98,592,228);centered(120,"Move this save and its backups",24);centered(158,"to the recoverable Trash folder?",24);centered(224,short_text(selected_save().title,38),24);
        button(24,348,284,"Move to Trash",1005,selected==0,44);button(332,348,284,"Cancel",1006,selected==1,44);
    }else if(page==19){
        frame(24,90,592,230);std::istringstream input(kfx_readiness_report());std::string line;int y=102;while(std::getline(input,line)&&y<300){text(42,y,line,20,556);y+=32;}
        button(24,336,284,"Check Lobby",1100,selected==0,44);button(332,336,284,"Copy Report",1102,selected==1,44);button(134,386,372,"Start Unchecked",1101,selected==2,32);
    }
}
bool quick_context(){return frontend_menu_state==FeSt_INITIAL&&game.game_kind!=GKind_Unset&&!game.packet_load_enable&&!(game.operation_flags&GOF_Paused)&&get_my_player()->view_type==PVT_DungeonTop;}
void wheel_close(bool accept){int choice=wheel_choice;wheel_open=false;kfx_controller_guide_visible=0;clear_controller_input();repeat_button=255;controller_suppress_until_neutral();if(SDL_GetKeyboardFocus())LbGrabMouseCheck(MG_OnFocusGained);else SDL_ShowCursor();if(accept&&quick_context()&&choice>=0&&choice<7)quick_pending=choice;wheel_choice=-1;}
int wheel_sector(float x,float y){if(x*x+y*y<.16f)return -1;double angle=atan2(y,x)+M_PI/2;if(angle<0)angle+=2*M_PI;return ((int)floor(angle/(M_PI/4)+.5))%8;}
void feature_tick(){
    if(calibration_until){
        if(page!=26||controller_active_id()!=calibration_owner){calibration_until=0;notice="Calibration cancelled: the active controller changed.";}
        else{for(int i=0;i<4;i++)calibration_peak=std::max(calibration_peak,std::abs(controller_raw_axis(i)));
            if(SDL_GetTicks()>=calibration_until){calibration_until=0;if(calibration_peak>.4f)notice="Stick movement detected. Rest the sticks and try again.";
                else{env_number("KFX_CONTROLLER_DEADZONE",std::clamp(calibration_peak+.03f,.05f,.5f));persist();notice="Resting drift measured. Dead zone saved; test the sticks below.";}}}
    }

    kfx_readiness_tick();
    if(wheel_open&&(!quick_context()||controller_active_id()!=wheel_owner))wheel_close(false);
    if(quick_pending<0)return;int action=quick_pending;quick_pending=-1;if(!quick_context())return;
    const int menus[]={GMnu_ROOM,GMnu_SPELL,0,0,0,GMnu_CREATURE,GMnu_TRAP};if(menus[action])set_menu_mode(menus[action]);else{quick_key=action==2?Gkey_SwitchToMap:action==3?Gkey_ZoomRoomHeart:Gkey_ZoomToFight;quick_key_until=SDL_GetTicks()+300;}
}
bool feature_event(const SDL_Event *event){
    if(page==26&&kfx_controller_guide_visible&&(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN||event->type==SDL_EVENT_GAMEPAD_BUTTON_UP)){
        if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&controller_owns_event(event->gbutton.which)){
            if(event->gbutton.button==SDL_GAMEPAD_BUTTON_START)activate(901);
            else if(event->gbutton.button==SDL_GAMEPAD_BUTTON_NORTH)feature_action(897);
            else if(event->gbutton.button==SDL_GAMEPAD_BUTTON_WEST)feature_action(898);
        }
        return true;
    }

    if(wheel_open){
        if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST||(event->type==SDL_EVENT_GAMEPAD_REMOVED&&event->gdevice.which==wheel_owner)){wheel_close(false);return false;}
        if(event->type==SDL_EVENT_KEY_DOWN){if(event->key.key==SDLK_ESCAPE)wheel_close(false);else if(event->key.key==SDLK_F9){wheel_close(false);return false;}return !(event->key.key==SDLK_Q&&(event->key.mod&SDL_KMOD_GUI));}
        if((event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN||event->type==SDL_EVENT_GAMEPAD_BUTTON_UP)&&controller_owns_event(event->gbutton.which)){
            if(event->type==SDL_EVENT_GAMEPAD_BUTTON_UP&&event->gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_STICK){wheel_close(true);return true;}
            if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN){if(pad_button(event->gbutton.button)==kfx_menu_back_button())wheel_close(false);else if(event->gbutton.button==SDL_GAMEPAD_BUTTON_DPAD_LEFT)wheel_choice=(wheel_choice+7)%8;else if(event->gbutton.button==SDL_GAMEPAD_BUTTON_DPAD_RIGHT)wheel_choice=(wheel_choice+1)%8;}return true;
        }
        if(event->type==SDL_EVENT_GAMEPAD_AXIS_MOTION&&controller_owns_event(event->gaxis.which)){if(event->gaxis.axis==SDL_GAMEPAD_AXIS_LEFTX)wheel_x=event->gaxis.value/32767.0f;if(event->gaxis.axis==SDL_GAMEPAD_AXIS_LEFTY)wheel_y=event->gaxis.value/32767.0f;int choice=wheel_sector(wheel_x,wheel_y);if(choice>=0)wheel_choice=choice;return true;}
        if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN)return true;
        return event->type>=SDL_EVENT_GAMEPAD_AXIS_MOTION&&event->type<=SDL_EVENT_GAMEPAD_BUTTON_UP;
    }
    if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&event->gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_STICK&&controller_owns_event(event->gbutton.which)&&quick_wheel_enabled&&!kfx_controller_guide_visible&&quick_context()){
        for(int i=0;i<GAME_KEYS_COUNT;i++)if(get_game_key_controller_buttons(i)&CBtn_RIGHTSTICK)return false;
        for(int key=0;key<KC_LIST_END;key++)if(lbKeyOn[key])keyboardControl(KActn_KEYUP,key,0,0);
        wheel_open=true;wheel_choice=-1;wheel_x=wheel_y=0;wheel_owner=controller_active_id();kfx_controller_guide_visible=1;clear_controller_input();LbSetMouseGrab(false);return true;
    }
    return false;
}
void wheel_draw(){
    if(!canvas)canvas=SDL_CreateSurface(WIDTH,HEIGHT,SDL_PIXELFORMAT_RGBA32);if(!canvas)return;
    SDL_FillSurfaceRect(canvas,nullptr,SDL_MapSurfaceRGBA(canvas,0,0,0,170));hits.clear();
    const char *names[]={"Rooms","Spells","Map","Heart","Battle","Units","Traps","Cancel"};const int positions[][2]={{245,62},{410,105},{440,222},{410,335},{245,378},{80,335},{50,222},{80,105}};
    for(int i=0;i<8;i++)button(positions[i][0],positions[i][1],150,names[i],1200+i,wheel_choice==i,40);
    frame(210,185,220,106);text(226,196,"Quick Menu",22,188);text(226,236,"Left stick",18,188);text(226,265,"Release R3",18,188);
    int w=RendererScreenWidth(),h=RendererScreenHeight();double scale=std::min(w/(double)WIDTH,h/(double)HEIGHT);RendererPresentImageDesc desc={};desc.dst_w=WIDTH*scale;desc.dst_h=HEIGHT*scale;desc.dst_x=(w-desc.dst_w)/2;desc.dst_y=(h-desc.dst_h)/2;desc.src=(const unsigned char*)canvas->pixels;desc.src_pitch=canvas->pitch;desc.src_w=WIDTH;desc.src_h=HEIGHT;desc.format=PRESENT_FORMAT_RGBA8;desc.kind=PRESENT_KIND_TRANSPARENT;desc.filter=PRESENT_FILTER_NEAREST;desc.exclusive=0;RendererPresentImage(&desc);
}
