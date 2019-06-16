/*
 * main.c - User interface and high-level operations.
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

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "database.h"
#include "jedec.h"
#include "minipro.h"
#include "version.h"

struct {
  int (*action)(const char *, minipro_handle_t *handle);
  char *filename;
  char *device;
  enum { UNSPECIFIED = 0, CODE, DATA, CONFIG } page;
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
  int vpp;
  int vdd;
  int vcc;
  int pulse_delay;
} cmdopts;

#define VPP_VOLTAGE 0
#define VCC_VOLTAGE 1

struct voltage_s {
  const char *name;
  uint8_t value;
} vpp_voltages[] = {{"10", 0x04}, {"12.5", 0x00}, {"13.5", 0x03}, {"14", 0x05},
                    {"16", 0x01}, {"18", 0x06},   {"21", 0x02},   {NULL, 0x00}},
  vcc_voltages[] = {{"3.3", 0x02}, {"4", 0x01},    {"4.5", 0x05}, {"5", 0x00},
                    {"5.5", 0x04}, {"6.25", 0x03}, {NULL, 0x00}};

int action_read(const char *filename, minipro_handle_t *handle);
int action_write(const char *filename, minipro_handle_t *handle);

void print_version_and_exit() {
  char output[] =
      "minipro version %s     A free and open TL866XX programmer\n"
      "Build date:\t%s\n"
      "Commit date:\t%s\n"
      "Git commit:\t%s\n"
      "Git branch:\t%s\n";
  fprintf(stderr, output, VERSION, build_timestamp, GIT_DATE, GIT_HASH,
          GIT_BRANCH);
  exit(EXIT_SUCCESS);
}

void print_help_and_exit(char *progname) {
  char usage[] =
      "minipro version %s     A free and open TL866XX programmer\n"
      "Usage: %s [options]\n"
      "options:\n"
      "	-l		List all supported devices\n"
      "	-L <search>	List devices like this\n"
      "	-d <device>	Show device information\n"
      "	-D 		Just read the chip ID\n"
      "	-r <filename>	Read memory\n"
      "	-w <filename>	Write memory\n"
      "	-e 		Do NOT erase device\n"
      "	-u 		Do NOT disable write-protect\n"
      "	-P 		Do NOT enable write-protect\n"
      "	-v		Do NOT verify after write\n"
      "	-p <device>	Specify device (use quotes)\n"
      "	-c <type>	Specify memory type (optional)\n"
      "			Possible values: code, data, config\n"
      "	-o <option>	Specify various programming options\n"
      "			For multiple options use -o for each option\n"
      "			Programming voltage     <vpp=value>   (10, 12.5, 13.5, "
      "14, 16, 18, 21)\n"
      "			VDD write voltage       <vdd=value>   (3.3, 4, 4.5, 5, "
      "5.5, 6.25)\n"
      "			VCC verify voltage      <vcc=value>   (3.3, 4, 4.5, 5, "
      "5.5, 6.25)\n"
      "			Programming pulse delay <pulse=value> (0-65535 usec)\n"
      "	-i		Use ICSP\n"
      "	-I		Use ICSP (without enabling Vcc)\n"
      "	-s		Do NOT error on file size mismatch (only a warning)\n"
      "	-S		No warning message for file size mismatch (can't "
      "combine with -s)\n"
      "	-x		Do NOT attempt to read ID (only valid in read mode)\n"
      "	-y		Do NOT error on ID mismatch\n"
      "	-V		Show version information\n"
      "	-t		Start hardware check\n"
      "	-f <filename>	Update firmware. <filename> should point to the "
      "update.dat file\n"
      "	-h		Show help (this text)\n";
  fprintf(stderr, usage, VERSION, basename(progname));
  exit(EXIT_SUCCESS);
}

void print_devices_and_exit(const char *device_name) {
  minipro_handle_t *handle;
  int count = minipro_get_devices_count(MP_TL866A) +
              minipro_get_devices_count(MP_TL866IIPLUS);
  if (!count) {
    fprintf(stderr,
            "No TL866 device found. Which database do you want to display?\n1) "
            "TL866A\n2) TL866II+\n3) Abort\n");
    handle = malloc(sizeof(minipro_handle_t));
    if (handle == NULL) {
      fprintf(stderr, "Out of memory!\n");
      exit(EXIT_FAILURE);
    }
    char c = getchar();
    switch (c) {
      case '1':
        handle->version = MP_TL866A;
        break;
      case '2':
        handle->version = MP_TL866IIPLUS;
        break;
      default:
        fprintf(stderr, "Aborted.\n");
        exit(EXIT_FAILURE);
    }
  } else {
    handle = minipro_open(NULL);
    if (!handle) exit(EXIT_FAILURE);
    minipro_print_system_info(handle);
    minipro_close(handle);
  }
  if (isatty(STDERR_FILENO) && device_name == NULL) {
    // stdout is a terminal, opening pager
    signal(SIGINT, SIG_IGN);
    char *pager_program = getenv("PAGER");
    if (!pager_program) pager_program = "less";
    FILE *pager = popen(pager_program, "w");
    dup2(fileno(pager), STDERR_FILENO);
  }

  device_t *device;
  for (device = get_device_table(handle); device[0].name;
       device = &(device[1])) {
    if (device_name == NULL ||
        strcasestr(device[0].name, device_name)) {
      fprintf(stderr, "%s\n", device->name);
    }
  }
  if (!count) free(handle);
  exit(EXIT_SUCCESS);
}

void print_device_info_and_exit(const char *device_name) {
  minipro_handle_t *handle = minipro_open(device_name);
  if (!handle) {
    exit(EXIT_FAILURE);
  }
  minipro_print_system_info(handle);
  minipro_close(handle);

  fprintf(stderr, "Name: %s\n", handle->device->name);

  /* Memory shape */
  fprintf(stderr, "Memory: %u",
          handle->device->code_memory_size / WORD_SIZE(handle->device));
  switch (handle->device->opts4 & 0xFF000000) {
    case 0x00000000:
      fprintf(stderr, " Bytes");
      break;
    case 0x01000000:
      fprintf(stderr, " Words");
      break;
    case 0x02000000:
      fprintf(stderr, " Bits");
      break;
    default:
      fprintf(stderr, "Unknown memory shape: 0x%x\n",
              handle->device->opts4 & 0xFF000000);
      exit(EXIT_FAILURE);
  }
  if (handle->device->data_memory_size) {
    fprintf(stderr, " + %u Bytes", handle->device->data_memory_size);
  }
  if (handle->device->data_memory2_size) {
    fprintf(stderr, " + %u Bytes", handle->device->data_memory2_size);
  }
  fprintf(stderr, "\n");

  uint8_t package_details[4];
  format_int(package_details, handle->device->package_details, 4,
             MP_LITTLE_ENDIAN);
  /* Package info */
  fprintf(stderr, "Package: ");
  if (package_details[0]) {
    fprintf(stderr, "Adapter%03d.JPG\n", package_details[0]);
  } else if (package_details[3]) {
    fprintf(stderr, "DIP%d\n", package_details[3] & 0x7F);
  } else {
    fprintf(stderr, "ICSP only\n");
  }

  /* ISP connection info */
  fprintf(stderr, "ICSP: ");
  if (package_details[1]) {
    fprintf(stderr, "ICP%03d.JPG\n", package_details[1]);
  } else {
    fprintf(stderr, "-\n");
  }

  fprintf(stderr, "Protocol: 0x%02x\n", handle->device->protocol_id);
  fprintf(stderr, "Read buffer size: %u Bytes\n",
          handle->device->read_buffer_size);
  fprintf(stderr, "Write buffer size: %u Bytes\n",
          handle->device->write_buffer_size);

  exit(EXIT_SUCCESS);
}

void get_voltage(uint8_t value, char **name, uint8_t type) {
  struct voltage_s *voltage = type == VPP_VOLTAGE ? vpp_voltages : vcc_voltages;
  while (voltage->name != NULL) {
    if (voltage->value == value) {
      *name = (char *)voltage->name;
      return;
    }
    voltage++;
  }
  *name = "unknown";
  return;
}

int set_voltage(char *value, int *target, uint8_t type) {
  struct voltage_s *voltage = type == VPP_VOLTAGE ? vpp_voltages : vcc_voltages;
  while (voltage->name != NULL) {
    if (!strcasecmp(voltage->name, value)) {
      *target = voltage->value;
      return EXIT_SUCCESS;
    }
    voltage++;
  }
  return EXIT_FAILURE;
}

int parse_options() {
  char option[64], value[64];
  uint32_t v;
  char *p_end;
  if (sscanf(optarg, "%[^=]=%[^=]", option, value) != 2) return EXIT_FAILURE;
  if (!strcasecmp(option, "pulse")) {
    // Parse the numeric value
    errno = 0;
    v = strtoul(value, &p_end, 10);
    if ((p_end == value) || errno) return EXIT_FAILURE;
    if (v > 0xffff) return EXIT_FAILURE;
    cmdopts.pulse_delay = (uint16_t)v;
  } else if (!strcasecmp(option, "vpp")) {
    if (set_voltage(value, &cmdopts.vpp, VPP_VOLTAGE)) return EXIT_FAILURE;
  } else if (!strcasecmp(option, "vdd")) {
    if (set_voltage(value, &cmdopts.vdd, VCC_VOLTAGE)) return EXIT_FAILURE;
  } else if (!strcasecmp(option, "vcc")) {
    if (set_voltage(value, &cmdopts.vcc, VCC_VOLTAGE)) return EXIT_FAILURE;
  } else
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

void hardware_check_and_exit() {
  minipro_handle_t *handle = minipro_open(NULL);
  if (!handle) {
    exit(EXIT_FAILURE);
  }

  minipro_print_system_info(handle);
  if (handle->status == MP_STATUS_BOOTLOADER) {
    fprintf(stderr, "in bootloader mode!\nExiting...\n");
    exit(EXIT_FAILURE);
  }

  int ret = minipro_hardware_check(handle);
  minipro_close(handle);
  exit(ret);
}

void firmware_update_and_exit(const char *firmware) {
  minipro_handle_t *handle = minipro_open(NULL);
  if (!handle) {
    exit(EXIT_FAILURE);
  }
  minipro_print_system_info(handle);
  if (handle->status == MP_STATUS_BOOTLOADER)
    fprintf(stderr, "in bootloader mode!\n");
  int ret = minipro_firmware_update(handle, firmware);
  minipro_close(handle);
  exit(ret);
}

void parse_cmdline(int argc, char **argv) {
  char c;
  memset(&cmdopts, 0, sizeof(cmdopts));
  cmdopts.vpp = -1;
  cmdopts.vcc = -1;
  cmdopts.vdd = -1;
  cmdopts.pulse_delay = -1;

  while ((c = getopt(argc, argv, "lL:d:euPvxyr:w:p:c:o:iIsSVhDtf:")) != -1) {
    switch (c) {
      case 'l':
        print_devices_and_exit(NULL);
        break;

      case 'L':
        print_devices_and_exit(optarg);
        break;
        break;

      case 'd':
        print_device_info_and_exit(optarg);
        break;

      case 'e':
        cmdopts.no_erase = 1;  // 1= do not erase
        break;

      case 'u':
        cmdopts.no_protect_off = 1;  // 1= do not disable write protect
        break;

      case 'P':
        cmdopts.no_protect_on = 1;  // 1= do not enable write protect
        break;

      case 'v':
        cmdopts.no_verify = 1;  // 1= do not verify
        break;

      case 'x':
        cmdopts.idcheck_skip = 1;  // 1= do not test id at all
        break;

      case 'y':
        cmdopts.idcheck_continue = 1;  // 1= do not stop on id mismatch
        break;

      case 'p':
        if (!strcasecmp(optarg, "help")) print_devices_and_exit(NULL);
        cmdopts.device = strdup(optarg);
        break;

      case 'c':
        if (!strcasecmp(optarg, "code")) cmdopts.page = CODE;
        if (!strcasecmp(optarg, "data")) cmdopts.page = DATA;
        if (!strcasecmp(optarg, "config")) cmdopts.page = CONFIG;
        if (!cmdopts.page) {
          fprintf(stderr, "Unknown memory type");
          exit(EXIT_FAILURE);
        }
        break;

      case 'r':
        cmdopts.action = action_read;
        cmdopts.filename = optarg;
        break;

      case 'w':
        cmdopts.action = action_write;
        cmdopts.filename = optarg;
        break;

      case 'i':
        cmdopts.icsp = MP_ICSP_ENABLE | MP_ICSP_VCC;
        break;

      case 'I':
        cmdopts.icsp = MP_ICSP_ENABLE;
        break;

      case 'S':
        cmdopts.size_nowarn = 1;
        cmdopts.size_error = 1;
        break;

      case 's':
        cmdopts.size_error = 1;
        break;

      case 'D':
        cmdopts.idcheck_only = 1;
        break;

      case 'h':
        print_help_and_exit(argv[0]);
        break;

      case 'V':
        print_version_and_exit();
        break;

      case 't':
        hardware_check_and_exit();
        break;

      case 'o':
        if (parse_options()) {
          fprintf(stderr, "Invalid option '%s'\n", optarg);
          exit(EXIT_FAILURE);
        }
        break;
      case 'f':
        firmware_update_and_exit(optarg);
        break;
      default:
        print_help_and_exit(argv[0]);
        break;
    }
  }
}

int get_config_value(uint8_t *buffer, size_t buffer_size, const char *name,
                     size_t name_size, uint32_t *value) {
  uint8_t *cur, *eol, *val;
  for (;;) {
    cur = memmem(buffer, buffer_size, name, name_size);  // find the line
    if (cur == NULL) return EXIT_FAILURE;
    eol = memmem(cur, buffer_size, (char *)"\n", 1);  // find the end of line
    if (cur == NULL) return EXIT_FAILURE;
    size_t len = eol - cur;
    cur = memmem(cur, len, (char *)"=",
                 1);  // find the '=' sign in the current line
    if (cur == NULL) return EXIT_FAILURE;
    cur = memmem(cur, len, (char *)"0x",
                 1);  // find the value in the current line
    if (cur == NULL) return EXIT_FAILURE;
    uint8_t num[len];
    val = num;
    cur += 2;  // Advances the pointer to the first numeric character
    while (cur < eol) {
      if (isxdigit(*cur++))  // check for hex digit
      {
        *val++ = *(cur - 1);  // put it in the buffer
      }
    }
    // here we reached the end of line
    *val = 0;        // insert null terminated string char
    if (val == num)  // if no numeric value found exit with error
      break;
    else {
      *value = strtol((char *)num, NULL, 16);  // convert value
      return EXIT_SUCCESS;
    }
  }
  return EXIT_FAILURE;
}

void update_status(char *status_msg, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "\r\e[K%s", status_msg);
  vfprintf(stderr, fmt, args);
  fflush(stderr);
  va_end(args);
}

int compare_memory(uint8_t *s1, uint8_t *s2, size_t size, uint8_t *c1,
                   uint8_t *c2) {
  size_t i;
  for (i = 0; i < size; i++) {
    if (s1[i] != s2[i]) {
      *c1 = s1[i];
      *c2 = s2[i];
      return i;
    }
  }
  return -1;
}

/* RAM-centric IO operations */
int read_page_ram(minipro_handle_t *handle, uint8_t *buf, uint8_t type,
                  const char *name, size_t size) {
  char status_msg[64];
  sprintf(status_msg, "Reading %s...  ", name);

  size_t blocks_count = size / handle->device->read_buffer_size;
  if (size % handle->device->read_buffer_size) blocks_count++;

  struct timeval begin, end;
  gettimeofday(&begin, NULL);
  uint32_t address;
  size_t i, len = handle->device->read_buffer_size;
  for (i = 0; i < blocks_count; i++) {
    update_status(status_msg, "%2d%%", i * 100 / blocks_count);
    // Translating address to protocol-specific
    address = i * handle->device->read_buffer_size;
    if ((handle->device->opts4 & MP_DATA_BUS_WIDTH) && type == MP_CODE)
      address = address >> 1;

    // Last block
    if ((i + 1) * len > size) len = size % len;
    if (minipro_read_block(handle, type, address,
                           buf + i * handle->device->read_buffer_size, len))
      return EXIT_FAILURE;

    uint8_t ovc;
    if (minipro_get_ovc_status(handle, NULL, &ovc)) return EXIT_FAILURE;
    if (ovc) {
      fprintf(stderr, "\nOvercurrent protection!\007\n");
      return EXIT_FAILURE;
    }
  }
  gettimeofday(&end, NULL);
  sprintf(status_msg, "Reading %s...  %.2fSec  OK", name,
          (double)(end.tv_usec - begin.tv_usec) / 1000000 +
              (double)(end.tv_sec - begin.tv_sec));
  update_status(status_msg, "\n");
  return EXIT_SUCCESS;
}

int write_page_ram(minipro_handle_t *handle, uint8_t *buffer, uint8_t type,
                   const char *name, size_t size) {
  char status_msg[64];
  sprintf(status_msg, "Writing  %s...  ", name);

  size_t blocks_count = size / handle->device->write_buffer_size;
  if (size % handle->device->write_buffer_size) blocks_count++;

  struct timeval begin, end;
  gettimeofday(&begin, NULL);
  minipro_status_t status;
  size_t i, len = handle->device->write_buffer_size;
  uint32_t address;
  for (i = 0; i < blocks_count; i++) {
    update_status(status_msg, "%2d%%", i * 100 / blocks_count);
    // Translating address to protocol-specific
    address = i * len;
    if ((handle->device->opts4 & MP_DATA_BUS_WIDTH) && type == MP_CODE)
      address = address >> 1;

    // Last block
    if ((i + 1) * len > size) len = size % len;
    if (minipro_write_block(handle, type, address,
                            buffer + i * handle->device->write_buffer_size,
                            len))
      return EXIT_FAILURE;

    uint8_t ovc = 0;
    if (minipro_get_ovc_status(handle, &status, &ovc)) return EXIT_FAILURE;
    if (ovc) {
      fprintf(stderr, "\nOvercurrent protection!\007\n");
      return EXIT_FAILURE;
    }
    if (status.error && !cmdopts.no_verify) {
      if (minipro_end_transaction(handle)) return EXIT_FAILURE;
      fprintf(stderr,
              "\nVerification failed at address 0x%04X: File=0x%02X, "
              "Device=0x%02X\n",
              status.address,
              status.c2 & (WORD_SIZE(handle->device) == 1 ? 0xFF : 0xFFFF),
              status.c1 & (WORD_SIZE(handle->device) == 1 ? 0xFF : 0xFFFF));
      return EXIT_FAILURE;
    }
  }
  gettimeofday(&end, NULL);
  sprintf(status_msg, "Writing %s...  %.2fSec  OK", name,
          (double)(end.tv_usec - begin.tv_usec) / 1000000 +
              (double)(end.tv_sec - begin.tv_sec));
  update_status(status_msg, "\n");
  return EXIT_SUCCESS;
}

// Read PLD device
int read_jedec(minipro_handle_t *handle, jedec_t *jedec) {
  size_t i, j;
  struct timeval begin, end;
  gettimeofday(&begin, NULL);

  char status_msg[64];
  sprintf(status_msg, "Reading device... ");
  uint8_t buffer[32];
  gal_config_t *config = (gal_config_t *)handle->device->config;

  uint8_t ovc = 0;
  if (minipro_get_ovc_status(handle, NULL, &ovc)) return EXIT_FAILURE;
  if (ovc) {
    fprintf(stderr, "\nOvercurrent protection!\007\n");
    return EXIT_FAILURE;
  }

  // Read fuses
  memset(jedec->fuses, 0, jedec->QF);
  for (i = 0; i < config->fuses_size; i++) {
    if (minipro_read_jedec_row(handle, buffer, i, config->row_width))
      return EXIT_FAILURE;
    // Unpacking the row
    for (j = 0; j < config->row_width; j++) {
      if (buffer[j / 8] & (0x80 >> (j & 0x07)))
        jedec->fuses[config->fuses_size * j + i] = 1;
    }
    update_status(status_msg, "%2d%%", i * 100 / config->fuses_size);
  }

  // Read user electronic signature (UES)
  if (minipro_read_jedec_row(handle, buffer, i, config->ues_size))
    return EXIT_FAILURE;
  for (j = 0; j < config->ues_size; j++) {
    if (buffer[j / 8] & (0x80 >> (j & 0x07)))
      jedec->fuses[config->ues_address + j] = 1;
  }

  // Read architecture control word (ACW)
  if (minipro_read_jedec_row(handle, buffer, config->acw_address,
                             config->acw_size))
    return EXIT_FAILURE;
  for (i = 0; i < config->acw_size; i++) {
    if (buffer[i / 8] & (0x80 >> (i & 0x07)))
      jedec->fuses[config->acw_bits[i]] = 1;
  }

  gettimeofday(&end, NULL);
  sprintf(status_msg, "Reading device...  %.2fSec  OK",
          (double)(end.tv_usec - begin.tv_usec) / 1000000 +
              (double)(end.tv_sec - begin.tv_sec));
  update_status(status_msg, "\n");
  return EXIT_SUCCESS;
}

// Write PLD device
int write_jedec(minipro_handle_t *handle, jedec_t *jedec) {
  size_t i, j;
  struct timeval begin, end;
  gettimeofday(&begin, NULL);

  char status_msg[64];
  sprintf(status_msg, "Writing jedec file... ");
  uint8_t buffer[32];
  gal_config_t *config = (gal_config_t *)handle->device->config;

  uint8_t ovc = 0;
  if (minipro_get_ovc_status(handle, NULL, &ovc)) return EXIT_FAILURE;
  if (ovc) {
    fprintf(stderr, "\nOvercurrent protection!\007\n");
    return EXIT_FAILURE;
  }

  // Write fuses
  for (i = 0; i < config->fuses_size; i++) {
    memset(buffer, 0, sizeof(buffer));
    // Building a row
    for (j = 0; j < config->row_width; j++) {
      if (jedec->fuses[config->fuses_size * j + i] == 1)
        buffer[j / 8] |= (0x80 >> (j & 0x07));
    }
    update_status(status_msg, "%2d%%", i * 100 / config->fuses_size);
    if (minipro_write_jedec_row(handle, buffer, i, config->row_width))
      return EXIT_FAILURE;
  }

  // Write user electronic signature (UES)
  memset(buffer, 0, sizeof(buffer));
  for (j = 0; j < config->ues_size; j++) {
    if (jedec->fuses[config->ues_address + j] == 1)
      buffer[j / 8] |= (0x80 >> (j & 0x07));
  }
  if (minipro_write_jedec_row(handle, buffer, i, config->ues_size))
    return EXIT_FAILURE;

  // Write architecture control word (ACW)
  memset(buffer, 0, sizeof(buffer));
  for (i = 0; i < config->acw_size; i++) {
    if (jedec->fuses[config->acw_bits[i]] == 1)
      buffer[i / 8] |= (0x80 >> (i & 0x07));
  }

  if (minipro_write_jedec_row(handle, buffer, config->acw_address,
                              config->acw_size))
    return EXIT_FAILURE;

  gettimeofday(&end, NULL);
  sprintf(status_msg, "Writing jedec file...  %.2fSec  OK",
          (double)(end.tv_usec - begin.tv_usec) / 1000000 +
              (double)(end.tv_sec - begin.tv_sec));
  update_status(status_msg, "\n");
  return EXIT_SUCCESS;
}

int erase_device(minipro_handle_t *handle) {
  struct timeval begin, end;
  if (cmdopts.no_erase == 0 &&
      (handle->device->opts4 &
       MP_ERASE_MASK))  // Not all chips can be erased...
  {
    fprintf(stderr, "Erasing... ");
    fflush(stderr);
    gettimeofday(&begin, NULL);
    if (minipro_erase(handle)) return EXIT_FAILURE;
    gettimeofday(&end, NULL);
    fprintf(stderr, "%.2fSec OK\n",
            (double)(end.tv_usec - begin.tv_usec) / 1000000 +
                (double)(end.tv_sec - begin.tv_sec));
  }
  return EXIT_SUCCESS;
}

/* Wrappers for operating with files */
int write_page_file(minipro_handle_t *handle, const char *filename,
                    uint8_t type, const char *name, size_t size) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("Couldn't open file for reading");
    return EXIT_FAILURE;
  }

  uint8_t *buffer = malloc(size);
  if (!buffer) {
    fclose(file);
    fprintf(stderr, "Out of memory!\n");
    return EXIT_FAILURE;
  }

  if (fread(buffer, 1, size, file) != size && !cmdopts.size_error) {
    free(buffer);
    fclose(file);
    fprintf(stderr, "Read error\n");
    return EXIT_FAILURE;
  }
  fclose(file);
  if (write_page_ram(handle, buffer, type, name, size)) {
    free(buffer);
    return EXIT_FAILURE;
  }
  free(buffer);
  return EXIT_SUCCESS;
}

int read_page_file(minipro_handle_t *handle, const char *filename, uint8_t type,
                   const char *name, size_t size) {
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    perror("Couldn't open file for writing\n");
    return EXIT_FAILURE;
  }

  uint8_t *buffer = malloc(size + 128);
  if (!buffer) {
    fprintf(stderr, "Out of memory\n");
    fclose(file);
    return EXIT_FAILURE;
  }

  if (read_page_ram(handle, buffer, type, name, size)) {
    fclose(file);
    free(buffer);
    return EXIT_FAILURE;
  }
  fwrite(buffer, 1, size, file);
  fclose(file);
  free(buffer);
  return EXIT_SUCCESS;
}

int verify_page_file(minipro_handle_t *handle, const char *filename,
                     uint8_t type, const char *name, size_t size) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("Couldn't open file for reading");
    return EXIT_FAILURE;
  }

  /* Loading file */
  uint8_t *file_data = malloc(size);
  if (!file_data) {
    fclose(file);
    fprintf(stderr, "Out of memory\n");
    return EXIT_FAILURE;
  }
  if (fread(file_data, 1, size, file) != size && !cmdopts.size_error) {
    fclose(file);
    free(file_data);
    fprintf(stderr, "File read error!\n");
    return EXIT_FAILURE;
  }
  fclose(file);

  /* Downloading data from chip*/
  uint8_t *chip_data = malloc(size + 128);
  if (!chip_data) {
    fprintf(stderr, "Out of memory\n");
    free(file_data);
    return EXIT_FAILURE;
  }
  if (read_page_ram(handle, chip_data, type, name, size)) {
    free(file_data);
    free(chip_data);
    return EXIT_FAILURE;
  }

  uint8_t c1, c2;
  int idx = compare_memory(file_data, chip_data, size, &c1, &c2);

  free(file_data);
  free(chip_data);

  if (idx != -1) {
    fprintf(
        stderr,
        "Verification failed at address 0x%04X: File=0x%02X, Device=0x%02X\n",
        idx, c1, c2);
    return EXIT_FAILURE;
  } else {
    fprintf(stderr, "Verification OK\n");
  }
  return EXIT_SUCCESS;
}

int read_fuses(minipro_handle_t *handle, const char *filename,
               fuse_decl_t *fuses) {
  size_t i;
  char *config = malloc(1024);
  if (!config) {
    fprintf(stderr, "Out of memory\n");
    return EXIT_FAILURE;
  }

  uint8_t buffer[64];
  struct timeval begin, end;
  memset(config, 0x00, 1024);

  if ((fuses->num_locks & 0x80) != 0) {
    free(config);
    fprintf(stderr, "Can't read the lock byte for this device!\n");
    return EXIT_FAILURE;
  }

  FILE *pFile = fopen(filename, "w");
  if (pFile == NULL) {
    free(config);
    perror("Couldn't create config file!");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "Reading fuses... ");
  fflush(stderr);
  gettimeofday(&begin, NULL);

  fuses->num_locks &= 0x7f;
  if (fuses->num_fuses > 0) {
    if (minipro_read_fuses(handle, MP_FUSE_CFG,
                           fuses->num_fuses * fuses->item_size,
                           fuses->item_size / fuses->word, buffer)) {
      free(config);
      fclose(pFile);
      return EXIT_FAILURE;
    }
    for (i = 0; i < fuses->num_fuses; i++) {
      uint32_t value =
          load_int(&(buffer[i * fuses->word]), fuses->word, MP_LITTLE_ENDIAN);
      sprintf(config + strlen(config),
              fuses->word == 1 ? "%s = 0x%02x\n" : "%s = 0x%04x\n",
              fuses->fnames[i], value);
    }
  }
  if (fuses->num_uids > 0) {
    if (minipro_read_fuses(handle, MP_FUSE_USER,
                           fuses->num_uids * fuses->item_size, 0, buffer)) {
      free(config);
      fclose(pFile);
      return EXIT_FAILURE;
    }
    for (i = 0; i < fuses->num_uids; i++) {
      uint32_t value =
          load_int(&(buffer[i * fuses->word]), fuses->word, MP_LITTLE_ENDIAN);
      sprintf(config + strlen(config),
              fuses->word == 1 ? "%s = 0x%02x\n" : "%s = 0x%04x\n",
              fuses->unames[i], value);
    }
  }
  if (fuses->num_locks > 0) {
    if (minipro_read_fuses(handle, MP_FUSE_LOCK,
                           fuses->num_locks * fuses->item_size,
                           fuses->item_size / fuses->word, buffer)) {
      free(config);
      fclose(pFile);
      return EXIT_FAILURE;
    }
    for (i = 0; i < fuses->num_locks; i++) {
      uint32_t value =
          load_int(&(buffer[i * fuses->word]), fuses->word, MP_LITTLE_ENDIAN);
      sprintf(config + strlen(config),
              fuses->word == 1 ? "%s = 0x%02x\n" : "%s = 0x%04x\n",
              fuses->lnames[i], value);
    }
  }

  fputs(config, pFile);
  fclose(pFile);
  free(config);
  gettimeofday(&end, NULL);
  fprintf(stderr, "%.2fSec  OK\n",
          (double)(end.tv_usec - begin.tv_usec) / 1000000 +
              (double)(end.tv_sec - begin.tv_sec));
  return EXIT_SUCCESS;
}

int write_fuses(minipro_handle_t *handle, const char *filename,
                fuse_decl_t *fuses) {
  size_t i;
  uint8_t wbuffer[64];
  uint8_t vbuffer[64];
  uint8_t config[500];
  struct timeval begin, end;

  FILE *pFile = fopen(filename, "r");
  if (pFile == NULL) {
    perror("Couldn't open config file!");
    return EXIT_FAILURE;
  }

  fread(config, sizeof(char), sizeof(config), pFile);
  fclose(pFile);

  fprintf(stderr, "Writing fuses... ");
  fflush(stderr);

  gettimeofday(&begin, NULL);
  if (fuses->num_fuses > 0) {
    for (i = 0; i < fuses->num_fuses; i++) {
      uint32_t value;
      if (get_config_value(config, sizeof(config), fuses->fnames[i],
                           strlen(fuses->fnames[i]), &value) == -1) {
        fprintf(stderr, "Could not read config %s value.\n", fuses->fnames[i]);
        return EXIT_FAILURE;
      }
      format_int(&(wbuffer[i * fuses->word]), value, fuses->word,
                 MP_LITTLE_ENDIAN);
    }
    if (minipro_write_fuses(handle, MP_FUSE_CFG,
                            fuses->num_fuses * fuses->item_size,
                            fuses->item_size / fuses->word, wbuffer))
      return EXIT_FAILURE;
    if (minipro_read_fuses(handle, MP_FUSE_CFG,
                           fuses->num_fuses * fuses->item_size,
                           fuses->item_size / fuses->word, vbuffer))
      return EXIT_FAILURE;
    if (memcmp(wbuffer, vbuffer, fuses->num_fuses * fuses->item_size)) {
      fprintf(stderr, "\nFuses verify error!\n");
    }
  }

  if (fuses->num_uids > 0) {
    for (i = 0; i < fuses->num_uids; i++) {
      uint32_t value;
      if (get_config_value(config, sizeof(config), fuses->unames[i],
                           strlen(fuses->unames[i]), &value) == -1) {
        fprintf(stderr, "Could not read config %s value.\n", fuses->unames[i]);
        return EXIT_FAILURE;
      }
      format_int(&(wbuffer[i * fuses->word]), value, fuses->word,
                 MP_LITTLE_ENDIAN);
    }
    if (minipro_write_fuses(handle, MP_FUSE_USER,
                            fuses->num_uids * fuses->item_size,
                            fuses->item_size / fuses->word, wbuffer))
      return EXIT_FAILURE;
    if (minipro_read_fuses(handle, MP_FUSE_USER,
                           fuses->num_uids * fuses->item_size,
                           fuses->item_size / fuses->word, vbuffer))
      return EXIT_FAILURE;
    if (memcmp(wbuffer, vbuffer, fuses->num_uids * fuses->item_size)) {
      fprintf(stderr, "\nUser ID verify error!\n");
    }
  }

  if (fuses->num_locks > 0) {
    for (i = 0; i < fuses->num_locks; i++) {
      uint32_t value;
      if (get_config_value(config, sizeof(config), fuses->lnames[i],
                           strlen(fuses->lnames[i]), &value) == -1) {
        fprintf(stderr, "Could not read config %s value.\n", fuses->lnames[i]);
        return EXIT_FAILURE;
      }
      format_int(&(wbuffer[i * fuses->word]), value, fuses->word,
                 MP_LITTLE_ENDIAN);
    }
    if (minipro_write_fuses(handle, MP_FUSE_LOCK,
                            fuses->num_locks * fuses->item_size,
                            fuses->item_size / fuses->word, wbuffer))
      return EXIT_FAILURE;
    if (minipro_read_fuses(handle, MP_FUSE_LOCK,
                           fuses->num_locks * fuses->item_size,
                           fuses->item_size / fuses->word, vbuffer))
      return EXIT_FAILURE;
    if (memcmp(wbuffer, vbuffer, fuses->num_locks * fuses->item_size)) {
      fprintf(stderr, "\nLock bytes verify error!\n");
    }
  }
  gettimeofday(&end, NULL);
  fprintf(stderr, "%.2fSec  OK\n",
          (double)(end.tv_usec - begin.tv_usec) / 1000000 +
              (double)(end.tv_sec - begin.tv_sec));
  return EXIT_SUCCESS;
}

/* Higher-level logic */
int action_read(const char *filename, minipro_handle_t *handle) {
  jedec_t jedec;

  char *data_filename = (char *)filename;
  char *config_filename = (char *)filename;

  char default_data_filename[strlen(filename) + 12];
  strcpy(default_data_filename, filename);
  char *dot = strrchr(default_data_filename, '.');
  strcpy(dot ? dot : default_data_filename + strlen(filename), ".eeprom.bin");

  char default_config_filename[strlen(filename) + 12];
  strcpy(default_config_filename, filename);
  dot = strrchr(default_config_filename, '.');
  strcpy(dot ? dot : default_config_filename + strlen(filename), ".fuses.conf");

  if (minipro_begin_transaction(handle)) return EXIT_FAILURE;
  switch (handle->device->protocol_id) {
    case PLD_PROTOCOL_16V8:
    case PLD_PROTOCOL_20V8:
    case PLD_PROTOCOL_22V10:
    case PLD_PROTOCOL2_16V8:
    case PLD_PROTOCOL2_20V8:
    case PLD_PROTOCOL2_22V10:
      jedec.QF = handle->device->code_memory_size;
      if (!jedec.QF) {
        fprintf(stderr, "Unknown fuse size!\n");
        return EXIT_FAILURE;
      }
      jedec.fuses = malloc(jedec.QF);
      if (!jedec.fuses) {
        fprintf(stderr, "Out of memory\n");
        return EXIT_FAILURE;
      }
      memset(jedec.fuses, 0, jedec.QF);
      jedec.F = 0;
      jedec.G = 0;
      jedec.QP = PINS_COUNT(handle->device->package_details);
      jedec.device_name = handle->device->name;

      if (read_jedec(handle, &jedec)) {
        free(jedec.fuses);
        return EXIT_FAILURE;
      }
      switch (write_jedec_file(filename, &jedec)) {
        case FILE_OPEN_ERROR:
          free(jedec.fuses);
          perror("File open error");
          return EXIT_FAILURE;
      }
      free(jedec.fuses);
      break;
    default:
      if (cmdopts.page == UNSPECIFIED) {
        data_filename = default_data_filename;
        config_filename = default_config_filename;
      }
      if (cmdopts.page == CODE || cmdopts.page == UNSPECIFIED) {
        if (read_page_file(handle, filename, MP_CODE, "Code",
                           handle->device->code_memory_size))
          return EXIT_FAILURE;
      }
      if ((cmdopts.page == DATA || cmdopts.page == UNSPECIFIED) &&
          handle->device->data_memory_size) {
        if (read_page_file(handle, data_filename, MP_DATA, "Data",
                           handle->device->data_memory_size))
          return EXIT_FAILURE;
      }
      if ((cmdopts.page == CONFIG || cmdopts.page == UNSPECIFIED) &&
          handle->device->config) {
        if (read_fuses(handle, config_filename, handle->device->config))
          return EXIT_FAILURE;
      }

      if (cmdopts.page == CONFIG && !handle->device->config) {
        fprintf(stderr, "Missing fuse type in database...\n");
        return EXIT_FAILURE;
      }
  }
  return EXIT_SUCCESS;
}

int action_write(const char *filename, minipro_handle_t *handle) {
  struct stat st;
  off_t file_size;
  jedec_t wjedec, rjedec;
  struct timeval begin, end;

  switch (handle->device->protocol_id) {
    case PLD_PROTOCOL_16V8:
    case PLD_PROTOCOL_20V8:
    case PLD_PROTOCOL_22V10:
    case PLD_PROTOCOL2_16V8:
    case PLD_PROTOCOL2_20V8:
    case PLD_PROTOCOL2_22V10:
      switch (read_jedec_file(filename, &wjedec)) {
        case NO_ERROR:
          if (wjedec.fuses == NULL) {
            fprintf(stderr, "This file has no fuses (L) declaration!\n");
            return EXIT_FAILURE;
          }

          if (handle->device->code_memory_size != wjedec.QF)
            fprintf(stderr,
                    "Warning! JED file doesn't match the selected device!\n");

          fprintf(
              stderr,
              "\nDeclared fuse checksum: 0x%04X Calculated: 0x%04X ... %s\n",
              wjedec.C, wjedec.fuse_checksum,
              wjedec.fuse_checksum == wjedec.C ? "OK" : "Mismatch!");

          fprintf(stderr,
                  "Declared file checksum: 0x%04X Calculated: 0x%04X ... %s\n",
                  wjedec.decl_file_checksum, wjedec.calc_file_checksum,
                  wjedec.decl_file_checksum == wjedec.calc_file_checksum
                      ? "OK"
                      : "Mismatch!");

          fprintf(stderr, "JED file parsed OK\n\n");
          if (cmdopts.no_protect_on == 0)
            fprintf(stderr, "Use -P to skip write protect\n\n");
          break;
        case FILE_OPEN_ERROR:
          perror("File open error");
          return EXIT_FAILURE;
        case SIZE_ERROR:
          fprintf(stderr, "File size error!\n");
          return EXIT_FAILURE;
        case FILE_READ_ERROR:
          fprintf(stderr, "File read error!\n");
          return EXIT_FAILURE;
        case BAD_FORMAT:
          fprintf(stderr, "JED file format error!\n");
          return EXIT_FAILURE;
        case MEMORY_ERROR:
          fprintf(stderr, "Out of memory!\n");
          return EXIT_FAILURE;
      }
      if (minipro_begin_transaction(handle)) {
        free(wjedec.fuses);
        return EXIT_FAILURE;
      }
      if (erase_device(handle)) {
        free(wjedec.fuses);
        return EXIT_FAILURE;
      }
      if (write_jedec(handle, &wjedec)) {
        free(wjedec.fuses);
        return EXIT_FAILURE;
      }
      if (minipro_end_transaction(handle)) {
        free(wjedec.fuses);
        return EXIT_FAILURE;
      }
      if (cmdopts.no_verify == 0) {
        rjedec.QF = wjedec.QF;
        rjedec.F = wjedec.F;
        rjedec.fuses = malloc(rjedec.QF);
        if (!rjedec.fuses) {
          free(wjedec.fuses);
          return EXIT_FAILURE;
        }
        // compare fuses
        if (minipro_begin_transaction(handle)) {
          free(wjedec.fuses);
          free(rjedec.fuses);
          return EXIT_FAILURE;
        }
        if (read_jedec(handle, &rjedec)) {
          free(wjedec.fuses);
          free(rjedec.fuses);
          return EXIT_FAILURE;
        }
        if (minipro_end_transaction(handle)) {
          free(wjedec.fuses);
          free(rjedec.fuses);
          return EXIT_FAILURE;
        }
        uint8_t c1, c2;
        int address =
            compare_memory(wjedec.fuses, rjedec.fuses, wjedec.QF, &c1, &c2);

        if (address != -1) {
          fprintf(stderr,
                  "Verification failed at address 0x%04X: File=0x%02X, "
                  "Device=0x%02X\n",
                  address, c1, c2);
          free(rjedec.fuses);
          return EXIT_FAILURE;
        } else {
          fprintf(stderr, "Verification OK\n");
        }
        free(rjedec.fuses);
      }
      free(wjedec.fuses);

      if (cmdopts.no_protect_on == 0) {
        fprintf(stderr, "Writing lock bit... ");
        fflush(stderr);
        gettimeofday(&begin, NULL);
        if (minipro_begin_transaction(handle)) return EXIT_FAILURE;
        if (minipro_write_fuses(handle, MP_FUSE_LOCK, 0, 0, NULL))
          return EXIT_FAILURE;
        if (minipro_end_transaction(handle)) return EXIT_FAILURE;
        gettimeofday(&end, NULL);
        fprintf(stderr, "%.2fSec OK\n",
                (double)(end.tv_usec - begin.tv_usec) / 1000000 +
                    (double)(end.tv_sec - begin.tv_sec));
      }
      return EXIT_SUCCESS;
    default:
      if (stat(filename, &st)) {
        perror("File open error");
        return EXIT_FAILURE;
      }
      file_size = st.st_size;
      if (minipro_begin_transaction(handle)) return EXIT_FAILURE;
      if (erase_device(handle)) return EXIT_FAILURE;
      if (cmdopts.no_protect_off == 0 &&
          (handle->device->opts4 & MP_PROTECT_MASK)) {
        fprintf(stderr, "Protect off...");
        fflush(stderr);
        minipro_protect_off(handle);
        fprintf(stderr, "OK\n");
      }
      switch (cmdopts.page) {
        case UNSPECIFIED:
        case CODE:
          if (file_size != handle->device->code_memory_size) {
            if (!cmdopts.size_error) {
              fprintf(stderr, "Incorrect file size: %zu (needed %u)\n",
                      file_size, handle->device->code_memory_size);
              return EXIT_FAILURE;
            } else if (cmdopts.size_nowarn == 0)
              fprintf(stderr, "Warning: Incorrect file size: %zu (needed %u)\n",
                      file_size, handle->device->code_memory_size);
          }
          if (write_page_file(handle, filename, MP_CODE, "Code",
                              handle->device->code_memory_size))
            return EXIT_FAILURE;
          if (cmdopts.no_verify == 0) {
            // We must reset the transaction for VCC verify to have effect
            if (minipro_end_transaction(handle)) return EXIT_FAILURE;
            if (minipro_begin_transaction(handle)) return EXIT_FAILURE;
            if (verify_page_file(handle, filename, MP_CODE, "Code",
                                 handle->device->code_memory_size))
              return EXIT_FAILURE;
          }
          break;
        case DATA:
          if (file_size != handle->device->data_memory_size) {
            if (!cmdopts.size_error) {
              fprintf(stderr, "Incorrect file size: %zu (needed %u)\n",
                      file_size, handle->device->data_memory_size);
              return EXIT_FAILURE;
            } else if (cmdopts.size_nowarn == 0)
              fprintf(stderr, "Warning: Incorrect file size: %zu (needed %u)\n",
                      file_size, handle->device->data_memory_size);
          }
          if (write_page_file(handle, filename, MP_DATA, "Data",
                              handle->device->data_memory_size))
            return EXIT_FAILURE;
          if (cmdopts.no_verify == 0) {
            if (minipro_end_transaction(handle)) return EXIT_FAILURE;
            if (minipro_begin_transaction(handle)) return EXIT_FAILURE;
            if (verify_page_file(handle, filename, MP_DATA, "Data",
                                 handle->device->data_memory_size))
              return EXIT_FAILURE;
          }
          break;
        case CONFIG:
          if (handle->device->config) {
            if (write_fuses(handle, filename, handle->device->config))
              return EXIT_FAILURE;
          }
          break;
      }
      if (cmdopts.no_protect_on == 0 &&
          (handle->device->opts4 & MP_PROTECT_MASK)) {
        fprintf(stderr, "Protect on...");
        fflush(stderr);
        if (minipro_protect_on(handle)) return EXIT_FAILURE;
        fprintf(stderr, "OK\n");
      }
  }
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  parse_cmdline(argc, argv);
  if (!cmdopts.filename && !cmdopts.idcheck_only) {
    print_help_and_exit(argv[0]);
  }
  // If -D option is enabled then you must supply a device name.
  if ((cmdopts.action && !cmdopts.device) ||
      (cmdopts.idcheck_only && !cmdopts.device)) {
    fprintf(stderr, "Device required. Use -p <device> to specify a device. ");
    print_help_and_exit(argv[0]);
  }

  // don't permit skipping the ID read in write-mode
  if (cmdopts.action == action_write && cmdopts.idcheck_skip) {
    print_help_and_exit(argv[0]);
  }
  // don't permit skipping the ID read in ID only mode
  if (cmdopts.idcheck_only && cmdopts.idcheck_skip) {
    print_help_and_exit(argv[0]);
  }

  minipro_handle_t *handle = minipro_open(cmdopts.device);
  if (!handle) {
    return EXIT_FAILURE;
  }

  minipro_print_system_info(handle);
  if (handle->status == MP_STATUS_BOOTLOADER) {
    fprintf(stderr, "in bootloader mode!\nExiting...\n");
    return EXIT_FAILURE;
  }

  switch (handle->device->protocol_id) {
    // Check for GAL/PLD
    case PLD_PROTOCOL_16V8:
    case PLD_PROTOCOL_20V8:
    case PLD_PROTOCOL_22V10:
    case PLD_PROTOCOL2_16V8:
    case PLD_PROTOCOL2_20V8:
    case PLD_PROTOCOL2_22V10:
      break;
    default:
      if (!handle->device->read_buffer_size || !handle->device->protocol_id) {
        minipro_close(handle);
        fprintf(stderr, "Unsupported device!\n");
        return EXIT_FAILURE;
      }
      break;
  }

  if (handle->version == MP_TL866IIPLUS && cmdopts.action == action_write &&
      (cmdopts.vcc != -1 || cmdopts.vdd != -1 || cmdopts.vpp != -1 ||
       cmdopts.pulse_delay != -1))
    fprintf(stderr, "The -o option is not yet implemented for TL866II+\n");

  // Check if the device has programming options
  char *voltage_name;
  if ((handle->device->opts7 == MP_VOLTAGES1 ||
       handle->device->opts7 == MP_VOLTAGES2) &&
      cmdopts.action == action_write) {
    // Insert VPP voltage
    if (cmdopts.vpp != -1)
      handle->device->opts1 =
          (handle->device->opts1 & 0xff0f) | (cmdopts.vpp << 4);

    if (handle->device->opts7 == MP_VOLTAGES1) {
      // Insert VDD voltage
      if (cmdopts.vdd != -1)
        handle->device->opts1 =
            (handle->device->opts1 & 0x0fff) | (cmdopts.vdd << 12);

      // Insert VCC voltage
      if (cmdopts.vcc != -1)
        handle->device->opts1 =
            (handle->device->opts1 & 0xf0ff) | (cmdopts.vcc << 8);

      // Insert pulse delay
      if (cmdopts.pulse_delay != -1)
        handle->device->opts3 = cmdopts.pulse_delay;

      get_voltage((uint8_t)handle->device->opts1 >> 4, &voltage_name,
                  VPP_VOLTAGE);
      fprintf(stderr, "\nVPP=%sV, ", voltage_name);
      get_voltage(handle->device->opts1 >> 12, &voltage_name, VCC_VOLTAGE);
      fprintf(stderr, "VDD=%sV, ", voltage_name);
      get_voltage((handle->device->opts1 >> 8) & 0x0f, &voltage_name,
                  VCC_VOLTAGE);
      fprintf(stderr, "VCC=%sV, ", voltage_name);
      fprintf(stderr, "Pulse=%uus\n", handle->device->opts3);
    } else {
      get_voltage((uint8_t)handle->device->opts1 >> 4, &voltage_name,
                  VPP_VOLTAGE);
      fprintf(stderr, "\nVPP=%sV\n", voltage_name);
    }
  }

  // Unlocking the TSOP48 adapter (if applicable)
  uint8_t status;
  switch (handle->device->package_details & ADAPTER_MASK) {
    case TSOP48_ADAPTER:
    case SOP44_ADAPTER:
    case SOP56_ADAPTER:
      if (minipro_unlock_tsop48(handle, &status)) {
        minipro_close(handle);
        return EXIT_FAILURE;
      }
      switch (status) {
        case MP_TSOP48_TYPE_V3:
          fprintf(stderr, "Found TSOP adapter V3\n");
          break;
        case MP_TSOP48_TYPE_NONE:
          minipro_end_transaction(handle);  // We need this to turn off the
                                            // power on the ZIF socket.
          minipro_close(handle);
          fprintf(stderr, "TSOP adapter not found!\n");
          return EXIT_FAILURE;
        case MP_TSOP48_TYPE_V0:
          fprintf(stderr, "Found TSOP adapter V0\n");
          break;
        case MP_TSOP48_TYPE_FAKE1:
        case MP_TSOP48_TYPE_FAKE2:
          fprintf(stderr, "Fake TSOP adapter found!\n");
          break;
      }
      break;
  }

  // Activate ICSP if the chip can only be programmed via ICSP.
  handle->icsp = 0;
  if ((handle->device->package_details & ICSP_MASK) &&
      ((handle->device->package_details & PIN_COUNT_MASK) == 0)) {
    handle->icsp = MP_ICSP_ENABLE | MP_ICSP_VCC;
  } else if (handle->device->package_details & ICSP_MASK)
    handle->icsp = cmdopts.icsp;
  if (handle->icsp) fprintf(stderr, "Activating ICSP...\n");

  uint8_t id_type;
  // Verifying Chip ID (if applicable)
  if (cmdopts.idcheck_skip) {
    fprintf(stderr, "WARNING: skipping Chip ID test\n");
  } else if ((handle->device->chip_id_bytes_count && handle->device->chip_id) ||
             (handle->device->opts4 & MP_ID_MASK)) {
    if (minipro_begin_transaction(handle)) {
      minipro_close(handle);
      return EXIT_FAILURE;
    }
    uint32_t chip_id;
    if (minipro_get_chip_id(handle, &id_type, &chip_id)) {
      minipro_close(handle);
      return EXIT_FAILURE;
    }
    if (minipro_end_transaction(handle)) {
      minipro_close(handle);
      return EXIT_FAILURE;
    }
    uint32_t chip_id_temp = chip_id;
    uint8_t shift = 0;
    /* The id_type will tell us the Chip ID type. There are 5 types */
    uint32_t ok = 0;
    switch (id_type) {
      case MP_ID_TYPE1:  // 1-3 bytes ID
      case MP_ID_TYPE2:  // 4 bytes ID
      case MP_ID_TYPE5:  // 3 bytes ID, this ID type is returning from 25 SPI
                         // series.
        ok = (chip_id == handle->device->chip_id);
        if (ok) {
          fprintf(stderr, "Chip ID OK: 0x%04X\n", chip_id);
        }
        break;
      case MP_ID_TYPE3:  // Microchip controllers with 5 bit revision number.
        ok = (handle->device->chip_id >> 5 ==
              (chip_id >> 5));  // Throw the chip revision (last 5 bits).
        if (ok) {
          fprintf(stderr, "Chip ID OK: 0x%04X Rev.0x%02X\n", chip_id >> 5,
                  chip_id & 0x1F);
        }
        chip_id >>= 5;
        chip_id_temp = chip_id << 5;
        shift = 5;
        break;
      case MP_ID_TYPE4:  // Microchip controllers with 4-5 bit revision number.
        ok = (handle->device->chip_id >>
                  ((fuse_decl_t *)handle->device->config)->rev_mask ==
              (chip_id >> ((fuse_decl_t *)handle->device->config)
                              ->rev_mask));  // Throw the chip revision (last
                                             // rev_mask bits).
        if (ok) {
          fprintf(
              stderr, "Chip ID OK: 0x%04X Rev.0x%02X\n", chip_id,
              chip_id &
                  ~(0xFF >> ((fuse_decl_t *)handle->device->config)->rev_mask));
        }
        chip_id >>= ((fuse_decl_t *)handle->device->config)->rev_mask;
        chip_id_temp = chip_id
                       << ((fuse_decl_t *)handle->device->config)->rev_mask;
        shift = ((fuse_decl_t *)handle->device->config)->rev_mask;
        break;
    }

    if (cmdopts.idcheck_only && ok) {
      minipro_close(handle);
      return EXIT_SUCCESS;
    }

    if (!ok) {
      const char *name =
          get_device_from_id(handle, chip_id_temp, handle->device->protocol_id);
      if (cmdopts.idcheck_only) {
        fprintf(stderr, "Chip ID mismatch: expected 0x%04X, got 0x%04X (%s)\n",
                handle->device->chip_id >> shift, chip_id_temp >> shift,
                name ? name : "unknown");
        minipro_close(handle);
        return EXIT_FAILURE;
      }
      if (cmdopts.idcheck_continue) {
        fprintf(stderr,
                "WARNING: Chip ID mismatch: expected 0x%04X, got 0x%04X (%s)\n",
                handle->device->chip_id >> shift, chip_id_temp >> shift,
                name ? name : "unknown");
      } else {
        fprintf(stderr,
                "Invalid Chip ID: expected 0x%04X, got 0x%04X (%s)\n(use '-y' "
                "to continue anyway at your own risk)\n",
                handle->device->chip_id >> shift, chip_id_temp,
                name ? name : "unknown");
        minipro_close(handle);
        return EXIT_FAILURE;
      }
    }
  } else if (!cmdopts.filename) {
    minipro_close(handle);
    fprintf(stderr, "Can't read the device ID for this chip!\n");
    return EXIT_FAILURE;
  }
  int ret = cmdopts.action(cmdopts.filename, handle);
  if (minipro_end_transaction(handle)) {
    minipro_close(handle);
    return EXIT_FAILURE;
  }
  minipro_close(handle);
  return ret;
}
