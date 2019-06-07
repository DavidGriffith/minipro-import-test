/*
 * tl866iiplus.c - Low level ops for TL866II+.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "tl866iiplus.h"
#include "usb.h"

#define TL866IIPLUS_BEGIN_TRANS 0x03
#define TL866IIPLUS_END_TRANS 0x04
#define TL866IIPLUS_READID 0x05
#define TL866IIPLUS_READ_USER 0x06
#define TL866IIPLUS_WRITE_USER 0x07
#define TL866IIPLUS_READ_CFG 0x08
#define TL866IIPLUS_WRITE_CFG 0x09
#define TL866IIPLUS_WRITE_CODE 0x0C
#define TL866IIPLUS_READ_CODE 0x0D
#define TL866IIPLUS_ERASE 0x0E
#define TL866IIPLUS_READ_DATA 0x10
#define TL866IIPLUS_WRITE_DATA 0x11
#define TL866IIPLUS_WRITE_LOCK 0x14
#define TL866IIPLUS_READ_LOCK 0x15
#define TL866IIPLUS_PROTECT_OFF 0x18
#define TL866IIPLUS_PROTECT_ON 0x19
#define TL866IIPLUS_READ_JEDEC 0x1D
#define TL866IIPLUS_WRITE_JEDEC 0x1E

#define TL866IIPLUS_UNLOCK_TSOP48 0x38
#define TL866IIPLUS_REQUEST_STATUS 0x39

static void msg_init(minipro_handle_t *handle, uint8_t command, uint8_t *buf,
                     size_t length) {
  assert(length >= 8);

  memset(buf, 0x00, length);
  buf[0] = command;
  if (handle->device) {
    buf[1] = handle->device->protocol_id;
    buf[2] = handle->device->variant;
  }
  buf[3] = handle->icsp;
}

int tl866iiplus_begin_transaction(minipro_handle_t *handle) {
  uint8_t msg[64];
  msg_init(handle, TL866IIPLUS_BEGIN_TRANS, msg, sizeof(msg));
  format_int(&(msg[40]), handle->device->package_details, 4, MP_LITTLE_ENDIAN);
  format_int(&(msg[16]), handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
  format_int(&(msg[14]), handle->device->data_memory2_size, 2,
             MP_LITTLE_ENDIAN);
  format_int(&(msg[12]), handle->device->opts3, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[10]), handle->device->opts2, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[8]), handle->device->data_memory_size, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[5]), handle->device->opts1, 2, MP_LITTLE_ENDIAN);
  return msg_send(handle->usb_handle, msg, sizeof(msg));
}

int tl866iiplus_end_transaction(minipro_handle_t *handle) {
  uint8_t msg[8];
  msg_init(handle, TL866IIPLUS_END_TRANS, msg, sizeof(msg));
  return msg_send(handle->usb_handle, msg, sizeof(msg));
}

int tl866iiplus_read_block(minipro_handle_t *handle, uint8_t type,
                           uint32_t addr, uint8_t *buf, size_t len) {
  uint8_t msg[64];

  if (type == MP_CODE) {
    type = TL866IIPLUS_READ_CODE;
  } else if (type == MP_DATA) {
    type = TL866IIPLUS_READ_DATA;
  } else {
    fprintf(stderr, "Unknown type for read_block (%d)\n", type);
    return EXIT_FAILURE;
  }

  msg_init(handle, type, msg, sizeof(msg));
  format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[4]), addr, 4, MP_LITTLE_ENDIAN);
  if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  return read_payload(handle->usb_handle, buf,
                      handle->device->read_buffer_size);
}

int tl866iiplus_write_block(minipro_handle_t *handle, uint8_t type,
                            uint32_t addr, uint8_t *buf, size_t len) {
  uint8_t msg[64];

  if (type == MP_CODE) {
    type = TL866IIPLUS_WRITE_CODE;
  } else if (type == MP_DATA) {
    type = TL866IIPLUS_WRITE_DATA;
  } else {
    fprintf(stderr, "Unknown type for write_block (%d)\n", type);
    return EXIT_FAILURE;
  }

  msg_init(handle, type, msg, sizeof(msg));
  format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[4]), addr, 4, MP_LITTLE_ENDIAN);
  if (len < 57) {                 // If the header + payload is up to 64 bytes
    memcpy(&(msg[8]), buf, len);  // Send the message over the endpoint 1
    if (msg_send(handle->usb_handle, msg, 8 + len)) return EXIT_FAILURE;
  } else {  // Otherwise send only the header over the endpoint 1
    if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
    if (write_payload(handle->usb_handle, buf,
                      handle->device->write_buffer_size))
      return EXIT_FAILURE;  // And payload to the endp.2 and 3
  }
  return EXIT_SUCCESS;
}

int tl866iiplus_read_fuses(minipro_handle_t *handle, uint8_t type,
                           size_t length, uint8_t items_count,
                           uint8_t *buffer) {
  uint8_t msg[16];

  if (type == MP_FUSE_USER) {
    type = TL866IIPLUS_READ_USER;
  } else if (type == MP_FUSE_CFG) {
    type = TL866IIPLUS_READ_CFG;
  } else if (type == MP_FUSE_LOCK) {
    type = TL866IIPLUS_READ_LOCK;
  } else {
    fprintf(stderr, "Unknown type for read_fuses (%d)\n", type);
    return EXIT_FAILURE;
  }

  memset(msg, 0, sizeof(msg));
  msg[0] = type;
  msg[1] = handle->device->protocol_id;
  msg[2] = items_count;
  format_int(&msg[4], handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
  if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, 8 + length)) return EXIT_FAILURE;
  memcpy(buffer, &(msg[8]), length);
  return EXIT_SUCCESS;
}

int tl866iiplus_write_fuses(minipro_handle_t *handle, uint8_t type,
                            size_t length, uint8_t items_count,
                            uint8_t *buffer) {
  uint8_t msg[16];

  if (type == MP_FUSE_USER) {
    type = TL866IIPLUS_WRITE_USER;
  } else if (type == MP_FUSE_CFG) {
    type = TL866IIPLUS_WRITE_CFG;
  } else if (type == MP_FUSE_LOCK) {
    type = TL866IIPLUS_WRITE_LOCK;
  } else {
    fprintf(stderr, "Unknown type for write_fuses (%d)\n", type);
  }

  memset(msg, 0, sizeof(msg));
  msg[0] = type;
  msg[1] = handle->device->protocol_id;
  msg[2] = items_count;
  format_int(&msg[4], handle->device->code_memory_size - 0x38, 4,
             MP_LITTLE_ENDIAN);  // 0x38, firmware bug?
  memcpy(&(msg[8]), buffer, length);
  return (msg_send(handle->usb_handle, msg, 8 + length));
}

int tl866iiplus_get_chip_id(minipro_handle_t *handle, uint8_t *type,
                            uint32_t *device_id) {
  uint8_t msg[8], format;
  msg_init(handle, TL866IIPLUS_READID, msg, sizeof(msg));
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, 6)) return EXIT_FAILURE;
  *type = msg[0];  // The Chip ID type (1-5)
  format = (*type == MP_ID_TYPE3 || *type == MP_ID_TYPE4 ? MP_LITTLE_ENDIAN
                                                         : MP_BIG_ENDIAN);
  msg[1] &= 0x03;  // The length byte is always 1-4 but never know, truncate to
                   // max. 4 bytes.
  *device_id = (msg[1] ? load_int(&(msg[2]), msg[1], format)
                       : 0);  // Check for positive length.
  return EXIT_SUCCESS;
}

int tl866iiplus_protect_off(minipro_handle_t *handle) {
  uint8_t msg[8];
  msg_init(handle, TL866IIPLUS_PROTECT_OFF, msg, sizeof(msg));
  return msg_send(handle->usb_handle, msg, sizeof(msg));
}

int tl866iiplus_protect_on(minipro_handle_t *handle) {
  uint8_t msg[8];
  msg_init(handle, TL866IIPLUS_PROTECT_ON, msg, sizeof(msg));
  return msg_send(handle->usb_handle, msg, sizeof(msg));
}

int tl866iiplus_erase(minipro_handle_t *handle) {
  // fprintf(stderr, "Erase\n");
  uint8_t msg[64];
  msg_init(handle, TL866IIPLUS_ERASE, msg, sizeof(msg));
  format_int(&(msg[2]), 0x03, 2, MP_LITTLE_ENDIAN);
  /* There's no "write unlock". This is how many fuses the controller have
   * or 1 if the device is something else.
   */
  switch (handle->device->protocol_id) {
    case PLD_PROTOCOL2_16V8:
    case PLD_PROTOCOL2_20V8:
    case PLD_PROTOCOL2_22V10:
      break;
    default:

      if (((fuse_decl_t *)handle->device->config) == NULL ||
          ((fuse_decl_t *)handle->device->config)->num_fuses == 0)
        msg[2] = 1;
      else {
        msg[2] = ((fuse_decl_t *)handle->device->config)->erase_num_fuses;
      }
  }
  if (msg_send(handle->usb_handle, msg, 15)) return EXIT_FAILURE;
  memset(msg, 0x00, sizeof(msg));
  if (msg_recv(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

int tl866iiplus_get_ovc_status(minipro_handle_t *handle,
                               minipro_status_t *status, uint8_t *ovc) {
  uint8_t msg[32];
  msg_init(handle, TL866IIPLUS_REQUEST_STATUS, msg, sizeof(msg));
  if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;
  if (status)  // Check for null
  {
    // This is verify while writing feature.
    status->error = msg[0];
    status->address = load_int(&msg[8], 4, MP_LITTLE_ENDIAN);
    status->c1 = load_int(&msg[2], 2, MP_LITTLE_ENDIAN);
    status->c2 = load_int(&msg[4], 2, MP_LITTLE_ENDIAN);
  }
  *ovc = msg[12];  // return the ovc status
  return EXIT_SUCCESS;
}

int tl866iiplus_unlock_tsop48(minipro_handle_t *handle, uint8_t *status) {
  uint8_t msg[48];
  uint16_t i, crc = 0;

  msg_init(handle, TL866IIPLUS_UNLOCK_TSOP48, msg, sizeof(msg));

  srandom(time(NULL));
  for (i = 8; i < 16; i++) {
    msg[i] = (uint8_t)random();
    // Calculate the crc16
    crc = (crc >> 8) | (crc << 8);
    crc ^= msg[i];
    crc ^= (crc & 0xFF) >> 4;
    crc ^= (crc << 12);
    crc ^= (crc & 0xFF) << 5;
  }
  msg[16] = msg[10];
  msg[17] = msg[12];
  msg[10] = (uint8_t)crc;
  msg[12] = (uint8_t)(crc >> 8);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  *status = msg[1];
  return EXIT_SUCCESS;
}
int tl866iiplus_write_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                                uint8_t row, size_t size) {
  uint8_t msg[64];
  memset(msg, 0, sizeof(msg));
  msg[0] = TL866IIPLUS_WRITE_JEDEC;
  msg[1] = handle->device->protocol_id;
  msg[2] = size;
  msg[4] = row;
  memcpy(&msg[8], buffer, size / 8 + 1);
  return msg_send(handle->usb_handle, msg, 64);
}

int tl866iiplus_read_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                               uint8_t row, size_t size) {
  uint8_t msg[32];
  memset(msg, 0, sizeof(msg));
  msg[0] = TL866IIPLUS_READ_JEDEC;
  msg[1] = handle->device->protocol_id;
  msg[2] = size;
  msg[4] = row;
  if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, 32)) return EXIT_FAILURE;
  memcpy(buffer, msg, size / 8 + 1);
  return EXIT_SUCCESS;
}
