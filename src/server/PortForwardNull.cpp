#include "net_portforward.h"
// Dedicated operators advertise an explicit reachable port; no router discovery.
extern "C" int port_forward_add_mapping(uint16_t){return 0;}
extern "C" void port_forward_remove_mapping(){}
