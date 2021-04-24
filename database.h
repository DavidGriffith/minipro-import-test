/*
 * database.h - Definitions and declarations for dealing with the
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

#include <stdint.h>
#include "minipro.h"

typedef struct fuse_decl {
  uint8_t num_fuses;
  uint8_t num_uids;
  uint8_t num_locks;
  uint8_t item_size;
  uint8_t word;
  uint8_t erase_num_fuses;
  uint8_t rev_mask;
  const char **fnames;
  const char **unames;
  const char **lnames;
} fuse_decl_t;

typedef struct gal_config {
  uint8_t fuses_size;    // fuses size in bytes
  uint8_t row_width;     // how many bytes a row have
  uint16_t ues_address;  // user electronic signature address
  uint8_t ues_size;      // ues size in bits
  uint8_t powerdown_row; // row address to disable power down feature
  uint8_t acw_address;   // row address of 'architecture control word'
  uint8_t acw_size;      // acw size in bits
  uint16_t *acw_bits;    // acw bits order
} gal_config_t;

typedef struct pin_map {
	uint8_t zero_c;
	uint8_t zero_t [4];
	uint8_t mask [40];
} pin_map_t;


pin_map_t *get_pin_map(uint8_t);
uint32_t get_pin_count(uint32_t);
int print_chip_count();
int list_devices(uint8_t, const char *, uint32_t, uint32_t, uint32_t *);
device_t *get_device_by_name(uint8_t, const char *);
const char *get_device_from_id(uint8_t, uint32_t, uint8_t);
void free_device(device_t *device);
#endif
