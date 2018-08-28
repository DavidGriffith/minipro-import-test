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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include "minipro.h"
#include "database.h"
#include "byte_utils.h"
#include "fuses.h"
#include "easyconfig.h"
#include "error.h"
#include "minipro.h"

void action_read(const char *filename, minipro_handle_t *handle, device_t *device);
void action_write(const char *filename, minipro_handle_t *handle, device_t *device);

struct
{
	void (*action)(const char *, minipro_handle_t *handle, device_t *device);
	char *filename;
	device_t *device;
	enum
	{
		UNSPECIFIED = 0, CODE, DATA, CONFIG
	} page;
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
} cmdopts;

void print_version_and_exit(uint32_t rv)
{
	char output[] = "minipro version %s     A free and open TL866XX programmer\n"
			"Build:\t\t%s\n"
			"Git commit:\t%s\n"
			"Git tag:\t%s\n"
			"Git branch:\t%s\n";
	fprintf(rv ? stderr : stdout, output, GIT_TAG, build_timestamp, GIT_HASH,
			GIT_TAG, GIT_BRANCH);
	exit(rv);
}

void print_help_and_exit(char *progname, uint32_t rv)
{
	char usage[] = "minipro version %s     A free and open TL866XX programmer\n"
			"Usage: %s [options]\n"
			"options:\n"
			"	-l		List all supported devices\n"
			"	-L <search>	List devices beginning like this\n"
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
			"	-i		Use ICSP\n"
			"	-I		Use ICSP (without enabling Vcc)\n"
			"	-s		Do NOT error on file size mismatch (only a warning)\n"
			"	-S		No warning message for file size mismatch (can't combine with -s)\n"
			"	-x		Do NOT attempt to read ID (only valid in read mode)\n"
			"	-y		Do NOT error on ID mismatch\n"
			"	-V		Show version information\n"
			"	-t		Start hardware check\n"
			"	-h		Show help (this text)\n";
	fprintf(rv ? stderr : stdout, usage, GIT_TAG, basename(progname));
	exit(rv);
}

void print_devices_and_exit()
{
	if (isatty(STDOUT_FILENO))
	{
		// stdout is a terminal, opening pager
		signal(SIGINT, SIG_IGN);
		char *pager_program = getenv("PAGER");
		if (!pager_program)
			pager_program = "less";
		FILE *pager = popen(pager_program, "w");
		dup2(fileno(pager), STDOUT_FILENO);
	}

	device_t *device;
	for (device = &(devices[0]); device[0].name; device = &(device[1]))
	{
		printf("%s\n", device->name);
	}
	exit(0);
}


void print_device_info_and_exit(device_t *device)
{
	printf("Name: %s\n", device->name);

	/* Memory shape */
	printf("Memory: %u", device->code_memory_size / WORD_SIZE(device));
	switch (device->opts4 & 0xFF000000)
	{
	case 0x00000000:
		printf(" Bytes");
		break;
	case 0x01000000:
		printf(" Words");
		break;
	case 0x02000000:
		printf(" Bits");
		break;
	default:
		ERROR2("Unknown memory shape: 0x%x\n", device->opts4 & 0xFF000000);
	}
	if (device->data_memory_size)
	{
		printf(" + %u Bytes", device->data_memory_size);
	}
	if (device->data_memory2_size)
	{
		printf(" + %zu Bytes", device->data_memory2_size);
	}
	printf("\n");

	uint8_t package_details[4];
	format_int(package_details, device->package_details, 4, MP_LITTLE_ENDIAN);
	/* Package info */
	printf("Package: ");
	if (package_details[0])
	{
		printf("Adapter%03d.JPG\n", package_details[0]);
	}
	else if (package_details[3])
	{
		printf("DIP%d\n", package_details[3] & 0x7F);
	}
	else
	{
		printf("ISP only\n");
	}

	/* ISP connection info */
	printf("ISP: ");
	if (package_details[1])
	{
		printf("ICP%03d.JPG\n", package_details[1]);
	}
	else
	{
		printf("-\n");
	}

	printf("Protocol: 0x%02x\n", device->protocol_id);
	printf("Read buffer size: %u Bytes\n", device->read_buffer_size);
	printf("Write buffer size: %u Bytes\n", device->write_buffer_size);
	exit(0);
}


void parse_cmdline(int argc, char **argv)
{
	int8_t c;
	memset(&cmdopts, 0, sizeof(cmdopts));
	device_t *device;

	while ((c = getopt(argc, argv, "lL:d:euPvxyr:w:p:c:iIsSVhDt")) != -1)
	{
		switch (c)
		{
		case 'l':
			print_devices_and_exit();
			exit(0);
			break;

		case 'L':
			cmdopts.device = get_device_by_name(optarg);
			for (device = &(devices[0]); device[0].name; device = &(device[1]))
			{
				if (!strncasecmp(device[0].name, optarg, strlen(optarg)))
				{
					printf("%s\n", device[0].name);
				}
			}
			exit(0);
			break;

		case 'd':
			cmdopts.device = get_device_by_name(optarg);
			if (!cmdopts.device)
				ERROR2("Unknown device: %s\n", optarg);
			print_device_info_and_exit(cmdopts.device);
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
			if (!strcasecmp(optarg, "help"))
				print_devices_and_exit();
			cmdopts.device = get_device_by_name(optarg);
			if (!cmdopts.device)
				ERROR("Unknown device");
			break;

		case 'c':
			if (!strcasecmp(optarg, "code"))
				cmdopts.page = CODE;
			if (!strcasecmp(optarg, "data"))
				cmdopts.page = DATA;
			if (!strcasecmp(optarg, "config"))
				cmdopts.page = CONFIG;
			if (!cmdopts.page)
				ERROR("Unknown memory type");
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
			print_help_and_exit(argv[0], 0);
			break;

		case 'V':
			print_version_and_exit(0);
			break;

		case 't':
			minipro_hardware_check();
			exit(0);
			break;
		default:
			print_help_and_exit(argv[0], -1);
			break;
		}
	}
}

off_t get_file_size(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (!file)
	{
		PERROR("Couldn't open file");
	}

	fseek(file, 0, SEEK_END);

	if (-1 == ftell(file))
	{
		PERROR("Couldn't tell file");
	}

	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	fclose(file);
	return (size);
}

void update_status(char *status_msg, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("\r\e[K%s", status_msg);
	vprintf(fmt, args);
	fflush(stdout);
	va_end(args);
}

int32_t compare_memory(uint8_t *buf1, uint8_t *buf2, size_t size, uint8_t *c1, uint8_t *c2)
{
	uint32_t i;
	for (i = 0; i < size; i++)
	{
		if (buf1[i] != buf2[i])
		{
			*c1 = buf1[i];
			*c2 = buf2[i];
			return (i);
		}
	}
	return (-1);
}

/* RAM-centric IO operations */
void read_page_ram(minipro_handle_t *handle, uint8_t *buf, uint32_t type, const char *name, size_t size)
{
	char status_msg[64];
	sprintf(status_msg, "Reading %s...  ", name);

	size_t blocks_count = size / handle->device->read_buffer_size;
	if (size % handle->device->read_buffer_size != 0)
	{
		blocks_count++;
	}

	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	uint32_t i;
	for (i = 0; i < blocks_count; i++)
	{
		update_status(status_msg, "%2d%%", i * 100 / blocks_count);
		// Translating address to protocol-specific
		size_t addr = i * handle->device->read_buffer_size;
		if (handle->device->opts4 & 0x2000)
		{
			addr = addr >> 1;
		}
		size_t len = handle->device->read_buffer_size;
		// Last block
		if ((i + 1) * len > size)
			len = size % len;
		minipro_read_block(handle, type, addr,
				buf + i * handle->device->read_buffer_size, len);

		if (minipro_get_ovc_status(handle, NULL))
		{
			minipro_close(handle);
			ERROR("\nOvercurrent protection!");
		}
	}
	gettimeofday(&end, NULL);
	sprintf(status_msg, "Reading %s...  %.2fSec  OK", name,
			(double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
	update_status(status_msg, "\n");
}

void write_page_ram(minipro_handle_t *handle, uint8_t *buf, uint32_t type, const char *name, size_t size)
{
	char status_msg[64];
	sprintf(status_msg, "Writing  %s...  ", name);

	size_t blocks_count = size / handle->device->write_buffer_size;
	if (size % handle->device->write_buffer_size != 0)
	{
		blocks_count++;
	}

	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	minipro_status_t status;
	uint32_t i;
	for (i = 0; i < blocks_count; i++)
	{
		update_status(status_msg, "%2d%%", i * 100 / blocks_count);
		// Translating address to protocol-specific
		uint32_t addr = i * handle->device->write_buffer_size;
		if (handle->device->opts4 & 0x2000)
		{
			addr = addr >> 1;
		}
		uint32_t len = handle->device->write_buffer_size;
		// Last block
		if ((i + 1) * len > size)
			len = size % len;
		minipro_write_block(handle, type, addr,
				buf + i * handle->device->write_buffer_size, len);

		if (minipro_get_ovc_status(handle, &status))
		{
			minipro_close(handle);
			ERROR("\nOvercurrent protection!");
		}
		if (status.error)
		{
			ERROR2(
					"\nVerification failed at address 0x%04X: File=0x%02X, Device=0x%02X\n",
					status.address,
					status.c2 & (WORD_SIZE(handle->device) == 1 ? 0xFF : 0xFFFF),
					status.c1 & (WORD_SIZE(handle->device) == 1 ? 0xFF : 0xFFFF));
		}
	}
	gettimeofday(&end, NULL);
	sprintf(status_msg, "Writing %s...  %.2fSec  OK", name,
			(double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
	update_status(status_msg, "\n");
}

/* Wrappers for operating with files */
void read_page_file(minipro_handle_t *handle, const char *filename, uint32_t type, const char *name, size_t size)
{
	FILE *file = fopen(filename, "w");
	if (file == NULL)
	{
		minipro_close(handle);
		PERROR("Couldn't open file for writing");
	}

	uint8_t *buf = malloc(size);
	if (!buf)
	{
		minipro_close(handle);
		ERROR("Can't malloc");
	}

	read_page_ram(handle, buf, type, name, size);
	fwrite(buf, 1, size, file);

	fclose(file);
	free(buf);
}

void write_page_file(minipro_handle_t *handle, const char *filename, uint32_t type, const char *name, size_t size)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL)
	{
		minipro_close(handle);
		PERROR("Couldn't open file for reading");
	}

	uint8_t *buf = malloc(size);
	if (!buf)
	{
		ERROR("Can't malloc");
	}

	if (fread(buf, 1, size, file) != size && !cmdopts.size_error)
	{
		free(buf);
		minipro_close(handle);
		ERROR("Short read");
	}
	write_page_ram(handle, buf, type, name, size);

	fclose(file);
	free(buf);
}

void read_fuses(minipro_handle_t *handle, const char *filename, fuse_decl_t *fuses)
{
	uint32_t i, d;
	uint8_t buf[11];
	struct timeval begin, end;
	uint8_t data_length = 0, opcode = fuses[0].minipro_cmd;

	if(opcode & 0x80)
	{
		minipro_close(handle);
		ERROR("Can't read the lock byte for this device!");
	}

	if (Config_init(filename))
	{
		minipro_close(handle);
		PERROR("Couldn't create config");
	}

	printf("Reading fuses... ");
	fflush(stdout);
	gettimeofday(&begin, NULL);
	minipro_begin_transaction(handle);
	for (i = 0; fuses[i].name; i++)
	{
		data_length += fuses[i].length;
		if (fuses[i].minipro_cmd < opcode)
		{
			minipro_close(handle);
			ERROR("fuse_decls are not sorted");
		}
		if (fuses[i + 1].name == NULL || fuses[i + 1].minipro_cmd > opcode)
		{
			minipro_read_fuses(handle, opcode, data_length, buf);
			// Unpacking received buf[] accordingly to fuse_decls with same minipro_cmd
			for (d = 0; fuses[d].name; d++)
			{
				if (fuses[d].minipro_cmd != opcode)
				{
					continue;
				}
				uint32_t value = load_int(&(buf[fuses[d].offset]),
						fuses[d].length, MP_LITTLE_ENDIAN);
				if (Config_set_int(fuses[d].name, value) == -1)
				{
					minipro_close(handle);
					ERROR("Couldn't set configuration");
				}
			}
			opcode = fuses[i + 1].minipro_cmd;
			data_length = 0;
		}
	}
	if (minipro_get_ovc_status(handle, NULL))
	{
		minipro_close(handle);
		ERROR("\nOvercurrent protection!");
	}
	minipro_end_transaction(handle);
	Config_close();
	gettimeofday(&end, NULL);
	printf("%.2fSec  OK\n",
			(double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
}

void write_fuses(minipro_handle_t *handle, const char *filename, fuse_decl_t *fuses)
{

	if (Config_open(filename))
	{
		minipro_close(handle);
		PERROR("Couldn't parse config");
	}

	printf("Writing fuses... ");
	fflush(stdout);
	minipro_begin_transaction(handle);
	uint8_t data_length = 0, opcode = fuses[0].minipro_cmd & 0x7f;
	uint8_t buf[11];
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	uint32_t i, d;
	for (i = 0; fuses[i].name; i++)
	{
		data_length += fuses[i].length;
		if ((fuses[i].minipro_cmd & 0x7f) < opcode)
		{
			minipro_close(handle);
			ERROR("fuse_decls are not sorted");
		}
		if (fuses[i + 1].name == NULL || (fuses[i + 1].minipro_cmd & 0x7f) > opcode)
		{
			for (d = 0; fuses[d].name; d++)
			{
				if ((fuses[d].minipro_cmd & 0x7f) != opcode)
				{
					continue;
				}
				int32_t value = Config_get_int(fuses[d].name);
				if (value == -1)
				{
					minipro_close(handle);
					ERROR("Could not read configuration");
				}
				format_int(&(buf[fuses[d].offset]), value, fuses[d].length,
						MP_LITTLE_ENDIAN);
			}
			minipro_write_fuses(handle, opcode, data_length, buf);

			opcode = fuses[i + 1].minipro_cmd & 0x7f;
			data_length = 0;
		}
	}
	if (minipro_get_ovc_status(handle, NULL))
	{
		minipro_close(handle);
		ERROR("\nOvercurrent protection!");
	}
	minipro_end_transaction(handle);
	Config_close();
	gettimeofday(&end, NULL);
	printf("%.2fSec  OK\n",
			(double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
}

void verify_page_file(minipro_handle_t *handle, const char *filename, uint32_t type, const char *name, size_t size)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL)
	{
		minipro_close(handle);
		PERROR("Couldn't open file for reading");
	}

	/* Loading file */
	size_t file_size = get_file_size(filename);
	uint8_t *file_data = malloc(file_size);
	if (fread(file_data, 1, file_size, file) != file_size)
	{
		minipro_close(handle);
		ERROR("Short read");
	}
	fclose(file);

	minipro_begin_transaction(handle);

	/* Downloading data from chip*/
	uint8_t *chip_data = malloc(size);
	if (cmdopts.size_error)
		read_page_ram(handle, chip_data, type, name, file_size);
	else
		read_page_ram(handle, chip_data, type, name, size);
	minipro_end_transaction(handle);

	uint8_t c1, c2;
	int32_t idx = compare_memory(file_data, chip_data, file_size, &c1, &c2);

	/* No memory leaks */
	free(file_data);
	free(chip_data);

	if (idx != -1)
	{
		minipro_close(handle);
		ERROR2("Verification failed at address 0x%04X: File=0x%02X, Device=0x%02X\n", idx, c1, c2);
	}
	else
	{
		printf("Verification OK\n");
	}
}

/* replace_filename_extension("filename.foo", ".bar") --> "filename.bar" */
static char* replace_filename_extension(const char* filename, const char* extension)
{
	char* buffer = malloc(strlen(filename) + strlen(extension) + 1);
	if (!buffer)
	{
		PERROR("Out of memory");
	}
	buffer[0] = '\0';
	strcat(buffer, filename);
	char* dot = strrchr(buffer, '.');
	if (dot)
	{
		*dot = '\0';
	}
	strcat(buffer, extension);
	return buffer;
}

/* Higher-level logic */
void action_read(const char *filename, minipro_handle_t *handle, device_t *device)
{
	char *code_filename = (char*) filename;
	char *data_filename = (char*) filename;
	char *config_filename = (char*) filename;
	char* default_data_filename = replace_filename_extension(filename,
			".eeprom.bin");
	char* default_config_filename = replace_filename_extension(filename,
			".fuses.conf");

	minipro_begin_transaction(handle); // Prevent device from hanging
	if (cmdopts.page == UNSPECIFIED)
	{
		data_filename = default_data_filename;
		config_filename = default_config_filename;
		cmdopts.page = CODE;
	}
	if (cmdopts.page == CODE)
	{
		read_page_file(handle, code_filename, MP_READ_CODE, "Code",
				device->code_memory_size);
	}
	if (cmdopts.page == DATA && device->data_memory_size)
	{

		read_page_file(handle, data_filename, MP_READ_DATA, "Data",
				device->data_memory_size);
	}
	if (cmdopts.page == CONFIG && device->fuses)
	{
		read_fuses(handle, config_filename, device->fuses);
	}

	minipro_end_transaction(handle);

	free(default_config_filename);
	free(default_data_filename);
}

void action_write(const char *filename, minipro_handle_t *handle, device_t *device)
{
	size_t fsize;
	switch (cmdopts.page)
	{
	case UNSPECIFIED:
	case CODE:
		fsize = get_file_size(filename);
		if (fsize != device->code_memory_size)
		{
			if (!cmdopts.size_error)
			{
				minipro_close(handle);
				ERROR2("Incorrect file size: %zu (needed %u)\n", fsize,
						device->code_memory_size);
			}
			else if (cmdopts.size_nowarn == 0)
				printf("Warning: Incorrect file size: %zu (needed %u)\n",
						fsize, device->code_memory_size);
		}
		break;
	case DATA:

		fsize = get_file_size(filename);
		if (fsize != device->data_memory_size)
		{
			if (!cmdopts.size_error)
			{
				minipro_close(handle);
				ERROR2("Incorrect file size: %zu (needed %u)\n", fsize,
						device->data_memory_size);
			}
			else if (cmdopts.size_nowarn == 0)
				printf("Warning: Incorrect file size: %zu (needed %u)\n",
						fsize, device->data_memory_size);
		}
		break;
	case CONFIG:
		break;
	}
	minipro_begin_transaction(handle);
	if (cmdopts.no_erase == 0)
	{
		printf("Erasing... ");
		fflush(stdout);
		struct timeval begin, end;
		gettimeofday(&begin, NULL);
		uint32_t erase = minipro_erase(handle); //Erase device..;
		if (!erase)
		{
			gettimeofday(&end, NULL);
			printf("%.2fSec OK\n",
					(double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
		}
		uint32_t ovc = minipro_get_ovc_status(handle, NULL);
		minipro_end_transaction(handle);
		if (ovc)
			fprintf(stderr, "Overcurrent protection!");
		if (erase || ovc)
		{
			minipro_close(handle);
			ERROR("Erase failed!\n");
		}
	}

	minipro_begin_transaction(handle);
	if (minipro_get_ovc_status(handle, NULL))
	{
		minipro_end_transaction(handle);
		minipro_close(handle);
		ERROR("Overcurrent protection!");
	}
	if (cmdopts.no_protect_off == 0 && (device->opts4 & 0xc000))
	{
		minipro_protect_off(handle);
	}

	switch (cmdopts.page)
	{
	case UNSPECIFIED:
	case CODE:
		write_page_file(handle, filename, MP_WRITE_CODE, "Code",
				device->code_memory_size);
		if (cmdopts.no_verify == 0)
			verify_page_file(handle, filename, MP_READ_CODE, "Code",
					device->code_memory_size);
		break;
	case DATA:
		write_page_file(handle, filename, MP_WRITE_DATA, "Data",
				device->data_memory_size);
		if (cmdopts.no_verify == 0)
			verify_page_file(handle, filename, MP_READ_DATA, "Data",
					device->data_memory_size);
		break;
	case CONFIG:
		if (device->fuses)
		{
			write_fuses(handle, filename, device->fuses);
		}
		break;
	}
	minipro_end_transaction(handle); // Let prepare_writing() to make an effect

	if (cmdopts.no_protect_on == 0 && (device->opts4 & 0xc000))
	{
		minipro_begin_transaction(handle);
		minipro_protect_on(handle);
		minipro_end_transaction(handle);
	}
}

int main(int argc, char **argv)
{
	parse_cmdline(argc, argv);
	if (!cmdopts.filename && !cmdopts.idcheck_only)
	{
		print_help_and_exit(argv[0], -1);
	}
	//If -D option is enabled then you must supply a device name.
	if ((cmdopts.action && !cmdopts.device) || (cmdopts.idcheck_only && !cmdopts.device))
	{
		USAGE_ERROR("Device required");
	}

	// don't permit skipping the ID read in write-mode
	if (cmdopts.action == action_write && cmdopts.idcheck_skip)
	{
		print_help_and_exit(argv[0], -1);
	}
	// don't permit skipping the ID read in ID only mode
	if (cmdopts.idcheck_only && cmdopts.idcheck_skip)
	{
		print_help_and_exit(argv[0], -1);
	}

	device_t *device = cmdopts.device;
	minipro_handle_t *handle = minipro_open(device);
	handle->icsp = cmdopts.icsp;

	if(!device->read_buffer_size || !device->protocol_id)
	{
		minipro_close(handle);
		ERROR("Unsupported device!");
	}

	// Printing system info
	minipro_print_device_info(handle);

	// Unlocking the TSOP48 adapter (if applicable)
	if (device && device->opts4 == 0x1002078)
	{
		switch (minipro_unlock_tsop48(handle))
		{
		case MP_TSOP48_TYPE_V3:
			printf("Found TSOP adapter V3\n");
			break;
		case MP_TSOP48_TYPE_NONE:
			minipro_end_transaction(handle); //We need this to turn off the power on the ZIF socket.
			minipro_close(handle);
			ERROR("TSOP adapter not found!");
			break;
		case MP_TSOP48_TYPE_V0:
			printf("Found TSOP adapter V0\n");
			break;
		case MP_TSOP48_TYPE_FAKE1:
		case MP_TSOP48_TYPE_FAKE2:
			printf("Fake TSOP adapter found!\n");
			break;
		}
	}

	uint8_t id_type;
	// Verifying Chip ID (if applicable)
	if (cmdopts.idcheck_skip)
	{
		printf("WARNING: skipping Chip ID test\n");
	}
	else if (device->chip_id_bytes_count && device->chip_id)
	{
		minipro_begin_transaction(handle);
		if (minipro_get_ovc_status(handle, NULL))
		{
			minipro_end_transaction(handle);
			minipro_close(handle);
			ERROR("Overcurrent protection!");
		}
		uint32_t chip_id = minipro_get_chip_id(handle, &id_type);
		minipro_end_transaction(handle);
		uint32_t chip_id_temp = chip_id;
		uint8_t shift = 0;
		/* The id_type will tell us the Chip ID type. There are 5 types */
		uint32_t ok = 0;
		switch (id_type)
		{
		case MP_ID_TYPE1: //1-3 bytes ID
		case MP_ID_TYPE2: //4 bytes ID
		case MP_ID_TYPE5: //3 bytes ID, this ID type is returning from 25 SPI series.
			ok = (chip_id == device->chip_id);
			if (ok)
			{
				printf("Chip ID OK: 0x%02X\n", chip_id);
			}
			break;
		case MP_ID_TYPE3: //Microchip controllers with 5 bit revision number.
			chip_id = (chip_id >> 8) | (uint16_t)(chip_id << 8);// firmware bug endianess
			ok = (device->chip_id >> 5  == (chip_id >> 5)); //Throw the chip revision (last 5 bits).
			if (ok)
			{
				printf("Chip ID OK: 0x%04X Rev.0x%02X\n", chip_id >> 5,
						chip_id & 0x1F);
			}
			chip_id >>= 5;
			chip_id_temp = chip_id << 5;
			shift = 5;
			break;
		case MP_ID_TYPE4: //Microchip controllers with 4-5 bit revision number.
			chip_id = (chip_id >> 8) | (uint16_t)(chip_id << 8);// firmware bug endianess
			ok = ( device->chip_id >> device->id_shift == (chip_id >> device->id_shift)); //Throw the chip revision (last .shift bits).
			if (ok)
			{
				printf("Chip ID OK: 0x%04X Rev.0x%02X\n",
						chip_id,
						chip_id & ~(0xFF >>  device->id_shift));
			}
			chip_id >>= device->id_shift;
			chip_id_temp = chip_id << device->id_shift;
			shift = device->id_shift;
			break;
		}

		if (cmdopts.idcheck_only && ok)
		{
			minipro_close(handle);
			exit (0);
		}

		if (!ok)
		{
			const char *name = get_device_from_id(chip_id_temp, device->protocol_id);
			if (cmdopts.idcheck_only)
			{
				printf("Chip ID mismatch: expected 0x%04X, got 0x%04X (%s)\n",
						device->chip_id >> shift, chip_id_temp >> shift, name ? name : "unknown");
				minipro_close(handle);
				exit (0);
			}
			if (cmdopts.idcheck_continue)
			{
				printf(
						"WARNING: Chip ID mismatch: expected 0x%04X, got 0x%04X (%s)\n",
						device->chip_id >> shift, chip_id_temp >> shift, name ? name : "unknown");
			}
			else
			{
				minipro_close(handle);
				ERROR2(
						"Invalid Chip ID: expected 0x%04X, got 0x%04X (%s)\n(use '-y' to continue anyway at your own risk)\n",
						device->chip_id >> shift, chip_id_temp, name ? name : "unknown");
			}
		}
	}
	else if(!cmdopts.filename)
	{
		minipro_close(handle);
		ERROR("Can't read the device ID for this device!");
	}

	cmdopts.action(cmdopts.filename, handle, device);
	minipro_close(handle);

	return (0);
}
