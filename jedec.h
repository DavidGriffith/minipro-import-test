#include <stdint.h>

#ifndef JEDEC_H_
#define JEDEC_H_

#define STX 0x02
#define ETX 0x03

#define JED_MIN_SIZE 8
#define JED_MAX_SIZE 1048576
#define ROW_SIZE 40
#define DELIMITER '*'

#define NO_ERROR 0
#define FILE_OPEN_ERROR -1
#define SIZE_ERROR -2
#define FILE_READ_ERROR -3
#define BAD_FORMAT -4
#define TOKEN_NOT_FOUND -5
#define MEMORY_ERROR -6

typedef struct jedec_s {
  const char *device_name;      // Device name
  uint8_t F;                    // Unlisted fuses value (0-1)
  uint8_t G;                    // Security Fuse
  uint16_t QF;                  // How many fuses in the JEDEC file are
  uint8_t QP;                   // Number of pins
  uint16_t C;                   // declared fuses checksum
  uint16_t fuse_checksum;       // calculated fuses checksum
  uint16_t calc_file_checksum;  // calculated file checksum
  uint16_t decl_file_checksum;  // declared file checksum
  uint8_t *fuses;               // Fuses array
} jedec_t;

int read_jedec_file(const char *filename, jedec_t *jedec);
int write_jedec_file(const char *filename, jedec_t *jedec);

#endif /* JEDEC_H_ */
