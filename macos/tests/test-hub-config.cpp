#include "../../src/net_hub.cpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
int main(int argc,char **argv) {
    if(argc>1){
        auto get=http(argv[1],"check","/api/streams/test?offset=4","","test-token");
        assert(get.status==200&&get.body=="{\"ok\":true}");
        auto post=http(argv[1],"check","/api/device/start","{}","test-token");
        assert(post.status==200&&post.body=="{\"ok\":true}");
        auto missing=http(argv[1],"check","/api/not-found","","test-token");
        assert(missing.status==404);
        return 0;
    }
    assert(std::string(kfx_hub_url()) == "https://keeper-enhanced.retrocs.org");
    char temporary[]="/tmp/kfx-hub-config-XXXXXX";
    const char *folder=mkdtemp(temporary);assert(folder);
    kfx_hub_initialize(folder);
    assert(std::string(kfx_hub_url()) == "https://keeper-enhanced.retrocs.org");
    assert(kfx_hub_set_url("https://my-keeper.example.org/"));
    assert(std::string(kfx_hub_url()) == "https://my-keeper.example.org");
    const std::string session=std::string(folder)+"/community-session.cfg";
    std::ifstream file(session);std::string saved;std::getline(file,saved);
    assert(saved == "https://my-keeper.example.org");file.close();
    assert(!kfx_hub_set_url("http://my-keeper.example.org"));
    assert(std::string(kfx_hub_url()) == saved);
    // A previous custom service and its account survive initialization.
    {std::ofstream out(session);out<<"https://another-keeper.example.org\ntest-token\nKeeper\naccount-id\n";}
    kfx_hub_initialize(folder);
    assert(std::string(kfx_hub_url()) == "https://another-keeper.example.org");
    assert(std::string(kfx_hub_account_name()) == "Keeper");
    assert(kfx_hub_set_url("https://keeper-enhanced.retrocs.org"));
    assert(std::string(kfx_hub_account_name()).empty());
    assert(kfx_hub_set_url("http://127.0.0.1:8765"));
    unlink(session.c_str());rmdir(folder);
}
