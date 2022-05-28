#ifndef __TRANSPOSE_PRIVATE_H
#define __TRANSPOSE_PRIVATE_H


#include <stdbool.h>
#include <stdint.h>


// Special commands used in encoded message sequences.
#define TP_MSG_TYPE_PACKET_HEADER      0x7f
#define TP_MSG_TYPE_UPDATE_INITIAL     0x80
#define TP_MSG_TYPE_UPDATE_DELTA       0x81

// 1 + maximum svc_* value from quakedef.h
#define TP_NUM_DEM_COMMANDS            57

// Flag for updates that have not yet been populated
#define TP_U_INVALID                   (1 << 31)

// Flag for client data that have not yet been populated
#define TP_SU_INVALID                  (1 << 31)

// timemsg_t is this value when no time is yet written.
#define TP_TIME_INVALID                 0x7fc00000  // nan

// Used on packet_header_t when no packet header has been written yet.
#define TP_PACKET_LEN_INVALID           0xffffffff

// Maximum value of entity_num in either baseline or updates
#define TP_MAX_ENT                     32768

// Sanity check on command sizes
#define TP_MAX_MSG_LEN                128000



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


typedef struct __attribute__((__packed__)) client_data_s {
    struct client_data_s *next;

    uint8_t view_height;
    uint8_t ideal_pitch;
    uint8_t punch1;
    uint8_t punch2;
    uint8_t punch3;
    uint8_t vel1;
    uint8_t vel2;
    uint8_t vel3;
    uint32_t items;
    uint16_t weapon_frame;
    uint16_t armor;
    uint16_t weapon;
    uint16_t health;
    uint16_t ammo;
    uint16_t shells;
    uint16_t nails;
    uint16_t rockets;
    uint16_t cells;
    uint8_t active_weapon;
    uint8_t weapon_alpha;

    uint32_t flags;
} client_data_t;


typedef struct __attribute__((__packed__)) timemsg_s {
    struct timemsg_s *next;

    uint32_t time;
} timemsg_t;


typedef struct __attribute__((__packed__)) packet_header_s {
    struct packet_header_s *next;

    uint32_t packet_len;
    uint32_t angle1;
    uint32_t angle2;
    uint32_t angle3;
} packet_header_t;


#endif  /* __TRANSPOSE_PRIVATE_H */
