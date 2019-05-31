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

device_t *get_device_table(minipro_handle_t *handle);
device_t *get_device_by_name(minipro_handle_t *handle, const char *name);
const char *get_device_from_id(minipro_handle_t *handle, uint32_t id,
                               uint8_t protocol);

#endif
