// Exercise the actual ENet resume handler with real loopback datagrams.
#include "../../src/bflib_enet.cpp"
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
extern "C" {
struct Game game={};
static TbClockMSec test_clock(void){return (TbClockMSec)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
TbClockMSec (*LbTimerClock)(void)=test_clock;
int LbNetLog(const char*,...){return 0;}
int LbWarnLog(const char*,...){return 0;}
GameTurn get_gameturn(void){return 1;}
int kfx_hub_relay_registration(int,char*,int,int*,char*,int){return 0;}
void kfx_network_issue(const char*,unsigned long,uint32_t,uint32_t){}
}
static int allocated=0,dropped=0;
static TbBool allocate_user(NetUserId *id){*id=1;allocated++;return true;}
static void dropped_user(NetUserId,NetDropReason){dropped++;}
int main(){
 assert(enet_initialize()==0);g_drop_callback=dropped_user;ENetAddress local={};enet_address_set_host_ip(&local,"127.0.0.1");local.port=0;
 host=enet_host_create(ENET_ADDRESS_TYPE_IPV4,&local,3,2,0,0);assert(host);struct sockaddr_in bound={};socklen_t bound_size=sizeof(bound);assert(getsockname(host->socket,(sockaddr*)&bound,&bound_size)==0);ENetAddress address={};enet_address_set_host_ip(&address,"127.0.0.1");address.port=ntohs(bound.sin_port);ENetHost *client=enet_host_create(ENET_ADDRESS_TYPE_IPV4,nullptr,1,2,0,0);assert(client);ENetPeer *peer=enet_host_connect(client,&address,2,RESUME_CAPABLE);assert(peer);std::array<unsigned char,32> key={};
 auto pump=[&](bool send_key,bool valid){for(int i=0;i<500;i++){bf_enet_read_event(allocate_user,0);ENetEvent event;while(enet_host_service(client,&event,0)>0){if(event.type==ENET_EVENT_TYPE_CONNECT&&send_key){unsigned char bytes[38];memcpy(bytes,"KFXRR1",6);memcpy(bytes+6,key.data(),32);if(!valid)bytes[10]^=1;enet_peer_send(peer,0,enet_packet_create(bytes,38,ENET_PACKET_FLAG_RELIABLE));enet_host_flush(client);}if(event.type==ENET_EVENT_TYPE_RECEIVE){if(event.packet->dataLength==38&&!memcmp(event.packet->data,"KFXRT1",6))memcpy(key.data(),event.packet->data+6,32);enet_packet_destroy(event.packet);}}std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 pump(false,true);assert(allocated==1&&key[0]+key[1]+key[2]+key[3]>0);
 auto reserve=[&](){for(size_t i=0;i<host->peerCount;i++)if(host->peers[i].state==ENET_PEER_STATE_CONNECTED)enet_peer_reset(host->peers+i);enet_peer_reset(peer);reserved_until[1]=LbTimerClock()+5000;};
 reserve();peer=enet_host_connect(client,&address,2,RESUME_CONNECTION);assert(peer);pump(true,false);assert(reserved_until[1]&&allocated==1&&dropped==0);
 enet_peer_reset(peer);peer=enet_host_connect(client,&address,2,RESUME_CONNECTION);assert(peer);pump(true,true);assert(!reserved_until[1]&&allocated==1&&dropped==0);
 const char payload[]="gameplay-packet";enet_peer_send(peer,0,enet_packet_create(payload,sizeof(payload),ENET_PACKET_FLAG_RELIABLE));enet_host_flush(client);pump(false,true);assert(oldest_packet[1]&&oldest_packet[1]->dataLength==sizeof(payload));
 reserved_until[1]=LbTimerClock()-1;resume_tick();assert((dropped==1&&resume_keys[1]==std::array<unsigned char,32>{}));
 enet_host_destroy(client);host_destroy();enet_deinitialize();puts("PASS: real ENet resume rejects a forged key, restores the same slot, routes gameplay packets and expires reservations");
}
