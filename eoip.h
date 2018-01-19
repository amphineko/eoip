#include <stdint.h>

const in_port_t EOIP_PROTO = 47;

struct eoip_pkt_t {
    uint8_t  magic[4];
    uint16_t len;
    uint16_t tid;
    uint8_t  payload[0];
};

struct eoip6_pkt_t {
    uint8_t head_p1;
    uint8_t head_p2;
    uint8_t payload[0];
};

union eoip6_unified {
    struct   eoip6_pkt_t eoip6;
    uint16_t header;
};
