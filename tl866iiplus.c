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
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "database.h"
#include "minipro.h"
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
#define TL866IIPLUS_AUTODETECT 0x37
#define TL866IIPLUS_UNLOCK_TSOP48 0x38
#define TL866IIPLUS_REQUEST_STATUS 0x39

#define TL866IIPLUS_BOOTLOADER_WRITE 0x3B
#define TL866IIPLUS_BOOTLOADER_ERASE 0x3C
#define TL866IIPLUS_SWITCH 0x3D

// Hardware Bit Banging
#define TL866IIPLUS_SET_OUT 0x31
#define TL866IIPLUS_SET_PULLUPS 0x32
#define TL866IIPLUS_SET_DIR 0x34
#define TL866IIPLUS_READ_PINS 0x35
#define TL866IIPLUS_SET_DIV 0x36

#define TL866IIPLUS_BTLDR_MAGIC 0xA578B986

static void msg_init(minipro_handle_t *handle, uint8_t command, uint8_t *buf,
                     size_t length) {
  assert(length >= 8);

  memset(buf, 0x00, length);
  buf[0] = command;
  if (handle->device) {
    buf[1] = handle->device->protocol_id;
    buf[2] = handle->device->variant;
    buf[3] = handle->icsp;
  }
}

int tl866iiplus_begin_transaction(minipro_handle_t *handle) {
  uint8_t msg[64];
  msg_init(handle, TL866IIPLUS_BEGIN_TRANS, msg, sizeof(msg));
  format_int(&(msg[40]), handle->device->package_details, 4, MP_LITTLE_ENDIAN);
  format_int(&(msg[44]), handle->device->read_buffer_size, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[16]), handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
  format_int(&(msg[14]), handle->device->data_memory2_size, 2,
             MP_LITTLE_ENDIAN);
  format_int(&(msg[12]), handle->device->opts3, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[10]), handle->device->opts2, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[8]), handle->device->data_memory_size, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[5]), handle->device->opts1, 2, MP_LITTLE_ENDIAN);

  //This is work in progress.
  msg[4] = (uint8_t)handle->device->opts5;
  msg[6] = (uint8_t)handle->device->opts7;
  msg[7] = (uint8_t)handle->device->opts8;
  msg[20] = (uint8_t)(handle->device->opts5 >> 16);

  if ((handle->device->opts5 & 0xf0) == 0xf0) {
    msg[21] = 0;
    msg[22] = (uint8_t)handle->device->opts5;
  } else {
    msg[21] = (uint8_t)handle->device->opts5 & 0x0f;
    msg[22] = (uint8_t)handle->device->opts5 & 0xf0;
  }
  if (handle->device->opts5 & 0x80000000)
    msg[22] = (handle->device->opts5 >> 16) & 0x0f;

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

int tl866iiplus_spi_autodetect(minipro_handle_t *handle, uint8_t type,
                               uint32_t *device_id) {
  uint8_t msg[64];
  memset(msg, 0, sizeof(msg));
  msg[0] = TL866IIPLUS_AUTODETECT;
  msg[8] = type;
  if (msg_send(handle->usb_handle, msg, 10)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, 16)) return EXIT_FAILURE;
  *device_id = load_int(&(msg[2]), 3, MP_BIG_ENDIAN);
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

  srand(time(NULL));
  for (i = 8; i < 16; i++) {
    msg[i] = (uint8_t)rand();
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

/* Firmware updater section
//////////////////////////////////////////////////////////////////////////////////

This is the UpdateII.dat file structure.
It has a variabile size. There are small data blocks of 272 bytes each followed by the last data block which always has 2064 bytes.
|============|===========|============|==============|=============|=============|===================|======================|
|File version| File CRC  | XOR Table  | Blocks count | Block 0     | Block 1     | Block N           | Last block           |
|============|===========|============|==============|=============|=============|===================|======================|
|  4 bytes   | 4 bytes   | 1024 bytes | 4 bytes      | 272 bytes   | 272 bytes   | 272 bytes         | 2064 bytes           |
|============|===========|============|==============|=============|=============|===================|======================|
|  offset 0  | offset 4  | offset 8   | offset 1032  | offset 1036 | offset 1308 | offset 1036+N*272 | offset block N + 272 |
|============|===========|============|==============|=============|=============|===================|======================|


The structure of each data block is as following:
|============|===================|====================|=============================|================|
| Block CRC  | XOR table pointer | Encrypted address  | Internal decryption pointer | Encrypted data |
|============|===================|====================|=============================|================|
| 4 bytes    | 4 bytes           | 4 bytes            | 4 bytes (only LSB is used)  | 256/2048 bytes |
|============|===================|====================|=============================|================|
| offset 0   | offset 4          | offset 8           | offset 12                   | offset 16      |
|============|===================|====================|=============================|================|

*/


// Performing a firmware update
int tl866iiplus_firmware_update(minipro_handle_t *handle,
                                const char *firmware) {
  uint8_t msg[264];
  struct stat st;
  if (stat(firmware, &st)) {
    fprintf(stderr, "%s open error!: ", firmware);
    perror("");
    return EXIT_FAILURE;
  }

  off_t file_size = st.st_size;
  // Check the update.dat size
  if (file_size < 3100 || file_size > 1048576) {
    fprintf(stderr, "%s file size error!\n", firmware);
    return EXIT_FAILURE;
  }

  // Open the update.dat firmware file
  FILE *file = fopen(firmware, "rb");
  if (file == NULL) {
    fprintf(stderr, "%s open error!: ", firmware);
    perror("");
    return EXIT_FAILURE;
  }
  uint8_t *update_dat = malloc(file_size);
  if (!update_dat) {
    fprintf(stderr, "Out of memory!\n");
    fclose(file);
    return EXIT_FAILURE;
  }

  // Read the updateII.dat file
  if (fread(update_dat, sizeof(char), st.st_size, file) != st.st_size) {
    fprintf(stderr, "%s file read error!\n", firmware);
    fclose(file);
    free(update_dat);
    return EXIT_FAILURE;
  }
  fclose(file);

  // Read the blocks count and check if correct
  uint32_t blocks = load_int(update_dat + 1032, 4, MP_LITTLE_ENDIAN);
  if (blocks * 272 + 3100 != file_size) {
    fprintf(stderr, "%s file size error!\n", firmware);
    free(update_dat);
    return EXIT_FAILURE;
  }

  // Compute the file CRC and compare
  uint32_t crc = 0xFFFFFFFF;
  // Note the order in which the crc is calculated!
  // First the data blocks crc
  if (blocks > 0) {
    crc = crc32(update_dat + 1036, blocks * 272, crc);
  }
  // Second the last block crc
  crc = crc32(update_dat + blocks * 272 + 1036, 2064, crc);
  // And last the xortable+blocks_count crc
  crc = crc32(update_dat + 8, 1028, crc);
  // The computed CRC32 must match the File CRC from the offset 4
  if (~crc != load_int(update_dat + 4, 4, MP_LITTLE_ENDIAN)) {
    fprintf(stderr, "%s file CRC error!\n", firmware);
    free(update_dat);
    return EXIT_FAILURE;
  }

  /*
   * Decrypting each data block (by deobfuscating the address)
   */

  size_t ptr = 1036;  // This is the offset of the first data block

  // The updateII.dat contains a xor table of 1024 bytes length at the offset 8.
  // This table is used to obfuscate the block address.
  uint32_t xorptr;
  for (uint32_t i = 0; i < blocks; i++) {
    xorptr = load_int(update_dat + ptr + 4, 4,
                      MP_LITTLE_ENDIAN);  // Load the xor table pointer

    /*
     * The destination address of each data block (offset 8) is obfuscated
     * by xoring the LSB part of the address against a xortable 264 times (44*6)
     */
    for (uint32_t i = 0; i < 44; i++) {
      update_dat[ptr + 8] ^= update_dat[(xorptr & 0x3FF) + 8];
      update_dat[ptr + 8] ^= update_dat[((xorptr + 1) & 0x3FF) + 8];
      update_dat[ptr + 8] ^= update_dat[((xorptr + 2) & 0x3FF) + 8];
      update_dat[ptr + 8] ^= update_dat[((xorptr + 3) & 0x3FF) + 8];
      update_dat[ptr + 8] ^= update_dat[((xorptr + 4) & 0x3FF) + 8];
      update_dat[ptr + 8] ^= update_dat[((xorptr + 5) & 0x3FF) + 8];
      xorptr += 6;
    }
    // After deobfuscating the address calculate the block crc and compare
    if (crc32(update_dat + ptr + 4, 268, 0) !=
        load_int(update_dat + ptr, 4, MP_LITTLE_ENDIAN)) {
      fprintf(stderr, "%s file CRC error!\n", firmware);
      free(update_dat);
      return EXIT_FAILURE;
    }
    ptr += 272;
  }

  /*
   * The last data block destination address is obfuscated
   * by xoring the LSB part of the address against a xortable 2056 times (514*4)
   */
  xorptr = load_int(update_dat + ptr + 4, 4, MP_LITTLE_ENDIAN);
  for (uint32_t i = 0; i < 514; i++) {
    update_dat[ptr + 8] ^= update_dat[(xorptr & 0x3FF) + 8];
    update_dat[ptr + 8] ^= update_dat[((xorptr + 1) & 0x3FF) + 8];
    update_dat[ptr + 8] ^= update_dat[((xorptr + 2) & 0x3FF) + 8];
    update_dat[ptr + 8] ^= update_dat[((xorptr + 3) & 0x3FF) + 8];
    xorptr += 4;
  }
  // After deobfuscating the address calculate the block crc and compare
  if (crc32(update_dat + ptr + 4, 2060, 0) !=
      load_int(update_dat + ptr, 4, MP_LITTLE_ENDIAN)) {
    fprintf(stderr, "%s file CRC error!\n", firmware);
    free(update_dat);
    return EXIT_FAILURE;
  }

  fprintf(stderr, "%s contains firmware version %u.%u.%u", firmware,
          update_dat[1] >> 4, update_dat[1] & 0x0F, update_dat[0]);

  if ((handle->firmware & 0xFF) > update_dat[0])
    fprintf(stderr, " (older)");
  else if ((handle->firmware & 0xFF) < update_dat[0])
    fprintf(stderr, " (newer)");

  fprintf(stderr, "\n\nDo you want to continue with firmware update? y/n:");
  fflush(stderr);
  char c = getchar();
  if (c != 'Y' && c != 'y') {
    free(update_dat);
    fprintf(stderr, "Firmware update aborted.\n");
    return EXIT_FAILURE;
  }

  // Switching to boot mode if necessary
  if (handle->status == MP_STATUS_NORMAL) {
    fprintf(stderr, "Switching to bootloader... ");
    fflush(stderr);

    memset(msg, 0, sizeof(msg));
    msg[0] = TL866IIPLUS_SWITCH;
    format_int(&msg[4], TL866IIPLUS_BTLDR_MAGIC, 4, MP_LITTLE_ENDIAN);
    if (msg_send(handle->usb_handle, msg, 8)) {
      free(update_dat);
      return EXIT_FAILURE;
    }
    if (minipro_reset(handle)) {
      fprintf(stderr, "failed!\n");
      free(update_dat);
      return EXIT_FAILURE;
    }
    handle = minipro_open(NULL);
    if (!handle) {
      fprintf(stderr, "failed!\n");
      free(update_dat);
      return EXIT_FAILURE;
    }

    if (handle->status == MP_STATUS_NORMAL) {
      fprintf(stderr, "failed!\n");
      free(update_dat);
      return EXIT_FAILURE;
    }
    fprintf(stderr, "OK\n");
  }

  // Erase device
  fprintf(stderr, "Erasing... ");
  fflush(stderr);
  memset(msg, 0, sizeof(msg));
  msg[0] = TL866IIPLUS_BOOTLOADER_ERASE;
  if (msg_send(handle->usb_handle, msg, 8)) {
    fprintf(stderr, "\nErase failed!\n");
    free(update_dat);
    return EXIT_FAILURE;
  }
  memset(msg, 0, sizeof(msg));
  if (msg_recv(handle->usb_handle, msg, 8)) {
    fprintf(stderr, "\nErase failed!\n");
    free(update_dat);
    return EXIT_FAILURE;
  }
  if (msg[0] != TL866IIPLUS_BOOTLOADER_ERASE) {
    fprintf(stderr, "failed\n");
    free(update_dat);
    return EXIT_FAILURE;
  }

  // Reflash firmware
  fprintf(stderr, "OK\n");
  fprintf(stderr, "Reflashing... ");
  fflush(stderr);

  ptr = 1036;  // First firmware block
  for (int i = 0; i < blocks; i++) {
    msg[0] = TL866IIPLUS_BOOTLOADER_WRITE;
    msg[1] = update_dat[ptr + 12] & 0x7F;         // Xor table index
    msg[2] = 0;                                   // Data Length LSB
    msg[3] = 1;                                   // Data length MSB (256 bytes)
    memcpy(&msg[4], &update_dat[ptr + 8], 4);     // Destination address
    memcpy(&msg[8], &update_dat[ptr + 16], 256);  // 256  bytes data

    // Send the command to the endpoint 1
    if (msg_send(handle->usb_handle, msg, 8)) {
      fprintf(stderr, "\nReflash failed\n");
      free(update_dat);
      return EXIT_FAILURE;
    }

    // And the payload to the endpoints 2 and 3
    if (write_payload(handle->usb_handle, msg + 8, 256)) {
      fprintf(stderr, "\nReflash failed\n");
      free(update_dat);
      return EXIT_FAILURE;
    }

    // Check if the firmware block was successfully written
    memset(msg, 0, sizeof(msg));
    msg[0] = TL866IIPLUS_REQUEST_STATUS;
    if (msg_send(handle->usb_handle, msg, 8)) {
      fprintf(stderr, "\nReflash... Failed\n");
      free(update_dat);
      return EXIT_FAILURE;
    }
    memset(msg, 0, sizeof(msg));
    if (msg_recv(handle->usb_handle, msg, 32)) {
      fprintf(stderr, "\nReflash... Failed\n");
      free(update_dat);
      return EXIT_FAILURE;
    }
    if (msg[1]) {
      fprintf(stderr, "\nReflash... Failed\n");
      free(update_dat);
      return EXIT_FAILURE;
    }
    ptr += 272;
    fprintf(stderr, "\r\e[KReflashing... %2d%%", i * 100 / blocks);
    fflush(stderr);
  }

  // Last firmware block
  uint8_t block[2056];

  block[0] = TL866IIPLUS_BOOTLOADER_WRITE;
  block[1] = update_dat[ptr + 12] | 0x80;
  block[2] = 0;
  block[3] = 8;
  memcpy(&block[4], &update_dat[ptr + 8], 4);
  memcpy(&block[8], &update_dat[ptr + 16], 2048);
  free(update_dat);

  // Send the command to the endpoint 1
  if (msg_send(handle->usb_handle, block, 8)) {
    fprintf(stderr, "\nReflash failed\n");
    return EXIT_FAILURE;
  }

  // And the payload to the endpoints 2 and 3
  if (write_payload(handle->usb_handle, block + 8, 2048)) {
    fprintf(stderr, "\nReflash failed\n");
    return EXIT_FAILURE;
  }

  // Check if the firmware block was successfully written
  memset(msg, 0, sizeof(msg));
  msg[0] = TL866IIPLUS_REQUEST_STATUS;
  if (msg_send(handle->usb_handle, msg, 8)) {
    fprintf(stderr, "\nReflash failed!\n");
    return EXIT_FAILURE;
  }
  memset(msg, 0, sizeof(msg));
  if (msg_recv(handle->usb_handle, msg, 32)) {
    fprintf(stderr, "\nReflash failed!\n");
    return EXIT_FAILURE;
  }
  if (msg[1]) {
    fprintf(stderr, "\nReflash... Failed\n");
    return EXIT_FAILURE;
  }
  fprintf(stderr, "\r\e[KReflashing... 100%%\n");

  // Switching back to normal mode
  fprintf(stderr, "Resetting device... ");
  fflush(stderr);
  if (minipro_reset(handle)) {
    fprintf(stderr, "failed!\n");
    free(update_dat);
    return EXIT_FAILURE;
  }
  handle = minipro_open(NULL);
  if (!handle) {
    fprintf(stderr, "failed!\n");
    free(update_dat);
    return EXIT_FAILURE;
  }
  fprintf(stderr, "OK\n");
  if (handle->status != MP_STATUS_NORMAL) {
    fprintf(stderr, "Reflash... failed\n");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "Reflash... OK\n");
  return EXIT_SUCCESS;
}

int tl866iiplus_pin_test(minipro_handle_t *handle) {
  // Get the chip pin mask for testing
  pin_map_t *map = get_pin_map(handle->device->opts8 & 0xFF);
  if (!map) return EXIT_FAILURE;

  // Set all pins to input
  uint8_t msg[48], pins[40];

  // Set the desired output pins
  msg[0] = TL866IIPLUS_SET_DIR;
  memset(&msg[8], 0x01, 40);
  if (map->zero_c) {
    for (uint32_t i = 0; i < (map->zero_c & 0x03); i++) {
      msg[map->zero_t[i] + 8] = 0;
    }
  }
  // Set the ZIF socket pins direction
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Set internal port
  msg[0] = TL866IIPLUS_SET_DIV;
  memset(&msg[8], 0x01, 40);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Set ZIF socket pull-ups
  msg[0] = TL866IIPLUS_SET_PULLUPS;
  memset(&msg[28], 0x00, 20);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Put the right side of the ZIF socket (pin 21-40) to the logic one
  msg[0] = TL866IIPLUS_SET_OUT;
  memset(&msg[8], 0x00, 20);
  memset(&msg[28], 0x01, 20);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Read ZIF socket pins and save the right side pins status
  msg[0] = TL866IIPLUS_READ_PINS;
  if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;
  memcpy(pins, &msg[8], 20);

  // Set ZIF socket pull-ups
  msg[0] = TL866IIPLUS_SET_PULLUPS;
  memset(&msg[8], 0x00, 20);
  memset(&msg[28], 0x01, 20);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Put the left side of the ZIF socket (pin 1-20) to the logic one
  msg[0] = TL866IIPLUS_SET_OUT;
  memset(&msg[8], 0x01, 20);
  memset(&msg[28], 0x00, 20);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Read ZIF socket pins and save the left side pins status
  msg[0] = TL866IIPLUS_READ_PINS;
  if (msg_send(handle->usb_handle, msg, 8)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;
  memcpy(&pins[20], &msg[28], 20);

  // Reset internal port
  msg[0] = TL866IIPLUS_SET_DIV;
  memset(&msg[8], 0x00, 40);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Reset ZIF socket pins direction
  msg[0] = TL866IIPLUS_SET_DIR;
  memset(&msg[8], 0x01, 40);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Reset pull-ups
  msg[0] = TL866IIPLUS_SET_PULLUPS;
  memset(&msg[8], 0x01, 40);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Reset ZIF socket outputs
  msg[0] = TL866IIPLUS_SET_OUT;
  memset(&msg[8], 0x00, 40);
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // End of transaction
  msg[0] = TL866IIPLUS_END_TRANS;
  if (msg_send(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  // Now check for bad pin contact
  int ret = EXIT_SUCCESS;
  for (uint32_t i = 0; i < 40; i++) {
    if (map->mask[i]) {
      if (!pins[i]) {
        fprintf(stderr, "Bad contact on pin:%u\n", i + 1);
        ret = EXIT_FAILURE;
      }
    }
  }
  if (!ret) fprintf(stderr, "Pin test passed.\n");
  return ret;
}
