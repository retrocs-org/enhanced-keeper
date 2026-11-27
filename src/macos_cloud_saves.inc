// Cloud backups are explicit and restore to a free local slot.
struct CloudSaveInfo { std::string id,title,device,date; };
std::vector<CloudSaveInfo> cloud_saves;
std::string cloud_save_notice;
int cloud_save_offset=0;
bool cloud_save_pending=false;
std::string cloud_upload_path, cloud_upload_revision;
std::string selected_cloud_backup,cloud_restore_manifest,cloud_restore_account;
std::vector<unsigned char> cloud_restore_data;
int cloud_restore_slot=-1,cloud_restored_slot=-1;
bool cloud_restore_verified=false;
std::string cloud_restore_report;

bool cloud_save_request(const char *operation,const std::string &route,const std::string &body={})
{
    const char *profile=getenv("KFX_PLAYER_PROFILE_DIR");
    if(cloud_save_pending||!kfx_hub_has_session()||!profile||
       profiles_base()+"/CloudProfiles/"+kfx_hub_account_id()!=profile)return false;
    const std::string tag=std::string("cloud-save:")+operation+":"+kfx_hub_account_id();
    cloud_save_pending=kfx_hub_request(tag.c_str(),route.c_str(),body.empty()?nullptr:body.c_str(),1);
    return cloud_save_pending;
}

static constexpr char save_base64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string encode_cloud_save(const unsigned char *bytes,size_t size)
{
    std::string encoded;
    encoded.reserve(4*((size+2)/3));
    for(size_t offset=0;offset<size;offset+=3){
        const unsigned a=bytes[offset], b=offset+1<size?bytes[offset+1]:0, c=offset+2<size?bytes[offset+2]:0;
        encoded+=save_base64[a>>2];
        encoded+=save_base64[((a&3)<<4)|(b>>4)];
        encoded+=offset+1<size?save_base64[((b&15)<<2)|(c>>6)]:'=';
        encoded+=offset+2<size?save_base64[c&63]:'=';
    }
    return encoded;
}

std::vector<unsigned char> decode_cloud_save(const std::string &encoded)
{
    if(encoded.empty()||encoded.size()>2796204||encoded.size()%4)return {};
    std::vector<unsigned char> result;
    result.reserve(encoded.size()/4*3);
    unsigned bits=0;
    int count=0;
    for(size_t index=0;index<encoded.size();++index){
        const char digit=encoded[index];
        if(digit=='='){
            if(index+2<encoded.size()||encoded.find_first_not_of('=',index)!=std::string::npos)return {};
            break;
        }
        const char *value=std::strchr(save_base64,digit);
        if(!value||!digit)return {};
        bits=(bits<<6)|static_cast<unsigned>(value-save_base64);
        count+=6;
        if(count>=8){count-=8;result.push_back(static_cast<unsigned char>(bits>>count));}
    }
    return result;
}

std::string cloud_slot_path(int slot)
{
    char filename[32];
    snprintf(filename,sizeof(filename),"fx1g%04d.sav",slot);
    return path(filename);
}

int find_cloud_slot(int slot,int direction)
{
    for(;slot>=0&&slot<SAVE_SLOTS_LIMIT;slot+=direction){
        const auto base=cloud_slot_path(slot);
        bool occupied=false;
        for(const char *suffix:{"",".requirements",".json",".png",".label",".protected",".backup1",".backup2",".backup3"}){
            struct stat info;
            if(lstat((base+suffix).c_str(),&info)==0){occupied=true;break;}
        }
        if(!occupied)return slot;
    }
    return -1;
}

void review_cloud_save(const std::string &encoded,const std::string &requirements)
{
    cloud_restore_data=decode_cloud_save(encoded);
    cloud_restore_manifest.clear();
    cloud_restore_verified=false;
    cloud_restore_report.clear();
    if(!requirements.empty()){
        const auto decoded=decode_cloud_save(requirements);
        cloud_restore_manifest.assign(decoded.begin(),decoded.end());
        if(cloud_restore_manifest.rfind("KFXREQ2\n",0)!=0){
            cloud_save_notice="The backup has damaged requirements.";
            return;
        }
    }
    if(cloud_restore_data.size()<sizeof(FileChunkHeader)+sizeof(CatalogueEntry)){
        cloud_save_notice="This backup is not a supported saved game.";
        return;
    }
    const std::string temporary=path("cloud-review.XXXXXX");
    std::vector<char> folder(temporary.begin(),temporary.end());
    folder.push_back(0);
    if(!mkdtemp(folder.data())){
        cloud_save_notice="Could not prepare the compatibility check.";
        return;
    }
    const std::string source=std::string(folder.data())+"/backup.sav";
    const auto *bytes=reinterpret_cast<const char *>(cloud_restore_data.data());
    const bool staged=write_small_file(source,std::string(bytes,cloud_restore_data.size()))&&
        (cloud_restore_manifest.empty()||write_small_file(source+".requirements",cloud_restore_manifest));
    char report[1024]={};
    const int compatible=staged?kfx_save_check_requirements(source.c_str(),report,sizeof(report)):-1;
    unlink((source+".requirements").c_str());
    unlink(source.c_str());
    rmdir(folder.data());
    cloud_restore_verified=compatible>=0;
    cloud_restore_account=kfx_hub_account_id();
    cloud_restore_report=staged?report:"Could not check this backup.";
    if(compatible==0)cloud_restore_report="Engine matches. This older backup has no recorded content requirements.";
    cloud_restore_slot=find_cloud_slot(0,1);
    cloud_save_notice=cloud_restore_slot<0?"All local save slots are full.":"";
}

void restore_cloud_save()
{
    if(!cloud_restore_verified||cloud_restore_slot<0||kfx_hub_busy()||
        cloud_restore_account!=kfx_hub_account_id())return;
    if(network_is_active()||game.game_kind!=GKind_Unset){
        cloud_save_notice="Restore from Saved Games at the main menu.";
        return;
    }
    if(find_cloud_slot(cloud_restore_slot,1)!=cloud_restore_slot){
        cloud_save_notice="That slot is in use. Choose another free slot.";
        return;
    }
    const auto destination=cloud_slot_path(cloud_restore_slot);
    const int fd=open(destination.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0){cloud_save_notice="Could not open the selected save slot.";return;}
    FILE *file=fdopen(fd,"wb");
    bool ok=false;
    if(file){
        ok=fwrite(cloud_restore_data.data(),1,cloud_restore_data.size(),file)==cloud_restore_data.size();
        ok=fflush(file)==0&&fsync(fd)==0&&ok;
        ok=fclose(file)==0&&ok;
    }else close(fd);
    if(ok&&!cloud_restore_manifest.empty())
        ok=write_small_file(destination+".requirements",cloud_restore_manifest);
    if(!ok){
        unlink(destination.c_str());
        cloud_save_notice="Could not write the local save.";
        return;
    }
    load_game_save_catalogue();
    scan_saves();
    cloud_restored_slot=cloud_restore_slot;
    cloud_restore_verified=false;
    cloud_restore_data.clear();
    selected_cloud_backup.clear();
    cloud_save_notice="Backup restored.";
}

void cloud_save_reply(const std::string &tag,const VALUE &root,const std::string &error)
{
    cloud_save_pending=false;
    const auto suffix=tag.rfind(':');
    if(suffix==std::string::npos||tag.substr(suffix+1)!=kfx_hub_account_id())return;
    if(tag.rfind("cloud-save:upload:",0)==0){
        const auto source=std::exchange(cloud_upload_path,{});
        const auto revision=std::exchange(cloud_upload_revision,{});
        if(kfx_hub_result_status()==200&&!source.empty()&&!hub_field(&root,"id").empty()){
            struct stat current{};
            if(stat(source.c_str(),&current)==0&&save_revision(current)==revision&&
               write_small_file(source+".cloud-backup",revision+"\n")&&page==39)scan_saves();
        }
    }
    if(kfx_hub_result_status()!=200){cloud_save_notice=error.empty()?"Cloud backup unavailable.":error;return;}
    if(tag.rfind("cloud-save:upload:",0)==0)cloud_save_notice="Manual save backed up.";
    else if(tag.rfind("cloud-save:review:",0)==0){
        if(page==39&&!selected_cloud_backup.empty())
            review_cloud_save(hub_field(&root,"data"),hub_field(&root,"requirements"));
    }
    else{
        cloud_saves.clear();
        const VALUE *rows=value_dict_get(&root,"saves");
        if(rows&&value_type(rows)==VALUE_ARRAY)for(size_t i=0;i<std::min<size_t>(8,value_array_size(rows));++i){
            const VALUE *row=value_array_get(rows,i);
            const VALUE *created=value_dict_get(row,"created");
            const time_t when=created?static_cast<time_t>(value_double(created)):0;
            struct tm local{};
            char date[40]={};
            if(localtime_r(&when,&local))strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&local);
            cloud_saves.push_back({hub_field(row,"id"),hub_field(row,"title"),hub_field(row,"device"),date});
        }
        cloud_save_offset=0;
        cloud_save_notice=cloud_saves.empty()?"No cloud backups yet.":"Choose a backup to restore into a free slot.";
    }
}

bool cloud_save_action(int action)
{
    if(page==39&&action==2185){
        restore_cloud_save();
    }else if(page==39&&action==2186){
        selected_cloud_backup.clear();
        cloud_restore_data.clear();
    }else if(page==39&&(action==2187||action==2188)){
        const int direction=action==2187?-1:1;
        const int slot=find_cloud_slot(cloud_restore_slot+direction,direction);
        if(slot>=0)cloud_restore_slot=slot;
    }else if(page==39&&action==2189){
        open_panel(2);
        save_filter=0;save_search.clear();filter_saves();
        const auto row=std::find_if(saves.begin(),saves.end(),[](const Save &entry){return entry.slot==cloud_restored_slot;});
        if(row!=saves.end())save_index=static_cast<int>(row-saves.begin());
    }else if(action==2180){
        cloud_restore_verified=false;cloud_restored_slot=-1;
        selected_cloud_backup.clear();
        cloud_saves.clear();cloud_save_offset=0;
        open_panel(39);
        cloud_save_notice=cloud_save_request("list","/api/cloud/saves")?"Loading backups...":"Sign in or wait for the current backup request.";
    }else if(action==2181&&page==12&&!saves.empty()){
        const Save entry=selected_save();
        if(entry.backup||automatic_save(entry)){notice="Choose a current manual save to back up.";return true;}
        if(!kfx_hub_has_session()){notice="Sign in to back up this save.";return true;}
        struct stat info;
        if(stat(entry.path.c_str(),&info)!=0||info.st_size<=0||info.st_size>2097152){
            notice="Cloud backup supports manual saves up to 2 MiB.";
            return true;
        }
        size_t size=0;
        std::unique_ptr<void,decltype(&SDL_free)> raw(SDL_LoadFile(entry.path.c_str(),&size),SDL_free);
        if(!raw||size==0||size>2097152){notice="Could not read the save.";return true;}
        const auto encoded=encode_cloud_save(static_cast<const unsigned char*>(raw.get()),size);
        const auto location=short_text(entry.campaign,40)+" / Level "+std::to_string(entry.level);
        const auto title=short_text(entry.title,100-location.size()-3)+" / "+location;
        char hostname[128]={};
        const bool named=gethostname(hostname,sizeof(hostname)-1)==0&&hostname[0];
        std::string manifest;
        const auto sidecar=entry.path+".requirements";
        struct stat requirement_info{};
        if(lstat(sidecar.c_str(),&requirement_info)==0&&S_ISREG(requirement_info.st_mode)&&
            requirement_info.st_size>0&&requirement_info.st_size<=98304){
            size_t length=0;
            std::unique_ptr<void,decltype(&SDL_free)> content(SDL_LoadFile(sidecar.c_str(),&length),SDL_free);
            if(content&&length<=98304)manifest=encode_cloud_save(static_cast<const unsigned char *>(content.get()),length);
        }
        const auto body="{\"title\":"+hub_quote(title)+",\"device\":"+hub_quote(named?short_text(hostname,50):"Mac")
            +",\"data\":"+hub_quote(encoded)+",\"requirements\":"+hub_quote(manifest)+"}";
        if(cloud_save_request("upload","/api/cloud/saves",body)){
            cloud_upload_path=entry.path;
            cloud_upload_revision=save_revision(info);
            open_panel(39);
            cloud_save_notice="Backing up this manual save...";
        }else notice="Wait for the current cloud backup request.";
    }else if(page==39&&action==2182){
        if(cloud_save_request("list","/api/cloud/saves"))cloud_save_notice="Loading backups...";
    }else if(page==39&&(action==2183||action==2184)){
        cloud_save_offset=action==2183?0:4;
    }else if(page==39&&action>=2190&&action<=2193){
        const size_t row=cloud_save_offset+action-2190;
        if(row<cloud_saves.size()&&cloud_save_request("review","/api/cloud/saves/"+cloud_saves[row].id)){
            selected_cloud_backup=cloud_saves[row].id;
            cloud_restore_verified=false;cloud_restore_report.clear();
            selected=0;
            cloud_save_notice="Downloading to check compatibility...";
        }
    }else return false;
    return true;
}

void cloud_save_draw()
{
    if(page!=39)return;
    const auto backup=std::find_if(cloud_saves.begin(),cloud_saves.end(),[](const CloudSaveInfo &entry){
        return entry.id==selected_cloud_backup;
    });
    if(!selected_cloud_backup.empty()&&backup!=cloud_saves.end()){
        centered(109,"Restore a copy?",24);
        small_line(48,164,backup->title,544);
        small_line(48,202,"Device: "+backup->device,544);
        small_line(48,240,"Backed up: "+backup->date,544);
        small_line(48,267,cloud_restore_report,544);
        if(cloud_restore_verified&&cloud_restore_slot>=0){
            button(48,299,110,"Previous",2187,static_cast<int>(hits.size())==selected,32);
            small_line(207,305,"Free slot "+std::to_string(cloud_restore_slot+1),200);
            button(482,299,110,"Next",2188,static_cast<int>(hits.size())==selected,32);
            button(340,342,252,"Restore Copy",2185,static_cast<int>(hits.size())==selected,38);
        }
        button(48,342,252,"Cancel",2186,static_cast<int>(hits.size())==selected,38);
        small_line(48,390,cloud_save_notice,544);
        return;
    }
    for(int i=0;i<4&&cloud_save_offset+i<static_cast<int>(cloud_saves.size());++i){
        const auto &backup=cloud_saves[cloud_save_offset+i];
        const int y=108+i*54;
        small_line(24,y,backup.title,362);
        small_line(24,y+20,backup.date+" / "+backup.device,362);
        button(412,y,196,"Review backup",2190+i,static_cast<int>(hits.size())==selected,38);
    }
    button(24,331,185,"Refresh backups",2182,static_cast<int>(hits.size())==selected,34);
    if(cloud_save_offset>0)button(225,331,150,"Previous",2183,static_cast<int>(hits.size())==selected,34);
    if(cloud_saves.size()>4&&cloud_save_offset==0)button(412,331,196,"More backups",2184,static_cast<int>(hits.size())==selected,34);
    if(cloud_restored_slot>=0)button(24,376,355,"Show in Saved Games",2189,static_cast<int>(hits.size())==selected,34);
    else small_line(24,394,cloud_save_notice,592);
}
