#include "net_hub.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
int main() {
    assert(std::string(kfx_hub_url()) == "https://keeperfx-e.retrocs.org");
    char temporary[]="/tmp/kfx-hub-config-XXXXXX";
    const char *folder=mkdtemp(temporary);assert(folder);
    kfx_hub_initialize(folder);
    assert(std::string(kfx_hub_url()) == "https://keeperfx-e.retrocs.org");
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
    assert(kfx_hub_set_url("https://keeperfx-e.retrocs.org"));
    assert(std::string(kfx_hub_account_name()).empty());
    assert(kfx_hub_set_url("http://127.0.0.1:8765"));
    unlink(session.c_str());rmdir(folder);
}
