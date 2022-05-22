#ifndef __TRANPOSE_PRIVATE_H
#define __TRANSPOSE_PRIVATE_H


#define TP_FOREACH_ERR(F)          \
    F(TP_ERR_SUCCESS)              \

#define TP_GENERATE_ENUM(ENUM) ENUM,
#define TP_GENERATE_STRING(STRING) #STRING,

typedef enum {
    TP_FOREACH_ERR(TP_GENERATE_ENUM)
} tp_err_t;


#endif  /* __TRANSPOSE_PRIVATE_H */
