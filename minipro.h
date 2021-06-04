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

#include <stdint.h>
#include <stddef.h>

#define MP_TL866A 1
#define MP_TL866CS 2
#define MP_TL866IIPLUS 5
#define MP_STATUS_NORMAL 1
#define MP_STATUS_BOOTLOADER 2

#define MP_CODE 0x00
#define MP_DATA 0x01

#define MP_FUSE_USER 0x00
#define MP_FUSE_CFG 0x01
#define MP_FUSE_LOCK 0x02

// ICSP
#define MP_ICSP_ENABLE 0x80
#define MP_ICSP_VCC 0x01

// TSOP48
#define MP_TSOP48_TYPE_V3 0x00
#define MP_TSOP48_TYPE_NONE 0x01
#define MP_TSOP48_TYPE_V0 0x02
#define MP_TSOP48_TYPE_FAKE1 0x03
#define MP_TSOP48_TYPE_FAKE2 0x04

#define MP_ID_TYPE1 0x01
#define MP_ID_TYPE2 0x02
#define MP_ID_TYPE3 0x03
#define MP_ID_TYPE4 0x04
#define MP_ID_TYPE5 0x05

// Various
#define MP_LITTLE_ENDIAN 0
#define MP_BIG_ENDIAN 1

// Opts 4
#define MP_ERASE_MASK 0x00000010
#define MP_ID_MASK 0x00000020
#define MP_PROTECT_MASK 0x0000C000
#define MP_DATA_BUS_WIDTH 0x00002000

// Opts 1
// for ATF20V10C and ATF16V8C variants
#define LAST_JEDEC_BIT_IS_POWERDOWN_ENABLE (0x10)
#define POWERDOWN_MODE_DISABLE (0x20)
#define ATF_IN_PAL_COMPAT_MODE (0x40)

// Opts 7
#define MP_VOLTAGES1 0x0006
#define MP_VOLTAGES2 0x0007

// Opts 7 for PIC family
//   PIC instruction word width (not clear which exact bit means what)
#define PIC_INSTR_WORD_WIDTH_MASK 0xff
#define PIC_INSTR_WORD_WIDTH_12 0x84
#define PIC_INSTR_WORD_WIDTH_14 0x83
#define PIC_INSTR_WORD_WIDTH_16_PIC18F 0x82
#define PIC_INSTR_WORD_WIDTH_16_PIC18J 0x85

// Adapters
#define ADAPTER_MASK 0x000000FF
#define TSOP48_ADAPTER 0x00000001
#define SOP44_ADAPTER 0x00000002
#define TSOP40_ADAPTER 0x00000003
#define VSOP40_ADAPTER 0x00000004
#define TSOP32_ADAPTER 0x00000005
#define SOP56_ADAPTER 0x00000006

// Package
#define PIN_COUNT_MASK 0X7F000000
#define SMD_MASK 0X80000000

// PLCC Mask
#define PLCC_MASK 0xFF000000
#define PLCC32_ADAPTER 0xFF000000
#define PLCC44_ADAPTER 0xFD000000

// ICSP MASK
#define ICSP_MASK 0x0000FF00

// Protocols
#define PLD_PROTOCOL_16V8 0xE0
#define PLD_PROTOCOL_20V8 0xE1
#define PLD_PROTOCOL_22V10 0xE2
#define PLD_PROTOCOL2_16V8 0x2A
#define PLD_PROTOCOL2_20V8 0x2B
#define PLD_PROTOCOL2_22V10 0x2C

#define TL866IIP_PIC_PROTOCOL_PIC18_ICSP 0x17
#define TL866IIP_PIC_PROTOCOL_PIC18 0x19
#define TL866IIP_PIC_PROTOCOL_1 0x18
#define TL866IIP_PIC_PROTOCOL_2 0x1a
#define TL866IIP_PIC_PROTOCOL_3 0x1b
#define TL866IIP_PIC_PROTOCOL_4 0x1c

#define TL866A_PIC_PROTOCOL_PIC18_ICSP 0x62
#define TL866A_PIC_PROTOCOL_PIC18 0x64
#define TL866A_PIC_PROTOCOL_1 0x63
#define TL866A_PIC_PROTOCOL_2 0x65
#define TL866A_PIC_PROTOCOL_3 0x66
#define TL866A_PIC_PROTOCOL_4 0x67


// Helper macros
#define PIN_COUNT(x) (((x)&PIN_COUNT_MASK) >> 24)
#define WORD_SIZE(device) (((device)->opts4 & 0xFF000000) == 0x01000000 ? 2 : 1)

typedef struct device {
  char name[40];
  uint8_t type;

  uint8_t protocol_id;
  uint8_t variant;
  uint16_t read_buffer_size;
  uint16_t write_buffer_size;
  uint32_t code_memory_size;  // Presenting for every device
  uint32_t data_memory_size;
  uint32_t data_memory2_size;
  uint32_t chip_id;  // A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48)
  uint8_t chip_id_bytes_count;
  uint32_t opts1;
  uint16_t opts2;
  uint32_t opts3;
  uint32_t opts4;
  uint32_t opts5;
  uint32_t opts6;
  uint16_t opts7;
  uint32_t opts8;
  uint32_t package_details;  // pins count or image ID for some devices
  void *config;  // Configuration bytes that's presenting in some architectures

  uint8_t voltage;
  uint8_t pin_count;
  uint8_t vector_count;
  uint8_t *vectors;
} device_t;

typedef struct minipro_status {
  uint8_t error;
  uint32_t address;
  uint32_t c1;
  uint32_t c2;
} minipro_status_t;

typedef struct cmdopts_s {
  char *filename;
  char *device;
  enum { UNSPECIFIED = 0, CODE, DATA, CONFIG } page;
  enum { NO_ACTION = 0, READ, WRITE, ERASE, VERIFY, BLANK_CHECK, LOGIC_IC_TEST } action;
  enum { NO_FORMAT = 0, IHEX, SREC} format;
  uint8_t no_erase;
  uint8_t no_protect_off;
  uint8_t no_protect_on;
  uint8_t size_error;
  uint8_t size_nowarn;
  uint8_t no_verify;
  uint8_t icsp;
  uint8_t idcheck_skip;
  uint8_t idcheck_continue;
  uint8_t idcheck_only;
  uint8_t pincheck;
  uint8_t is_pipe;
  uint8_t version;
} cmdopts_t;

typedef struct minipro_handle {
  char *model;
  char firmware_str[16];
  char device_code[9];
  char serial_number[25];
  uint32_t firmware;
  uint8_t status;
  uint8_t version;

  device_t *device;
  uint8_t icsp;

  void *usb_handle;
  cmdopts_t *cmdopts;

  int (*minipro_begin_transaction)(struct minipro_handle *);
  int (*minipro_end_transaction)(struct minipro_handle *);
  int (*minipro_protect_off)(struct minipro_handle *);
  int (*minipro_protect_on)(struct minipro_handle *);
  int (*minipro_get_ovc_status)(struct minipro_handle *,
                                struct minipro_status *, uint8_t *);
  int (*minipro_read_block)(struct minipro_handle *, uint8_t, uint32_t,
                            uint8_t *, size_t);
  int (*minipro_write_block)(struct minipro_handle *, uint8_t, uint32_t,
                             uint8_t *, size_t);
  int (*minipro_get_chip_id)(struct minipro_handle *, uint8_t *, uint32_t *);
  int (*minipro_spi_autodetect)(struct minipro_handle *, uint8_t, uint32_t *);
  int (*minipro_read_fuses)(struct minipro_handle *, uint8_t, size_t, uint8_t,
                            uint8_t *);
  int (*minipro_write_fuses)(struct minipro_handle *, uint8_t, size_t, uint8_t,
                             uint8_t *);
  int (*minipro_erase)(struct minipro_handle *);
  int (*minipro_unlock_tsop48)(struct minipro_handle *, uint8_t *);
  int (*minipro_hardware_check)(struct minipro_handle *);
  int (*minipro_write_jedec_row)(struct minipro_handle *, uint8_t *, uint8_t,
                                 uint8_t, size_t);
  int (*minipro_read_jedec_row)(struct minipro_handle *, uint8_t *, uint8_t,
                                uint8_t, size_t);
  int (*minipro_firmware_update)(struct minipro_handle *, const char *);
  int (*minipro_pin_test)(struct minipro_handle *);
  int (*minipro_logic_ic_test)(struct minipro_handle *);
} minipro_handle_t;

typedef struct minipro_report_info {
  uint8_t echo;
  uint8_t device_status;
  uint16_t report_size;
  uint8_t firmware_version_minor;
  uint8_t firmware_version_major;
  uint16_t device_version;
  uint8_t device_code[8];
  uint8_t serial_number[24];
  uint8_t hardware_version;
  uint8_t buffer[20]; /* for future autoelectric expansion */
} minipro_report_info_t;

enum verbosity { NO_VERBOSE = 0, VERBOSE };

// These are old byte_utils functions
void format_int(uint8_t *out, uint32_t in, size_t size, uint8_t endianness);
uint32_t load_int(uint8_t *buffer, size_t size, uint8_t endianness);

// Helper functions
int minipro_get_system_info(minipro_handle_t *handle,
                            minipro_report_info_t *info);
void minipro_print_system_info(minipro_handle_t *handle);
uint32_t crc32(uint8_t *data, size_t size, uint32_t initial);
int minipro_reset(minipro_handle_t *handle);
int minipro_get_devices_count(uint8_t version);

/*
 * Standard interface functions compatible with both TL866A/TL866II+
 * programmers. High level logic should include this file not the
 * tl866a.h/tl866iiplus.h device specific headers. These functions will return 0
 * on success and 1 on failure. This way we always return the success status to
 * the higher logic routines to exit cleanly leaving the device in a clean
 * state.
 */
minipro_handle_t *minipro_open(const char *device_name, uint8_t verbose);
void minipro_close(minipro_handle_t *handle);
int minipro_begin_transaction(minipro_handle_t *handle);
int minipro_end_transaction(minipro_handle_t *handle);
int minipro_protect_off(minipro_handle_t *handle);
int minipro_protect_on(minipro_handle_t *handle);
int minipro_get_ovc_status(minipro_handle_t *handle, minipro_status_t *status,
                           uint8_t *ovc);
int minipro_read_block(minipro_handle_t *handle, uint8_t type, uint32_t addr,
                       uint8_t *buffer, size_t len);
int minipro_write_block(minipro_handle_t *handle, uint8_t type, uint32_t addr,
                        uint8_t *bufffer, size_t len);
int minipro_get_chip_id(minipro_handle_t *handle, uint8_t *type,
                        uint32_t *device_id);
int minipro_spi_autodetect(minipro_handle_t *handle, uint8_t type,
                           uint32_t *device_id);
int minipro_read_fuses(minipro_handle_t *handle, uint8_t type, size_t length,
                       uint8_t items_count, uint8_t *buffer);
int minipro_write_fuses(minipro_handle_t *handle, uint8_t type, size_t length,
                        uint8_t items_count, uint8_t *buffer);
int minipro_write_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                            uint8_t row, uint8_t flags, size_t size);
int minipro_read_jedec_row(minipro_handle_t *handle, uint8_t *buffer,
                           uint8_t row, uint8_t flags, size_t size);
int minipro_erase(minipro_handle_t *handle);
int minipro_unlock_tsop48(minipro_handle_t *handle, uint8_t *status);
int minipro_hardware_check(minipro_handle_t *handle);
int minipro_firmware_update(minipro_handle_t *handle, const char *firmware);
int minipro_pin_test(minipro_handle_t *handle);
int minipro_logic_ic_test(minipro_handle_t *handle);

#endif
