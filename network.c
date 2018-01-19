#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <assert.h>
#include "network.h"

void socket_read(int fd, fd_set *fds, char *buf, size_t bufsize, ssize_t *size) {
    FD_ZERO(fds);
    FD_SET(fd, fds);
    select(fd, fds, NULL, NULL, NULL);
    *size = read(fd, buf, bufsize);
}

void socket_listen(sa_family_t af, int fd, int tap_fd, uint16_t tid) {
    fd_set *fds = malloc(sizeof(fd_set));
    char buffer[BUFFER_LENGTH];
    union eoip_unified_t *pkt;
    ssize_t recv_size;
    size_t hdr_size, read_offset;

    const uint8_t tpl6[] = {(uint8_t) ((((uint8_t *) tid)[0] << 4) | 0x03), (uint8_t) tid};

    fprintf(stderr, "[INFO] Socket listener started\n");

    while (true) {
        socket_read(fd, fds, &buffer[0], BUFFER_LENGTH, &recv_size);

        switch (af) {
#if __BYTE_ORDER == __BIG_ENDIAN
            default:
                assert(false);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
            case AF_INET:
                hdr_size = ((struct iphdr *) buffer)->ihl * 4;                              // ip header size
                pkt = (union eoip_unified_t *) (buffer + hdr_size);
                read_offset = (size_t) (buffer + hdr_size + 8);

                if (recv_size < hdr_size + 8)                                               // invalid eoip header
                    continue;
                if (memcmp(pkt, EOIP_GRE_MAGIC, 4) != 0)                                    // bad GRE header
                    continue;
                if (ntohs(pkt->pkt.len) != recv_size - hdr_size - 8)
                    continue;

                break;
            case AF_INET6:
                pkt = (union eoip_unified_t *) buffer;
                read_offset = (size_t) (buffer + 2);

                if (recv_size < 2)                                                          // invalid eoip6 header
                    continue;
                if (memcmp(pkt, tpl6, 2) != 0)
                    continue;

                break;
#endif
            default:
                assert(false);
        }

        write(fd, buffer + read_offset, recv_size - read_offset);
    }
}

int socket_open(sa_family_t af, in_port_t proto, const struct sockaddr *addr, const socklen_t addr_len) {
    int sock_fd = socket(af, SOCK_RAW, proto);
    if (bind(sock_fd, addr, addr_len) < 0) {
        fprintf(stderr, "[ERR] can't bind socket: %s.\n", strerror(errno));
        exit(errno);
    }
}
