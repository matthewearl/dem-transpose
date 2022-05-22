#include <stdlib.h>
#include <string.h>

#include "transpose.h"
#include "buf.h"


#define BUFFER_MAX_SIZE     (5 << 20)


static update_t *initial_updates[MAX_ENT] = {};
static update_t **initial_updates_next[MAX_ENT] = {};
static update_t *delta_updates[MAX_ENT] = {};
static update_t **delta_updates_next[MAX_ENT] = {};
static void *buf = NULL;
static void *ptr = NULL;
static void *buf_end = NULL;
static int total_msg_size = 0;


tp_err_t
buf_init (void)
{
    int i;

    buf = malloc(BUFFER_MAX_SIZE);
    if (buf == NULL) {
        return TP_ERR_NO_MEM;
    }
    ptr = buf;
    buf_end = buf + BUFFER_MAX_SIZE;
    memset(initial_updates, 0, sizeof(initial_updates));
    memset(delta_updates, 0, sizeof(initial_updates));

    for (i = 0; i < MAX_ENT; i++) {
        initial_updates_next = &initial_updates[i];
        delta_updates_next = &delta_updates[i];
    }

    return TP_ERR_SUCCESS;
}


void
buf_cleanup (void)
{
    if (buf != NULL) {
        free(buf);
    }
    buf = NULL;
    ptr = NULL;
    buf_end = NULL;
}


tp_err_t
buf_add_packet_header (void *header)
{
    if (ptr + 17 > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    *(uint8_t *)ptr = MSG_TYPE_PACKET_HEADER;
    memcpy(ptr + 1, header, 16);
    ptr += 17;
}


tp_err_t
buf_add_message (void *msg, int msg_len)
{
    if (ptr + msg_len > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    memcpy(ptr, msg, msg_len);
    ptr += msg_len;

    return TP_ERR_SUCCESS;
}


tp_err_t
buf_add_update (update_t *update, int entity_num, bool delta)
{
    short entity_num_le;
    update_t *dest_update;

    if (ptr + 3 + sizeof(update_t) > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    *(uint8_t *)ptr = MSG_TYPE_PACKET_HEADER;
    entity_num_le = htole16(entity_num);
    memcpy(ptr + 1, &entity_num_le, 2);
    memcpy(ptr + 3, update, sizeof(update_t));
    dest_update = ptr + 3;
    dest_update->next = NULL;
    if (delta) {
        // TODO:  alignment?
        *delta_updates_next[entity_num] = dest_update;
        delta_updates_next[entity_num] = &dest_update->next;
    } else {
        *initial_updates_next[entity_num] = dest_update;
        initial_updates_next[entity_num] = &dest_update->next;
    }

    return TP_ERR_SUCCESS;
}


void
buf_iter_updates (buf_update_iter_t *out_iter, bool delta)
{
    iter->delta = delta;

    out_iter->entity_num = 0;
    if (delta) {
        iter->next = &delta_updates[0];
    } else {
        iter->next = &initial_updates[0];
    }
}


void
buf_next_update (buf_update_iter_t *iter, update_t **out_update);
{
    // Step through the lists for each entity in turn.
    while (iter->entity_num < MAX_ENTS && iter->next == NULL) {
       iter->entity_num++;
       iter->next = &(iter->delta
                      ? delta_updates
                      : initial_updates)[iter->entity_num];
    }

    if (iter < MAX_ENTS) {
        *out_update = iter->next;
        iter->next = iter->next->next;
    } else {
        *out_update = NULL;
    }
}


void
buf_iter_messages (buf_msg_iter_t *out_iter)
{
    out_iter->ptr = buf;
}


void
buf_next_message (buf_msg_iter_t *iter, void **out_msg, int *out_len)
{
    tp_err_t rc;
    uint8_t cmd;
    int msg_len;

    if (iter->ptr >= ptr) {
        *out_msg = NULL;
        out_len = 0;
    } else {
        cmd = *(uint8_t *)iter->ptr;

        if (cmd == TP_MSG_TYPE_UPDATE_INITIAL
                || cmd == TP_MSG_TYPE_UPDATE_DELTA) {
            msg_len = 3 + sizeof(update_t);
        } else if (cmd == TP_MSG_TYPE_PACKET_HEADER) {
            msg_len = 17;
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            rc = msglen_get_length(iter->ptr, ptr, &msg_len);
            assert(rc == TP_ERR_SUCCESS);
        } else {
            assert(!"Invalid internal message type");
        }

        *out_msg = iter->ptr;
        *out_len = msg_len;

        iter->ptr += msg_len;
        assert(iter->ptr <= ptr);
    }
}


void
buf_write_messages(void)
{
    uint8_t cmd;
    buf_msg_iter_t it;
    void *msg;
    int msg_len;

    buf_iter_messages(&it);
    buf_next_message(&it, &msg, &msg_len);
    while (msg != NULL) {
        assert(msg_len >= 1);
        cmd = *(uint8_t *)msg;

        // TODO: finish writing this

        buf_next_message(&it, &msg, &msg_len);
    }
}

// TODO:  finish writing the rest of the definitions.
