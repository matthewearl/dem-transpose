#include <endian.h>

#include "transpose.h"


#define MAX_PACKET_SIZE     128000


static update_t baselines[MAX_ENT];

// `in_last_packet[i]` is true iff entity `i` was in the last packet.
static qboolean in_last_packet[MAX_ENT];

// Updates from the previous packet that contained updates.
static update_t last_updates[MAX_ENT];


// `in_packet[i]` is tue iff entity `i` is in this packet.
static qboolean in_packet[MAX_ENT];

// Updates from this packet.
static update_t updates[MAX_ENT];


// diff two updates
static void
enc_diff_update (update_t *update1, update_t *update2, update_t *out_diff)
{

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
        dp_err_t __rc;                  \
        __rc = (expr);                  \
        if (__rc != DP_ERR_SUCCESS) {   \
            return __rc;                \
        }                               \
    } while(false);


static tp_err_t
enc_read_uint16 (uint16_t *out)
{
    uint16_t s;

    CHECK_RC(read_in(&s, sizeof(uint16_t)));
    *out = le16toh(s);

    return TP_ERR_SUCCESS;
}


static tp_err_t
enc_parse_update(void *buf, void *buf_end, update_t *baselines, int *out_len,
                 update_t *out_update, int *out_entity_num)
{
    uint32_t flags;
    byte more_flags, extend1_flags, extend2_flags;
    unsigned int flags = base_flags;
    int entity_num;
    update_t update = {};

    flags = (*(uint8_t *)buf) & 0x7f;
    if (flags & U_MOREBITS) {
        CHECK_RC(read_in(&more_flags, 1));
        flags |= more_flags << 8;
    }
    if (flags & U_EXTEND1) {
        CHECK_RC(read_in(&extend1_flags, 1));
        flags |= extend1_flags << 16;
    }
    if (flags & U_EXTEND2) {
        CHECK_RC(read_in(&extend2_flags, 1));
        flags |= extend2_flags << 24;
    }
    update.flags = flags;

    if (flags & U_LONGENTITY) {
        uint16_t entity_num_s;
        CHECK_RC(read_uint16(&entity_num_s));
        entity_num = entity_num_s;
    } else {
        uint8_t entity_num_b;
        CHECK_RC(read_in(&entity_num_b, 1));
        entity_num = entity_num_b;
    }

    if (flags & U_MODEL) {
        uint8_t model_num_b;
        CHECK_RC(read_in(&model_num_b, 1));
        update.model_num = model_num_b;
    }

    if (flags & U_FRAME) {
        uint8_t frame_b;
        CHECK_RC(read_in(&frame_b, 1));
        update.frame = frame_b;
    }
    if (flags & U_COLORMAP) {
        uint8_t colormap_b;
        CHECK_RC(read_in(&colormap_b, 1));
        update.colormap = colormap_b;
    }
    if (flags & U_SKIN) {
        uint8_t skin_b;
        CHECK_RC(read_in(&skin_b, 1));
        update.skin = skin_b;
    }
    if (flags & U_EFFECTS) {
        uint8_t effects_b;
        CHECK_RC(read_in(&effects_b, 1));
        update.effects = effects_b;
    }
    if (flags & U_ORIGIN1) {
        uint16_t coord;
        CHECK_RC(read_uint16(&coord));
        update.origin1 = entity_num_s;
    }
    if (flags & U_ANGLE1) {
        uint8_t angle_b;
        CHECK_RC(read_in(&effects_b, 1));
        update.angle1 = angle_b;
    }
    if (flags & U_ORIGIN2) {
        uint16_t coord;
        CHECK_RC(read_uint16(&coord));
        update.origin2 = entity_num_s;
    }
    if (flags & U_ANGLE2) {
        uint8_t angle_b;
        CHECK_RC(read_in(&effects_b, 1));
        update.angle2 = angle_b;
    }
    if (flags & U_ORIGIN3) {
        uint16_t coord;
        CHECK_RC(read_uint16(&coord));
        update.origin3 = entity_num_s;
    }
    if (flags & U_ANGLE3) {
        uint8_t angle_b;
        CHECK_RC(read_in(&effects_b, 1));
        update.angle3 = angle_b;
    }
    // TODO: finish this off
}


static tp_err_t
enc_parse_baseline (void *buf, void *buf_end, int version, int *out_len,
                    update_t *out_baselines)
{
    // TODO: implement this.
}


static tp_err_t
enc_read_packet_header (uint8_t *out_header, uint32_t *out_packet_len,
                        void *out_packet)
{
    tp_err_t rc
    uint32_t packet_len;

    rc = read_in(out_header, 16);
    if (rc != TP_ERR_SUCCESS) {
        return rc;
    }

    packet_len = getlong(out_header);
    if (packet_len > MAX_PACKET_SIZE) {
        return TP_ERR_PACKET_TOO_LARGE;
    }

    rc = read_in(out_packet, packet_len);
    if (rc != TP_ERR_SUCCESS) {
        return rc;
    }
    *out_packet_len = packet_len;
}


static void
enc_flush_field(int offs, int size, bool delta)
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


// Write the contents of the buffer out to disk.
static void
enc_flush(void)
{
    int i;
    bool delta;
    int entity_num = -1;
    update_t *update;
    void *msg;
    int msg_len;
    buf_msg_iter_t it;

    // Write out the messages.
    buf_write_messages();

#define FLUSH_FIELD(x, (y))  \
    enc_flush_field(offsetof(update_t, x), sizeof(update->x), (y))

    // Write out the initial values (first iter) then the deltas (second iter).
    for (i = 0; i < 2; i++) {
        delta = (i == 1);
        FLUSH_FIELD(model_num, delta);
        FLUSH_FIELD(origin1, delta);
        FLUSH_FIELD(origin2, delta);
        FLUSH_FIELD(origin3, delta);
        FLUSH_FIELD(angle1, delta);
        FLUSH_FIELD(angle2, delta);
        FLUSH_FIELD(angle3, delta);
        FLUSH_FIELD(frame, delta);
        FLUSH_FIELD(color_map, delta);
        FLUSH_FIELD(skin, delta);
        FLUSH_FIELD(effects, delta);
        FLUSH_FIELD(alpha, delta);
        FLUSH_FIELD(scale, delta);
        FLUSH_FIELD(lerp_finish, delta);
        FLUSH_FIELD(flags, delta);
    }

    buf_clear();
}


static tp_err_t
enc_compress_message(void *buf, void *buf_end, void *out_buf,
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
            }
            if (rc == TP_ERR_SUCCESS) {
                rc = buf_add_update(update, entity_num, delta);
                if (rc == TP_ERR_BUFFER_FULL) {
                    rc = enc_flush();
                    if (rc == TP_ERR_SUCCESS) {
                        rc = buf_add_update(update, entity_num, delta);
                    }
                }
            }
            if (rc == TP_ERR_SUCCESS) {
                rc = buf_add_update_stub(entity_num);
                if (rc == TP_ERR_BUFFER_FULL) {
                    rc = enc_flush();
                    if (rc == TP_ERR_SUCCESS) {
                        rc = buf_add_update_stub(entity_num);
                    }
                }
            }
            if (rc == TP_ERR_SUCCESS) {
                buf += msg_len;
            }
        } else if (cmd == svc_spawnbaseline || cmd == svc_spawnbaseline2) {
            // Baseline messages are parsed and then copied verbatim.
            rc = enc_parse_baseline(buf, buf_end,
                                    cmd == svc_spawnbaseline ? 1 : 2,
                                    &msg_len, baselines);
            if (rc == TP_ERR_SUCCESS) {
                rc = buf_add_message(buf, msg_len);
                if (rc == TP_ERR_BUFFER_FULL) {
                    rc = enc_flush();
                    if (rc == TP_ERR_SUCCESS) {
                        rc = buf_add_message(buf, msg_len);
                    }
                }
                buf += msg_len:
            }
        } else if (cmd > 0 && cmd < TP_NUM_DEM_COMMANDS) {
            // Any other message is copied verbatim.
            rc = msglen_get_length(buf, buf_end, &msg_len);
            if (rc == TP_ERR_SUCCESS) {
                rc = buf_add_message(buf, msg_len);
                if (rc == TP_ERR_BUFFER_FULL) {
                    rc = enc_flush();
                    if (rc == TP_ERR_SUCCESS) {
                        rc = buf_add_message(buf, msg_len);
                    }
                }
                buf += msg_len:
            }
        } else {
            return TP_ERR_INVALID_MSG_TYPE;
        }
    }

    if (rc == TP_ERR_SUCCESS) {
        *out_has_update = has_update;
    }

    return rc;
}


tp_err_t
tp_encode (void)
{
    tp_err_t rc;
    uint8_t packet_header[16];
    uint8_t packet[MAX_PACKET_SIZE];
    uint32_t packet_len;
    uint8_t ptr;
    bool has_update;

    memset(baselines, 0, sizeof(baselines));

    rc = enc_read_cd_string();

    while (rc == TP_ERR_SUCCESS) {
        rc = enc_read_packet_header(packet_header, &packet_len, packet);
        if (rc == TP_ERR_SUCCESS) {
            rc = buf_add_packet_header(packet_header):
        }

        ptr = packet;
        memset(updates, 0, sizeof(updates));
        memset(in_packet, 0, sizeof(updates));
        has_update = false;
        while (rc == TP_ERR_SUCCESS && ptr < packet + packet_len) {
            rc = enc_compress_message(ptr, packet + packet_len, &ptr,
                                      &has_update);

            if (rc == TP_ERR_SUCCESS && has_update) {
                memcpy(last_updates, updates, sizeof(last_updates));
                memcpy(in_last_packet, in_packet, sizeof(last_updates));
            }
        }
    }

    return rc;
}
