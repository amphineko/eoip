#ifndef EOIP_SOCKET_H_
#define EOIP_SOCKET_H_

#include <sys/socket.h>
#include <netinet/in.h>

int socket_open(sa_family_t af, in_port_t proto, const struct sockaddr *addr, socklen_t addr_len);

#endif //EOIP_SOCKET_H_
