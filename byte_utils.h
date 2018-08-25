/*
 * byte_utils.c - Definitions and declarations for dealing with
 *		  byte format idiosyncrasies.
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

#ifndef __BYTE_UTILS_H
#define __BYTE_UTILS_H

#define MP_LITTLE_ENDIAN 0
#define MP_BIG_ENDIAN 1

#include <stdint.h>

void format_int(uint8_t *out, uint32_t in, uint8_t length, uint8_t endianess);
uint32_t load_int(uint8_t *buf, uint32_t length, uint32_t endianess);

#endif
