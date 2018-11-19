/*
 * tl866a.c - Low level ops for TL866A/CS.
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#ifdef __APPLE__
#include <stdio.h>
#else
#include <malloc.h>
#endif
#include "minipro.h"
#include "tl866a.h"
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

void tl866a_begin_transaction(minipro_handle_t *handle)
{
	memset(msg, 0, sizeof(msg));
	msg_init(msg, TL866A_REQUEST_STATUS1_MSG1, handle->device, handle->icsp);
	msg_send(handle, msg, 48);
}

void tl866a_end_transaction(minipro_handle_t *handle)
{
	msg_init(msg, TL866A_END_TRANSACTION, handle->device, handle->icsp);
	msg[3] = 0x00;
	msg_send(handle, msg, 4);
}

void tl866a_protect_off(minipro_handle_t *handle)
{
	memset(msg, 0, sizeof(msg));
	msg_init(msg, TL866A_PROTECT_OFF, handle->device, handle->icsp);
	msg_send(handle, msg, 10);
}

void tl866a_protect_on(minipro_handle_t *handle)
{
	memset(msg, 0, sizeof(msg));
	msg_init(msg, TL866A_PROTECT_ON, handle->device, handle->icsp);
	msg_send(handle, msg, 10);
}

uint32_t tl866a_get_ovc_status(minipro_handle_t *handle,
		minipro_status_t *status)
{
	msg_init(msg, TL866A_REQUEST_STATUS1_MSG2, handle->device, handle->icsp);
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

void tl866a_read_block(minipro_handle_t *handle, uint32_t type, uint32_t addr,
		uint8_t *buf, size_t len)
{
	if (type == MP_CODE) {
		type = TL866A_READ_CODE;
	} else if (type == MP_DATA) {
		type = TL866A_READ_DATA;
	} else {
		ERROR2("Unknown type for read_block (%d)\n", type);
	}

	msg_init(msg, type, handle->device, handle->icsp);
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 3, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, 18);
	msg_recv(handle, buf, len);
}

void tl866a_write_block(minipro_handle_t *handle, uint32_t type, uint32_t addr,
		uint8_t *buf, size_t len)
{
	if (type == MP_CODE) {
		type = TL866A_WRITE_CODE;
	} else if (type == MP_DATA) {
		type = TL866A_WRITE_DATA;
	} else {
		ERROR2("Unknown type for write_block (%d)\n", type);
	}

	msg_init(msg, type, handle->device, handle->icsp);
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 3, MP_LITTLE_ENDIAN);
	memcpy(&(msg[7]), buf, len);
	msg_send(handle, msg, 7 + len);
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
uint32_t tl866a_get_chip_id(minipro_handle_t *handle, uint8_t *type)
{
	msg_init(msg, TL866A_GET_CHIP_ID, handle->device, handle->icsp);
	msg_send(handle, msg, 8);
	msg_recv(handle, msg, 32);
	*type = msg[0]; //The Chip ID type (1-5)
	msg[1] &= 0x03; //The length byte is always 1-3 but never know, truncate to max. 4 bytes.
	return (msg[1] ? load_int(&(msg[2]), msg[1], MP_BIG_ENDIAN) : 0); //Check for positive length.
}

void tl866a_read_fuses(minipro_handle_t *handle, uint32_t type, size_t length,
		uint8_t *buf)
{
	if (type == MP_FUSE_USER) {
		type = TL866A_READ_USER;
	} else if (type == MP_FUSE_CFG) {
		type = TL866A_READ_CFG;
	} else if (type == MP_FUSE_LOCK) {
		type = TL866A_READ_LOCK;
	} else {
		ERROR2("Unknown type for read_fuses (%d)\n", type);
	}

	msg_init(msg, type, handle->device, handle->icsp);
	msg[2] = (type == TL866A_READ_CFG && length == 4) ? 2 : 1; // note that PICs with 1 config word will show length==2
	msg[5] = 0x10;
	msg_send(handle, msg, 18);
	msg_recv(handle, msg, 7 + length);
	memcpy(buf, &(msg[7]), length);
}

void tl866a_write_fuses(minipro_handle_t *handle, uint32_t type, size_t length,
		uint8_t *buf)
{
	if (type == MP_FUSE_USER) {
		type = TL866A_READ_USER;
	} else if (type == MP_FUSE_CFG) {
		type = TL866A_READ_CFG;
	} else if (type == MP_FUSE_LOCK) {
		type = TL866A_READ_LOCK;
	} else {
		ERROR2("Unknown type for write_fuses (%d)\n", type);
	}

	// Perform actual writing
	switch (type & 0xf0)
	{
	case 0x10: // TL866A_READ_CFG, TL866A_READ_USER
		msg_init(msg, type + 1, handle->device, handle->icsp);
		msg[2] = (length == 4) ? 0x02 : 0x01;  // 2 fuse PICs have len=8
		msg[4] = 0xc8;
		msg[5] = 0x0f;
		msg[6] = 0x00;
		memcpy(&(msg[7]), buf, length);

		msg_send(handle, msg, 64);
		break;
	case 0x40: // TL866A_READ_LOCK, TL866A_PROTECT_ON
		msg_init(msg, type - 1, handle->device, handle->icsp);
		memcpy(&(msg[7]), buf, length);

		msg_send(handle, msg, 10);
		break;
	}

	// The device waits us to get the status now
	msg_init(msg, type, handle->device, handle->icsp);
	msg[2] = (type == TL866A_READ_CFG && length == 4) ? 2 : 1; // note that PICs with 1 config word will show length==2
	memcpy(&(msg[7]), buf, length);

	msg_send(handle, msg, 18);
	msg_recv(handle, msg, 7 + length);

	if (memcmp(buf, &(msg[7]), length))
	{
		minipro_close(handle);
		ERROR("Failed while writing config bytes");
	}
}

uint32_t tl866a_erase(minipro_handle_t *handle)
{
	msg_init(msg, TL866A_ERASE, handle->device, handle->icsp);
	format_int(&(msg[2]), 0x03, 2, MP_LITTLE_ENDIAN);
	msg[2] = handle->device->write_unlock;
	msg_send(handle, msg, 15);
	memset(msg, 0x00, sizeof(msg));
	msg_recv(handle, msg, sizeof(msg));
	return msg[0] != TL866A_ERASE;

}

//Unlocking the TSOP48 adapter.
uint8_t tl866a_unlock_tsop48(minipro_handle_t *handle)
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
	msg[0] = TL866A_UNLOCK_TSOP48;
	msg[15] = msg[9];
	msg[16] = msg[11];
	msg[9] = (uint8_t) crc;
	msg[11] = (uint8_t) (crc >> 8);
	msg_send(handle, msg, 17);
	msg_recv(handle, msg, sizeof(msg));
	return msg[1];
}

//Minipro hardware check
void tl866a_hardware_check(minipro_handle_t *handle)
{
	uint8_t read_buffer[64];
	memset(&msg, 0x00, 32);

	uint8_t i, error = 0;
	//Reset pin drivers state
	msg[0] = TL866A_RESET_PIN_DRIVERS;
	msg_send(handle, msg, 10);

	//Testing 16 VPP pin drivers
	for (i = 0; i < 16; i++)
	{
		msg[0] = TL866A_SET_LATCH;
		msg[7] = 1; //This is number of latches we want to set (1-8)
		msg[8] = vpp_pins[i].oe; //This is Output Enable we want to select(/OE) (1=OE_VPP, 2=OE_VCC+GND, 3=BOTH)
		msg[9] = vpp_pins[i].latch; //This is Latch number we want to set (0-7; see the schematic diagram)
		msg[10] = vpp_pins[i].mask; //This is latch value we want to write (see the schematic diagram)
		msg_send(handle, msg, 32);
		usleep(5000);
		msg[0] = TL866A_READ_ZIF_PINS;
		msg_send(handle, msg, 18);
		msg_recv(handle, read_buffer, sizeof(read_buffer));
		if (read_buffer[1])
		{
			msg[0] = TL866A_RESET_PIN_DRIVERS;
			msg_send(handle, msg, 10);
			msg[0] = TL866A_END_TRANSACTION;
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
		msg[0] = TL866A_SET_LATCH;
		msg[7] = 1;
		msg[8] = vcc_pins[i].oe;
		msg[9] = vcc_pins[i].latch;
		msg[10] = vcc_pins[i].mask;
		msg_send(handle, msg, 32);
		usleep(5000);
		msg[0] = TL866A_READ_ZIF_PINS;
		msg_send(handle, msg, 18);
		msg_recv(handle, read_buffer, sizeof(read_buffer));
		if (read_buffer[1])
		{
			msg[0] = TL866A_RESET_PIN_DRIVERS;
			msg_send(handle, msg, 10);
			msg[0] = TL866A_END_TRANSACTION;
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
		msg[0] 	= TL866A_SET_LATCH;
		msg[7] 	= gnd_pins[i].pin == 20 ? 9 : 1; //Special handle of pin GND20
		msg[8] 	= gnd_pins[i].oe;
		msg[9] 	= gnd_pins[i].latch;
		msg[10] = gnd_pins[i].mask;
		msg_send(handle, msg, 32);
		usleep(5000);
		msg[0] = TL866A_READ_ZIF_PINS;
		msg_send(handle, msg, 18);
		msg_recv(handle, read_buffer, sizeof(read_buffer));
		if (read_buffer[1])
		{
			msg[0] = TL866A_RESET_PIN_DRIVERS;
			msg_send(handle, msg, 10);
			msg[0] = TL866A_END_TRANSACTION;
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
	msg[0] 	= TL866A_SET_LATCH;
	msg[7] 	= 2; // We will set two latches
	msg[8] 	= 3; //Both OE_VPP and OE_GND active
	msg[9] 	= vpp_pins[VPP1].latch;
	msg[10] = vpp_pins[VPP1].mask; //Put the VPP voltage to the ZIF pin1
	msg[11] = gnd_pins[GND1].latch;
	msg[12] = gnd_pins[GND1].mask; //Now put the same pin ZIF 1 to the GND
	msg_send(handle, msg, 32);
	msg[0] = TL866A_READ_ZIF_PINS; //Now read back the OVC status
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
	msg[0] 	= TL866A_SET_LATCH;
	msg[7] 	= 2; // We will set two latches
	msg[8] 	= 3; //OE GND is active
	msg[9] 	= vcc_pins[VCC40].latch;
	msg[10] = vcc_pins[VCC40].mask; //Put the VCC voltage to the ZIF pin 40
	msg[11] = gnd_pins[GND40].latch;
	msg[12] = gnd_pins[GND40].mask; //Now put the same pin ZIF 40 to the GND
	msg_send(handle, msg, 32);
	msg[0] = TL866A_READ_ZIF_PINS; //Read back the OVC status
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

	msg[0] = TL866A_RESET_PIN_DRIVERS;
	msg_send(handle, msg, 10);
	msg[0] = TL866A_END_TRANSACTION;
	msg_send(handle, msg, 4);
}

