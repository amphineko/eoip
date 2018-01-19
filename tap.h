#ifndef EOIP_TAP_H_
#define EOIP_TAP_H_

#include "main.h"
#include "network.h"

void tap_listen(sa_family_t af, int fd, int sock_fd, uint16_t tid, const struct sockaddr *dst, socklen_t dst_len);

int tap_open(const char *if_name, int mtu);

#endif //EOIP_TAP_H_
