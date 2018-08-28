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

/*
 * This header only contains low-level wrappers against typical requests.
 * Please refer main.c if you're looking for higher-level logic.
 */

#include <libusb.h>

#include "version.h"

/*
 * These are the known firmware versions along with the versions of the
 * official software from whence they came.
 *
 * Firmware	Official	Release		Firmware
 * Version	Program		Date		Version
 * String	Version		Date		ID
 *
 * 3.2.85	6.82		Jul 14, 2018	0x0255
 * 3.2.82	6.71		Apr 17, 2018	0x0252
 * 3.2.81	6.70		Mar  7, 2018	0x0251
 * 3.2.80	6.60		May  9, 2017	0x0250
 * 3.2.72	6.50		Dec 25, 2015	0x0248
 * 3.2.69	6.17		Jul 11, 2015	0x0245
 * 3.2.68	6.16		Jun 12, 2015	0x0244
 * 3.2.66	6.13		Jun  9, 2015	0x0242
 * 3.2.63	6.10		Jul 16, 2014	0x023f
 * 3.2.62	6.00		Jan  7, 2014	0x023e
 * 3.2.61	5.91		Mar  9, 2013	0x023d
 * 3.2.60	5.90		Mar  4, 2013	0x023c
 * 3.2.59	5.80		Nov  1, 2012
 *		5.70		Aug 27, 2012
 *		1.00		Jun 18, 2010
 *
 */

#define MP_TL866A 1
#define MP_TL866CS 2
#define MP_STATUS_NORMAL 1
#define MP_STATUS_BOOTLOADER 2

#define MP_FIRMWARE_VERSION 0x0255
#define MP_FIRMWARE_STRING "03.2.85"

#define MP_REQUEST_STATUS1_MSG1 0x03
#define MP_REQUEST_STATUS1_MSG2 0xfe

#define MP_GET_SYSTEM_INFO 0x00
#define MP_END_TRANSACTION 0x04
#define MP_GET_CHIP_ID 0x05
#define MP_READ_CODE 0x21
#define MP_READ_DATA 0x30
#define MP_WRITE_CODE 0x20
#define MP_WRITE_DATA 0x31
#define MP_ERASE 0x22

#define MP_READ_USER 0x10
#define MP_WRITE_USER 0x11

#define MP_READ_CFG 0x12
#define MP_WRITE_CFG 0x13

#define MP_WRITE_LOCK 0x40
#define MP_READ_LOCK 0x41

#define MP_PROTECT_OFF 0x44
#define MP_PROTECT_ON 0x45

#define MP_ICSP_ENABLE 0x80
#define MP_ICSP_VCC 0x01

//TSOP48
#define MP_UNLOCK_TSOP48 0xFD
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
	uint8_t device_version;
	uint8_t device_code[8];
	uint8_t serial_number[24];
	uint8_t hardware_version;
} minipro_report_info_t;

typedef struct minipro_handle
{
	libusb_device_handle *usb_handle;
	libusb_context *ctx;
	device_t *device;
	uint32_t icsp;
} minipro_handle_t;

typedef struct minipro_status_s
{
	uint32_t error;
	uint32_t address;
	uint16_t c1;
	uint16_t c2;

} minipro_status_t;


//Hardware Bit Banging
#define MP_RESET_PIN_DRIVERS 0xD0
#define MP_SET_LATCH 0xD1
#define MP_READ_ZIF_PINS 0xD2
#define MP_OE_VPP 0x01
#define MP_OE_VCC_GND 0x02
#define MP_OE_ALL 0x03
typedef struct zif_pins_s
{
	uint8_t pin;
	uint8_t latch;
	uint8_t oe;
	uint8_t mask;
} zif_pins_t;

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
void minipro_begin_transaction(minipro_handle_t *handle);
void minipro_end_transaction(minipro_handle_t *handle);
void minipro_protect_off(minipro_handle_t *handle);
void minipro_protect_on(minipro_handle_t *handle);
uint32_t minipro_get_ovc_status(minipro_handle_t *handle, minipro_status_t *status);
void minipro_read_block(minipro_handle_t *handle, uint32_t type, uint32_t addr, uint8_t *buf, size_t len);
void minipro_write_block(minipro_handle_t *handle, uint32_t type, uint32_t addr, uint8_t *buf, size_t len);
uint32_t minipro_get_chip_id(minipro_handle_t *handle, uint8_t *type);
void minipro_read_fuses(minipro_handle_t *handle, uint8_t command, size_t length, uint8_t *buf);
void minipro_write_fuses(minipro_handle_t *handle, uint8_t command, size_t length, uint8_t *buf);
uint32_t minipro_erase(minipro_handle_t *handle);
void minipro_print_device_info(minipro_handle_t *handle);
uint8_t minipro_unlock_tsop48(minipro_handle_t *handle);
void minipro_hardware_check();

#endif
