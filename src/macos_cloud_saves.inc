// Cloud backups are explicit and restore to a free local slot.
struct CloudSaveInfo { std::string id,title,device,date; };
std::vector<CloudSaveInfo> cloud_saves;
std::string cloud_save_notice;
int cloud_save_offset=0;
bool cloud_save_pending=false;

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

void restore_cloud_save(const std::string &encoded)
{
    if(network_is_active()||game.game_kind!=GKind_Unset){
        cloud_save_notice="Restore from Saved Games at the main menu.";
        return;
    }
    const auto data=decode_cloud_save(encoded);
    const size_t size=data.size();
    FileChunkHeader header{};
    if(size>=sizeof(header))memcpy(&header,data.data(),sizeof(header));
    if(size<sizeof(header)+sizeof(CatalogueEntry)||size>2097152||header.id!=SGC_InfoBlock||header.len!=sizeof(CatalogueEntry)){
        cloud_save_notice="This backup is not a supported saved game.";
        return;
    }
    for(int slot=0;slot<SAVE_SLOTS_LIMIT;++slot){
        char filename[32];
        snprintf(filename,sizeof(filename),"fx1g%04d.sav",slot);
        const auto destination=path(filename);
        bool occupied=false;
        for(const char *suffix:{"",".requirements",".json",".png",".label",".protected",".backup1",".backup2",".backup3"}){
            struct stat info;
            if(lstat((destination+suffix).c_str(),&info)==0){occupied=true;break;}
        }
        if(occupied)continue;
        int fd=open(destination.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
        if(fd<0){if(errno==EEXIST)continue;break;}
        FILE *file=fdopen(fd,"wb");
        if(!file){close(fd);unlink(destination.c_str());break;}
        bool ok=fwrite(data.data(),1,size,file)==static_cast<size_t>(size);
        ok=fflush(file)==0&&fsync(fd)==0&&ok;
        ok=fclose(file)==0&&ok;
        if(!ok){unlink(destination.c_str());break;}
        load_game_save_catalogue();
        scan_saves();
        cloud_save_notice="Restored to local slot "+std::to_string(slot)+".";
        return;
    }
    cloud_save_notice="Could not create a free local save slot.";
}

void cloud_save_reply(const std::string &tag,const VALUE &root,const std::string &error)
{
    cloud_save_pending=false;
    const auto suffix=tag.rfind(':');
    if(suffix==std::string::npos||tag.substr(suffix+1)!=kfx_hub_account_id())return;
    if(kfx_hub_result_status()!=200){cloud_save_notice=error.empty()?"Cloud backup unavailable.":error;return;}
    if(tag.rfind("cloud-save:upload:",0)==0)cloud_save_notice="Manual save backed up.";
    else if(tag.rfind("cloud-save:restore:",0)==0)restore_cloud_save(hub_field(&root,"data"));
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
    if(action==2180){
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
        const auto title=short_text(entry.title+" / "+entry.campaign+" "+std::to_string(entry.level),100);
        char hostname[128]={};
        const bool named=gethostname(hostname,sizeof(hostname)-1)==0&&hostname[0];
        const auto body="{\"title\":"+hub_quote(title)+",\"device\":"+hub_quote(named?short_text(hostname,50):"Mac")
            +",\"data\":"+hub_quote(encoded)+"}";
        if(cloud_save_request("upload","/api/cloud/saves",body)){
            open_panel(39);
            cloud_save_notice="Backing up this manual save...";
        }else notice="Wait for the current cloud backup request.";
    }else if(page==39&&action==2182){
        if(cloud_save_request("list","/api/cloud/saves"))cloud_save_notice="Loading backups...";
    }else if(page==39&&(action==2183||action==2184)){
        cloud_save_offset=action==2183?0:4;
    }else if(page==39&&action>=2190&&action<=2193){
        const size_t row=cloud_save_offset+action-2190;
        if(row<cloud_saves.size()&&cloud_save_request("restore","/api/cloud/saves/"+cloud_saves[row].id))
            cloud_save_notice="Downloading backup...";
    }else return false;
    return true;
}

void cloud_save_draw()
{
    if(page!=39)return;
    for(int i=0;i<4&&cloud_save_offset+i<static_cast<int>(cloud_saves.size());++i){
        const auto &backup=cloud_saves[cloud_save_offset+i];
        const int y=108+i*54;
        small_line(24,y,backup.title,362);
        small_line(24,y+20,backup.date+" / "+backup.device,362);
        button(412,y,196,"Restore copy",2190+i,static_cast<int>(hits.size())==selected,38);
    }
    button(24,331,185,"Refresh backups",2182,static_cast<int>(hits.size())==selected,34);
    if(cloud_save_offset>0)button(225,331,150,"Previous",2183,static_cast<int>(hits.size())==selected,34);
    if(cloud_saves.size()>4&&cloud_save_offset==0)button(412,331,196,"More backups",2184,static_cast<int>(hits.size())==selected,34);
    small_line(24,394,cloud_save_notice,592);
}
