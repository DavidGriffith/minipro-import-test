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

#include "database.h"
#include <stdio.h>
#include <string.h>

fuse_decl_t atmel_lock[] = {{.num_fuses = 0,
                             .num_locks = 0x81,
                             .num_uids = 0,
                             .item_size = 1,
                             .word = 1,
                             .erase_num_fuses = 0,
                             .rev_mask = 0,
                             .fnames = NULL,
                             .unames = NULL,
                             .lnames = (const char *[]){"lock_byte"}}};

fuse_decl_t avr_fuses[] = {{.num_fuses = 1,
                            .num_locks = 1,
                            .num_uids = 0,
                            .item_size = 1,
                            .word = 1,
                            .erase_num_fuses = 1,
                            .rev_mask = 0,
                            .fnames = (const char *[]){"fuses"},
                            .unames = NULL,
                            .lnames = (const char *[]){"lock_byte"}}};

fuse_decl_t avr2_fuses[] = {{.num_fuses = 2,
                             .num_locks = 1,
                             .num_uids = 0,
                             .item_size = 1,
                             .word = 1,
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
     .word = 1,
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

device_t infoic_devices[] = {
#include "infoic_devices.h"
    {.name = NULL},
};

device_t infoic2plus_devices[] = {
#include "infoic2plus_devices.h"
    {.name = NULL},
};

device_t *get_device_table(minipro_handle_t *handle) {
  if (handle->version == MP_TL866IIPLUS) {
    return &(infoic2plus_devices[0]);
  }

  return &(infoic_devices[0]);
}

device_t *get_device_by_name(minipro_handle_t *handle, const char *name) {
  device_t *device;

  for (device = get_device_table(handle); device[0].name;
       device = &(device[1])) {
    if (!strcasecmp(name, device->name)) return (device);
  }
  return NULL;
}

const char *get_device_from_id(minipro_handle_t *handle, uint32_t id,
                               uint8_t protocol) {
  device_t *device;
  for (device = get_device_table(handle); device[0].name;
       device = &(device[1])) {
    if (device->chip_id == id && device->protocol_id == protocol &&
        device->chip_id && device->chip_id_bytes_count)
      return (device->name);
  }
  return NULL;
}
