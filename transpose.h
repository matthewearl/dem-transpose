#ifndef __TRANSPOSE_H
#define __TRANSPOSE_H


#include <stdint.h>
#include "tp_private.h"


#define TP_FOREACH_ERR(F)          \
    F(TP_ERR_SUCCESS)              \
    F(TP_ERR_NOT_ENOUGH_INPUT)     \
    F(TP_ERR_PACKET_TOO_LARGE)     \
    F(TP_ERR_INVALID_MSG_TYPE)     \
    F(TP_ERR_BUFFER_FULL)          \
    F(TP_ERR_NOMEM)                \
    F(TP_ERR_USAGE)                \
    F(TP_ERR_BAD_CD_STRING)

#define TP_GENERATE_ENUM(ENUM) ENUM,
#define TP_GENERATE_STRING(STRING) #STRING,

typedef enum {
    TP_FOREACH_ERR(TP_GENERATE_ENUM)
} tp_err_t;


// Read demo from `infile` and write compressed demo with `dzWrite`.
// `len` is the maximum amount that should be read.
tp_err_t tp_compress (void);


// Read compressed demo from from ??? and write demo with `Outfile_Write`.
// `len` is the size of the decompressed file.
tp_err_t tp_decompress (void);

#endif /* __TRANSPOSE_H */
