/*
 * tl866iplusa.h - Low level ops for TL866II+ declarations and definations
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

#ifndef __TL866IIPLUS_H
#define __TL866IIPLUS_H

#define TL866IIPLUS_FIRMWARE_VERSION 0x0263
#define TL866IIPLUS_FIRMWARE_STRING "04.2.99"

#define TL866IIPLUS_BEGIN_TRANS     0x03
#define TL866IIPLUS_END_TRANS       0x04
#define TL866IIPLUS_READID          0x05
#define TL866IIPLUS_READ_USER       0x06
#define TL866IIPLUS_WRITE_USER      0x07
#define TL866IIPLUS_READ_CFG        0x08
#define TL866IIPLUS_WRITE_CFG       0x09
#define TL866IIPLUS_WRITE_CODE      0x0C
#define TL866IIPLUS_READ_CODE       0x0D
#define TL866IIPLUS_ERASE           0x0E
#define TL866IIPLUS_READ_DATA       0x10
#define TL866IIPLUS_WRITE_DATA      0x11
#define TL866IIPLUS_WRITE_LOCK      0x14
#define TL866IIPLUS_READ_LOCK       0x15
#define TL866IIPLUS_PROTECT_OFF     0x18
#define TL866IIPLUS_PROTECT_ON      0x19

#define TL866IIPLUS_REQUEST_STATUS  0x39

void tl866iiplus_begin_transaction(minipro_handle_t *handle);
void tl866iiplus_end_transaction(minipro_handle_t *handle);
uint32_t tl866iiplus_get_chip_id(minipro_handle_t *handle, uint8_t *type);
void tl866iiplus_read_block(minipro_handle_t *handle, uint32_t type, uint32_t addr, uint8_t *buf, size_t len);
void tl866iiplus_write_block(minipro_handle_t *handle, uint32_t type, uint32_t addr, uint8_t *buf, size_t len);
void tl866iiplus_protect_off(minipro_handle_t *handle);
void tl866iiplus_protect_on(minipro_handle_t *handle);
uint32_t tl866iiplus_erase(minipro_handle_t *handle);
void tl866iiplus_read_fuses(minipro_handle_t *handle, uint32_t type, size_t length, uint8_t *buf);
void tl866iiplus_write_fuses(minipro_handle_t *handle, uint32_t type, size_t length, uint8_t *buf);
uint32_t tl866iiplus_get_ovc_status(minipro_handle_t *handle, minipro_status_t *status);

#endif

