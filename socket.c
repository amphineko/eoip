#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "socket.h"

int socket_open(sa_family_t af, in_port_t proto, const struct sockaddr *addr, const socklen_t addr_len) {
    int sock_fd = socket(af, SOCK_RAW, proto);
    if (bind(sock_fd, addr, addr_len) < 0) {
        fprintf(stderr, "[ERR] can't bind socket: %s.\n", strerror(errno));
        exit(errno);
    }
}
