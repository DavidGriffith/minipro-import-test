/*
 * minipro.c - Low level operations.
 *
 * This file is a part of Minipro.
 *
 * Minipro is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Minipro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <assert.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#if defined (__APPLE__) || defined (__FreeBSD__)
#include <stdio.h>
#else
#include <malloc.h>
#endif
#include "minipro.h"
#include "tl866a.h"
#include "tl866iiplus.h"
#include "byte_utils.h"
#include "error.h"


minipro_handle_t * minipro_open(const char *device_name)
{
	int32_t  ret;
	uint16_t expected_firmware;
	char    *expected_firmware_str;

	minipro_report_info_t info;

	minipro_handle_t *handle = malloc(sizeof(minipro_handle_t));
	if (handle == NULL)
	{
		ERROR("Couldn't malloc");
	}

	ret = libusb_init(&(handle->ctx));
	if (ret < 0)
	{
		free(handle);
		ERROR2("Error initializing libusb: %s", libusb_error_name(ret));
	}

	handle->usb_handle = libusb_open_device_with_vid_pid(handle->ctx, 0x04d8, 0xe11c);
	if (handle->usb_handle == NULL)
	{
		// We didn't match the vid / pid of the "original" TL866 - so try the new TL866II+
		handle->usb_handle = libusb_open_device_with_vid_pid(handle->ctx, 0xa466, 0x0a53);

		// If we don't get that either report error in connecting
		if (handle->usb_handle == NULL)
		{
			free(handle);
			ERROR("Error opening device");
		}
	}

	ret = libusb_claim_interface(handle->usb_handle, 0);
	if (ret != 0)
	{
		minipro_close(handle);
		ERROR2("IO error: claim_interface: %s\n", libusb_error_name(ret));
	}

	minipro_get_system_info(handle, &info);

	switch (info.device_status) {
	case MP_STATUS_NORMAL:
		break;
	case MP_STATUS_BOOTLOADER:
		minipro_close(handle);
		ERROR("In bootloader mode!\nExiting...\n");
		break;
	default:
		minipro_close(handle);
		ERROR("Unknown device status!\nExiting...\n");
	}

	switch (info.device_version) {
	case MP_TL866A:
	case MP_TL866CS:
		expected_firmware = TL866A_FIRMWARE_VERSION;
		expected_firmware_str = TL866A_FIRMWARE_STRING;
		if (info.device_version == MP_TL866A) {
			strcpy(handle->model, "TL866A");
		} else {
			strcpy(handle->model, "TL866CS");
		}
		handle->minipro_begin_transaction = tl866a_begin_transaction;
		handle->minipro_end_transaction = tl866a_end_transaction;
		handle->minipro_protect_off = tl866a_protect_off;
		handle->minipro_protect_on = tl866a_protect_on;
		handle->minipro_get_ovc_status = tl866a_get_ovc_status;
		handle->minipro_read_block = tl866a_read_block;
		handle->minipro_write_block = tl866a_write_block;
		handle->minipro_get_chip_id = tl866a_get_chip_id;
		handle->minipro_read_fuses = tl866a_read_fuses;
		handle->minipro_write_fuses = tl866a_write_fuses;
		handle->minipro_erase = tl866a_erase;
		handle->minipro_unlock_tsop48 = tl866a_unlock_tsop48;
		handle->minipro_hardware_check = tl866a_hardware_check;
		break;
	case MP_TL866IIPLUS:
		expected_firmware = TL866IIPLUS_FIRMWARE_VERSION;
		expected_firmware_str = TL866IIPLUS_FIRMWARE_STRING;
		strcpy(handle->model, "TL866II+");
		handle->minipro_begin_transaction = tl866iiplus_begin_transaction;
		handle->minipro_end_transaction = tl866iiplus_end_transaction;
		handle->minipro_get_chip_id = tl866iiplus_get_chip_id;
		handle->minipro_read_block = tl866iiplus_read_block;
		handle->minipro_write_block = tl866iiplus_write_block;
		handle->minipro_protect_off = tl866iiplus_protect_off;
		handle->minipro_protect_on = tl866iiplus_protect_on;
		handle->minipro_erase = tl866iiplus_erase;
		handle->minipro_read_fuses = tl866iiplus_read_fuses;
		handle->minipro_write_fuses = tl866iiplus_write_fuses;
		break;
	}

	handle->firmware = load_int(&info.firmware_version_minor,
	                            2, MP_LITTLE_ENDIAN);
	sprintf(handle->firmware_str, "%02d.%d.%d", info.hardware_version,
	        info.firmware_version_major, info.firmware_version_minor);
	printf("Found %s %s (%#03x)\n", handle->model,
	       handle->firmware_str, handle->firmware);

	if (handle->firmware < expected_firmware)
	{
		fprintf(stderr, "Warning: Firmware is out of date.\n");
		fprintf(stderr, "  Expected  %s (%#03x)\n", expected_firmware_str,
		expected_firmware);
		fprintf(stderr, "  Found     %s (%#03x)\n", handle->firmware_str, handle->firmware);
	}
	else if (handle->firmware > expected_firmware)
	{
		fprintf(stderr, "Warning: Firmware is newer than expected.\n");
		fprintf(stderr, "  Expected  %s (%#03x)\n", expected_firmware_str,
		expected_firmware);
		fprintf(stderr, "  Found     %s (%#03x)\n", handle->firmware_str, handle->firmware);
	}

	if (device_name != NULL) {
		handle->device = get_device_by_name(handle, device_name);
		if (handle->device == NULL) {
			minipro_close(handle);
			ERROR2("Device %s not found\n", device_name);
		}
	}

	return (handle);
}

void minipro_close(minipro_handle_t *handle)
{
	int32_t ret;

	ret = libusb_release_interface(handle->usb_handle, 0);
	if (ret != 0)
	{
		minipro_close(handle);
		ERROR2("IO error: release_interface: %s\n", libusb_error_name(ret));
	}
	libusb_close(handle->usb_handle);
	free(handle);
}

void minipro_get_system_info(minipro_handle_t *handle, minipro_report_info_t *info)
{
	uint8_t msg[sizeof(*info)];

	memset(info, 0x0, sizeof(*info));
	memset(msg, 0x0, sizeof(msg));
	msg_send(handle, msg, 5);
	msg_recv(handle, msg, sizeof(msg));

	switch (msg[6]) {
	case MP_TL866IIPLUS:
		memcpy(info, msg, sizeof(*info));
		break;
	case MP_TL866A:
	case MP_TL866CS:
		info->echo = msg[0];
		info->device_status = msg[1];
		info->report_size = load_int((msg+2), 2, MP_LITTLE_ENDIAN);
		info->firmware_version_minor = msg[4];
		info->firmware_version_major = msg[5];
		info->device_version = msg[6];
		memcpy(info->device_code, (msg+7), 32);
		info->hardware_version = msg[39];
		break;
	default:
		minipro_close(handle);
		ERROR("Unknown Device!");
	}
}

static size_t msg_transfer(minipro_handle_t *handle, uint8_t *buf,
		size_t length, uint32_t direction)
{
	int bytes_transferred;
	uint32_t ret;
	ret = libusb_bulk_transfer(handle->usb_handle, (1 | direction), buf,
			(int) length, &bytes_transferred, 0);

	if (ret != 0)
	{
		minipro_close(handle);
		ERROR2("IO error: bulk_transfer: %s\n", libusb_error_name(ret));
	}

	return (size_t) bytes_transferred;
}

#ifndef TEST
uint32_t msg_send(minipro_handle_t *handle, uint8_t *buf, size_t length)
{
	uint32_t bytes_transferred = msg_transfer(handle, buf, length,
			LIBUSB_ENDPOINT_OUT);

	if (bytes_transferred != length)
	{
		minipro_close(handle);
		ERROR2("IO error: expected %zu bytes but %d bytes transferred\n",
				length, bytes_transferred);
	}
	return bytes_transferred;
}

uint32_t msg_recv(minipro_handle_t *handle, uint8_t *buf, size_t length)
{
	return msg_transfer(handle, buf, length, LIBUSB_ENDPOINT_IN);
}
#endif

void minipro_begin_transaction(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_begin_transaction) {
		handle->minipro_begin_transaction(handle);
	} else {
		fprintf(stderr, "%s: begin_transaction not implemented\n", handle->model);
	}
}

void minipro_end_transaction(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_end_transaction) {
		handle->minipro_end_transaction(handle);
	} else {
		fprintf(stderr, "%s: end_transaction not implementedi\n", handle->model);
	}
}

void minipro_protect_off(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_protect_off) {
		handle->minipro_protect_off(handle);
	} else {
		fprintf(stderr, "%s: protect_off not implemented\n", handle->model);
	}
}

void minipro_protect_on(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_protect_on) {
		handle->minipro_protect_on(handle);
	} else {
		fprintf(stderr, "%s: protect_on not implemented\n", handle->model);
	}
}

uint32_t minipro_get_ovc_status(minipro_handle_t *handle,
		minipro_status_t *status)
{
	assert(handle != NULL);

	if (status){
		memset (status, 0x00, sizeof(*status));
	}

	if (handle->minipro_get_ovc_status) {
		return handle->minipro_get_ovc_status(handle, status);
	}
	fprintf(stderr, "%s: get_ovc_status not implemented\n", handle->model);
	return 0;
}

void minipro_read_block(minipro_handle_t *handle, uint32_t type, uint32_t addr,
		uint8_t *buf, size_t len)
{
	assert(handle != NULL);

	if (handle->minipro_read_block) {
		handle->minipro_read_block(handle, type, addr, buf, len);
	} else {
		fprintf(stderr, "%s: read_block not implemented\n", handle->model);
	}
}

void minipro_write_block(minipro_handle_t *handle, uint32_t type, uint32_t addr,
		uint8_t *buf, size_t len)
{
	assert(handle != NULL);

	if (handle->minipro_write_block) {
		handle->minipro_write_block(handle, type, addr, buf, len);
	} else {
		fprintf(stderr, "%s: write_block not implemented\n", handle->model);
	}
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
uint32_t minipro_get_chip_id(minipro_handle_t *handle, uint8_t *type)
{
	assert(handle != NULL);

	if (handle->minipro_get_chip_id) {
		return handle->minipro_get_chip_id(handle, type);
	}
	fprintf(stderr, "%s: get_chip_id not implemented\n", handle->model);
	return 0;
}

void minipro_read_fuses(minipro_handle_t *handle, uint32_t type, size_t length,
		uint8_t *buf)
{
	assert(handle != NULL);

	if (handle->minipro_read_fuses) {
		handle->minipro_read_fuses(handle, type, length, buf);
	} else {
		fprintf(stderr, "%s: read_fuses not implemented\n", handle->model);
	}
}

void minipro_write_fuses(minipro_handle_t *handle, uint32_t type, size_t length,
		uint8_t *buf)
{
	assert(handle != NULL);

	if (handle->minipro_write_fuses) {
		handle->minipro_write_fuses(handle, type, length, buf);
	} else {
		fprintf(stderr, "%s: write_fuses not implemented\n", handle->model);
	}
}

uint32_t minipro_erase(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_erase) {
		return handle->minipro_erase(handle);
	}
	fprintf(stderr, "%s: erase not implemented\n", handle->model);
	return 0;
}

//Unlocking the TSOP48 adapter.
uint8_t minipro_unlock_tsop48(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_unlock_tsop48) {
		return handle->minipro_unlock_tsop48(handle);
	}
	fprintf(stderr, "%s: unlock_tsop48 not implemented\n", handle->model);
	return 0;
}

//Minipro hardware check
void minipro_hardware_check(minipro_handle_t *handle)
{
	assert(handle != NULL);

	if (handle->minipro_hardware_check) {
		handle->minipro_hardware_check(handle);
	} else {
		fprintf(stderr, "%s: hardware_check not implemented\n", handle->model);
	}
}
