#include <assert.h>
#include <endian.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "buf.h"
#include "main.h"
#include "transpose.h"
#include "quakedef.h"


static update_t updates[TP_MAX_ENT];


// Last client_data parsed
static client_data_t last_client_data = {.flags = TP_SU_INVALID};
static timemsg_t last_time = {.time = TP_TIME_INVALID};
static packet_header_t last_packet_header =
    {.packet_len = TP_PACKET_LEN_INVALID};


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
dec_apply_client_data_delta (client_data_t *initial, client_data_t *delta,
                             client_data_t *out)
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


static void
dec_apply_time_delta (timemsg_t *initial, timemsg_t *delta, timemsg_t *out)
{
    APPLY_DELTA_FIELD(time);
}


static void
dec_apply_packet_header_delta (packet_header_t *initial, packet_header_t *delta,
                               packet_header_t *out)
{
    APPLY_DELTA_FIELD(packet_len);
    APPLY_DELTA_FIELD(angle1);
    APPLY_DELTA_FIELD(angle2);
    APPLY_DELTA_FIELD(angle3);
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
dec_read_update_field (int offs, int size, bool delta)
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


// Generic linked list struct read func
static tp_err_t
dec_read_field(void **list, int offs, int size)
{
    tp_err_t rc;

    while (list != NULL) {
        rc = read_in(((uint8_t *)list) + offs, size);
        if (rc != TP_ERR_SUCCESS) {
            return rc;
        }
        list = *list;
    }

    return TP_ERR_SUCCESS;
}

static tp_err_t
dec_read_buffer (void)
{
    int i;
    bool delta;
    tp_err_t rc;
    client_data_t *client_datas;
    packet_header_t *packet_headers;

    // Read in the messages.  Update structs will exist but won't be
    // populated.
    rc = buf_read_messages();
    if (rc != TP_ERR_SUCCESS) {
        return rc;
    }

    // Read in update objects (initial valuesfirst, then deltas).
#define READ_UPDATE_FIELD(x, y)                                   \
    do {                                                          \
        rc = dec_read_update_field(offsetof(update_t, x),         \
                                   sizeof(((update_t *)NULL)->x), \
                                   (y));                          \
        if (rc != TP_ERR_SUCCESS) {                               \
            return rc;                                            \
        }                                                         \
    } while(0)

    for (i = 0; i < 2; i++) {
        delta = (i == 1);
        READ_UPDATE_FIELD(model_num, delta);
        READ_UPDATE_FIELD(origin1, delta);
        READ_UPDATE_FIELD(origin2, delta);
        READ_UPDATE_FIELD(origin3, delta);
        READ_UPDATE_FIELD(angle1, delta);
        READ_UPDATE_FIELD(angle2, delta);
        READ_UPDATE_FIELD(angle3, delta);
        READ_UPDATE_FIELD(frame, delta);
        READ_UPDATE_FIELD(color_map, delta);
        READ_UPDATE_FIELD(skin, delta);
        READ_UPDATE_FIELD(effects, delta);
        READ_UPDATE_FIELD(alpha, delta);
        READ_UPDATE_FIELD(scale, delta);
        READ_UPDATE_FIELD(lerp_finish, delta);
        READ_UPDATE_FIELD(flags, delta);
        // TODO: endian swap?
    }
#undef READ_UPDATE_FIELDS

#define READ_FIELD(list, type, x)                       \
    do {                                                \
        rc = dec_read_field((void **)list,              \
                            offsetof(type, x),          \
                            sizeof(((type *)NULL)->x)); \
        if (rc != TP_ERR_SUCCESS) {                     \
            return rc;                                  \
        }                                               \
        fprintf(stderr, "%s: %ld\n", #x, input_pos());  \
    } while(0)
#define READ_CLIENT_DATA_FIELD(x) READ_FIELD(client_datas, client_data_t, x)
    client_datas = buf_get_client_data_list();
    READ_CLIENT_DATA_FIELD(view_height);
    READ_CLIENT_DATA_FIELD(ideal_pitch);
    READ_CLIENT_DATA_FIELD(punch1);
    READ_CLIENT_DATA_FIELD(punch2);
    READ_CLIENT_DATA_FIELD(punch3);
    READ_CLIENT_DATA_FIELD(vel1);
    READ_CLIENT_DATA_FIELD(vel2);
    READ_CLIENT_DATA_FIELD(vel3);
    READ_CLIENT_DATA_FIELD(items);
    READ_CLIENT_DATA_FIELD(weapon_frame);
    READ_CLIENT_DATA_FIELD(armor);
    READ_CLIENT_DATA_FIELD(weapon);
    READ_CLIENT_DATA_FIELD(health);
    READ_CLIENT_DATA_FIELD(ammo);
    READ_CLIENT_DATA_FIELD(shells);
    READ_CLIENT_DATA_FIELD(nails);
    READ_CLIENT_DATA_FIELD(rockets);
    READ_CLIENT_DATA_FIELD(cells);
    READ_CLIENT_DATA_FIELD(active_weapon);
    READ_CLIENT_DATA_FIELD(weapon_alpha);
    READ_CLIENT_DATA_FIELD(flags);
#undef READ_CLIENT_DATA_FIELD

    READ_FIELD(buf_get_time_list(), timemsg_t, time);

    packet_headers = buf_get_packet_header_list();
    READ_FIELD(packet_headers, packet_header_t, packet_len);
    READ_FIELD(packet_headers, packet_header_t, angle1);
    READ_FIELD(packet_headers, packet_header_t, angle2);
    READ_FIELD(packet_headers, packet_header_t, angle3);

#undef READ_FIELD

    return TP_ERR_SUCCESS;
}


#define TP_EMIT_FIELD(field, type, shift)                              \
    do {                                                               \
        type##_t __t;                                                  \
        __t = (s->field >> shift) & ((1LL << (sizeof(__t) * 8)) - 1);  \
        write_out(&__t, sizeof(__t));                                  \
    } while (false)


#define TP_EMIT_FIELD_CONDITIONAL(flag, field, type, shift)  \
    do {                                                     \
        if (s->flags & (flag)) {                             \
            TP_EMIT_FIELD(field, type, shift);               \
        }                                                    \
    } while (false)


static void
dec_emit_update (int entity_num)
{
    uint8_t cmd;
    update_t *s = &updates[entity_num];

    cmd = 0x80 | (s->flags & 0x7f);
    write_out(&cmd, 1);

    TP_EMIT_FIELD_CONDITIONAL(U_MOREBITS, flags, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(U_EXTEND1, flags, uint8, 16);
    TP_EMIT_FIELD_CONDITIONAL(U_EXTEND2, flags, uint8, 24);

    if (s->flags & U_LONGENTITY) {
        uint16_t entity_num_s = htole16(entity_num);
        write_out(&entity_num_s, 2);
    } else {
        uint8_t entity_num_b = entity_num & 0xff;
        write_out(&entity_num_b, 1);
    }

    TP_EMIT_FIELD_CONDITIONAL(U_MODEL, model_num, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_FRAME, frame, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_COLORMAP, color_map, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_SKIN, skin, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_EFFECTS, effects, uint8, 0);

    TP_EMIT_FIELD_CONDITIONAL(U_ORIGIN1, origin1, uint16, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_ANGLE1, angle1, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_ORIGIN2, origin2, uint16, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_ANGLE2, angle2, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_ORIGIN3, origin3, uint16, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_ANGLE3, angle3, uint8, 0);

    TP_EMIT_FIELD_CONDITIONAL(U_ALPHA, alpha, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(U_SCALE, scale, uint8, 0);

    TP_EMIT_FIELD_CONDITIONAL(U_FRAME2, frame, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(U_MODEL2, model_num, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(U_LERPFINISH, lerp_finish, uint8, 0);
}


static void
dec_emit_client_data (void)
{
    uint8_t cmd = svc_clientdata;

    client_data_t *s = &last_client_data;

    write_out(&cmd, 1);

    TP_EMIT_FIELD(flags, uint16, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_EXTEND1, flags, uint8, 16);
    TP_EMIT_FIELD_CONDITIONAL(SU_EXTEND2, flags, uint8, 24);

    TP_EMIT_FIELD_CONDITIONAL(SU_VIEWHEIGHT, view_height, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_IDEALPITCH, ideal_pitch, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_PUNCH1, punch1, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_VELOCITY1, vel1, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_PUNCH2, punch2, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_VELOCITY2, vel2, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_PUNCH3, punch3, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_VELOCITY3, vel3, uint8, 0);

    TP_EMIT_FIELD(items, uint32, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_WEAPONFRAME, weapon_frame, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_ARMOR, armor, uint8, 0);
    TP_EMIT_FIELD_CONDITIONAL(SU_WEAPON, weapon, uint8, 0);
    TP_EMIT_FIELD(health, uint16, 0);
    TP_EMIT_FIELD(ammo, uint8, 0);
    TP_EMIT_FIELD(shells, uint8, 0);
    TP_EMIT_FIELD(nails, uint8, 0);
    TP_EMIT_FIELD(rockets, uint8, 0);
    TP_EMIT_FIELD(cells, uint8, 0);
    TP_EMIT_FIELD(active_weapon, uint8, 0);

    TP_EMIT_FIELD_CONDITIONAL(SU_WEAPON2, weapon, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_ARMOR2, armor, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_AMMO2, ammo, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_SHELLS2, shells, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_NAILS2, nails, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_ROCKETS2, rockets, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_CELLS2, cells, uint8, 8);
    TP_EMIT_FIELD_CONDITIONAL(SU_WEAPONALPHA, weapon_alpha, uint8, 0);
}


static void
dec_emit_time (void)
{
    uint8_t cmd = svc_time;
    timemsg_t *s = &last_time;

    write_out(&cmd, 1);
    TP_EMIT_FIELD(time, uint32, 0);
}


static void
dec_emit_packet_header (void)
{
    packet_header_t *s = &last_packet_header;

    TP_EMIT_FIELD(packet_len, uint32, 0);
    TP_EMIT_FIELD(angle1, uint32, 0);
    TP_EMIT_FIELD(angle2, uint32, 0);
    TP_EMIT_FIELD(angle3, uint32, 0);
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
        } else if (cmd == svc_clientdata) {
            client_data_t client_data;
            memcpy(&client_data, msg + 1, sizeof(client_data_t));
            if (last_client_data.flags != TP_SU_INVALID) {
                // This is a delta.
                dec_apply_client_data_delta(&last_client_data, &client_data,
                                            &last_client_data);
            } else {
                // This is the initial value.
                memcpy(&last_client_data, &client_data, sizeof(client_data_t));
            }
            dec_emit_client_data();
        } else if (cmd == svc_time) {
            timemsg_t time;
            memcpy(&time, msg + 1, sizeof(timemsg_t));
            if (last_time.time != TP_TIME_INVALID) {
                dec_apply_time_delta(&last_time, &time, &last_time);
            } else {
                memcpy(&last_time, &time, sizeof(timemsg_t));
            }
            dec_emit_time();
        } else if (cmd == TP_MSG_TYPE_PACKET_HEADER) {
            packet_header_t packet_header;
            memcpy(&packet_header, msg + 1, sizeof(packet_header_t));
            if (last_packet_header.packet_len != TP_PACKET_LEN_INVALID) {
                dec_apply_packet_header_delta(&last_packet_header,
                                              &packet_header,
                                              &last_packet_header);
            } else {
                memcpy(&last_packet_header, &packet_header,
                       sizeof(packet_header_t));
            }
            dec_emit_packet_header();
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            // Write the command out verbatim.
            write_out(msg, msg_len);
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
