/*
 * tl866a.h - Low level ops for TL866A/CS declarations and definations
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

#ifndef __TL866A_H
#define __TL866A_H

#include "minipro.h"

#define TL866A_FIRMWARE_VERSION 0x0256
#define TL866A_FIRMWARE_STRING "03.2.86"

// TL866A/CS low level functions.
int tl866a_begin_transaction(minipro_handle_t *handle);
int tl866a_end_transaction(minipro_handle_t *handle);
int tl866a_protect_off(minipro_handle_t *handle);
int tl866a_protect_on(minipro_handle_t *handle);
int tl866a_get_ovc_status(minipro_handle_t *handle, minipro_status_t *status,
                          uint8_t *ovc);
int tl866a_read_block(minipro_handle_t *handle, uint8_t type, uint32_t addr,
                      uint8_t *buffer, size_t len);
int tl866a_write_block(minipro_handle_t *handle, uint8_t type, uint32_t addr,
                       uint8_t *buffer, size_t len);
int tl866a_get_chip_id(minipro_handle_t *handle, uint8_t *type,
                       uint32_t *device_id);
int tl866a_read_fuses(minipro_handle_t *handle, uint8_t type, size_t size,
                      uint8_t items_count, uint8_t *buffer);
int tl866a_write_fuses(minipro_handle_t *handle, uint8_t type, size_t size,
                       uint8_t items_count, uint8_t *buffer);
int tl866a_erase(minipro_handle_t *handle);
int tl866a_unlock_tsop48(minipro_handle_t *handle, uint8_t *status);
int tl866a_hardware_check(minipro_handle_t *handle);
int tl866a_write_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                           uint8_t row, size_t size);
int tl866a_read_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                          uint8_t row, size_t size);
int tl866a_firmware_update(minipro_handle_t *handle, const char *firmware);

#endif
