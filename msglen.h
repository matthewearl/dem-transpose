#ifndef __TRANSPOSE_MSGLEN_H
#define __TRANSPOSE_MSGLEN_H


#include "tp_private.h"

// Return the length of the message at `buf`.
// Does not handle update / baseline / baseline2 messages.
tp_err_t msglen_get_length(void *buf, void *buf_end, int *out_len);


#endif /* __TRANSPOSE_MSGLEN_H */
