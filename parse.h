#ifndef __TRANSPOSE_PARSE_H
#define __TRANSPOSE_PARSE_H


#include "tp_private.h"

// Parse a baseline message.
tp_err_t parse_baseline(void *buf, void *buf_end, int version, int *out_len,
                        update_t *out_baselines);

// Parse an update message
// assert entity num < MAX_ENTS
tp_err_t parse_update(void *buf, void *buf_end, update_t *baselines,
                      int *out_len, update_t *out_update, int *out_entity_num);


#endif /* __TRANSPOSE_PARSE_H */
