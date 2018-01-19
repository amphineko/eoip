#include <fcntl.h>
#include <string.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include "tap.h"

inline void tap_read(int fd, fd_set *fds, char *buffer, size_t bufsize, ssize_t *size) {
    FD_ZERO(fds);
    FD_SET(fd, fds);
    select(fd, fds, NULL, NULL, NULL);
    *size = read(fd, buffer, bufsize);
}

void tap_listen(sa_family_t af, int fd, int sock_fd, uint16_t tid, const struct sockaddr *dst, socklen_t dst_len) {
    fd_set *fds;
    union eoip_unified_t pkt;
    ssize_t frame_size;
    size_t send_size;

    fprintf(stderr, "TAP listener started\n");

    /* prepare packet header */
    switch (af) {
#if __BYTE_ORDER == __BIG_ENDIAN
        default:
            assert(false);  // TODO: BE to LE
#elif __BYTE_ORDER == LITTLE_ENDIAN
        case AF_INET:
            pkt.pkt.tid = tid;
        case AF_INET6:
            pkt.pkt6.hdr1 = (uint8_t) ((((uint8_t *) tid)[0] << 4) | 0x03);
            pkt.pkt6.hdr2 = (uint8_t) tid;
#endif

        default:
            assert(false);
    }

    while (true) {
        switch (af) {
            case AF_INET:
                tap_read(fd, fds, pkt.pkt.payload, PAYLOAD_LENGTH, &frame_size);
                pkt.pkt.len = htons((uint16_t) frame_size);
                send_size = (size_t) (frame_size + 8);
                break;
            case AF_INET6:
                tap_read(fd, fds, pkt.pkt6.payload, PAYLOAD_LENGTH, &frame_size);
                send_size = (size_t) (frame_size + 2);
            default:
                assert(false);
        }

        sendto(sock_fd, &pkt, send_size, 0, dst, dst_len);
    }
}

int tap_open(const char *if_name, int mtu) {
    int tap_fd = open("/dev/net/tun", O_RDWR);
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

    ifr.ifr_flags = IFF_NO_PI | IFF_TAP;
    if (ioctl(tap_fd, TUNSETIFF, (void *) &ifr)) {
        fprintf(stderr, "[ERR] can't TUNSETIFF: %s.\n", strerror(errno));
        exit(errno);
    }

    ifr.ifr_mtu = mtu;

    if (ioctl(socket(AF_INET, SOCK_STREAM, IPPROTO_IP), SIOCSIFMTU, (void *) &ifr) < 0)
        fprintf(stderr, "[WARN] can't SIOCSIFMTU (%s), please set MTU of %s to %d manually.\n", strerror(errno),
                ifr.ifr_name, ifr.ifr_mtu);
}
