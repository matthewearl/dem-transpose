#ifndef __TRANSPOSE_MAIN_H
#define __TRANSPOSE_MAIN_H

#include "transpose.h"

tp_err_t read_in (void *buf, int len);
void write_out (void *buf, int len);
bool at_end_of_input (void);

#endif /* __TRANSPOSE_MAIN_H */
