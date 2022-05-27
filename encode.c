#include <assert.h>
#include <endian.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "buf.h"
#include "main.h"
#include "msglen.h"
#include "transpose.h"
#include "quakedef.h"


#define ENC_MAX_PACKET_SIZE     128000


static update_t baselines[TP_MAX_ENT];

// `in_last_packet[i]` is true iff entity `i` was in the last packet.
static bool in_last_packet[TP_MAX_ENT];

// Updates from the previous packet that contained updates.
static update_t last_updates[TP_MAX_ENT];

// Last client_data received
static client_data_t last_client_data = {.flags = TP_SU_INVALID};

// `in_packet[i]` is tue iff entity `i` is in this packet.
static bool in_packet[TP_MAX_ENT];

// Updates from this packet.
static update_t updates[TP_MAX_ENT];

// How an svc_disconnect message been parsed?
static bool disconnected = false;


#define DIFF_FIELD(f)     out_diff->f = (b->f - a->f)

static void
enc_diff_update (update_t *a, update_t *b, update_t *out_diff)
{
    DIFF_FIELD(model_num);
    DIFF_FIELD(origin1);
    DIFF_FIELD(origin2);
    DIFF_FIELD(origin3);
    DIFF_FIELD(angle1);
    DIFF_FIELD(angle2);
    DIFF_FIELD(angle3);
    DIFF_FIELD(frame);
    DIFF_FIELD(color_map);
    DIFF_FIELD(skin);
    DIFF_FIELD(effects);
    DIFF_FIELD(alpha);
    DIFF_FIELD(scale);
    DIFF_FIELD(lerp_finish);
    DIFF_FIELD(flags);
}


static void
enc_diff_client_data (client_data_t *a, client_data_t *b,
                      client_data_t *out_diff)
{
    DIFF_FIELD(view_height);
    DIFF_FIELD(ideal_pitch);
    DIFF_FIELD(punch1);
    DIFF_FIELD(punch2);
    DIFF_FIELD(punch3);
    DIFF_FIELD(vel1);
    DIFF_FIELD(vel2);
    DIFF_FIELD(vel3);
    DIFF_FIELD(items);
    DIFF_FIELD(weapon_frame);
    DIFF_FIELD(armor);
    DIFF_FIELD(weapon);
    DIFF_FIELD(health);
    DIFF_FIELD(ammo);
    DIFF_FIELD(shells);
    DIFF_FIELD(nails);
    DIFF_FIELD(rockets);
    DIFF_FIELD(cells);
    DIFF_FIELD(active_weapon);
    DIFF_FIELD(weapon_alpha);
    DIFF_FIELD(flags);
}


static tp_err_t
enc_read_cd_string (void)
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


#define CHECK_RC(expr)                  \
    do {                                \
        tp_err_t __rc;                  \
        __rc = (expr);                  \
        if (__rc != TP_ERR_SUCCESS) {   \
            return __rc;                \
        }                               \
    } while(false);


static tp_err_t
enc_read_uint32 (void **buf, void *buf_end, uint32_t *out)
{
    uint32_t s;

    if (*buf + 4 > buf_end) {
        return TP_ERR_NOT_ENOUGH_INPUT;
    }
    memcpy(&s, *buf, 4);
    *buf += 4;
    *out = le32toh(s);

    return TP_ERR_SUCCESS;
}


static tp_err_t
enc_read_uint16 (void **buf, void *buf_end, uint16_t *out)
{
    uint16_t s;

    if (*buf + 2 > buf_end) {
        return TP_ERR_NOT_ENOUGH_INPUT;
    }
    memcpy(&s, *buf, 2);
    *buf += 2;
    *out = le16toh(s);

    return TP_ERR_SUCCESS;
}


static tp_err_t
enc_read_uint8 (void **buf, void *buf_end, uint8_t *out)
{
    uint8_t b;

    if (*buf + 1 > buf_end) {
        return TP_ERR_NOT_ENOUGH_INPUT;
    }
    memcpy(&b, *buf, 1);
    *buf += 1;
    *out = b;

    return TP_ERR_SUCCESS;
}


#define TP_READ(field, type, shift)                            \
    do {                                                       \
        type##_t __t;                                          \
        CHECK_RC(enc_read_##type(&buf, buf_end, &__t));        \
        s.field |= __t << shift;                               \
    } while(false)


#define TP_READ_CONDITIONAL(flag, field, type, shift)              \
    do {                                                           \
        if (s.flags & (flag)) {                                    \
            type##_t __t;                                          \
            CHECK_RC(enc_read_##type(&buf, buf_end, &__t));        \
            if (sizeof(s.field) <= sizeof(__t)) {                  \
                assert(sizeof(s.field) == sizeof(__t));            \
                assert(shift == 0);                                \
                s.field = __t;                                     \
            } else {                                               \
                typeof(s.field) __mask = 1;                        \
                __mask = (__mask << (sizeof(__t) * 8)) - 1;        \
                __mask = __mask << shift;                          \
                __mask = ~__mask;                                  \
                s.field &= __mask;                                 \
                s.field |= (__t << shift);                         \
            }                                                      \
        }                                                          \
    } while(false)


static tp_err_t
enc_parse_update(void *buf, void *buf_end, update_t *baselines, int *out_len,
                 update_t *out_update, int *out_entity_num)
{
    void *start_buf = buf;
    uint32_t flags;
    int entity_num;
    update_t s = {.flags = 0};

    TP_READ(flags, uint8, 0);
    s.flags &= 0x7f;
    TP_READ_CONDITIONAL(U_MOREBITS, flags, uint8, 8);
    TP_READ_CONDITIONAL(U_EXTEND1, flags, uint8, 16);
    TP_READ_CONDITIONAL(U_EXTEND2, flags, uint8, 24);

    if (s.flags & U_LONGENTITY) {
        uint16_t entity_num_s;
        CHECK_RC(enc_read_uint16(&buf, buf_end, &entity_num_s));
        entity_num = entity_num_s;
    } else {
        uint8_t entity_num_b;
        CHECK_RC(enc_read_uint8(&buf, buf_end, &entity_num_b));
        entity_num = entity_num_b;
    }
    flags = s.flags;
    memcpy(&s, &baselines[entity_num], sizeof(update_t));
    s.flags = flags;

    TP_READ_CONDITIONAL(U_MODEL, model_num, uint8, 0);
    TP_READ_CONDITIONAL(U_FRAME, frame, uint8, 0);
    TP_READ_CONDITIONAL(U_COLORMAP, color_map, uint8, 0);
    TP_READ_CONDITIONAL(U_SKIN, skin, uint8, 0);
    TP_READ_CONDITIONAL(U_EFFECTS, effects, uint8, 0);
    TP_READ_CONDITIONAL(U_ORIGIN1, origin1, uint16, 0);
    TP_READ_CONDITIONAL(U_ANGLE1, angle1, uint8, 0);
    TP_READ_CONDITIONAL(U_ORIGIN2, origin2, uint16, 0);
    TP_READ_CONDITIONAL(U_ANGLE2, angle2, uint8, 0);
    TP_READ_CONDITIONAL(U_ORIGIN3, origin3, uint16, 0);
    TP_READ_CONDITIONAL(U_ANGLE3, angle3, uint8, 0);

    TP_READ_CONDITIONAL(U_ALPHA, alpha, uint8, 0);
    TP_READ_CONDITIONAL(U_SCALE, scale, uint8, 0);
    TP_READ_CONDITIONAL(U_FRAME2, frame, uint8, 8);
    TP_READ_CONDITIONAL(U_MODEL2, model_num, uint8, 8);
    TP_READ_CONDITIONAL(U_LERPFINISH, lerp_finish, uint8, 0);

    memcpy(out_update, &s, sizeof(update_t));
    *out_entity_num = entity_num;
    *out_len = buf - start_buf;

    return TP_ERR_SUCCESS;
}


static tp_err_t
enc_parse_client_data (void *buf, void *buf_end, int *out_len,
                       client_data_t *out_client_data)
{
    void *start_buf = buf;
    client_data_t s;

    memset(&s, 0, sizeof(client_data_t));

    buf++;  // skip command

    TP_READ(flags, uint16, 0);
    TP_READ_CONDITIONAL(SU_EXTEND1, flags, uint8, 16);
    TP_READ_CONDITIONAL(SU_EXTEND2, flags, uint8, 24);

    TP_READ_CONDITIONAL(SU_VIEWHEIGHT, view_height, uint8, 0);
    TP_READ_CONDITIONAL(SU_IDEALPITCH, ideal_pitch, uint8, 0);
    TP_READ_CONDITIONAL(SU_PUNCH1, punch1, uint8, 0);
    TP_READ_CONDITIONAL(SU_VELOCITY1, vel1, uint8, 0);
    TP_READ_CONDITIONAL(SU_PUNCH2, punch2, uint8, 0);
    TP_READ_CONDITIONAL(SU_VELOCITY2, vel2, uint8, 0);
    TP_READ_CONDITIONAL(SU_PUNCH3, punch3, uint8, 0);
    TP_READ_CONDITIONAL(SU_VELOCITY3, vel3, uint8, 0);

    TP_READ(items, uint32, 0);
    TP_READ_CONDITIONAL(SU_WEAPONFRAME, weapon_frame, uint8, 0);
    TP_READ_CONDITIONAL(SU_ARMOR, armor, uint8, 0);
    TP_READ_CONDITIONAL(SU_WEAPON, weapon, uint8, 0);
    TP_READ(health, uint16, 0);
    TP_READ(ammo, uint8, 0);
    TP_READ(shells, uint8, 0);
    TP_READ(nails, uint8, 0);
    TP_READ(rockets, uint8, 0);
    TP_READ(cells, uint8, 0);
    TP_READ(active_weapon, uint8, 0);

    TP_READ_CONDITIONAL(SU_WEAPON2, weapon, uint8, 8);
    TP_READ_CONDITIONAL(SU_ARMOR2, armor, uint8, 8);
    TP_READ_CONDITIONAL(SU_AMMO2, ammo, uint8, 8);
    TP_READ_CONDITIONAL(SU_SHELLS2, shells, uint8, 8);
    TP_READ_CONDITIONAL(SU_NAILS2, nails, uint8, 8);
    TP_READ_CONDITIONAL(SU_ROCKETS2, rockets, uint8, 8);
    TP_READ_CONDITIONAL(SU_CELLS2, cells, uint8, 8);
    TP_READ_CONDITIONAL(SU_WEAPONALPHA, weapon_alpha, uint8, 0);

    memcpy(out_client_data, &s, sizeof(client_data_t));
    *out_len = buf - start_buf;

    return TP_ERR_SUCCESS;
}


static tp_err_t
enc_parse_baseline (void *buf, void *buf_end, int version, int *out_len)
{
    void *buf_start = buf;
    uint16_t entity_num;
    update_t s;

    memset(&s, 0, sizeof(update_t));
    buf++;

    CHECK_RC(enc_read_uint16(&buf, buf_end, &entity_num));
    if (entity_num >= TP_MAX_ENT) {
        return TP_ERR_INVALID_ENTITY_NUM;
    }

    if (version == 2) {
        TP_READ(flags, uint8, 0);
    }

    TP_READ(model_num, uint8, 0);
    TP_READ_CONDITIONAL(B_LARGEMODEL, model_num, uint8, 8);
    TP_READ(frame, uint8, 0);
    TP_READ_CONDITIONAL(B_LARGEFRAME, frame, uint8, 8);
    TP_READ(color_map, uint8, 0);
    TP_READ(skin, uint8, 0);
    TP_READ(origin1, uint16, 0);
    TP_READ(angle1, uint8, 0);
    TP_READ(origin2, uint16, 0);
    TP_READ(angle2, uint8, 0);
    TP_READ(origin3, uint16, 0);
    TP_READ(angle3, uint8, 0);
    TP_READ_CONDITIONAL(B_ALPHA, alpha, uint8, 0);

    memcpy(&baselines[entity_num], &s, sizeof(update_t));
    *out_len = buf - buf_start;

    return TP_ERR_SUCCESS;
}


static tp_err_t
enc_read_packet_header (uint8_t *out_header, uint32_t *out_packet_len,
                        void *out_packet)
{
    tp_err_t rc;
    uint32_t packet_len;

    rc = read_in(out_header, 16);
    if (rc != TP_ERR_SUCCESS) {
        return rc;
    }

    memcpy(&packet_len, out_header, sizeof(packet_len));
    packet_len = le32toh(packet_len);
    if (packet_len > ENC_MAX_PACKET_SIZE) {
        return TP_ERR_PACKET_TOO_LARGE;
    }

    rc = read_in(out_packet, packet_len);
    if (rc != TP_ERR_SUCCESS) {
        return rc;
    }
    *out_packet_len = packet_len;

    return TP_ERR_SUCCESS;
}


static void
enc_flush_update_field(int offs, int size, bool delta)
{
    buf_update_iter_t it;
    update_t *update;

    buf_iter_updates(&it, delta);
    buf_next_update(&it, &update);
    while (update != NULL) {
        write_out(((uint8_t *)update) + offs, size);
        buf_next_update(&it, &update);
    }
}


// Generic linked list struct flush func
static void
enc_flush_field(void **list, int offs, int size)
{
    while (list != NULL) {
        write_out(((uint8_t *)list) + offs, size);
        list = *list;
    }
}


// Write the contents of the buffer out to disk.
static void
enc_flush(void)
{
    int i;
    bool delta;
    client_data_t *client_datas;

    // Write out the messages.
    buf_write_messages();


#define DUMP_OFFSET_INFO(type, field, delta)                        \
    fprintf(stderr,                                                 \
            "{\"field\": \"%s\", \"offs\": %ld, \"delta\": %d, "    \
            "\"type\": \"%s\"},\n",                                 \
            #field, output_pos(), (delta), #type)                   \

    // Write out updates (initial values first, then deltas).
    //
#define FLUSH_UPDATE_FIELD(field, delta)                            \
    do {                                                            \
        enc_flush_update_field(offsetof(update_t, field),           \
                               sizeof(((update_t *)NULL)->field),   \
                               (delta));                            \
        DUMP_OFFSET_INFO(update, field, delta);                     \
    } while (false)
    for (i = 0; i < 2; i++) {
        delta = (i == 1);
        FLUSH_UPDATE_FIELD(model_num, delta);
        FLUSH_UPDATE_FIELD(origin1, delta);
        FLUSH_UPDATE_FIELD(origin2, delta);
        FLUSH_UPDATE_FIELD(origin3, delta);
        FLUSH_UPDATE_FIELD(angle1, delta);
        FLUSH_UPDATE_FIELD(angle2, delta);
        FLUSH_UPDATE_FIELD(angle3, delta);
        FLUSH_UPDATE_FIELD(frame, delta);
        FLUSH_UPDATE_FIELD(color_map, delta);
        FLUSH_UPDATE_FIELD(skin, delta);
        FLUSH_UPDATE_FIELD(effects, delta);
        FLUSH_UPDATE_FIELD(alpha, delta);
        FLUSH_UPDATE_FIELD(scale, delta);
        FLUSH_UPDATE_FIELD(lerp_finish, delta);
        FLUSH_UPDATE_FIELD(flags, delta);
    }
#undef FLUSH_UPDATE_FIELD

    // Write out client datas.
#define FLUSH_CLIENT_DATA_FIELD(field)                            \
    do {                                                          \
        enc_flush_field((void **)client_datas,                    \
                        offsetof(client_data_t, field),           \
                        sizeof(((client_data_t *)NULL)->field));  \
        DUMP_OFFSET_INFO(client_data, field, 1);                  \
    } while (false)

    client_datas = buf_get_client_data_list();
    FLUSH_CLIENT_DATA_FIELD(view_height);
    FLUSH_CLIENT_DATA_FIELD(ideal_pitch);
    FLUSH_CLIENT_DATA_FIELD(punch1);
    FLUSH_CLIENT_DATA_FIELD(punch2);
    FLUSH_CLIENT_DATA_FIELD(punch3);
    FLUSH_CLIENT_DATA_FIELD(vel1);
    FLUSH_CLIENT_DATA_FIELD(vel2);
    FLUSH_CLIENT_DATA_FIELD(vel3);
    FLUSH_CLIENT_DATA_FIELD(items);
    FLUSH_CLIENT_DATA_FIELD(weapon_frame);
    FLUSH_CLIENT_DATA_FIELD(armor);
    FLUSH_CLIENT_DATA_FIELD(weapon);
    FLUSH_CLIENT_DATA_FIELD(health);
    FLUSH_CLIENT_DATA_FIELD(ammo);
    FLUSH_CLIENT_DATA_FIELD(shells);
    FLUSH_CLIENT_DATA_FIELD(nails);
    FLUSH_CLIENT_DATA_FIELD(rockets);
    FLUSH_CLIENT_DATA_FIELD(cells);
    FLUSH_CLIENT_DATA_FIELD(active_weapon);
    FLUSH_CLIENT_DATA_FIELD(weapon_alpha);
    FLUSH_CLIENT_DATA_FIELD(flags);
#undef FLUSH_CLIENT_DATA_FIELD

#undef DUMP_OFFSET_INFO

    buf_clear();
}


static tp_err_t
enc_compress_message(void *buf, void *buf_end, void **out_buf,
                     bool *out_has_update)
{
    tp_err_t rc = TP_ERR_SUCCESS;
    uint8_t cmd;
    int msg_len;
    int entity_num;
    update_t update;
    bool delta;
    bool has_update = *out_has_update;

    if (buf + 1 > buf_end) {
        rc = TP_ERR_NOT_ENOUGH_INPUT;
    }

    if (rc == TP_ERR_SUCCESS) {
        cmd = *(uint8_t *)buf;
        if (cmd & 0x80) {
            // Updates are parsed, stubs are written, and the updates themselves
            // are added to the update lists ready to be transposed.
            has_update = true;
            rc = enc_parse_update(buf, buf_end, baselines, &msg_len, &update,
                                  &entity_num);
            if (rc == TP_ERR_SUCCESS) {
                memcpy(&updates[entity_num], &update, sizeof(update_t));
                in_packet[entity_num] = true;
                delta = in_last_packet[entity_num];
                if (delta) {
                    enc_diff_update(&last_updates[entity_num], &update,
                                    &update);
                }

                rc = buf_add_update(&update, entity_num, delta);
                if (rc == TP_ERR_BUFFER_FULL) {
                    enc_flush();
                    rc = buf_add_update(&update, entity_num, delta);
                }
            }
            if (rc == TP_ERR_SUCCESS) {
                buf += msg_len;
            }
        } else if (cmd == svc_clientdata) {
            client_data_t client_data;
            client_data_t client_data_to_add; // either a delta or absolute
            rc = enc_parse_client_data(buf, buf_end, &msg_len, &client_data);
            if (rc == TP_ERR_SUCCESS) {
                if (last_client_data.flags != TP_SU_INVALID) {
                    // Add a delta.
                    enc_diff_client_data(&last_client_data, &client_data,
                                         &client_data_to_add);
                } else {
                    // Add initial value.
                    memcpy(&client_data_to_add, &client_data,
                           sizeof(client_data_t));
                }
                memcpy(&last_client_data, &client_data, sizeof(client_data_t));

                rc = buf_add_client_data(&client_data_to_add);
                if (rc == TP_ERR_BUFFER_FULL) {
                    enc_flush();
                    rc = buf_add_client_data(&client_data_to_add);
                }
            }
            if (rc == TP_ERR_SUCCESS) {
                buf += msg_len;
            }
        } else if (cmd == svc_spawnbaseline || cmd == svc_spawnbaseline2) {
            // Baseline messages are parsed and then copied verbatim.
            rc = enc_parse_baseline(buf, buf_end,
                                    cmd == svc_spawnbaseline ? 1 : 2,
                                    &msg_len);
            if (rc == TP_ERR_SUCCESS) {
                rc = buf_add_message(buf, msg_len);
                if (rc == TP_ERR_BUFFER_FULL) {
                    enc_flush();
                    rc = buf_add_message(buf, msg_len);
                }
                buf += msg_len;
            }
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            // Any other message is copied verbatim.
            rc = msglen_get_length(buf, buf_end, &msg_len);
            if (rc == TP_ERR_SUCCESS) {
                rc = buf_add_message(buf, msg_len);
                if (rc == TP_ERR_BUFFER_FULL) {
                    enc_flush();
                    rc = buf_add_message(buf, msg_len);
                }
                buf += msg_len;
            }

            if (rc == TP_ERR_SUCCESS && cmd == svc_disconnect) {
                disconnected = true;
            }
        } else {
            return TP_ERR_INVALID_MSG_TYPE;
        }
    }

    if (rc == TP_ERR_SUCCESS) {
        *out_has_update = has_update;
        *out_buf = buf;
    }

    return rc;
}


tp_err_t
tp_encode (void)
{
    tp_err_t rc;
    uint8_t packet_header[16];
    uint8_t packet[ENC_MAX_PACKET_SIZE];
    uint32_t packet_len;
    void *ptr;
    void *packet_end;
    bool has_update;

    disconnected = false;
    memset(baselines, 0, sizeof(baselines));

    rc = enc_read_cd_string();

    while (rc == TP_ERR_SUCCESS && !disconnected) {
        rc = enc_read_packet_header(packet_header, &packet_len, packet);
        if (rc == TP_ERR_SUCCESS) {
            packet_end = packet + packet_len;
            rc = buf_add_packet_header(packet_header);
            if (rc == TP_ERR_BUFFER_FULL) {
                enc_flush();
                rc = buf_add_packet_header(packet_header);
            }
        }

        ptr = packet;
        memset(updates, 0, sizeof(updates));
        memset(in_packet, 0, sizeof(updates));
        has_update = false;
        while (rc == TP_ERR_SUCCESS && ptr < packet_end) {
            if (disconnected) {
                rc = TP_ERR_DISCONNECT_MID_PACKET;
            }

            if (rc == TP_ERR_SUCCESS) {
                rc = enc_compress_message(ptr, packet_end, &ptr,
                                          &has_update);
            }
        }
        if (rc == TP_ERR_SUCCESS && has_update) {
            memcpy(last_updates, updates, sizeof(last_updates));
            memcpy(in_last_packet, in_packet, sizeof(last_updates));
        }
    }

    if (rc == TP_ERR_SUCCESS && !buf_is_empty()) {
        enc_flush();
    }

    // TODO:  Print any remaining data verbatim

    return rc;
}
