#ifndef __TRANSPOSE_BUF_H
#define __TRANSPOSE_BUF_H


#include <stdbool.h>

#include "transpose.h"
#include "tp_private.h"


/*
 * The buffer is a sequence of messages, with some structure linking them.
 * Messages are one of three types:
 *  - Non-update messages from a demo file.
 *  - Update messages from a demo file.
 *  - Packet headers.
 *
 * Non-update messages are represented as they are in the demo file (one-byte
 * message type followed by a variable sequence of bytes).  Updates are
 * represented by a one-byte message type (always 0x80) followed by an entity
 * id, followed by the update_t structure.  Packet headers are represented by a
 * new one-byte message type, followed by the 16 byte header from the header
 * file.
 *
 * All messages can be iterated in insertion order.  On iteration updates are
 * returned as:
 *
 *   [byte] (0x80|0x81)   // message type
 *                        // 0x80 indicates initial value
 *                        // 0x81 indicates delta
 *   [short] entity num
 *   [update_t] the update itself
 *
 * Updates can be iterated in (packet_num, entity_num) order.  In this case the
 * update_t objects are returned.
 */


typedef struct  {
    int entity_num;
    update_t *next;
    bool delta;
} buf_update_iter_t;


typedef struct  {
    void *ptr;
} buf_msg_iter_t;


// Module initialization / shutdown.
tp_err_t buf_init(void);
void buf_cleanup(void);

// Add 16-byte packet header.
tp_err_t buf_add_packet_header(void *ptr);

// Add a message to the front of the buffer.
// TODO:  should msg_len be computed internally using msglen module?
tp_err_t buf_add_message(void *msg, int msg_len);

// Add an update message.
tp_err_t buf_add_update(update_t *update, int entity_num, bool delta);

// Add a client update message.
tp_err_t buf_add_client_data(client_data_t *client_data);

// Iterate update_t objects in (packet num, entity_num) order
void buf_iter_updates(buf_update_iter_t *out_iter, bool delta);
void buf_next_update(buf_update_iter_t *iter, update_t **out_update);

// Iterate messages.
void buf_iter_messages(buf_msg_iter_t *out_iter);
void buf_next_message(buf_msg_iter_t *iter,
                      void **out_msg, int *out_len);


// Write all messages to file.  (includes size header)
void buf_write_messages(void);

// Read all messages from file. (includes size header)
// (update_t structures are linked into lists, but otherwise left uninitialized)
tp_err_t buf_read_messages(void);

// Remove all messages and updates from the buffer.
void buf_clear(void);

// Is the buffer empty?
bool buf_is_empty(void);

#endif  /* __TRANSPOSE_BUF_H */
