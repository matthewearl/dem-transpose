#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "transpose.h"
#include "buf.h"
#include "msglen.h"
#include "main.h"
#include "quakedef.h"


#define BUFFER_MAX_SIZE     (100 << 20)


static update_t *initial_updates[TP_MAX_ENT] = {};
static update_t **initial_updates_next[TP_MAX_ENT] = {};
static update_t *delta_updates[TP_MAX_ENT] = {};
static update_t **delta_updates_next[TP_MAX_ENT] = {};

static client_data_t *client_datas = NULL;
static client_data_t **client_data_next = NULL;

static timemsg_t *times = NULL;
static timemsg_t **times_next = NULL;

static packet_header_t *packet_headers = NULL;
static packet_header_t **packet_headers_next = NULL;

static void *buf = NULL;
static void *ptr = NULL;
static void *buf_end = NULL;
static int total_message_size = 0;


tp_err_t
buf_init (void)
{
    buf = malloc(BUFFER_MAX_SIZE);
    if (buf == NULL) {
        return TP_ERR_NO_MEM;
    }
    buf_end = buf + BUFFER_MAX_SIZE;

    buf_clear();

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
buf_add_message (void *msg, int msg_len)
{
    if (ptr + msg_len > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    memmove(ptr, msg, msg_len);
    ptr += msg_len;
    total_message_size += msg_len;

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

    *(uint8_t *)ptr = (delta
                       ? TP_MSG_TYPE_UPDATE_DELTA
                       : TP_MSG_TYPE_UPDATE_INITIAL);
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

    ptr += 3 + sizeof(update_t);
    total_message_size += 3;

    return TP_ERR_SUCCESS;
}


tp_err_t
buf_add_client_data (client_data_t *client_data)
{
    client_data_t *dest_client_data;
    if (ptr + 1 + sizeof(client_data_t) > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    *(uint8_t *)ptr = svc_clientdata;
    dest_client_data = ptr + 1;
    memcpy(dest_client_data, client_data, sizeof(client_data_t));
    dest_client_data->next = NULL;
    ptr += 1 + sizeof(client_data_t);
    total_message_size += 1;

    *client_data_next = dest_client_data;
    client_data_next = &dest_client_data->next;

    return TP_ERR_SUCCESS;
}


tp_err_t
buf_add_time(timemsg_t *time)
{
    timemsg_t *dest;
    if (ptr + 1 + sizeof(timemsg_t) > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    *(uint8_t *)ptr = svc_time;
    dest = ptr + 1;
    memcpy(dest, time, sizeof(timemsg_t));
    dest->next = NULL;
    ptr += 1 + sizeof(timemsg_t);
    total_message_size += 1;

    *times_next = dest;
    times_next = &dest->next;

    return TP_ERR_SUCCESS;
}


tp_err_t
buf_add_packet_header(packet_header_t *packet_header)
{
    packet_header_t *dest;
    if (ptr + 1 + sizeof(packet_header_t) > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    *(uint8_t *)ptr = TP_MSG_TYPE_PACKET_HEADER;
    dest = ptr + 1;
    memcpy(dest, packet_header, sizeof(packet_header_t));
    dest->next = NULL;
    ptr += 1 + sizeof(packet_header_t);
    total_message_size += 1;

    *packet_headers_next = dest;
    packet_headers_next = &dest->next;

    return TP_ERR_SUCCESS;
}


void
buf_iter_updates (buf_update_iter_t *out_iter, bool delta)
{
    out_iter->delta = delta;

    out_iter->entity_num = 0;
    if (delta) {
        out_iter->next = delta_updates[0];
    } else {
        out_iter->next = initial_updates[0];
    }
}


void
buf_next_update (buf_update_iter_t *iter, update_t **out_update)
{
    // Step through the lists for each entity in turn.
    while (iter->entity_num < TP_MAX_ENT && iter->next == NULL) {
       iter->entity_num++;
       iter->next = (iter->delta
                      ? delta_updates
                      : initial_updates)[iter->entity_num];
    }

    if (iter->entity_num < TP_MAX_ENT) {
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


static tp_err_t
buf_get_internal_message_length (void *buf, void *buf_end, bool stub_update,
                                 int *out_len)
{
    tp_err_t rc;
    uint8_t cmd = *(uint8_t *)buf;

    if (cmd == TP_MSG_TYPE_UPDATE_INITIAL
            || cmd == TP_MSG_TYPE_UPDATE_DELTA) {
        if (buf + 3 > buf_end) {
            return TP_ERR_NOT_ENOUGH_INPUT;
        }
        if (stub_update) {
            *out_len = 3;
        } else {
            *out_len = 3 + sizeof(update_t);
        }
    } else if (cmd == svc_clientdata) {
        if (stub_update) {
            *out_len = 1;
        } else {
            *out_len = 1 + sizeof(client_data_t);
        }
    } else if (cmd == svc_time) {
        if (stub_update) {
            *out_len = 1;
        } else {
            *out_len = 1 + sizeof(timemsg_t);
        }
    } else if (cmd == TP_MSG_TYPE_PACKET_HEADER) {
        if (stub_update) {
            *out_len = 1;
        } else {
            *out_len = 1 + sizeof(packet_header_t);
        }
    } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
        rc = msglen_get_length(buf, buf_end, out_len);
        if (rc != TP_ERR_SUCCESS) {
            return rc;
        }
    } else {
        return TP_ERR_INVALID_MSG_TYPE;
    }

    return TP_ERR_SUCCESS;
}


void
buf_next_message (buf_msg_iter_t *iter, void **out_msg, int *out_len)
{
    tp_err_t rc;
    int msg_len;

    if (iter->ptr >= ptr) {
        *out_msg = NULL;
        out_len = 0;
    } else {
        rc = buf_get_internal_message_length(iter->ptr, ptr, false, &msg_len);
        assert(rc == TP_ERR_SUCCESS);  // message was checked on insert

        *out_msg = iter->ptr;
        *out_len = msg_len;
        iter->ptr += msg_len;
        assert(iter->ptr <= ptr);
    }
}


// Write messages from the internal buffer to disk.  This is part of the
// encoding process.
void
buf_write_messages (void)
{
    uint8_t cmd;
    buf_msg_iter_t it;
    void *msg;
    int msg_len;
    uint32_t total_msg_size_le;
    uint32_t written = 0;

    total_msg_size_le = htole32(total_message_size);
    write_out(&total_msg_size_le, sizeof(total_msg_size_le));

    buf_iter_messages(&it);
    buf_next_message(&it, &msg, &msg_len);
    while (msg != NULL) {
        assert(msg_len >= 1);
        cmd = *(uint8_t *)msg;

        if (cmd == TP_MSG_TYPE_UPDATE_DELTA
                || cmd == TP_MSG_TYPE_UPDATE_INITIAL) {
            // Don't write the update_t, just the command and entity num
            assert(msg_len >= 3);
            write_out(msg, 3);
            written += 3;
        } else if (cmd == svc_clientdata || cmd == svc_time
                    || cmd == TP_MSG_TYPE_PACKET_HEADER) {
            assert(msg_len >= 1);
            write_out(msg, 1);
            written += 1;
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            // Otherwise, verbatim dump the entire command.
            write_out(msg, msg_len);
            written += msg_len;
        } else {
            assert(!"invalid message type");
        }

        buf_next_message(&it, &msg, &msg_len);
    }

    assert(written == total_message_size);
}


tp_err_t
buf_read_messages (void)
{
    tp_err_t rc;
    uint8_t cmd;
    int msg_len;
    void *read_ptr;

    // Check module is initialized but buffer is empty.
    assert(total_message_size == 0);
    assert(buf == ptr);
    assert(buf != NULL);

    read_in(&total_message_size, sizeof(total_message_size));
    total_message_size = le32toh(total_message_size);

    if (buf + total_message_size > buf_end) {
        return TP_ERR_BUFFER_FULL;
    }

    // Read into the end of the buffer to start with, then move to the start,
    // inserting update_t messages along the way.
    read_ptr = buf_end - total_message_size;
    read_in(read_ptr, total_message_size);

    while (read_ptr < buf_end) {
        buf_get_internal_message_length(read_ptr, buf_end, true, &msg_len);

        cmd = *(uint8_t *)read_ptr;

        if (cmd == TP_MSG_TYPE_UPDATE_DELTA
                || cmd == TP_MSG_TYPE_UPDATE_INITIAL) {
            uint16_t entity_num_s;
            update_t update = {.flags = TP_U_INVALID};
            assert(msg_len == 3);

            memcpy(&entity_num_s, read_ptr + 1, 2);
            entity_num_s = le16toh(entity_num_s);

            rc = buf_add_update(&update, entity_num_s,
                                cmd == TP_MSG_TYPE_UPDATE_DELTA);
            if (rc != TP_ERR_SUCCESS) {
                return rc;
            }
        } else if (cmd == svc_clientdata) {
            client_data_t client_data = {.flags = TP_SU_INVALID};
            assert(msg_len == 1);
            rc = buf_add_client_data(&client_data);
            if (rc != TP_ERR_SUCCESS) {
                return rc;
            }
        } else if (cmd == svc_time) {
            timemsg_t time = {.time = TP_TIME_INVALID};
            assert(msg_len == 1);
            rc = buf_add_time(&time);
            if (rc != TP_ERR_SUCCESS) {
                return rc;
            }
        } else if (cmd == TP_MSG_TYPE_PACKET_HEADER) {
            packet_header_t packet_header =
                {.packet_len = TP_PACKET_LEN_INVALID};
            assert(msg_len == 1);
            rc = buf_add_packet_header(&packet_header);
            if (rc != TP_ERR_SUCCESS) {
                return rc;
            }
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            rc = buf_add_message(read_ptr, msg_len);
            if (rc != TP_ERR_SUCCESS) {
                return rc;
            }
        } else {
            return TP_ERR_INVALID_MSG_TYPE;
        }

        read_ptr += msg_len;
        if (ptr > read_ptr) {
            return TP_ERR_BUFFER_FULL;
        }
    }

    return TP_ERR_SUCCESS;
}


client_data_t *
buf_get_client_data_list (void)
{
    return client_datas;
}


timemsg_t *
buf_get_time_list (void)
{
    return times;
}


packet_header_t *
buf_get_packet_header_list (void)
{
    return packet_headers;
}


void
buf_clear (void)
{
    int i;

    ptr = buf;
    memset(initial_updates, 0, sizeof(initial_updates));
    memset(delta_updates, 0, sizeof(initial_updates));

    for (i = 0; i < TP_MAX_ENT; i++) {
        initial_updates_next[i] = &initial_updates[i];
        delta_updates_next[i] = &delta_updates[i];
    }

    client_datas = NULL;
    client_data_next = &client_datas;

    times = NULL;
    times_next = &times;

    packet_headers = NULL;
    packet_headers_next = &packet_headers;

    total_message_size = 0;
}


bool
buf_is_empty (void)
{
    return total_message_size == 0;
}
