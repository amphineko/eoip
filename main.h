#ifndef EOIP_MAIN_H_
#define EOIP_MAIN_H_

#include <stdint.h>

#define BUFFER_LENGTH 65536

#define PAYLOAD_LENGTH 65536

#define EOIP_GRE_MAGIC "\x20\x01\x64\x00"

#define EOIP_PROTO 47

struct eoip_pkt_t {
    uint8_t magic[4];
    uint16_t len;
    uint16_t tid;
    char payload[PAYLOAD_LENGTH];
};

struct eoip6_pkt_t {
    uint8_t hdr1;
    uint8_t hdr2;
    char payload[PAYLOAD_LENGTH];
};

union eoip_unified_t {
    struct eoip_pkt_t pkt;
    struct eoip6_pkt_t pkt6;
};

#endif // EOIP_MAIN_H_
