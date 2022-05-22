#include "transpose.h"
#include "msglen.h"


static tp_err_t
msglen_sound(void *buf, void *buf_end, void **msg_end)
{
	uint8_t mask;

	if (buf >= buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	mask = *(uint8_t *)buf;
	buf ++;

	if (mask & SND_VOLUME) buf ++;
	if (mask & SND_ATTENUATION) buf++;
	if (mask & SND_LARGEENTITY) buf++; buf += 2;
	if (mask & SND_LARGESOUND) buf++; buf++;
	buf += 6; // coords

	if (buf > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	*msg_end = buf;
	return TP_ERR_SUCCESS;
}


static tp_err_t
msglen_string(void *buf, void *buf_end, void **msg_end)
{
	int len = 0;
	while (buf < buf_end && *(char *)buf) {
		len ++;
		buf ++;
	}
	len ++; // null terminator

	if (buf > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	*msg_end = buf;
	return TP_ERR_SUCCESS;
}


static tp_err_t
msglen_serverinfo(void *buf, void *buf_end, void **msg_end)
{
	tp_err_t rc = 0;
	int str_len;

	if (buf + 6 > buf_end) rc = 1;
	buf += 6;

	str_len = -1;
	while (rc == 0 && str_len != 0) {
		rc = msglen_string(buf, buf_end, &buf);
	}
	str_len = -1;
	while (rc == 0 && str_len != 0) {
		rc = msglen_string(buf, buf_end, &buf);
	}

	if (rc == 0) {
		*msg_end = buf;
	}

	return rc;
}


static tp_err_t
msglen_bytestring (void *buf, void *buf_end, void **msg_end)
{
	tp_err_t rc = 0;

	if (buf + 1 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	buf++;

	return msglen_string(buf, buf_end, msg_end);
}


const uint8_t te_size[] = {8, 8, 8, 8, 8, 16, 16, 8, 8, 16,
							8, 8, 10, 16, 8, 8, 14};
static tp_err_t
msglen_temp_entity (void *buf, void *buf_end, void **msg_end)
{
	uint8_t entity_type;

	if (buf + 1 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	entity_type = *(uint8_t *)buf;
	buf++;

	if (entity_type == 17)
	{
		// TODO:  add nehahra support
		fprintf(stderr, "error: nehahra temp ents not supported\n");
		return 1;
	}
	if (entity_type > 17) {
		fprintf(stderr, "error: unsupported entity type %d\n", entity_type);
		return 1;
	}
	buf += te_size[entity_type];

	if (buf > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	*msg_end = buf;
	return TP_ERR_SUCCESS;
}


static tp_err_t
msglen_clientdata (void *buf, void *buf_end, void **msg_end)
{
    uint8_t extend1_flags, extend2_flags;
	uint32_t flags;
	int skip;

	if (buf + 2 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	flags = *(uint16_t *)buf;
	buf += 2;

	if (flags & SU_EXTEND1) {
		if (buf + 1 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
		flags |= (*(uint8_t *)buf) << 16;
		buf++;
	}

	if (flags & SU_EXTEND2) {
		if (buf + 1 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
		flags |= (*(uint8_t *)buf) << 24;
		buf++;
	}

    skip = ((flags & SU_VIEWHEIGHT) != 0)
           + ((flags & SU_IDEALPITCH) != 0)
           + ((flags & SU_PUNCH1) != 0)
           + ((flags & SU_PUNCH2) != 0)
           + ((flags & SU_PUNCH3) != 0)
           + ((flags & SU_VELOCITY1) != 0)
           + ((flags & SU_VELOCITY2) != 0)
           + ((flags & SU_VELOCITY3) != 0)
           + 4  // items
           + ((flags & SU_WEAPONFRAME) != 0)
           + ((flags & SU_ARMOR) != 0)
           + ((flags & SU_WEAPON) != 0)
           + 2 + 6  // health,ammo,shells,nails,rockets,cells,active_weapon
           + ((flags & SU_WEAPON2) != 0)
           + ((flags & SU_ARMOR2) != 0)
           + ((flags & SU_AMMO2) != 0)
           + ((flags & SU_SHELLS2) != 0)
           + ((flags & SU_NAILS2) != 0)
           + ((flags & SU_ROCKETS2) != 0)
           + ((flags & SU_CELLS2) != 0)
           + ((flags & SU_WEAPONFRAME2) != 0)
           + ((flags & SU_WEAPONALPHA) != 0);

	buf += skip;

	if (buf > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	*msg_end = buf;
	return TP_ERR_SUCCESS;
}


static tp_err_t
msglen_spawnstatic (void *buf, void *buf_end, void **msg_end, int version)
{
	uchar bits = 0;

	if (version == 2) {
		if (buf + 1 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
		bits = *(uchar *)buf;
		buf ++;
	}

	if (bits & B_LARGEMODEL) buf ++;
	buf ++; // model num
	if (bits & B_LARGEFRAME) buf ++;
	buf ++; // frame
	buf += 2; // colormap, skin
	buf += 9; // coords / angle
	if (bits & B_ALPHA) buf ++;

	if (buf > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	*msg_end = buf;
	return TP_ERR_SUCCESS;
}


static tp_err_t
msglen_spawnstatic_v1 (void *buf, void *buf_end, void **msg_end)
{
	return msglen_spawnstatic(buf, buf_end, msg_end, 1);
}


static tp_err_t
msglen_spawnstatic_v2 (void *buf, void *buf_end, void **msg_end)
{
	return msglen_spawnstatic(buf, buf_end, msg_end, 2);
}


static tp_err_t
msglen_spawnbaseline_v1 (void *buf, void *buf_end, void **msg_end)
{
	if (buf + 2 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	buf += 2; // entity num
	return msglen_spawnstatic(buf, buf_end, msg_end, 1);
}


static tp_err_t
msglen_spawnbaseline_v2 (void *buf, void *buf_end, void **msg_end)
{
	if (buf + 2 > buf_end) return TP_ERR_NOT_ENOUGH_INPUT;
	buf += 2; // entity num
	return msglen_spawnstatic(buf, buf_end, msg_end, 2);
}


static int (* const msglen_message[])(void) = {
	NULL, NULL, NULL, NULL,
	NULL, msglen_sound, NULL, msglen_string,
	msglen_string,	NULL, msglen_serverinfo, msglen_bytestring,
	msglen_bytestring,	NULL, msglen_clientdata, NULL,
	NULL, NULL, NULL, NULL,
	NULL, msglen_spawnbaseline, msglen_temp_entity, NULL,
	NULL, msglen_string, NULL, NULL,
	NULL, NULL, msglen_string, NULL,
	NULL, msglen_string,
	NULL, NULL, NULL,	/* nehahra */
	NULL, NULL, NULL, NULL, msglen_spawnbaseline2, msglen_spawnstatic2,
	NULL
};


/* 2.10: removed all the dem_* functions that just did
	copy_msg(#) and replaced with this */
static const uchar msglen_copy[] = {
	1, 1, 6, 5, 3, 0, 5, 0, 0, 4, 0, 0, 0, 4, 0, 3,
	3,12, 9,14, 1, 0, 0, 2, 2, 0, 1, 1,10, 1, 0, 3,
	1, 0, 0, 0, 0,
	0, 0, 0, 6, 0, 0, 11
};


// Read a message at `msg`.  The message can be of any type, except for
// update.  Returns non-zero if the true length exceeds the buffer length.
int
msglen_get_length (void *buf, void *buf_end, int out_len)
{
	tp_err_t rc = 0;
	void *buf_start = buf;
	uchar cmd;

	if (buf >= buf_end) {
		return TP_ERR_SUCCESS;
	}

	cmd = *(uchar *)buf;
	buf++;
	if (cmd > 0 && cmd <= TP_NUM_DEM_COMMANDS) {
		if (msglen_copy[cmd - 1]) {
			len = msglen_copy[cmd - 1];
			buf += len - 1;
		} else if (msglen_messages[cmd - 1] != NULL) {
			rc = msglen_messages[cmd - 1](buf, buf_end, &buf);
		} else {
			fprintf(stderr, "error: unsupported message type %d\n", cmd);
			rc = 1;
		}
	}

	if (rc == 0) {
		*out_len = buf - buf_start;
	}

	return rc;
}
