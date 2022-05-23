#ifndef __TRANSPOSE_H
#define __TRANSPOSE_H


#include <stdint.h>
#include "tp_private.h"


#define TP_FOREACH_ERR(F)           \
    F(TP_ERR_SUCCESS)               \
    F(TP_ERR_NOT_ENOUGH_INPUT)      \
    F(TP_ERR_PACKET_TOO_LARGE)      \
    F(TP_ERR_INVALID_MSG_TYPE)      \
    F(TP_ERR_INVALID_ENTITY_NUM)    \
    F(TP_ERR_BUFFER_FULL)           \
    F(TP_ERR_NO_MEM)                \
    F(TP_ERR_USAGE)                 \
    F(TP_ERR_BAD_CD_STRING)         \
    F(TP_ERR_UNSUPPORTED)           \
    F(TP_ERR_MSG_TOO_LONG)          \
    F(TP_ERR_DISCONNECT_MID_PACKET) \

#define TP_GENERATE_ENUM(ENUM) ENUM,
#define TP_GENERATE_STRING(STRING) #STRING,

typedef enum {
    TP_FOREACH_ERR(TP_GENERATE_ENUM)
} tp_err_t;


// Read demo from input file and write encoded demo to output file.
tp_err_t tp_encode (void);


// Read encoded demo from input file and write demo to output file.
tp_err_t tp_decode (void);

#endif /* __TRANSPOSE_H */
