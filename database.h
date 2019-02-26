/*
 * database.c - Definitions and declarations for dealing with the
 *		device database.
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

#ifndef __DATABASE_H
#define __DATABASE_H

#include <stddef.h>
#include <stdint.h>
#include "fuses.h"

typedef struct minipro_handle minipro_handle_t;

typedef struct device
{
	const char *name;
	uint8_t protocol_id;
	uint8_t variant;
	uint32_t read_buffer_size;
	uint32_t write_buffer_size;
	uint32_t code_memory_size; // Presenting for every device
	uint32_t data_memory_size;
	size_t data_memory2_size;
	uint32_t chip_id; // A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48)
	uint8_t chip_id_bytes_count;
	uint16_t opts1;
	uint16_t opts2;
	uint32_t opts3;
	uint32_t opts4;
	uint32_t package_details; // pins count or image ID for some devices
	uint16_t write_unlock;
	fuse_decl_t *fuses; // Configuration bytes that's presenting in some architectures
	uint8_t	id_shift;//PIC controllers device ID have a variable bitfield Revision number
} device_t;


#define WORD_SIZE(device) (((device)->opts4 & 0xFF000000) == 0x01000000 ? 2 : 1)

device_t *get_device_table(minipro_handle_t *handle);
device_t *get_device_by_name(minipro_handle_t *handle, const char *name);

#endif
