#include <assert.h>
#include <endian.h>
#include <stddef.h>
#include <string.h>

#include "buf.h"
#include "main.h"
#include "transpose.h"
#include "quakedef.h"


static update_t updates[TP_MAX_ENT];


#define APPLY_DELTA_FIELD(f)    out->f = initial->f + delta->f

static void
dec_apply_update_delta (update_t *initial, update_t *delta, update_t *out)
{
    APPLY_DELTA_FIELD(model_num);
    APPLY_DELTA_FIELD(origin1);
    APPLY_DELTA_FIELD(origin2);
    APPLY_DELTA_FIELD(origin3);
    APPLY_DELTA_FIELD(angle1);
    APPLY_DELTA_FIELD(angle2);
    APPLY_DELTA_FIELD(angle3);
    APPLY_DELTA_FIELD(frame);
    APPLY_DELTA_FIELD(color_map);
    APPLY_DELTA_FIELD(skin);
    APPLY_DELTA_FIELD(effects);
    APPLY_DELTA_FIELD(alpha);
    APPLY_DELTA_FIELD(scale);
    APPLY_DELTA_FIELD(lerp_finish);
    APPLY_DELTA_FIELD(flags);
}


static void
dec_apply_client_data_delta (client_data *initial, client_data *delta,
                             client_data *out)
{
    APPLY_DELTA_FIELD(view_height);
    APPLY_DELTA_FIELD(ideal_pitch);
    APPLY_DELTA_FIELD(punch1);
    APPLY_DELTA_FIELD(punch2);
    APPLY_DELTA_FIELD(punch3);
    APPLY_DELTA_FIELD(vel1);
    APPLY_DELTA_FIELD(vel2);
    APPLY_DELTA_FIELD(vel3);
    APPLY_DELTA_FIELD(items);
    APPLY_DELTA_FIELD(weapon_frame);
    APPLY_DELTA_FIELD(armor);
    APPLY_DELTA_FIELD(weapon);
    APPLY_DELTA_FIELD(health);
    APPLY_DELTA_FIELD(ammo);
    APPLY_DELTA_FIELD(shells);
    APPLY_DELTA_FIELD(nails);
    APPLY_DELTA_FIELD(rockets);
    APPLY_DELTA_FIELD(cells);
    APPLY_DELTA_FIELD(active_weapon);
    APPLY_DELTA_FIELD(weapon_alpha);
    APPLY_DELTA_FIELD(flags);
}


static tp_err_t
dec_read_cd_string (void)
{
    tp_err_t rc;
    int i = 0;
	char cd_string[12];

    for (i = 0; i < sizeof(cd_string); i++) {
        rc = read_in(&cd_string[i], 1);
        if (rc != TP_ERR_SUCCESS) {
            return rc;
        }
        if (cd_string[i] == '\n') {
            break;
        }
    }

    if (cd_string[i] == '\n') {
        write_out(cd_string, i + 1);
        return TP_ERR_SUCCESS;
    } else {
        return TP_ERR_BAD_CD_STRING;;
    }
}


static tp_err_t
dec_read_field (int offs, int size, bool delta)
{
    tp_err_t rc;
    buf_update_iter_t it;
    update_t *update;

    buf_iter_updates(&it, delta);
    buf_next_update(&it, &update);
    while (update != NULL) {
        rc = read_in(((uint8_t *)update) + offs, size);
        if (rc != TP_ERR_SUCCESS) {
            return rc;
        }
        buf_next_update(&it, &update);
    }

    return TP_ERR_SUCCESS;
}


static tp_err_t
dec_read_buffer (void)
{
    int i;
    bool delta;
    tp_err_t rc;

    // Read in the messages.  Update structs will exist but won't be
    // populated.
    rc = buf_read_messages();

    // Populate the structs.
    if (rc == TP_ERR_SUCCESS) {
#define READ_FIELD(x, y)                                       \
        do {                                                   \
            rc = dec_read_field(offsetof(update_t, x),         \
                                sizeof(((update_t *)NULL)->x), \
                                (y));                          \
            if (rc != TP_ERR_SUCCESS) {                        \
                return rc;                                     \
            }                                                  \
        } while(0)

        // Read in the initial values (first iter) then the deltas
        // (second iter).
        for (i = 0; i < 2; i++) {
            delta = (i == 1);
            READ_FIELD(model_num, delta);
            READ_FIELD(origin1, delta);
            READ_FIELD(origin2, delta);
            READ_FIELD(origin3, delta);
            READ_FIELD(angle1, delta);
            READ_FIELD(angle2, delta);
            READ_FIELD(angle3, delta);
            READ_FIELD(frame, delta);
            READ_FIELD(color_map, delta);
            READ_FIELD(skin, delta);
            READ_FIELD(effects, delta);
            READ_FIELD(alpha, delta);
            READ_FIELD(scale, delta);
            READ_FIELD(lerp_finish, delta);
            READ_FIELD(flags, delta);

            // TODO: endian swap?
        }
    }

    return rc;
}


static void
dec_emit_update (int entity_num)
{
    uint8_t cmd;
    update_t *update = &updates[entity_num];
    uint32_t flags = update->flags;

    cmd = 0x80 | (flags & 0x7f);
    write_out(&cmd, 1);

    if (flags & U_MOREBITS) {
        uint8_t more_flags = (flags >> 8) & 0xff;
        write_out(&more_flags, 1);
    }
    if (flags & U_EXTEND1) {
        uint8_t extend1_flags = (flags >> 16) & 0xff;
        write_out(&extend1_flags, 1);
    }
    if (flags & U_EXTEND2) {
        uint8_t extend2_flags = (flags >> 16) & 0xff;
        write_out(&extend2_flags, 1);
    }
    if (flags & U_LONGENTITY) {
        uint16_t entity_num_s = htole16(entity_num);
        write_out(&entity_num_s, 2);
    } else {
        uint8_t entity_num_b = entity_num & 0xff;
        write_out(&entity_num_b, 1);
    }
    if (flags & U_MODEL) {
        uint8_t model_num_b = update->model_num & 0xff;
        write_out(&model_num_b, 1);
    }
    if (flags & U_FRAME) {
        uint8_t frame_b = update->frame & 0xff;
        write_out(&frame_b, 1);
    }
    if (flags & U_COLORMAP) {
        write_out(&update->color_map, 1);
    }
    if (flags & U_SKIN) {
        write_out(&update->skin, 1);
    }
    if (flags & U_EFFECTS) {
        write_out(&update->effects, 1);
    }
    if (flags & U_ORIGIN1) {
        uint16_t coord = htole16(update->origin1);
        write_out(&coord, 2);
    }
    if (flags & U_ANGLE1) {
        write_out(&update->angle1, 1);
    }
    if (flags & U_ORIGIN2) {
        uint16_t coord = htole16(update->origin2);
        write_out(&coord, 2);
    }
    if (flags & U_ANGLE2) {
        write_out(&update->angle2, 1);
    }
    if (flags & U_ORIGIN3) {
        uint16_t coord = htole16(update->origin3);
        write_out(&coord, 2);
    }
    if (flags & U_ANGLE3) {
        write_out(&update->angle3, 1);
    }
    if (flags & U_ALPHA) {
        write_out(&update->alpha, 1);
    }
    if (flags & U_SCALE) {
        write_out(&update->effects, 1);
    }
    if (flags & U_FRAME2) {
        uint8_t frame2 = (update->frame >> 8) & 0xff;
        write_out(&frame2, 1);
    }
    if (flags & U_MODEL2) {
        uint8_t model2 = (update->model_num >> 8) & 0xff;
        write_out(&model2, 1);
    }
    if (flags & U_LERPFINISH) {
        write_out(&update->lerp_finish, 1);
    }
}


// Loop through the messages, apply deltas, and emit demo file.
static void
dec_write_messages(void)
{
    void *msg;
    uint8_t cmd;
    int msg_len;
    buf_msg_iter_t it;

    buf_iter_messages(&it);
    buf_next_message(&it, &msg, &msg_len);
    while (msg != NULL) {
        cmd = *(uint8_t *)msg;
        if (cmd == TP_MSG_TYPE_UPDATE_INITIAL
                || cmd == TP_MSG_TYPE_UPDATE_DELTA) {
            // Turn the update_t into an .dem update message.
            uint16_t entity_num;
            update_t update;
            memcpy(&update, msg + 3, sizeof(update_t));
            memcpy(&entity_num, msg + 1, sizeof(short));
            entity_num = le16toh(entity_num);
            if (cmd == TP_MSG_TYPE_UPDATE_DELTA) {
                dec_apply_update_delta(&updates[entity_num], &update,
                                       &updates[entity_num]);
            } else {
                memcpy(&updates[entity_num], &update, sizeof(update_t));
            }
            dec_emit_update(entity_num);
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            // Write the command out verbatim.
            write_out(msg, msg_len);
        } else if (cmd == TP_MSG_TYPE_PACKET_HEADER) {
            // Skip the command but otherwise write the header out verbatim.
            write_out(msg + 1, msg_len - 1);
        } else {
            assert(!"invalid message type");    // should be checked on entry
        }
        buf_next_message(&it, &msg, &msg_len);
    }
}


tp_err_t
tp_decode (void)
{
    tp_err_t rc;

    rc = dec_read_cd_string();
    while (rc == TP_ERR_SUCCESS) {
        rc = dec_read_buffer();
        if (buf_is_empty()) {
            break;
        }
        if (rc == TP_ERR_SUCCESS) {
            dec_write_messages();
            buf_clear();
        }

        // TODO: break on disconnect
    }

    return rc;
}
