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

int open_tap(const char *if_name, int mtu) {
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
