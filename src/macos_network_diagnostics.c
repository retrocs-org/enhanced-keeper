#include "pre_inc.h"
#include "macos_network_diagnostics.h"
#include "config.h"
#include "bflib_fileio.h"
#include <stdio.h>
#include <string.h>
#include "post_inc.h"
void kfx_network_issue(const char *reason,unsigned long turn,unsigned int host_checksum,unsigned int peer_checksum)
{
#ifdef __APPLE__
    // A single bounded report. Reasons are fixed engine labels, never remote text.
    if(strcmp(reason,"checksum-mismatch")&&strcmp(reason,"missing-packet")&&strcmp(reason,"peer-timeout")&&strcmp(reason,"peer-left"))return;
    char destination[2048],pending[2060];
    snprintf(destination,sizeof(destination),"%s",prepare_file_path(FGrp_Save,"network-diagnostics.json"));
    snprintf(pending,sizeof(pending),"%s.pending",destination);
    FILE *file=fopen(pending,"w");if(!file)return;
    fprintf(file,"{\"reason\":\"%s\",\"turn\":%lu,\"host_checksum\":%u,\"peer_checksum\":%u,\"recovery\":\"Wait for automatic repair or the reserved-slot reconnect attempt. A clean departure or expired reservation requires rejoining the lobby.\"}\n",reason,turn,host_checksum,peer_checksum);
    if(fclose(file)==0)rename(pending,destination);
#else
    (void)reason;(void)turn;(void)host_checksum;(void)peer_checksum;
#endif
}
