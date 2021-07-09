/*
 * database.c - Functions for dealing with the device database.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "xml.h"
#include "database.h"

#ifdef _WIN32
	#include <Shlobj.h>
	#include <shlwapi.h>
	#define STRCASESTR StrStrIA
#else
	#define STRCASESTR strcasestr
#endif

#define PIN_MAP_COUNT 117
pin_map_t pin_map_table[] = {
#include "pin_map.h"
};

// infoic.xml name and tag names
#define INFOIC_NAME "infoic.xml"
#define LOGICIC_NAME "logicic.xml"
#define DEVICE_TAG "device"
#define MANUF_TAG "manufacturer"
#define CUSTOM_TAG "custom"
#define IC_TAG "ic"
#define VECTOR_TAG "vector"
#define NAME_ATTRIBUTE "name"
#define FUSE_ATTRIBUTE "fuses"
#define VOLTAGE_ATTRIBUTE "voltage"
#define TL866II_ATTR_NAME "TL866II"
#define TL866A_ATTR_NAME "TL866A"


// State machine structure used by sax xml parser callback function
// for persistent data between calls.
typedef struct state_machine {
  device_t *device;
  int version;
  int sm_version;
  int custom;
  int print_name;
  uint32_t found;
  int match_id;
  const char *device_name;
  uint32_t tl866a_count;
  uint32_t tl866a_custom_count;
  uint32_t tl866ii_count;
  uint32_t tl866ii_custom_count;
  uint8_t load_vectors;
} state_machine_t;


fuse_decl_t atmel_lock[] = {
    {.num_fuses = 0,
     .num_locks = 0x81,
     .num_uids = 0,
     .item_size = 1,
     .word = 0,
     .erase_num_fuses = 0,
     .rev_mask = 0,
     .fnames = NULL,
     .unames = NULL,
     .lnames = (const char *[]){"lock_byte"}}};

fuse_decl_t avr_fuses[] = {
    {.num_fuses = 1,
     .num_locks = 1,
     .num_uids = 0,
     .item_size = 1,
     .word = 0,
     .erase_num_fuses = 1,
     .rev_mask = 0,
     .fnames = (const char *[]){"fuses"},
     .unames = NULL,
     .lnames = (const char *[]){"lock_byte"}}};

fuse_decl_t avr2_fuses[] = {
    {.num_fuses = 2,
     .num_locks = 1,
     .num_uids = 0,
     .item_size = 1,
     .word = 0,
     .erase_num_fuses = 2,
     .rev_mask = 0,
     .fnames = (const char *[]){"fuses_lo", "fuses_hi"},
     .unames = NULL,
     .lnames = (const char *[]){"lock_byte"}}};

fuse_decl_t avr3_fuses[] = {
    {.num_fuses = 3,
     .num_locks = 1,
     .num_uids = 0,
     .item_size = 1,
     .word = 0,
     .erase_num_fuses = 3,
     .rev_mask = 0,
     .fnames = (const char *[]){"fuses_lo", "fuses_hi", "fuses_ext"},
     .unames = NULL,
     .lnames = (const char *[]){"lock_byte"}}};

fuse_decl_t pic_fuses[] = {
    {.num_fuses = 1,
     .num_locks = 0,
     .num_uids = 4,
     .item_size = 2,
     .word = 2,
     .erase_num_fuses = 1,
     .rev_mask = 5,
     .fnames = (const char *[]){"conf_word"},
     .unames = (const char *[]){"user_id0", "user_id1", "user_id2", "user_id3"},
     .lnames = NULL}};

fuse_decl_t pic2_fuses[] = {
    {.num_fuses = 2,
     .num_locks = 0,
     .num_uids = 4,
     .item_size = 2,
     .word = 2,
     .erase_num_fuses = 2,
     .rev_mask = 5,
     .fnames = (const char *[]){"conf_word1", "conf_word2"},
     .unames = (const char *[]){"user_id0", "user_id1", "user_id2", "user_id3"},
     .lnames = NULL}};

fuse_decl_t pic3_fuses[] = {
    {.num_fuses = 14,
     .num_locks = 0,
     .num_uids = 8,
     .item_size = 1,
     .word = 1,
     .erase_num_fuses = 1,
     .rev_mask = 4,
     .fnames = (const char *[]){"conf_byte0", "conf_byte1", "conf_byte2",
                                "conf_byte3", "conf_byte4", "conf_byte5",
                                "conf_byte6", "conf_byte7", "conf_byte8",
                                "conf_byte9", "conf_byte10", "conf_byte11",
                                "conf_byte12", "conf_byte13"},
     .unames = (const char *[]){"user_id0", "user_id1", "user_id2", "user_id3",
                                "user_id4", "user_id5", "user_id6", "user_id7"},
     .lnames = NULL}};

fuse_decl_t pic4_fuses[] = {
    {.num_fuses = 8,
     .num_locks = 0,
     .num_uids = 0,
     .item_size = 1,
     .word = 1,
     .erase_num_fuses = 1,
     .rev_mask = 4,
     .fnames = (const char *[]){"conf_byte0", "conf_byte1", "conf_byte2",
                                "conf_byte3", "conf_byte4", "conf_byte5",
                                "conf_byte6", "conf_byte7"},
     .unames = NULL,
     .lnames = NULL}};

gal_config_t gal1_acw[] = {
    {.acw_bits =
         (uint16_t[]){
             2128, 2129, 2130, 2131, 2132, 2133, 2134, 2135, 2136, 2137, 2138,
             2139, 2140, 2141, 2142, 2143, 2144, 2145, 2146, 2147, 2148, 2149,
             2150, 2151, 2152, 2153, 2154, 2155, 2156, 2157, 2158, 2159, 2048,
             2049, 2050, 2051, 2193, 2120, 2121, 2122, 2123, 2124, 2125, 2126,
             2127, 2192, 2052, 2053, 2054, 2055, 2160, 2161, 2162, 2163, 2164,
             2165, 2166, 2167, 2168, 2169, 2170, 2171, 2172, 2173, 2174, 2175,
             2176, 2177, 2178, 2179, 2180, 2181, 2182, 2183, 2184, 2185, 2186,
             2187, 2188, 2189, 2190, 2191},
     .fuses_size = 0x20,
     .row_width = 0x40,
     .ues_address = 2056,
     .ues_size = 64,
     .acw_address = 0x3c,
     .acw_size = 0x52}};

gal_config_t gal2_acw[] = {
    {.acw_bits =
         (uint16_t[]){
             2048, 2049, 2050, 2051, 2193, 2120, 2121, 2122, 2123, 2128, 2129,
             2130, 2131, 2132, 2133, 2134, 2135, 2136, 2137, 2138, 2139, 2140,
             2141, 2142, 2143, 2144, 2145, 2146, 2147, 2148, 2149, 2150, 2151,
             2152, 2153, 2154, 2155, 2156, 2157, 2158, 2159, 2160, 2161, 2162,
             2163, 2164, 2165, 2166, 2167, 2168, 2169, 2170, 2171, 2172, 2173,
             2174, 2175, 2176, 2177, 2178, 2179, 2180, 2181, 2182, 2183, 2184,
             2185, 2186, 2187, 2188, 2189, 2190, 2191, 2124, 2125, 2126, 2127,
             2192, 2052, 2053, 2054, 2055},
     .fuses_size = 0x20,
     .row_width = 0x40,
     .ues_address = 2056,
     .ues_size = 64,
     .acw_address = 0x3c,
     .acw_size = 0x52}};

gal_config_t atf16V8c_acw[] = { // ATF16V8C and ATF16V8CZ
    {.acw_bits =
         (uint16_t[]){
             2048, 2049, 2050, 2051, 2193, 2120, 2121, 2122, 2123, 2128, 2129,
             2130, 2131, 2132, 2133, 2134, 2135, 2136, 2137, 2138, 2139, 2140,
             2141, 2142, 2143, 2144, 2145, 2146, 2147, 2148, 2149, 2150, 2151,
             2152, 2153, 2154, 2155, 2156, 2157, 2158, 2159, 2160, 2161, 2162,
             2163, 2164, 2165, 2166, 2167, 2168, 2169, 2170, 2171, 2172, 2173,
             2174, 2175, 2176, 2177, 2178, 2179, 2180, 2181, 2182, 2183, 2184,
             2185, 2186, 2187, 2188, 2189, 2190, 2191, 2124, 2125, 2126, 2127,
             2192, 2052, 2053, 2054, 2055},
     .fuses_size = 0x20,
     .row_width = 0x40,
     .ues_address = 2056,
     .ues_size = 64,
     .powerdown_row = 0x3b,
     .acw_address = 0x3c,
     .acw_size = 0x52}};

gal_config_t gal3_acw[] = {
    {.acw_bits =
         (uint16_t[]){
             2640, 2641, 2642, 2643, 2644, 2645, 2646, 2647, 2648, 2649, 2650,
             2651, 2652, 2653, 2654, 2655, 2656, 2657, 2658, 2659, 2660, 2661,
             2662, 2663, 2664, 2665, 2666, 2667, 2668, 2669, 2670, 2671, 2560,
             2561, 2562, 2563, 2705, 2632, 2633, 2634, 2635, 2636, 2637, 2638,
             2639, 2704, 2564, 2565, 2566, 2567, 2672, 2673, 2674, 2675, 2676,
             2677, 2678, 2679, 2680, 2681, 2682, 2683, 2684, 2685, 2686, 2687,
             2688, 2689, 2690, 2691, 2692, 2693, 2694, 2695, 2696, 2697, 2698,
             2699, 2700, 2701, 2702, 2703},
     .fuses_size = 0x28,
     .row_width = 0x40,
     .ues_address = 2568,
     .ues_size = 64,
     .acw_address = 0x3c,
     .acw_size = 0x52}};

gal_config_t gal4_acw[] = {
    {.acw_bits =
         (uint16_t[]){
             2560, 2561, 2562, 2563, 2705, 2632, 2633, 2634, 2635, 2640, 2641,
             2642, 2643, 2644, 2645, 2646, 2647, 2648, 2649, 2650, 2651, 2652,
             2653, 2654, 2655, 2656, 2657, 2658, 2659, 2660, 2661, 2662, 2663,
             2664, 2665, 2666, 2667, 2668, 2669, 2670, 2671, 2672, 2673, 2674,
             2675, 2676, 2677, 2678, 2679, 2680, 2681, 2682, 2683, 2684, 2685,
             2686, 2687, 2688, 2689, 2690, 2691, 2692, 2693, 2694, 2695, 2696,
             2697, 2698, 2699, 2700, 2701, 2702, 2703, 2636, 2637, 2638, 2639,
             2704, 2564, 2565, 2566, 2567},
     .fuses_size = 0x28,
     .row_width = 0x40,
     .ues_address = 2568,
     .ues_size = 64,
     .acw_address = 0x3c,
     .acw_size = 0x52}};

gal_config_t gal5_acw[] = {
    {.acw_bits = (uint16_t[]){5809, 5808, 5811, 5810, 5813, 5812, 5815,
                              5814, 5817, 5816, 5819, 5818, 5821, 5820,
                              5823, 5822, 5825, 5824, 5827, 5826},
     .fuses_size = 0x2C,
     .row_width = 0x84,
     .ues_address = 5828,
     .ues_size = 64,
     .acw_address = 0x10,
     .acw_size = 0x14}};

gal_config_t atf22v10c_acw[] = { // ATF22V10C(Q)
    {.acw_bits = (uint16_t[]){5809, 5808, 5811, 5810, 5813, 5812, 5815,
                              5814, 5817, 5816, 5819, 5818, 5821, 5820,
                              5823, 5822, 5825, 5824, 5827, 5826},
     .fuses_size = 0x2C,
     .row_width = 0x84,
     .ues_address = 5828,
     .ues_size = 64,
     .powerdown_row = 0x3b,
     .acw_address = 0x10,
     .acw_size = 0x14}};

gal_config_t atf750c_acw[] = { // ATF750C(L)
    {.acw_bits = (uint16_t[]){ // TODO: order unclear
      //14394, // security?
      //14501, // ???
      //14502, // ???
      //14503, // ???
      14398, 14397, 14396, 14395, 14366, 14465, 14464, //  Q9 S6..S0 (pin 23 on DIP24)
      14402, 14401, 14400, 14399, 14369, 14468, 14467, //  Q8 S6..S0
      14406, 14405, 14404, 14403, 14372, 14471, 14470, //  Q7 S6..S0
      14410, 14409, 14408, 14407, 14375, 14474, 14473, //  Q6 S6..S0
      14414, 14413, 14412, 14411, 14378, 14477, 14476, //  Q5 S6..S0
      14418, 14417, 14416, 14415, 14381, 14480, 14479, //  Q4 S6..S0
      14422, 14421, 14420, 14419, 14384, 14483, 14482, //  Q3 S6..S0
      14426, 14425, 14424, 14423, 14387, 14486, 14485, //  Q2 S6..S0
      14430, 14429, 14428, 14427, 14390, 14489, 14488, //  Q1 S6..S0
      14434, 14433, 14432, 14431, 14393, 14492, 14491, //  Q0 S6..S0 (pin 14 on DIP24)
    },
     .fuses_size = 84,
     .row_width = 171,
     .ues_address = 14435,
     .ues_size = 64,
     .acw_address = 0x10,
     .acw_size = 3*10 + 4*10}};

/* TODO: remove once order of config bits is clear
gal_config_t gal6b_acw[] = { // e.g. for ATF750C
    {.acw_bits = (uint16_t[]){ // TODO: order unclear
      //14394, // security?
      //14501, // ???
      //14502, // ???
      //14503, // ???
      14366, 14465, 14464, //  Q9 S2..S0 (pin 23 on DIP24)
      14369, 14468, 14467, //  Q8 S2..S0
      14372, 14471, 14470, //  Q7 S2..S0
      14375, 14474, 14473, //  Q6 S2..S0
      14378, 14477, 14476, //  Q5 S2..S0
      14381, 14480, 14479, //  Q4 S2..S0
      14384, 14483, 14482, //  Q3 S2..S0
      14387, 14486, 14485, //  Q2 S2..S0
      14390, 14489, 14488, //  Q1 S2..S0
      14393, 14492, 14491, //  Q0 S2..S0 (pin 14 on DIP24)
      14398, 14397, 14396, 14395, // Q9 S6..S3     
      14402, 14401, 14400, 14399, // Q8 S6..S3     
      14406, 14405, 14404, 14403, // Q7 S6..S3     
      14410, 14409, 14408, 14407, // Q6 S6..S3     
      14414, 14413, 14412, 14411, // Q5 S6..S3     
      14418, 14417, 14416, 14415, // Q4 S6..S3     
      14422, 14421, 14420, 14419, // Q3 S6..S3     
      14426, 14425, 14424, 14423, // Q2 S6..S3     
      14430, 14429, 14428, 14427, // Q1S6..S3     
      14434, 14433, 14432, 14431  // Q0 S6..S3     
    },
     .fuses_size = 84,
     .row_width = 171,
     .ues_address = 14435,
     .ues_size = 64,
     .acw_address = 0x10,
     .acw_size = 3*10 + 4*10}};
*/

// Parse a numeric value from an attribute tag
static uint32_t get_value(const uint8_t *xml_device, size_t size,
                          char *attr_name, int *err) {
  Memblock memblock = get_attribute(
      xml_device, size, (Memblock){strlen(attr_name), (uint8_t *)attr_name});
  char attr[64];
  if (memblock.b && memblock.z < sizeof(attr)) {
    memcpy(attr, memblock.b, memblock.z);
    attr[memblock.z] = 0;
    return strtoul(attr, NULL, 0);
  }
  (*err)++;
  return 0;
}

static int load_mem_device(const uint8_t *xml_device, size_t size, device_t *device,
                           uint8_t version) {
  int err = 0;

  device->protocol_id = get_value(xml_device, size, "protocol_id", &err);
  device->variant = get_value(xml_device, size, "variant", &err);
  device->read_buffer_size =
      get_value(xml_device, size, "read_buffer_size", &err);
  device->write_buffer_size =
      get_value(xml_device, size, "write_buffer_size", &err);
  device->code_memory_size =
      get_value(xml_device, size, "code_memory_size", &err);
  device->data_memory_size =
      get_value(xml_device, size, "data_memory_size", &err);
  device->data_memory2_size =
      get_value(xml_device, size, "data_memory2_size", &err);
  device->chip_id = get_value(xml_device, size, "chip_id", &err);
  device->chip_id_bytes_count =
      get_value(xml_device, size, "chip_id_bytes_count", &err);
  device->opts1 = get_value(xml_device, size, "opts1", &err);
  device->opts2 = get_value(xml_device, size, "opts2", &err);
  device->opts3 = get_value(xml_device, size, "opts3", &err);
  device->opts4 = get_value(xml_device, size, "opts4", &err);
  device->opts5 = get_value(xml_device, size, "opts5", &err);
  device->opts6 = get_value(xml_device, size, "opts6", &err);
  device->opts7 = get_value(xml_device, size, "opts7", &err);
  if (version == MP_TL866IIPLUS)
    device->opts8 = get_value(xml_device, size, "opts8", &err);
  device->package_details =
      get_value(xml_device, size, "package_details", &err);

  if (err) return EXIT_FAILURE;

  // Parse configuration name
  Memblock fuses = get_attribute(
      xml_device, size,
      (Memblock){strlen(FUSE_ATTRIBUTE), (uint8_t *)FUSE_ATTRIBUTE});
  if (!fuses.b) return EXIT_FAILURE;

  if (!strncasecmp((char *)fuses.b, "atmel_lock", fuses.z))
    device->config = atmel_lock;
  else if (!strncasecmp((char *)fuses.b, "avr_fuses", fuses.z))
    device->config = avr_fuses;
  else if (!strncasecmp((char *)fuses.b, "avr2_fuses", fuses.z))
    device->config = avr2_fuses;
  else if (!strncasecmp((char *)fuses.b, "avr3_fuses", fuses.z))
    device->config = avr3_fuses;
  else if (!strncasecmp((char *)fuses.b, "pic_fuses", fuses.z))
    device->config = pic_fuses;
  else if (!strncasecmp((char *)fuses.b, "pic2_fuses", fuses.z))
    device->config = pic2_fuses;
  else if (!strncasecmp((char *)fuses.b, "pic3_fuses", fuses.z))
    device->config = pic3_fuses;
  else if (!strncasecmp((char *)fuses.b, "pic4_fuses", fuses.z))
    device->config = pic4_fuses;
  else if (!strncasecmp((char *)fuses.b, "gal1_acw", fuses.z))
    device->config = gal1_acw;
  else if (!strncasecmp((char *)fuses.b, "gal2_acw", fuses.z))
    device->config = gal2_acw;
  else if (!strncasecmp((char *)fuses.b, "gal3_acw", fuses.z))
    device->config = gal3_acw;
  else if (!strncasecmp((char *)fuses.b, "gal4_acw", fuses.z))
    device->config = gal4_acw;
  else if (!strncasecmp((char *)fuses.b, "gal5_acw", fuses.z))
    device->config = gal5_acw;
  else if (!strncasecmp((char *)fuses.b, "atf16V8c_acw", fuses.z))
    device->config = atf16V8c_acw;
  else if (!strncasecmp((char *)fuses.b, "atf22v10c_acw", fuses.z))
    device->config = atf22v10c_acw;
  else if (!strncasecmp((char *)fuses.b, "atf750c_acw", fuses.z))
    device->config = atf750c_acw;
  else if (!strncasecmp((char *)fuses.b, "NULL", fuses.z))
    device->config = NULL;
  else
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

// Load a device from an xml 'ic' tag
static int load_logic_device(const uint8_t *xml_device, size_t size, device_t *device) {
  Memblock voltage = get_attribute(
      xml_device, size,
      (Memblock){strlen(VOLTAGE_ATTRIBUTE), (uint8_t *)VOLTAGE_ATTRIBUTE});
  if (!voltage.b) return EXIT_FAILURE;

  if (!strncasecmp((char *)voltage.b, "5V", voltage.z))
    device->voltage = 0;
  else if (!strncasecmp((char *)voltage.b, "3V3", voltage.z))
    device->voltage = 1;
  else if (!strncasecmp((char *)voltage.b, "2V5", voltage.z))
    device->voltage = 2;
  else if (!strncasecmp((char *)voltage.b, "1V8", voltage.z))
    device->voltage = 3;
  else
    return EXIT_FAILURE;

  int err = 0;
  device->pin_count = get_value(xml_device, size, "pins", &err);
  if (err) return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

// Load a device from an xml 'ic' tag
static int load_device(const uint8_t *xml_device, size_t size, device_t *device,
                       uint8_t version) {
  int err = 0;

  Memblock memblock = get_attribute(
      xml_device, size,
      (Memblock){strlen(NAME_ATTRIBUTE), (uint8_t *)NAME_ATTRIBUTE});
  if (!memblock.b || memblock.z > sizeof(device->name)) return EXIT_FAILURE;
  memcpy(device->name, memblock.b, memblock.z);

  device->type = get_value(xml_device, size, "type", &err);
    if (err) return EXIT_FAILURE;
  int ret;
  if (device->type == 5) {
    ret = load_logic_device(xml_device, size, device);
  } else {
    ret = load_mem_device(xml_device, size, device, version);
  }
  return ret;
}

// Compare a device by protocol ID/device ID or protocol ID/package
// If the device match the device name is returned in device->name
static int compare_device(const uint8_t *xml_device, size_t size,
                          device_t *device, uint8_t version) {
  int err = 0;

  uint8_t protocol_id = get_value(xml_device, size, "protocol_id", &err);
  uint8_t chip_id_bytes_count =
      get_value(xml_device, size, "chip_id_bytes_count", &err);
  uint32_t chip_id = get_value(xml_device, size, "chip_id", &err);
  uint32_t package_details =
      get_value(xml_device, size, "package_details", &err);

  if (err) return EXIT_FAILURE;

  uint32_t pin_count = get_pin_count(package_details);
  uint8_t match_package =
      device->package_details ? (device->package_details == pin_count) : 1;

  if (chip_id && chip_id_bytes_count && match_package && device->chip_id &&
      device->chip_id == chip_id &&
      (match_package || device->protocol_id == protocol_id)) {
    Memblock memblock = get_attribute(
        xml_device, size,
        (Memblock){strlen(NAME_ATTRIBUTE), (uint8_t *)NAME_ATTRIBUTE});
    if (!memblock.b || memblock.z > sizeof(device->name)) return EXIT_FAILURE;
    memcpy(device->name, memblock.b, memblock.z);
  }
  return EXIT_SUCCESS;
}

// XML SAX parser handler. Each xml tag pair is dispatched here.
// The persistent state machine data is kept in parser->userdata structure
static int sax_callback(int type, const uint8_t *tag, size_t taglen,
                        Parser *parser) {
  state_machine_t *sm = parser->userdata;
  Memblock memblock;

  if (type == OPENTAG_ || type == SELFCLOSE_) {
    // Get database version
    memblock = get_attribute(
        tag, taglen, (Memblock){strlen(DEVICE_TAG), (uint8_t *)DEVICE_TAG});
    if (memblock.b &&
        !strncasecmp((char *)memblock.b, TL866II_ATTR_NAME, memblock.z))
      sm->sm_version = MP_TL866IIPLUS;
    else if (memblock.b &&
             !strncasecmp((char *)memblock.b, TL866A_ATTR_NAME, memblock.z))
      sm->sm_version = MP_TL866A;

    // Get manufacturer/custom item
    if (taglen && !strncasecmp((char *)tag, MANUF_TAG, strlen(MANUF_TAG))) {
      sm->custom = 0;
      return XML_OK;
    }
    if (taglen && !strncasecmp((char *)tag, CUSTOM_TAG, strlen(CUSTOM_TAG))) {
      sm->custom = 1;
      return XML_OK;
    }

    // Filter by "IC tag" below
    if (taglen && strncasecmp((char *)tag, IC_TAG, strlen(IC_TAG)))
      return XML_OK;
    if (sm->sm_version == MP_TL866IIPLUS) {
      sm->custom ? sm->tl866ii_custom_count++ : sm->tl866ii_count++;
    } else if (sm->sm_version == MP_TL866A) {
      sm->custom ? sm->tl866a_custom_count++ : sm->tl866a_count++;
    }

    /*
     * Filter only devices from the desired database.
     * We pass 0 to sm->version to just traverse the entire xml
     * and count all chips.
     */
    if (sm->sm_version != sm->version) return XML_OK;

    // Grab the device name
    memblock = get_attribute(
        tag, taglen,
        (Memblock){strlen(NAME_ATTRIBUTE), (uint8_t *)NAME_ATTRIBUTE});
    if (!memblock.b) return EXIT_FAILURE;
    char name[sizeof((device_t){0}).name];
    if (memblock.z > sizeof(name)) return EXIT_FAILURE;
    memset(name, 0, sizeof(name));
    memcpy(name, memblock.b, memblock.z);

    // Only print device name
    if (sm->print_name) {
      // Print only devices that match the chip ID (SPI autodetect -a)
      if (sm->match_id) {
        if (compare_device(tag, taglen, sm->device, sm->version))
          return EXIT_FAILURE;
        if (strlen(sm->device->name)) {
          fprintf(stdout, "%s%s\n", sm->device->name,
                  sm->custom == 1 ? "(custom)" : "");
          fflush(stdout);
          sm->found++;
          memset(sm->device->name, 0, sizeof(sm->device->name));
        }
        return XML_OK;
      }

      // Print all device that match the name (-l and -L)
      if (!sm->device_name || STRCASESTR(name, sm->device_name)) {
        fprintf(stdout, "%s%s\n", name, sm->custom == 1 ? "(custom)" : "");
        fflush(stdout);
      }
      return XML_OK;
    }

    // Search by chip ID (get_device_from_id)
    if (!sm->device_name) {
      if (sm->found && !sm->custom) return XML_OK;
      if (compare_device(tag, taglen, sm->device, sm->version))
        return EXIT_FAILURE;
      if (strlen(sm->device->name)) sm->found = 1;
      return XML_OK;
    }

    // Search and load device (-p and -d)
    if (strcasecmp(sm->device_name, name)) return XML_OK;
    if (sm->found && !sm->custom) return XML_OK;
    if (load_device(tag, taglen, sm->device, sm->version))
      return EXIT_FAILURE;
    sm->found = 1;
    sm->load_vectors = 1;
  }

  if (type == SELFCLOSE_ || type == NORMALCLOSE_ || type == FRAMECLOSE_) {
    if (taglen < 1)
      return XML_OK;
    if (!strncasecmp((char *)tag+1, IC_TAG, taglen-1))
      sm->load_vectors = 0;
    if (sm->load_vectors && !strncasecmp((char *)tag+1, VECTOR_TAG, taglen-1)) {
      sm->device->vectors = realloc(sm->device->vectors, sm->device->pin_count * (sm->device->vector_count + 1));
      uint8_t *vector = sm->device->vectors + sm->device->pin_count * sm->device->vector_count;
      int n = 0;
      int i;
      for (i = 0; i < parser->contentlen; i++) {
        switch (parser->content[i]) {
        case ' ': case '\r': case '\n': case '\t': break;
        case '0': vector[n++] = 0; break;
        case '1': vector[n++] = 1; break;
        case 'L': vector[n++] = 2; break;
        case 'H': vector[n++] = 3; break;
        case 'C': vector[n++] = 4; break;
        case 'Z': vector[n++] = 5; break;
        case 'X': vector[n++] = 6; break;
        case 'G': vector[n++] = 7; break;
        case 'V': vector[n++] = 8; break;
        default: return EXIT_FAILURE;
        }
        if (n > sm->device->pin_count)
          return EXIT_FAILURE;
      }
      if (n < sm->device->pin_count)
        return EXIT_FAILURE;
      sm->device->vector_count++;
    }
  }

  return XML_OK;
}

// Search and return database xml file
static FILE* get_database_file(const char *name){
#ifdef _WIN32
  char path[MAX_PATH];
  SHGetSpecialFolderPathA(NULL, path, CSIDL_COMMON_APPDATA, 0);
  strcat(path, "\\minipro\\");
#else
  char path[PATH_MAX] = SHARE_INSTDIR "/";
#endif
  strncat(path, name, sizeof(path));
  path[sizeof(path)-1] = '\0';

  // Open datbase xml file
  FILE *file = fopen(path, "rb");
  if (!file)
    file = fopen(name, "rb");
  if (!file) {
    perror(name);
    return NULL;
  }
  return file;
}

// Parse xml database file
static int parse_xml_file(state_machine_t *sm, const char *name) {
  // Open datbase xml file
  FILE *file = get_database_file(name);
  if (!file) return EXIT_FAILURE;

  // Begin xml parse
  Parser parser = {file, sax_callback, sm};

  int ret = parse(&parser);
  done(&parser);
  fclose(file);
  if (ret) {
    fprintf(stderr, "An error occurred while parsing XML database.\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

// Parse xml database file
static int parse_xml(state_machine_t *sm) {
  int ret = parse_xml_file(sm, LOGICIC_NAME);
  if (ret)
    return ret;
  return parse_xml_file(sm, INFOIC_NAME);
}

void free_device(device_t *device) {
  if (device)
    free(device->vectors);
  free(device);
}

// XML based device search
device_t *get_device_by_name(uint8_t version, const char *name) {
  if (!name) return NULL;

  // Initialize state machine structure
  device_t *device = device = calloc(1, sizeof(device_t));

  if (!device) {
    fprintf(stderr, "Out of memory\n");
    return NULL;
  }

  if (version == MP_TL866CS) version = MP_TL866A;
  state_machine_t sm = {device, version, -1, -1, 0, 0, 0, name, 0, 0, 0, 0};
  int ret = parse_xml(&sm);

  if (ret || !sm.found) {
    free_device(device);
    device = NULL;
  }
  return device;
}

// Get first device name found in the database from a device ID
const char *get_device_from_id(uint8_t version, uint32_t chip_id, uint8_t protocol) {
  // Initialize state machine structure
  device_t device;
  device.chip_id = chip_id;
  device.protocol_id = protocol;
  device.package_details = 0;
  memset(device.name, 0 , sizeof(device.name));
  if (version == MP_TL866CS) version = MP_TL866A;
  state_machine_t sm = {&device, version, -1, -1, 0, 0, 1, NULL, 0, 0, 0, 0};

  if(parse_xml(&sm)) return NULL;
  return sm.found ? strdup(device.name) : NULL;
}

/* List all devices from XML
 * If name == NULL list all devices
 */
int list_devices(uint8_t version, const char *name, uint32_t chip_id,
                 uint32_t package_details, uint32_t *count) {
  // Initialize state machine structure
  device_t device;
  device.chip_id = chip_id;
  device.package_details = package_details;
  memset(device.name, 0, sizeof(device.name));
  if (version == MP_TL866CS) version = MP_TL866A;
  int flag = (chip_id || package_details) ? 1 : 0;
  state_machine_t sm = {&device, version, -1, -1, 1, 0, flag, name, 0, 0, 0, 0};

  if (parse_xml(&sm)) return EXIT_FAILURE;
  if (count) *count = sm.found;
  return EXIT_SUCCESS;
}

// Print database chip count
int print_chip_count() {
  // Initialize state machine structure
  state_machine_t sm = {NULL, 0, -1, -1, 0, 0, 0, NULL, 0, 0, 0, 0};

  if (parse_xml(&sm)) return EXIT_FAILURE;

  fprintf(stderr,
          "TL866A/CS:\t%u devices, %u custom\nTL866II+:\t%u devices, %u custom\n",
          sm.tl866a_count, sm.tl866a_custom_count, sm.tl866ii_count,
          sm.tl866ii_custom_count);
  return EXIT_SUCCESS;
}

uint32_t get_pin_count(uint32_t package_details) {
  if (package_details == 0xff000000) return 32;
  return PIN_COUNT(package_details);
}

pin_map_t *get_pin_map(uint8_t index){
	if(index >= PIN_MAP_COUNT)
		return NULL;
	return &pin_map_table[index];
}

