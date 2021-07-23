/*
 * minipro.c - Low level operations.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "database.h"
#include "minipro.h"
#include "tl866a.h"
#include "tl866iiplus.h"
#include "usb.h"

#define TL866A_RESET 0xFF
#define TL866IIPLUS_RESET 0x3F

#define CRC32_POLYNOMIAL 0xEDB88320


void format_int(uint8_t *out, uint32_t in, size_t size, uint8_t endianness) {
  uint32_t idx;
  size_t i;
  for (i = 0; i < size; i++) {
    idx = (endianness == MP_LITTLE_ENDIAN ? i : size - 1 - i);
    out[i] = (in & 0xFF << idx * 8) >> idx * 8;
  }
}

uint32_t load_int(uint8_t *buffer, size_t size, uint8_t endianness) {
  uint32_t idx, result = 0;
  size_t i;
  for (i = 0; i < size; i++) {
    idx = (endianness == MP_LITTLE_ENDIAN ? i : size - 1 - i);
    result |= (buffer[i] << idx * 8);
  }
  return result;
}

// Simple crc32
uint32_t crc32(uint8_t *data, size_t size, uint32_t initial) {
  uint32_t i, j, crc;
  crc = initial;
  for (i = 0; i < size; i++) {
    crc = crc ^ data[i];
    for (j = 0; j < 8; j++)
      crc = (crc >> 1) ^ (CRC32_POLYNOMIAL & (-(crc & 1)));
  }
  return crc;
}

minipro_handle_t *minipro_open(const char *device_name, uint8_t verbose) {
  minipro_handle_t *handle = malloc(sizeof(minipro_handle_t));
  if (handle == NULL) {
    if(verbose)
    	fprintf(stderr, "Out of memory!\n");
    return NULL;
  }

  handle->usb_handle = usb_open(verbose);
  if (!handle->usb_handle) {
    free(handle);
    return NULL;
  }

  minipro_report_info_t info;
  if (minipro_get_system_info(handle, &info)) return NULL;

  switch (info.device_version) {
    case MP_TL866A:
    case MP_TL866CS:
      switch (info.device_status) {
        case MP_STATUS_NORMAL:
        case MP_STATUS_BOOTLOADER:
          handle->status = info.device_status;
          break;
        default:
          minipro_close(handle);
          if (verbose)
        	  fprintf(stderr, "\nUnknown device status!\nExiting...\n");
          return NULL;
      }
      handle->model = info.device_version == MP_TL866A ? "TL866A" : "TL866CS";
      memcpy(handle->device_code, info.device_code, 8);
      memcpy(handle->serial_number, info.serial_number, 24);
      handle->minipro_begin_transaction = tl866a_begin_transaction;
      handle->minipro_end_transaction = tl866a_end_transaction;
      handle->minipro_protect_off = tl866a_protect_off;
      handle->minipro_protect_on = tl866a_protect_on;
      handle->minipro_get_ovc_status = tl866a_get_ovc_status;
      handle->minipro_read_block = tl866a_read_block;
      handle->minipro_write_block = tl866a_write_block;
      handle->minipro_get_chip_id = tl866a_get_chip_id;
      handle->minipro_spi_autodetect = tl866a_spi_autodetect;
      handle->minipro_read_fuses = tl866a_read_fuses;
      handle->minipro_write_fuses = tl866a_write_fuses;
      handle->minipro_erase = tl866a_erase;
      handle->minipro_unlock_tsop48 = tl866a_unlock_tsop48;
      handle->minipro_hardware_check = tl866a_hardware_check;
      handle->minipro_read_jedec_row = tl866a_read_jedec_row;
      handle->minipro_write_jedec_row = tl866a_write_jedec_row;
      handle->minipro_firmware_update = tl866a_firmware_update;
      handle->minipro_pin_test = NULL;
      break;
    case MP_TL866IIPLUS:
      handle->status = info.firmware_version_minor == 0 ? MP_STATUS_BOOTLOADER
                                                        : MP_STATUS_NORMAL;
      handle->model = "TL866II+";
      memcpy(handle->device_code, info.device_code, 8);
      memcpy(handle->serial_number, info.serial_number, 20);
      handle->minipro_begin_transaction = tl866iiplus_begin_transaction;
      handle->minipro_end_transaction = tl866iiplus_end_transaction;
      handle->minipro_get_chip_id = tl866iiplus_get_chip_id;
      handle->minipro_spi_autodetect = tl866iiplus_spi_autodetect;
      handle->minipro_read_block = tl866iiplus_read_block;
      handle->minipro_write_block = tl866iiplus_write_block;
      handle->minipro_protect_off = tl866iiplus_protect_off;
      handle->minipro_protect_on = tl866iiplus_protect_on;
      handle->minipro_erase = tl866iiplus_erase;
      handle->minipro_read_fuses = tl866iiplus_read_fuses;
      handle->minipro_write_fuses = tl866iiplus_write_fuses;
      handle->minipro_get_ovc_status = tl866iiplus_get_ovc_status;
      handle->minipro_unlock_tsop48 = tl866iiplus_unlock_tsop48;
      handle->minipro_hardware_check = tl866iiplus_hardware_check;
      handle->minipro_read_jedec_row = tl866iiplus_read_jedec_row;
      handle->minipro_write_jedec_row = tl866iiplus_write_jedec_row;
      handle->minipro_firmware_update = tl866iiplus_firmware_update;
      handle->minipro_pin_test = tl866iiplus_pin_test;
      handle->minipro_logic_ic_test = tl866iiplus_logic_ic_test;
      break;
    default:
      minipro_close(handle);
      if(verbose)
    	  fprintf(stderr, "Unknown programmer model!\n");
      return NULL;
  }

  handle->firmware =
      load_int(&info.firmware_version_minor, 2, MP_LITTLE_ENDIAN);
  sprintf(handle->firmware_str, "%02d.%d.%d", info.hardware_version,
          info.firmware_version_major, info.firmware_version_minor);
  handle->version = info.device_version;

  handle->device = NULL;
  if (device_name != NULL) {
    handle->device = get_device_by_name(handle->version, device_name);
    if (handle->device == NULL) {
      minipro_close(handle);
      if(verbose)
    	  fprintf(stderr, "Device %s not found!\n", device_name);
      return NULL;
    }
  }
  return handle;
}

void minipro_close(minipro_handle_t *handle) {
  usb_close(handle->usb_handle);
  free_device(handle->device);
  free(handle);
}

// Reset TL866 device
int minipro_reset(minipro_handle_t *handle) {
  uint8_t msg[8];
  uint8_t version = handle->version;

  memset(msg, 0, sizeof(msg));
  msg[0] = version == MP_TL866IIPLUS ? TL866IIPLUS_RESET : TL866A_RESET;
  if (msg_send(handle->usb_handle, msg, version == MP_TL866IIPLUS ? 8 : 4)) {
    return EXIT_FAILURE;
  }

  uint32_t wait = 200;  // 20 Sec wait to disappear
  do {
    wait--;
    usleep(100000);
  } while (minipro_get_devices_count(version) && wait);
  if (!wait) {
    return EXIT_FAILURE;
  }

  wait = 200;  // 20 Sec wait to appear
  do {
    wait--;
    usleep(100000);
  } while (!minipro_get_devices_count(version) && wait);
  if (!wait) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

void minipro_print_system_info(minipro_handle_t *handle) {
  uint16_t expected_firmware;
  char *expected_firmware_str;

  switch (handle->version) {
    case MP_TL866A:
    case MP_TL866CS:
      expected_firmware = TL866A_FIRMWARE_VERSION;
      expected_firmware_str = TL866A_FIRMWARE_STRING;
      break;
    case MP_TL866IIPLUS:
      expected_firmware = TL866IIPLUS_FIRMWARE_VERSION;
      expected_firmware_str = TL866IIPLUS_FIRMWARE_STRING;
  }

  if (handle->status == MP_STATUS_BOOTLOADER) {
    fprintf(stderr, "Found %s ", handle->model);
    return;
  }

  fprintf(stderr, "Found %s %s (%#03x)\n", handle->model, handle->firmware_str,
          handle->firmware);

  if (handle->firmware < expected_firmware) {
    fprintf(stderr, "Warning: Firmware is out of date.\n");
    fprintf(stderr, "  Expected  %s (%#03x)\n", expected_firmware_str,
            expected_firmware);
    fprintf(stderr, "  Found     %s (%#03x)\n", handle->firmware_str,
            handle->firmware);
  } else if (handle->firmware > expected_firmware) {
    fprintf(stderr, "Warning: Firmware is newer than expected.\n");
    fprintf(stderr, "  Expected  %s (%#03x)\n", expected_firmware_str,
            expected_firmware);
    fprintf(stderr, "  Found     %s (%#03x)\n", handle->firmware_str,
            handle->firmware);
  }
  // fprintf(stderr, "Device code:%s\nSerial code:%s\n", handle->device_code,
  // handle->serial_number);
}

int minipro_get_system_info(minipro_handle_t *handle,
                            minipro_report_info_t *info) {
  uint8_t msg[sizeof(minipro_report_info_t)];

  memset(info, 0x0, sizeof(*info));
  memset(msg, 0x0, sizeof(msg));
  if (msg_send(handle->usb_handle, msg, 5)) return EXIT_FAILURE;
  if (msg_recv(handle->usb_handle, msg, sizeof(msg))) return EXIT_FAILURE;

  switch (msg[6]) {
    case MP_TL866IIPLUS:
      msg[7] = 0;
      memcpy(info, msg, sizeof(*info));

      // Manually fixup 16-bit fields to account for endianness
      info->report_size = load_int(msg+2, 2, MP_LITTLE_ENDIAN);
      info->device_version = load_int(msg+6, 2, MP_LITTLE_ENDIAN);

      break;
    case MP_TL866A:
    case MP_TL866CS:
      info->echo = msg[0];
      info->device_status = msg[1];
      info->report_size = load_int((msg + 2), 2, MP_LITTLE_ENDIAN);
      info->firmware_version_minor = msg[4];
      info->firmware_version_major = msg[5];
      info->device_version = msg[6];
      memcpy(info->device_code, (msg + 7), 8);
      memcpy(info->serial_number, (msg + 15), 24);
      info->hardware_version = msg[39];
      break;
    default:
      minipro_close(handle);
      fprintf(stderr, "Unknown Device!");
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int minipro_begin_transaction(minipro_handle_t *handle) {
  assert(handle != NULL);
  // fprintf(stderr, "start transaction\n");
  if (handle->minipro_begin_transaction) {
    return handle->minipro_begin_transaction(handle);
  } else {
    fprintf(stderr, "%s: begin_transaction not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_end_transaction(minipro_handle_t *handle) {
  assert(handle != NULL);
  // fprintf(stderr, "end transaction\n");
  if (handle->minipro_end_transaction) {
    return handle->minipro_end_transaction(handle);
  } else {
    fprintf(stderr, "%s: end_transaction not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_protect_off(minipro_handle_t *handle) {
  assert(handle != NULL);

  if (handle->minipro_protect_off) {
    return handle->minipro_protect_off(handle);
  } else {
    fprintf(stderr, "%s: protect_off not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_protect_on(minipro_handle_t *handle) {
  assert(handle != NULL);

  if (handle->minipro_protect_on) {
    return handle->minipro_protect_on(handle);
  } else {
    fprintf(stderr, "%s: protect_on not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_get_ovc_status(minipro_handle_t *handle, minipro_status_t *status,
                           uint8_t *ovc) {
  assert(handle != NULL);
  // fprintf(stderr, "get ovc\n");
  if (status) {
    memset(status, 0x00, sizeof(*status));
  }

  if (handle->minipro_get_ovc_status) {
    return handle->minipro_get_ovc_status(handle, status, ovc);
  }
  fprintf(stderr, "%s: get_ovc_status not implemented\n", handle->model);
  return EXIT_FAILURE;
}

int minipro_erase(minipro_handle_t *handle) {
  assert(handle != NULL);
  // fprintf(stderr, "Erase\n");
  if (handle->minipro_erase) {
    return handle->minipro_erase(handle);
  }
  fprintf(stderr, "%s: erase not implemented\n", handle->model);
  return EXIT_FAILURE;
}

int minipro_read_block(minipro_handle_t *handle, uint8_t type, uint32_t addr,
                       uint8_t *buffer, size_t len) {
  assert(handle != NULL);
  // fprintf(stderr, "Read block\n");
  if (handle->minipro_read_block) {
    return handle->minipro_read_block(handle, type, addr, buffer, len);
  } else {
    fprintf(stderr, "%s: read_block not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_write_block(minipro_handle_t *handle, uint8_t type, uint32_t addr,
                        uint8_t *buffer, size_t len) {
  assert(handle != NULL);
  // fprintf(stderr, "Write block\n");
  if (handle->minipro_write_block) {
    return handle->minipro_write_block(handle, type, addr, buffer, len);
  } else {
    fprintf(stderr, "%s: write_block not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
int minipro_get_chip_id(minipro_handle_t *handle, uint8_t *type,
                        uint32_t *device_id) {
  assert(handle != NULL);
  // fprintf(stderr, "get id\n");
  if (handle->minipro_get_chip_id) {
    return handle->minipro_get_chip_id(handle, type, device_id);
  }
  fprintf(stderr, "%s: get_chip_id not implemented\n", handle->model);
  return EXIT_FAILURE;
}

int minipro_spi_autodetect(minipro_handle_t *handle, uint8_t type,
                        uint32_t *device_id) {
  assert(handle != NULL);
  if (handle->minipro_spi_autodetect) {
    return handle->minipro_spi_autodetect(handle, type, device_id);
  }
  fprintf(stderr, "%s: spi_autodetect not implemented\n", handle->model);
  return EXIT_FAILURE;
}

int minipro_read_fuses(minipro_handle_t *handle, uint8_t type, size_t length,
                       uint8_t items_count, uint8_t *buffer) {
  assert(handle != NULL);

  if (handle->minipro_read_fuses) {
    return handle->minipro_read_fuses(handle, type, length, items_count,
                                      buffer);
  } else {
    fprintf(stderr, "%s: read_fuses not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_write_fuses(minipro_handle_t *handle, uint8_t type, size_t length,
                        uint8_t items_count, uint8_t *buffer) {
  assert(handle != NULL);

  if (handle->minipro_write_fuses) {
    return handle->minipro_write_fuses(handle, type, length, items_count,
                                       buffer);
  } else {
    fprintf(stderr, "%s: write_fuses not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_write_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                            uint8_t row, uint8_t flags, size_t size) {
  assert(handle != NULL);
  if (handle->minipro_write_jedec_row) {
    return handle->minipro_write_jedec_row(handle, buffer, row, flags, size);
  } else {
    fprintf(stderr, "%s: write jedec row not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_read_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                           uint8_t row, uint8_t flags, size_t size) {
  assert(handle != NULL);
  if (handle->minipro_read_jedec_row) {
    return handle->minipro_read_jedec_row(handle, buffer, row, flags, size);
  } else {
    fprintf(stderr, "%s: read jedec row not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

// Unlocking the TSOP48 adapter.
int minipro_unlock_tsop48(minipro_handle_t *handle, uint8_t *status) {
  assert(handle != NULL);

  if (handle->minipro_unlock_tsop48) {
    return handle->minipro_unlock_tsop48(handle, status);
  }
  fprintf(stderr, "%s: unlock_tsop48 not implemented\n", handle->model);
  return EXIT_FAILURE;
}

// Minipro hardware check
int minipro_hardware_check(minipro_handle_t *handle) {
  assert(handle != NULL);

  if (handle->minipro_hardware_check) {
    return handle->minipro_hardware_check(handle);
  } else {
    fprintf(stderr, "%s: hardware_check not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

int minipro_firmware_update(minipro_handle_t *handle, const char *firmware) {
  assert(handle != NULL);
  if (handle->minipro_firmware_update) {
    return handle->minipro_firmware_update(handle, firmware);
  } else {
    fprintf(stderr, "%s: firmware update not implemented\n", handle->model);
  }
  return EXIT_FAILURE;
}

// Pin contact test
int minipro_pin_test(minipro_handle_t *handle) {
	  assert(handle != NULL);
	  if (handle->minipro_pin_test) {
	    return handle->minipro_pin_test(handle);
	  } else {
	    fprintf(stderr, "%s: pin test not implemented\n", handle->model);
	  }
	  return EXIT_FAILURE;
}

int minipro_logic_ic_test(minipro_handle_t *handle) {
  assert(handle != NULL);
  if (handle->minipro_logic_ic_test) {
    return handle->minipro_logic_ic_test(handle);
  }
  fprintf(stderr, "%s: logic IC test not implemented\n", handle->model);
  return EXIT_FAILURE;
}
