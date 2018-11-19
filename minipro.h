/*
 * minipro.h - Low level operations declarations and definitions.
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

#ifndef __MINIPRO_H
#define __MINIPRO_H

#include <libusb.h>

#include "version.h"

#define MP_TL866A 1
#define MP_TL866CS 2
#define MP_TL866IIPLUS 5
#define MP_STATUS_NORMAL 1
#define MP_STATUS_BOOTLOADER 2

#define MP_FIRMWARE_VERSION 0x0255
#define MP_FIRMWARE_STRING "03.2.85"

#define MP_CODE                 0x00
#define MP_DATA                 0x01

#define MP_FUSE_USER            0x00
#define MP_FUSE_CFG             0x01
#define MP_FUSE_LOCK            0x02

#define MP_ICSP_ENABLE          0x80
#define MP_ICSP_VCC             0x01

#define MP_TSOP48_TYPE_V3	0x00
#define	MP_TSOP48_TYPE_NONE	0x01
#define	MP_TSOP48_TYPE_V0	0x02
#define	MP_TSOP48_TYPE_FAKE1	0x03
#define	MP_TSOP48_TYPE_FAKE2	0x04

#define MP_ID_TYPE1		0x01
#define MP_ID_TYPE2		0x02
#define MP_ID_TYPE3		0x03
#define MP_ID_TYPE4		0x04
#define MP_ID_TYPE5		0x05

#include "database.h"

typedef struct minipro_report_info
{
	uint8_t echo;
	uint8_t device_status;
	uint16_t report_size;
	uint8_t firmware_version_minor;
	uint8_t firmware_version_major;
	uint16_t device_version;
	uint8_t device_code[8];
	uint8_t serial_number[24];
	uint8_t hardware_version;
} minipro_report_info_t;

typedef struct minipro_status_s
{
	uint32_t error;
	uint32_t address;
	uint16_t c1;
	uint16_t c2;

} minipro_status_t;

typedef struct zif_pins_s
{
	uint8_t pin;
	uint8_t latch;
	uint8_t oe;
	uint8_t mask;
} zif_pins_t;

typedef struct minipro_handle
{
	char model[16];
	char firmware_str[16];
	uint32_t firmware;

	libusb_device_handle *usb_handle;
	libusb_context *ctx;

	void (*minipro_begin_transaction)(struct minipro_handle *);
	void (*minipro_end_transaction)(struct minipro_handle *);
	void (*minipro_protect_off)(struct minipro_handle *);
	void (*minipro_protect_on)(struct minipro_handle *);
	uint32_t (*minipro_get_ovc_status)(struct minipro_handle *, struct minipro_status_s *);
	void (*minipro_read_block)(struct minipro_handle *, uint32_t, uint32_t, uint8_t *, size_t);
	void (*minipro_write_block)(struct minipro_handle *, uint32_t, uint32_t, uint8_t *, size_t);
	uint32_t (*minipro_get_chip_id)(struct minipro_handle *, uint8_t *);
	void (*minipro_read_fuses)(struct minipro_handle *, uint32_t, size_t, uint8_t *);
	void (*minipro_write_fuses)(struct minipro_handle *, uint32_t, size_t, uint8_t *);
	uint32_t (*minipro_erase)(struct minipro_handle *);
	uint8_t (*minipro_unlock_tsop48)(struct minipro_handle *);
	void (*minipro_hardware_check)(struct minipro_handle *);

	device_t *device;
	uint32_t icsp;
} minipro_handle_t;

enum VPP_PINS
	{
		VPP1,	VPP2,	VPP3,	VPP4,	VPP9,	VPP10,	VPP30,	VPP31,
		VPP32,	VPP33,	VPP34,	VPP36,	VPP37,	VPP38,	VPP39,	VPP40
	};

enum VCC_PINS
	{
		VCC1,	VCC2,	VCC3,	VCC4,	VCC5,	VCC6,	VCC7,	VCC8,
		VCC9,	VCC10,	VCC11,	VCC12,	VCC13,	VCC21,	VCC30,	VCC32,
		VCC33,	VCC34,	VCC35,	VCC36,	VCC37,	VCC38,	VCC39,	VCC40
	};

enum GND_PINS
	{
		GND1,	GND2,	GND3,	GND4,	GND5,	GND6,	GND7,	GND8,
		GND9,	GND10,	GND11,	GND12,	GND14,	GND16,	GND20,	GND30,
		GND31,	GND32,	GND34,	GND35,	GND36,	GND37,	GND38,	GND39,
		GND40
	};

minipro_handle_t *minipro_open(device_t *device);
void minipro_close(minipro_handle_t *handle);
void minipro_get_system_info(minipro_handle_t *handle, minipro_report_info_t *info);
uint32_t msg_send(minipro_handle_t *handle, uint8_t *buf, size_t length);
uint32_t msg_recv(minipro_handle_t *handle, uint8_t *buf, size_t length);

void minipro_begin_transaction(minipro_handle_t *handle);
void minipro_end_transaction(minipro_handle_t *handle);
void minipro_protect_off(minipro_handle_t *handle);
void minipro_protect_on(minipro_handle_t *handle);
uint32_t minipro_get_ovc_status(minipro_handle_t *handle, minipro_status_t *status);
void minipro_read_block(minipro_handle_t *handle, uint32_t type, uint32_t addr, uint8_t *buf, size_t len);
void minipro_write_block(minipro_handle_t *handle, uint32_t type, uint32_t addr, uint8_t *buf, size_t len);
uint32_t minipro_get_chip_id(minipro_handle_t *handle, uint8_t *type);
void minipro_read_fuses(minipro_handle_t *handle, uint32_t type, size_t length, uint8_t *buf);
void minipro_write_fuses(minipro_handle_t *handle, uint32_t type, size_t length, uint8_t *buf);
uint32_t minipro_erase(minipro_handle_t *handle);
uint8_t minipro_unlock_tsop48(minipro_handle_t *handle);
void minipro_hardware_check(minipro_handle_t *handle);

#endif
