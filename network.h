#ifndef EOIP_SOCKET_H_
#define EOIP_SOCKET_H_

#include <sys/socket.h>
#include <netinet/in.h>

#include "main.h"
#include "tap.h"

void socket_listen(sa_family_t af, int fd, int tap_fd, uint16_t tid);

int socket_open(sa_family_t af, in_port_t proto, const struct sockaddr *addr, socklen_t addr_len);

#endif //EOIP_SOCKET_H_
