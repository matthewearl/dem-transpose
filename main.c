#include <stdio.h>

#include "transpose.h"


static FILE *out_file = stdout;
static FILE *in_file = stdin;


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


int
main (int argc, char **argv)
{
    tp_err_t rc;

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
            show_usage = true;
        }
    }

    if (rc == TP_ERR_USAGE) {
        fprintf(stderr, "usage: %s [encode|decode]\n");
    } else if (rc != TP_ERR_SUCCESS) {
        fprintf(stderr, "failed: %d\n", rc);
    }

    // cleanup
    buf_cleanup();

    return rc;
}

