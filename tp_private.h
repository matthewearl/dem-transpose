#ifndef __TRANSPOSE_PRIVATE_H
#define __TRANSPOSE_PRIVATE_H


#include <stdint.h>


// Special commands used in encoded message sequences.
#define TP_MSG_TYPE_PACKET_HEADER      0x7f
#define TP_MSG_TYPE_UPDATE_INITIAL     0x80
#define TP_MSG_TYPE_UPDATE_DELTA       0x81

// 1 + maximum svc_* value from quakedef.h
#define TP_NUM_DEM_COMMANDS            57

// Flag for updates that have not yet been populated
#define TP_U_INVALID                   (1 << 31)

// Maximum value of entity_num in either baseline or updates
#define TP_MAX_ENT                     32768

// Internal representation of updates, or update deltas.
typedef struct __attribute__((__packed__)) update_s {
    struct update_s *next;  // link to next update with same entity

    uint16_t model_num;
    uint16_t origin1;
    uint16_t origin2;
    uint16_t origin3;
    uint8_t angle1;
    uint8_t angle2;
    uint8_t angle3;
    uint16_t frame;
    uint8_t color_map;
    uint8_t skin;
    uint8_t effects;
    uint8_t alpha;
    uint8_t scale;
    uint8_t lerp_finish;
    uint32_t flags;
} update_t;


#endif  /* __TRANSPOSE_PRIVATE_H */
