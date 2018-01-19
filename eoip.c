#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/wait.h>


#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>

#include "eoip.h"
#include "tap.h"
#include "socket.h"

#define GRE_MAGIC "\x20\x01\x64\x00"
#define EIPHEAD(tid) 0x3000 | tid
#define BITSWAP(c) (((c & 0xf0) >> 4) | ((c & 0x0f) << 4))

union packet {
  uint16_t header;
  uint8_t  buffer[65536];
  struct iphdr ip;
  struct eoip_pkt_t eoip;
  struct eoip6_pkt_t eoip6;
};

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

int main (int argc, char** argv) {
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

  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "-4")) strncpy(ifname, argv[++i], IFNAMSIZ);
    if(!strcmp(argv[i], "-6")) {
      strncpy(ifname, argv[++i], INET6_ADDRSTRLEN);
      af = AF_INET6;
      proto = 97;
    }
    if(!strcmp(argv[i], "id")) tid = atoi(argv[++i]);
    if(!strcmp(argv[i], "local")) strncpy(src, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "remote")) strncpy(dst, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "mtu")) mtu = atoi(argv[++i]);
    if(!strcmp(argv[i], "gid")) gid = atoi(argv[++i]);
    if(!strcmp(argv[i], "uid")) uid = atoi(argv[++i]);
    if(!strcmp(argv[i], "fork")) daemon = 1;
  }

  if(daemon) {
    pid = fork();
    if(pid < 0) {
      fprintf(stderr, "[ERR] can't daemonize: %s\n", strerror(errno));
      exit(errno);
    }
    if(pid > 0) {
      printf("%d\n", pid);
      exit(0);
    }
  }



  struct sockaddr_storage laddr, raddr;
  socklen_t laddrlen, raddrlen;
  populate_sockaddr(af, proto, src, &laddr, &laddrlen);
  populate_sockaddr(af, proto, dst, &raddr, &raddrlen);

  int sock_fd = socket_open(af, proto, laddr, laddrlen);

  int tap_fd = tap_open(ifname, mtu);

  FD_ZERO(&fds);

  if (uid > 0) setuid(uid);
  if (gid > 0) setgid(gid);

  union packet packet;
  union eoip6_unified eoip6_hdr;

  struct eoip_pkt_t eoip_hdr;

  eoip6_hdr.header = htons((uint16_t) (EIPHEAD(tid)));
  eoip6_hdr.eoip6.head_p1 = (uint8_t) (BITSWAP(eoip6_hdr.eoip6.head_p1));

  eoip_hdr.tid = tid;
  memcpy(&eoip_hdr.magic, GRE_MAGIC, 4);

  fprintf(stderr, "[INFO] attached to %s, mode %s, remote %s, local %s, tid %d, mtu %d.\n", ifname, af == AF_INET6 ? "EoIPv6" : "EoIP", dst, src, tid, mtu);

  pid_t writer = 1, sender, dead;
  int res, wdead = 0;

  do {
    if (writer == 1) writer = fork();
    if (writer > 1 && !wdead) sender = fork();
    if (wdead) wdead = 0;
    if (writer > 0  && sender > 0) { // we are master
      dead = waitpid(-1, &res, 0);
      if (dead == writer) {
        writer = wdead = 1;
      }
      continue;
    }
    if (!sender) { // we are sender
      do {
        FD_SET(tap_fd, &fds);
        select(tap_fd + 1, &fds, NULL, NULL, NULL);
        if (af == AF_INET) {
          len = read(tap_fd, packet.eoip.payload, sizeof(packet));
          memcpy(packet.eoip.magic, &eoip_hdr, 8);
          packet.eoip.len = htons((uint16_t) len);
          len += 8;
        } else {
          len = read(tap_fd, packet.eoip6.payload, sizeof(packet)) + 2;
          memcpy(&packet.header, &eoip6_hdr.header, 2);
        }
        sendto(sock_fd, packet.buffer, (size_t) len, 0, (struct sockaddr*) &raddr, raddrlen);
      } while (1);
    }
    if (!writer) { // we are writer
      do {
        FD_SET(sock_fd, &fds);
        select(sock_fd + 1, &fds, NULL, NULL, NULL);
        len = recv(sock_fd, packet.buffer, sizeof(packet), 0);
        if (af == AF_INET) {
          buffer = packet.buffer;
          buffer += packet.ip.ihl * 4;
          len -= packet.ip.ihl * 4;
          if (memcmp(buffer, GRE_MAGIC, 4)) continue;
          ptid = ((uint16_t *) buffer)[3];
          if (ptid != tid) continue;
          buffer += 8;
          len -= 8;
        } else {
          if(packet.header != eoip6_hdr.header) continue;
          buffer = packet.buffer + 2;
          len -= 2;
        }
        if(len <= 0) continue;
        write(tap_fd, buffer, (size_t) len);
      } while (1);
    }
  } while(1);
}
