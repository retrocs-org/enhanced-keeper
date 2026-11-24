#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
struct Body{char text[16385];size_t used;};
static size_t receive(char *bytes,size_t size,size_t count,void *context){struct Body *body=context;size_t total=size*count;if(total>16384-body->used)return 0;memcpy(body->text+body->used,bytes,total);body->used+=total;body->text[body->used]=0;return total;}
static int request(const char *url,const char *data,struct Body *body){
 CURL *curl=curl_easy_init();if(!curl)return 0;body->used=0;body->text[0]=0;
 struct curl_slist *headers=curl_slist_append(NULL,"Content-Type: application/json");
 curl_easy_setopt(curl,CURLOPT_URL,url);curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,2000L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,5000L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,body);
 if(data)curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
 CURLcode result=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_slist_free_all(headers);curl_easy_cleanup(curl);return result==CURLE_OK&&status==200;
}
static int yes(const char *json,const char *field){char compact[80],spaced[80];snprintf(compact,sizeof(compact),"\"%s\":true",field);snprintf(spaced,sizeof(spaced),"\"%s\": true",field);return strstr(json,compact)||strstr(json,spaced);}
int main(int argc,char **argv){
 const char *service=NULL,*endpoint=NULL;int port=5556,hosting=0;
 for(int i=1;i+1<argc;i+=2){if(!strcmp(argv[i],"--service"))service=argv[i+1];else if(!strcmp(argv[i],"--endpoint"))endpoint=argv[i+1];else if(!strcmp(argv[i],"--port"))port=atoi(argv[i+1]);else if(!strcmp(argv[i],"--hosting"))hosting=atoi(argv[i+1]);else return 2;}
 if(port<1||port>65535){puts("Choose a UDP port between 1 and 65535.");return 2;}
 curl_global_init(CURL_GLOBAL_DEFAULT);int healthy=0;struct Body body={0};char url[1024];
 if(service&&strlen(service)<768){snprintf(url,sizeof(url),"%s/api/health",service);healthy=request(url,NULL,&body)&&yes(body.text,"ok");puts(healthy?"Community service: ready.":"Community service: unavailable or not ready. LAN and direct connections remain independent.");
  puts(healthy&&yes(body.text,"relay")?"Relay service: available. A successful relay join still requires host registration and a compatible game.":"Relay service: not available or not verified.");
  if(healthy&&hosting&&endpoint&&strlen(endpoint)<160&&!strpbrk(endpoint,"\"\\\r\n")){char data[256];snprintf(data,sizeof(data),"{\"endpoint\":\"%s\"}",endpoint);snprintf(url,sizeof(url),"%s/api/connectivity",service);
   if(request(url,data,&body)){if(yes(body.text,"reachable"))puts("Direct hosting: verified by an incoming UDP challenge from the service.");else puts("Direct hosting: not verified. A local service cannot check Internet reachability. With a public service, retry with your public address; firewall, NAT or CGNAT may block replies.");}
   else puts("Direct hosting: not verified. Check that the advertised host resolves to your public address, and that the lobby is open. The service may also be busy.");
  }else puts("Direct hosting: open a lobby, configure its public address, then retry this test.");
 }else puts("Set a community service in Network > Server Browser > Account before testing its readiness.");
 int udp=socket(AF_INET,SOCK_DGRAM,0);if(udp>=0){struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons((unsigned short)port);if(bind(udp,(struct sockaddr*)&address,sizeof(address))==0)printf("Local UDP port %d: available. This alone does not prove Internet reachability.\n",port);else if(errno==EADDRINUSE)printf("Local UDP port %d: in use%s.\n",port,hosting?" (expected while hosting)":"; another game may be using it");else puts("Local UDP port could not be checked.");close(udp);}
 puts("Next steps: allow KeeperFX-E in the macOS firewall. Try direct play on LAN, or use the community relay when direct Internet hosting is blocked. Keep both players on matching engine/content versions.");curl_global_cleanup();return healthy?0:1;
}
