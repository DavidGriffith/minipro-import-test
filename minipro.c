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
#include "byte_utils.h"
#include "error.h"

// 16 VPP pins. NPN trans. mask
zif_pins_t vpp_pins[] =
{
{ .pin = 1, .latch = 1, .oe = 1, .mask = 0x04 },
{ .pin = 2, .latch = 1, .oe = 1, .mask = 0x08 },
{ .pin = 3, .latch = 0, .oe = 1, .mask = 0x04 },
{ .pin = 4, .latch = 0, .oe = 1, .mask = 0x08 },
{ .pin = 9, .latch = 0, .oe = 1, .mask = 0x20 },
{ .pin = 10, .latch = 0, .oe = 1, .mask = 0x10 },
{ .pin = 30, .latch = 1, .oe = 1, .mask = 0x01 },
{ .pin = 31, .latch = 0, .oe = 1, .mask = 0x01 },
{ .pin = 32, .latch = 1, .oe = 1, .mask = 0x80 },
{ .pin = 33, .latch = 0, .oe = 1, .mask = 0x40 },
{ .pin = 34, .latch = 0, .oe = 1, .mask = 0x02 },
{ .pin = 36, .latch = 1, .oe = 1, .mask = 0x02 },
{ .pin = 37, .latch = 0, .oe = 1, .mask = 0x80 },
{ .pin = 38, .latch = 1, .oe = 1, .mask = 0x40 },
{ .pin = 39, .latch = 1, .oe = 1, .mask = 0x20 },
{ .pin = 40, .latch = 1, .oe = 1, .mask = 0x10 }
};

// 24 VCC Pins. PNP trans. mask
zif_pins_t vcc_pins[] =
{
{ .pin = 1, .latch = 2, .oe = 2, .mask = 0x7f },
{ .pin = 2, .latch = 2, .oe = 2, .mask = 0xef },
{ .pin = 3, .latch = 2, .oe = 2, .mask = 0xdf },
{ .pin = 4, .latch = 3, .oe = 2, .mask = 0xfe },
{ .pin = 5, .latch = 2, .oe = 2, .mask = 0xfb },
{ .pin = 6, .latch = 3, .oe = 2, .mask = 0xfb },
{ .pin = 7, .latch = 4, .oe = 2, .mask = 0xbf },
{ .pin = 8, .latch = 4, .oe = 2, .mask = 0xfd },
{ .pin = 9, .latch = 4, .oe = 2, .mask = 0xfb },
{ .pin = 10, .latch = 4, .oe = 2, .mask = 0xf7 },
{ .pin = 11, .latch = 4, .oe = 2, .mask = 0xfe },
{ .pin = 12, .latch = 4, .oe = 2, .mask = 0x7f },
{ .pin = 13, .latch = 4, .oe = 2, .mask = 0xef },
{ .pin = 21, .latch = 4, .oe = 2, .mask = 0xdf },
{ .pin = 30, .latch = 3, .oe = 2, .mask = 0xbf },
{ .pin = 32, .latch = 3, .oe = 2, .mask = 0x7f },
{ .pin = 33, .latch = 3, .oe = 2, .mask = 0xdf },
{ .pin = 34, .latch = 3, .oe = 2, .mask = 0xf7 },
{ .pin = 35, .latch = 3, .oe = 2, .mask = 0xef },
{ .pin = 36, .latch = 3, .oe = 2, .mask = 0x7f },
{ .pin = 37, .latch = 2, .oe = 2, .mask = 0xf7 },
{ .pin = 38, .latch = 2, .oe = 2, .mask = 0xbf },
{ .pin = 39, .latch = 2, .oe = 2, .mask = 0xfe },
{ .pin = 40, .latch = 2, .oe = 2, .mask = 0xfd }
};

// 25 GND Pins. NPN trans. mask
zif_pins_t gnd_pins[] =
{
{ .pin = 1, .latch = 6, .oe = 2, .mask = 0x04 },
{ .pin = 2, .latch = 6, .oe = 2, .mask = 0x08 },
{ .pin = 3, .latch = 6, .oe = 2, .mask = 0x40 },
{ .pin = 4, .latch = 6, .oe = 2, .mask = 0x02 },
{ .pin = 5, .latch = 5, .oe = 2, .mask = 0x04 },
{ .pin = 6, .latch = 5, .oe = 2, .mask = 0x08 },
{ .pin = 7, .latch = 5, .oe = 2, .mask = 0x40 },
{ .pin = 8, .latch = 5, .oe = 2, .mask = 0x02 },
{ .pin = 9, .latch = 5, .oe = 2, .mask = 0x01 },
{ .pin = 10, .latch = 5, .oe = 2, .mask = 0x80 },
{ .pin = 11, .latch = 5, .oe = 2, .mask = 0x10 },
{ .pin = 12, .latch = 5, .oe = 2, .mask = 0x20 },
{ .pin = 14, .latch = 7, .oe = 2, .mask = 0x08 },
{ .pin = 16, .latch = 7, .oe = 2, .mask = 0x40 },
{ .pin = 20, .latch = 9, .oe = 2, .mask = 0x01 },
{ .pin = 30, .latch = 7, .oe = 2, .mask = 0x04 },
{ .pin = 31, .latch = 6, .oe = 2, .mask = 0x01 },
{ .pin = 32, .latch = 6, .oe = 2, .mask = 0x80 },
{ .pin = 34, .latch = 6, .oe = 2, .mask = 0x10 },
{ .pin = 35, .latch = 6, .oe = 2, .mask = 0x20 },
{ .pin = 36, .latch = 7, .oe = 2, .mask = 0x20 },
{ .pin = 37, .latch = 7, .oe = 2, .mask = 0x10 },
{ .pin = 38, .latch = 7, .oe = 2, .mask = 0x02 },
{ .pin = 39, .latch = 7, .oe = 2, .mask = 0x80 },
{ .pin = 40, .latch = 7, .oe = 2, .mask = 0x01 }
};

minipro_handle_t * minipro_open(device_t *device)
{
	int32_t ret;
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

		// If we don't get that either report error in connecting; otherwise report incompatability...
		if (handle->usb_handle == NULL)
		{
			free(handle);
			ERROR("Error opening device");
		}
		else
		{
			free(handle);
			ERROR("This version of the software is not compatible with the TL866 II+");
		}
	}

	handle->device = device;

	return (handle);
}

void minipro_close(minipro_handle_t *handle)
{
	libusb_close(handle->usb_handle);
	free(handle);
}

uint8_t msg[1024];

static void msg_init(uint8_t *out_buf, uint8_t cmd, device_t *device, int icsp)
{
	out_buf[0] = cmd;
	out_buf[1] = device->protocol_id;
	out_buf[2] = device->variant;
	out_buf[3] = 0x00;
	out_buf[4] = device->data_memory_size >> 8 & 0xFF;

	format_int(&(out_buf[5]), device->opts1, 2, MP_LITTLE_ENDIAN);
	out_buf[8] = out_buf[6];
	format_int(&(out_buf[6]), device->opts2, 2, MP_LITTLE_ENDIAN);
	format_int(&(out_buf[9]), device->opts3, 2, MP_LITTLE_ENDIAN);

	out_buf[11] = icsp;
	format_int(&(out_buf[12]), device->code_memory_size, 4, MP_LITTLE_ENDIAN);
}

static size_t msg_transfer(minipro_handle_t *handle, uint8_t *buf,
		size_t length, uint32_t direction)
{
	int bytes_transferred;
	uint32_t ret;
	ret = libusb_claim_interface(handle->usb_handle, 0);

	if (ret != 0)
	{
		minipro_close(handle);
		ERROR2("IO error: claim_interface: %s\n", libusb_error_name(ret));
	}
	ret = libusb_bulk_transfer(handle->usb_handle, (1 | direction), buf,
			(int) length, &bytes_transferred, 0);

	if (ret != 0)
	{
		minipro_close(handle);
		ERROR2("IO error: bulk_transfer: %s\n", libusb_error_name(ret));
	}

	ret = libusb_release_interface(handle->usb_handle, 0);
	if (ret != 0)
	{
		minipro_close(handle);
		ERROR2("IO error: release_interface: %s\n", libusb_error_name(ret));
	}
	return (size_t) bytes_transferred;
}

#ifndef TEST
static uint32_t msg_send(minipro_handle_t *handle, uint8_t *buf, size_t length)
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

static uint32_t msg_recv(minipro_handle_t *handle, uint8_t *buf, size_t length)
{
	return msg_transfer(handle, buf, length, LIBUSB_ENDPOINT_IN);
}
#endif

void minipro_begin_transaction(minipro_handle_t *handle)
{
	memset(msg, 0, sizeof(msg));
	msg_init(msg, MP_REQUEST_STATUS1_MSG1, handle->device, handle->icsp);
	msg_send(handle, msg, 48);
}

void minipro_end_transaction(minipro_handle_t *handle)
{
	msg_init(msg, MP_END_TRANSACTION, handle->device, handle->icsp);
	msg[3] = 0x00;
	msg_send(handle, msg, 4);
}

void minipro_protect_off(minipro_handle_t *handle)
{
	memset(msg, 0, sizeof(msg));
	msg_init(msg, MP_PROTECT_OFF, handle->device, handle->icsp);
	msg_send(handle, msg, 10);
}

void minipro_protect_on(minipro_handle_t *handle)
{
	memset(msg, 0, sizeof(msg));
	msg_init(msg, MP_PROTECT_ON, handle->device, handle->icsp);
	msg_send(handle, msg, 10);
}

uint32_t minipro_get_ovc_status(minipro_handle_t *handle,
		minipro_status_t *status)
{
	msg_init(msg, MP_REQUEST_STATUS1_MSG2, handle->device, handle->icsp);
	msg_send(handle, msg, 5);
	memset(msg, 0, sizeof(msg));
	msg_recv(handle, msg, sizeof(msg));
	if (status) //Check for null
	{
		//This is verify while writing feature.
		status->error = msg[0];
		status->address = load_int(&msg[6], 3, MP_LITTLE_ENDIAN);
		status->c1 = load_int(&msg[2], 2, MP_LITTLE_ENDIAN);
		status->c2 = load_int(&msg[4], 2, MP_LITTLE_ENDIAN);
	}
	return msg[9]; //return the ovc status
}

void minipro_read_block(minipro_handle_t *handle, uint32_t type, uint32_t addr,
		uint8_t *buf, size_t len)
{
	msg_init(msg, type, handle->device, handle->icsp);
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 3, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, 18);
	msg_recv(handle, buf, len);
}

void minipro_write_block(minipro_handle_t *handle, uint32_t type, uint32_t addr,
		uint8_t *buf, size_t len)
{
	msg_init(msg, type, handle->device, handle->icsp);
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 3, MP_LITTLE_ENDIAN);
	memcpy(&(msg[7]), buf, len);
	msg_send(handle, msg, 7 + len);
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
uint32_t minipro_get_chip_id(minipro_handle_t *handle, uint8_t *type)
{
	msg_init(msg, MP_GET_CHIP_ID, handle->device, handle->icsp);
	msg_send(handle, msg, 8);
	msg_recv(handle, msg, 32);
	*type = msg[0]; //The Chip ID type (1-5)
	msg[1] &= 0x03; //The length byte is always 1-3 but never know, truncate to max. 4 bytes.
	return (msg[1] ? load_int(&(msg[2]), msg[1], MP_BIG_ENDIAN) : 0); //Check for positive length.
}

void minipro_read_fuses(minipro_handle_t *handle, uint32_t type, size_t length,
		uint8_t *buf)
{
	msg_init(msg, type, handle->device, handle->icsp);
	msg[2] = (type == MP_READ_CFG && length == 4) ? 2 : 1; // note that PICs with 1 config word will show length==2
	msg[5] = 0x10;
	msg_send(handle, msg, 18);
	msg_recv(handle, msg, 7 + length);
	memcpy(buf, &(msg[7]), length);
}

void minipro_write_fuses(minipro_handle_t *handle, uint32_t type, size_t length,
		uint8_t *buf)
{
	// Perform actual writing
	switch (type & 0xf0)
	{
	case 0x10: // MP_READ_CFG, MP_READ_USER
		msg_init(msg, type + 1, handle->device, handle->icsp);
		msg[2] = (length == 4) ? 0x02 : 0x01;  // 2 fuse PICs have len=8
		msg[4] = 0xc8;
		msg[5] = 0x0f;
		msg[6] = 0x00;
		memcpy(&(msg[7]), buf, length);

		msg_send(handle, msg, 64);
		break;
	case 0x40: // MP_READ_LOCK, MP_PROTECT_ON
		msg_init(msg, type - 1, handle->device, handle->icsp);
		memcpy(&(msg[7]), buf, length);

		msg_send(handle, msg, 10);
		break;
	}

	// The device waits us to get the status now
	msg_init(msg, type, handle->device, handle->icsp);
	msg[2] = (type == MP_READ_CFG && length == 4) ? 2 : 1; // note that PICs with 1 config word will show length==2
	memcpy(&(msg[7]), buf, length);

	msg_send(handle, msg, 18);
	msg_recv(handle, msg, 7 + length);

	if (memcmp(buf, &(msg[7]), length))
	{
		minipro_close(handle);
		ERROR("Failed while writing config bytes");
	}
}

void minipro_print_device_info(minipro_handle_t *handle)
{
	minipro_report_info_t info;

	memset(msg, 0x0, sizeof(msg));
	msg[0] = MP_GET_SYSTEM_INFO;
	msg_send(handle, msg, 5);
	msg_recv(handle, msg, sizeof(msg));
	memcpy(&info, msg, sizeof(info));

	// Model
	char *model;
	switch (info.device_version)
	{
	case MP_TL866A:
		model = "TL866A";
		break;
	case MP_TL866CS:
		model = "TL866CS";
		break;
	default:
	{
		minipro_close(handle);
		ERROR("Unknown device!");
	}
	}

	// Device status
	switch (info.device_status)
	{
	case MP_STATUS_NORMAL:
		break;
	case MP_STATUS_BOOTLOADER:
	{
		minipro_close(handle);
		ERROR2("Found %s in bootloader mode!\nExiting...\n", model);
	}
		break;
	default:
	{
		minipro_close(handle);
		ERROR2("Found %s with unknown device status!\nExiting...\n", model);
	}
	}

	// Firmware
	uint32_t firmware = load_int(&info.firmware_version_minor, 2,
	MP_LITTLE_ENDIAN);
	char firmware_str[16];
	sprintf(firmware_str, "%02d.%d.%d", info.hardware_version,
			info.firmware_version_major, info.firmware_version_minor);
	printf("Found %s %s (%#03x)\n", model, firmware_str, firmware);

	if (firmware < MP_FIRMWARE_VERSION)
	{
		fprintf(stderr, "Warning: Firmware is out of date.\n");
		fprintf(stderr, "  Expected  %s (%#03x)\n", MP_FIRMWARE_STRING,
		MP_FIRMWARE_VERSION);
		fprintf(stderr, "  Found     %s (%#03x)\n", firmware_str, firmware);
	}
	else if (firmware > MP_FIRMWARE_VERSION)
	{
		fprintf(stderr, "Warning: Firmware is newer than expected.\n");
		fprintf(stderr, "  Expected  %s (%#03x)\n", MP_FIRMWARE_STRING,
		MP_FIRMWARE_VERSION);
		fprintf(stderr, "  Found     %s (%#03x)\n", firmware_str, firmware);
	}
}

uint32_t minipro_erase(minipro_handle_t *handle)
{
	msg_init(msg, MP_ERASE, handle->device, handle->icsp);
	format_int(&(msg[2]), 0x03, 2, MP_LITTLE_ENDIAN);
	msg[2] = handle->device->write_unlock;
	msg_send(handle, msg, 15);
	memset(msg, 0x00, sizeof(msg));
	msg_recv(handle, msg, sizeof(msg));
	return msg[0] != MP_ERASE;

}

//Unlocking the TSOP48 adapter.
uint8_t minipro_unlock_tsop48(minipro_handle_t *handle)
{
	memset(&msg, 0x00, sizeof(msg));
	srand(time(NULL));
	uint16_t i, crc = 0;
	for (i = 7; i < 15; i++)
	{
		msg[i] = (uint8_t) rand();
		//Calculate the crc16
		crc = (crc >> 8) | (crc << 8);
		crc ^= msg[i];
		crc ^= (crc & 0xFF) >> 4;
		crc ^= (crc << 12);
		crc ^= (crc & 0xFF) << 5;
	}
	msg[0] = MP_UNLOCK_TSOP48;
	msg[15] = msg[9];
	msg[16] = msg[11];
	msg[9] = (uint8_t) crc;
	msg[11] = (uint8_t) (crc >> 8);
	msg_send(handle, msg, 17);
	msg_recv(handle, msg, sizeof(msg));
	return msg[1];
}

//Minipro hardware check
void minipro_hardware_check()
{
	uint8_t read_buffer[64];
	minipro_handle_t *handle = minipro_open(NULL);
	minipro_print_device_info(handle);
	memset(&msg, 0x00, 32);

	uint8_t i, error = 0;
	//Reset pin drivers state
	msg[0] = MP_RESET_PIN_DRIVERS;
	msg_send(handle, msg, 10);

	//Testing 16 VPP pin drivers
	for (i = 0; i < 16; i++)
	{
		msg[0] = MP_SET_LATCH;
		msg[7] = 1; //This is number of latches we want to set (1-8)
		msg[8] = vpp_pins[i].oe; //This is Output Enable we want to select(/OE) (1=OE_VPP, 2=OE_VCC+GND, 3=BOTH)
		msg[9] = vpp_pins[i].latch; //This is Latch number we want to set (0-7; see the schematic diagram)
		msg[10] = vpp_pins[i].mask; //This is latch value we want to write (see the schematic diagram)
		msg_send(handle, msg, 32);
		usleep(5000);
		msg[0] = MP_READ_ZIF_PINS;
		msg_send(handle, msg, 18);
		msg_recv(handle, read_buffer, sizeof(read_buffer));
		if (read_buffer[1])
		{
			msg[0] = MP_RESET_PIN_DRIVERS;
			msg_send(handle, msg, 10);
			msg[0] = MP_END_TRANSACTION;
			msg_send(handle, msg, 4);
			minipro_close(handle);
			ERROR2(
					"Overcurrent protection detected while testing VPP pin driver %d!\007\n",
					vpp_pins[i].pin);

		}
		if (!read_buffer[6 + vpp_pins[i].pin])
			error = 1;
		printf("VPP driver pin %d is %s\n", vpp_pins[i].pin,
				read_buffer[6 + vpp_pins[i].pin] ? "OK" : "Bad");
	}
	printf("\n");
	//Testing 24 VCC pin drivers
	for (i = 0; i < 24; i++)
	{
		msg[0] = MP_SET_LATCH;
		msg[7] = 1;
		msg[8] = vcc_pins[i].oe;
		msg[9] = vcc_pins[i].latch;
		msg[10] = vcc_pins[i].mask;
		msg_send(handle, msg, 32);
		usleep(5000);
		msg[0] = MP_READ_ZIF_PINS;
		msg_send(handle, msg, 18);
		msg_recv(handle, read_buffer, sizeof(read_buffer));
		if (read_buffer[1])
		{
			msg[0] = MP_RESET_PIN_DRIVERS;
			msg_send(handle, msg, 10);
			msg[0] = MP_END_TRANSACTION;
			msg_send(handle, msg, 4);
			minipro_close(handle);
			ERROR2(
					"Overcurrent protection detected while testing VCC pin driver %d!\007\n",
					vcc_pins[i].pin);

		}
		if (!read_buffer[6 + vcc_pins[i].pin])
			error = 1;
		printf("VCC driver pin %d is %s\n", vcc_pins[i].pin,
				read_buffer[6 + vcc_pins[i].pin] ? "OK" : "Bad");
	}
	printf("\n");
	//Testing 25 GND pin drivers
	for (i = 0; i < 24; i++)
	{
		msg[0] 	= MP_SET_LATCH;
		msg[7] 	= gnd_pins[i].pin == 20 ? 9 : 1; //Special handle of pin GND20
		msg[8] 	= gnd_pins[i].oe;
		msg[9] 	= gnd_pins[i].latch;
		msg[10] = gnd_pins[i].mask;
		msg_send(handle, msg, 32);
		usleep(5000);
		msg[0] = MP_READ_ZIF_PINS;
		msg_send(handle, msg, 18);
		msg_recv(handle, read_buffer, sizeof(read_buffer));
		if (read_buffer[1])
		{
			msg[0] = MP_RESET_PIN_DRIVERS;
			msg_send(handle, msg, 10);
			msg[0] = MP_END_TRANSACTION;
			msg_send(handle, msg, 4);
			minipro_close(handle);
			ERROR2(
					"Overcurrent protection detected while testing GND pin driver %d!\007\n",
					gnd_pins[i].pin);

		}
		if (read_buffer[6 + gnd_pins[i].pin])
			error = 1;
		printf("GND driver pin %d is %s\n", gnd_pins[i].pin,
				read_buffer[6 + gnd_pins[i].pin] ? "Bad" : "OK");
	}

	printf("\n");
	//Testing VPP overcurrent protection
	msg[0] 	= MP_SET_LATCH;
	msg[7] 	= 2; // We will set two latches
	msg[8] 	= 3; //Both OE_VPP and OE_GND active
	msg[9] 	= vpp_pins[VPP1].latch;
	msg[10] = vpp_pins[VPP1].mask; //Put the VPP voltage to the ZIF pin1
	msg[11] = gnd_pins[GND1].latch;
	msg[12] = gnd_pins[GND1].mask; //Now put the same pin ZIF 1 to the GND
	msg_send(handle, msg, 32);
	msg[0] = MP_READ_ZIF_PINS; //Now read back the OVC status
	msg_send(handle, msg, 18);
	msg_recv(handle, read_buffer, sizeof(read_buffer));
	if (read_buffer[1])
	{
		printf("VPP overcurrent protection is OK.\n");
	}
	else
	{
		printf("VPP overcurrent protection failed!\n");
		error = 1;
	}

	//Testing VCC overcurrent protection
	msg[0] 	= MP_SET_LATCH;
	msg[7] 	= 2; // We will set two latches
	msg[8] 	= 3; //OE GND is active
	msg[9] 	= vcc_pins[VCC40].latch;
	msg[10] = vcc_pins[VCC40].mask; //Put the VCC voltage to the ZIF pin 40
	msg[11] = gnd_pins[GND40].latch;
	msg[12] = gnd_pins[GND40].mask; //Now put the same pin ZIF 40 to the GND
	msg_send(handle, msg, 32);
	msg[0] = MP_READ_ZIF_PINS; //Read back the OVC status
	msg_send(handle, msg, 18);
	msg_recv(handle, read_buffer, sizeof(read_buffer));
	if (read_buffer[1])
	{
		printf("VCC overcurrent protection is OK.\n");
	}
	else
	{
		printf("VCC overcurrent protection failed!\n");
		error = 1;
	}

	printf("\n%s\n",
			error ? "Hardware test completed with error(s).\007\n" : "Hardware test completed successfully!\n");

	msg[0] = MP_RESET_PIN_DRIVERS;
	msg_send(handle, msg, 10);
	msg[0] = MP_END_TRANSACTION;
	msg_send(handle, msg, 4);
	minipro_close(handle);
}
