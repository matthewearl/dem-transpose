#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "transpose.h"


static FILE *out_file = NULL;
static FILE *in_file = NULL;


tp_err_t
read_in (void *buf, int len)
{
    if (fread(buf, len, 1, in_file) != 1) {
        return TP_ERR_NOT_ENOUGH_INPUT;
    } else {
        return TP_ERR_SUCCESS;
    }
}


void
write_out (void *buf, int len)
{
    assert(fwrite(buf, len, 1, out_file) == 1);
}


long int
output_pos (void)
{
    return ftell(out_file);
}


long int
input_pos (void)
{
    return ftell(in_file);
}


static const char *tp_err_strings[] = {
    TP_FOREACH_ERR(TP_GENERATE_STRING)
};


int
main (int argc, char **argv)
{
    tp_err_t rc;

    in_file = stdin;
    out_file = stdout;

    if (argc != 2) {
        rc = TP_ERR_USAGE;
    }

    if (rc == TP_ERR_SUCCESS) {
        rc = buf_init();
    }

    if (rc == TP_ERR_SUCCESS) {
        if (!strcmp(argv[1], "encode")) {
            rc = tp_encode();
        } else if (!strcmp(argv[1], "decode")) {
            rc = tp_decode();
        } else {
            rc = TP_ERR_USAGE;
        }
    }

    if (rc == TP_ERR_USAGE) {
        fprintf(stderr, "usage: %s [encode|decode]\n", argv[0]);
    } else if (rc != TP_ERR_SUCCESS) {
        fprintf(stderr, "failed: %s\n", tp_err_strings[rc]);
    }

    // cleanup
    buf_cleanup();

    return rc;
}

