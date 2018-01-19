#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>


#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <sys/wait.h>

#include "network.h"

void populate_sockaddr(sa_family_t af, in_port_t port, char addr[],
                       struct sockaddr_storage *dst, socklen_t *addrlen) {
    // from https://stackoverflow.com/questions/48328708/, many thanks.
    if (af == AF_INET) {
        struct sockaddr_in *dst_in4 = (struct sockaddr_in *) dst;
        *addrlen = sizeof(*dst_in4);
        memset(dst_in4, 0, *addrlen);
        dst_in4->sin_family = af;
        dst_in4->sin_port = htons(port);
        inet_pton(af, addr, &dst_in4->sin_addr);
    } else {
        struct sockaddr_in6 *dst_in6 = (struct sockaddr_in6 *) dst;
        *addrlen = sizeof(*dst_in6);
        memset(dst_in6, 0, *addrlen);
        dst_in6->sin6_family = af;
        dst_in6->sin6_port = htons(port);
        inet_pton(af, addr, &dst_in6->sin6_addr);
    }
}

int main(int argc, char **argv) {
    fd_set fds;
    char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN], ifname[IFNAMSIZ];
    unsigned char *buffer;
    unsigned int ptid, mtu = 1500, daemon = 0;
    sa_family_t af = AF_INET;
    in_port_t proto = EOIP_PROTO;
    uint16_t tid;
    ssize_t len;
    uid_t uid = 0;
    gid_t gid = 0;
    pid_t pid = 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: eoip [ OPTIONS ] IFNAME { remote RADDR } { local LADDR } { id TID }\n");
        fprintf(stderr, "                               [ mtu MTU ] [ uid UID ] [ gid GID ] [ fork ]\n");
        fprintf(stderr, "where: OPTIONS := { -4 | -6 }\n");
        exit(1);
    }

    strncpy(ifname, argv[1], IFNAMSIZ);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-4")) strncpy(ifname, argv[++i], IFNAMSIZ);
        if (!strcmp(argv[i], "-6")) {
            strncpy(ifname, argv[++i], INET6_ADDRSTRLEN);
            af = AF_INET6;
            proto = 97;
        }
        if (!strcmp(argv[i], "id")) tid = atoi(argv[++i]);
        if (!strcmp(argv[i], "local")) strncpy(src, argv[++i], INET6_ADDRSTRLEN);
        if (!strcmp(argv[i], "remote")) strncpy(dst, argv[++i], INET6_ADDRSTRLEN);
        if (!strcmp(argv[i], "mtu")) mtu = atoi(argv[++i]);
        if (!strcmp(argv[i], "gid")) gid = atoi(argv[++i]);
        if (!strcmp(argv[i], "uid")) uid = atoi(argv[++i]);
        if (!strcmp(argv[i], "fork")) daemon = 1;
    }

    if (daemon) {
        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "[ERR] can't daemonize: %s\n", strerror(errno));
            exit(errno);
        }
        if (pid > 0) {
            printf("%d\n", pid);
            exit(0);
        }
    }

    struct sockaddr_storage laddr, raddr;
    socklen_t laddrlen, raddrlen;
    populate_sockaddr(af, proto, src, &laddr, &laddrlen);
    populate_sockaddr(af, proto, dst, &raddr, &raddrlen);

    int fd_so = socket_open(af, proto, &laddr, laddrlen);

    int fd_tap = tap_open(ifname, mtu);

    fprintf(stderr, "[INFO] attached to %s, mode %s, remote %s, local %s, tid %d, mtu %d.\n", ifname,
            af == AF_INET6 ? "EoIPv6" : "EoIP", dst, src, tid, mtu);

    pid_t ch_so, ch_tap;

    switch (ch_tap = fork()) {
        case 0:
            tap_listen(af, fd_tap, fd_so, tid, (const struct sockaddr *) &raddr, raddrlen);
            break;
        case -1:
            fprintf(stderr, "[ERROR] failed to fork TAP listener");
            exit(errno);
        default:
            break;
    }

    switch (ch_so = fork()) {
        case 0:
            socket_listen(af, fd_so, fd_tap, tid);
            break;
        case -1:
            fprintf(stderr, "[ERROR] failed to fork socket listener");
            exit(errno);
        default:
            break;
    }

    waitpid(-1, NULL, 0);
}
