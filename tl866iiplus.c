

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "byte_utils.h"
#include "error.h"
#include "minipro.h"
#include "tl866iiplus.h"

static void msg_init(minipro_handle_t *handle, uint8_t command, uint8_t *buf, size_t length)
{
	assert(length >= 8);

	memset(buf, 0x00, length);
	buf[0] = command;
	buf[1] = handle->device->protocol_id;
	buf[2] = handle->device->variant;
	buf[3] = handle->icsp;
}

static void payload_transfer_cb(struct libusb_transfer *transfer)
{
	int *completed = transfer->user_data;
	if (completed != NULL) {
		*completed = 1;
	}
}

static uint32_t payload_transfer(minipro_handle_t *handle, uint8_t direction,
	                         uint8_t *ep2_buffer, size_t ep2_length,
	                         uint8_t *ep3_buffer, size_t ep3_length)
{
	struct libusb_transfer *ep2_urb;
	struct libusb_transfer *ep3_urb;
	int32_t ret;
	int32_t ep2_completed = 0;
	int32_t ep3_completed = 0;

	ep2_urb = libusb_alloc_transfer(0);
	ep3_urb = libusb_alloc_transfer(0);
	if (ep2_urb == NULL || ep3_urb == NULL) {
		ERROR("Out of memory");
	}

	libusb_fill_bulk_transfer(ep2_urb, handle->usb_handle, (0x02 | direction), ep2_buffer, ep2_length,
		payload_transfer_cb, &ep2_completed, 5000);
	libusb_fill_bulk_transfer(ep3_urb, handle->usb_handle, (0x03 | direction), ep3_buffer, ep3_length,
		payload_transfer_cb, &ep3_completed, 5000);

	ret = libusb_submit_transfer(ep2_urb);
	if (ret < 0) {
		minipro_close(handle);
		ERROR2("IO error: submit_transfer: %s\n", libusb_error_name(ret));
	}
	ret = libusb_submit_transfer(ep3_urb);
	if (ret < 0) {
		minipro_close(handle);
		ERROR2("IO error: submit_transfer: %s\n", libusb_error_name(ret));
	}

	while (!ep2_completed) {
		ret = libusb_handle_events_completed(handle->ctx, &ep2_completed);
		if (ret < 0) {
			if (ret == LIBUSB_ERROR_INTERRUPTED)
				continue;
			libusb_cancel_transfer(ep2_urb);
			libusb_cancel_transfer(ep3_urb);
			continue;
		}
	}
	while (!ep3_completed) {
		ret = libusb_handle_events_completed(handle->ctx, &ep3_completed);
		if (ret < 0) {
			if (ret == LIBUSB_ERROR_INTERRUPTED)
				continue;
			libusb_cancel_transfer(ep2_urb);
			libusb_cancel_transfer(ep3_urb);
			continue;
		}
	}

	if (ep2_urb->status != 0 || ep3_urb->status != 0) {
		minipro_close(handle);
		ERROR("IO Error: Async transfer failed.");
	}

	ret = ep2_urb->actual_length + ep3_urb->actual_length;

	libusb_free_transfer(ep2_urb);
	libusb_free_transfer(ep3_urb);
	return ret;
}

static uint32_t write_payload(minipro_handle_t *handle, uint8_t *buffer, size_t length)
{
	uint8_t *data;
	int32_t ret;
	size_t ep_length = length/2;
	size_t blocks = length/64;

	data = malloc(length);

	if (data == NULL) {
		ERROR("Out of memory");
	}

	for (int i = 0; i < blocks; ++i) {
		uint8_t *ep_buf;
		if (i % 2 == 0) {
			ep_buf = data;
		} else {
			ep_buf = data + ep_length;
		}
		memcpy(ep_buf + ((i/2)*64), buffer+(i*64), 64);
	}

	ret = payload_transfer(handle, LIBUSB_ENDPOINT_OUT, data, ep_length, data+ep_length, ep_length);

	free(data);
	return ret;
}

static uint32_t read_payload(minipro_handle_t *handle, uint8_t *buffer, size_t length)
{
	uint8_t *data;
	int32_t ret;
	size_t ep_length = length/2;
	size_t blocks = length/64;

	data = malloc(length);

	if (data == NULL) {
		ERROR("Out of memory");
	}

	ret = payload_transfer(handle, LIBUSB_ENDPOINT_IN, data, ep_length, data+ep_length, ep_length);

	for (int i = 0; i < blocks; ++i) {
		uint8_t *ep_buf;
		if (i % 2 == 0) {
			ep_buf = data;
		} else {
			ep_buf = data + ep_length;
		}
		memcpy(buffer+(i*64), ep_buf + ((i/2)*64), 64);
	}

	free(data);
	return ret;
}

void tl866iiplus_begin_transaction(minipro_handle_t *handle)
{
	uint8_t msg[64];
	msg_init(handle, TL866IIPLUS_BEGIN_TRANS, msg, sizeof(msg));
	format_int(&(msg[40]), handle->device->package_details, 4, MP_LITTLE_ENDIAN);
	format_int(&(msg[16]), handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
	format_int(&(msg[14]), handle->device->data_memory2_size, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[12]), handle->device->opts3, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[10]), handle->device->opts2, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[8]), handle->device->data_memory_size, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[5]), handle->device->opts1, 2, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, sizeof(msg));
}

void tl866iiplus_end_transaction(minipro_handle_t *handle)
{
	uint8_t msg[8];
	msg_init(handle, TL866IIPLUS_END_TRANS, msg, sizeof(msg));
	msg_send(handle, msg, sizeof(msg));

}

void tl866iiplus_read_block(minipro_handle_t *handle, uint32_t type,
	                    uint32_t addr, uint8_t *buf, size_t len)
{
	uint8_t msg[64];

	if (type == MP_CODE) {
		type = TL866IIPLUS_READ_CODE;
	} else if (type == MP_DATA) {
		type = TL866IIPLUS_READ_DATA;
	} else {
		ERROR2("Unknown type for read_block (%d)\n", type);
	}

	msg_init(handle, type, msg, sizeof(msg));
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 4, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, 8);
	if (len < 64) {
		msg_recv(handle, buf, len);
	} else {
		read_payload(handle, buf, len);
	}
}

void tl866iiplus_write_block(minipro_handle_t *handle, uint32_t type,
	                     uint32_t addr, uint8_t *buf, size_t len)
{
	uint8_t msg[72];

	if (type == MP_CODE) {
		type = TL866IIPLUS_WRITE_CODE;
	} else if (type == MP_DATA) {
		type = TL866IIPLUS_WRITE_DATA;
	} else {
		ERROR2("Unknown type for write_block (%d)\n", type);
	}

	msg_init(handle, type, msg, sizeof(msg));
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 4, MP_LITTLE_ENDIAN);
	if (len < 64) {
		memcpy(&(msg[8]), buf, len);
		msg_send(handle, msg, 8 + len);
	} else {
		msg_send(handle, msg, 8);
		write_payload(handle, buf, len);
	}
}

uint32_t tl866iiplus_get_chip_id(minipro_handle_t *handle, uint8_t *type)
{
	uint8_t msg[8];
	msg_init(handle, TL866IIPLUS_READID, msg, sizeof(msg));
	msg_send(handle, msg, sizeof(msg));
	msg_recv(handle, msg, 6);
	*type = msg[0]; //The Chip ID type (1-5)
	msg[1] &= 0x03; //The length byte is always 1-4 but never know, truncate to max. 4 bytes.
	return (msg[1] ? load_int(&(msg[2]), msg[1], MP_BIG_ENDIAN) : 0); //Check for positive length.
}

void tl866iiplus_protect_off(minipro_handle_t *handle)
{
	uint8_t msg[8];
	msg_init(handle, TL866IIPLUS_PROTECT_OFF, msg, sizeof(msg));
	msg_send(handle, msg, sizeof(msg));
}

void tl866iiplus_protect_on(minipro_handle_t *handle)
{
	uint8_t msg[8];
	msg_init(handle, TL866IIPLUS_PROTECT_ON, msg, sizeof(msg));
	msg_send(handle, msg, sizeof(msg));
}

uint32_t tl866iiplus_erase(minipro_handle_t *handle)
{
	uint8_t msg[8];
	msg_init(handle, TL866IIPLUS_ERASE, msg, sizeof(msg));
	format_int(&(msg[2]), handle->device->write_unlock, 2, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, sizeof(msg));
	msg_recv(handle, msg, sizeof(msg));
	return msg[0] != TL866IIPLUS_ERASE;
}

