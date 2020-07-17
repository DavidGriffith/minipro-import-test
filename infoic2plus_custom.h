/*
 * infoic2plus_custom.h - Custom devices and overrides.
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

//Here you can define your own device or overriding existing one.
{
    .name = "AT28C256E",
    .protocol_id = 0x07,
    .variant = 0x26,
    .read_buffer_size =  0x200,
    .write_buffer_size = 0x80,
    .code_memory_size = 0x8000,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x02,
    .opts2 = 0x0040,
    .opts3 = 0x2710,
    .opts4 = 0xc010,
    .opts5 = 0x0200,
    .opts6 = 0x0000,
    .opts7 = 0x0000,
    .opts8 = 0x0c14,
    .package_details = 0x1c000000,
    .config = NULL
},
{
    .name = "AT28C256E@PLCC32",
    .protocol_id = 0x0a,
    .variant = 0x80,
    .read_buffer_size =  0x200,
    .write_buffer_size = 0x80,
    .code_memory_size = 0x8000,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x02,
    .opts2 = 0x0040,
    .opts3 = 0x2710,
    .opts4 = 0xc010,
    .opts5 = 0x0200,
    .opts6 = 0x0000,
    .opts7 = 0x0000,
    .opts8 = 0x751a,
    .package_details = 0xff000000,
    .config = NULL
},
{
    .name = "AT28C256E@SOIC28",
    .protocol_id = 0x07,
    .variant = 0x26,
    .read_buffer_size =  0x200,
    .write_buffer_size = 0x80,
    .code_memory_size = 0x8000,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x02,
    .opts2 = 0x0040,
    .opts3 = 0x2710,
    .opts4 = 0xc010,
    .opts5 = 0x0200,
    .opts6 = 0x0000,
    .opts7 = 0x0000,
    .opts8 = 0x0c14,
    .package_details = 0x9c000000,
    .config = NULL
},
{
    .name = "ATF750C-TEST",
    .protocol_id = 0x2c,
    .variant = 0x0d, // TODO
    .read_buffer_size =  0x00,
    .write_buffer_size = 0x00,
    .code_memory_size = 14504,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x22,
    .opts2 = 0x0084,
    .opts3 = 0x0064,
    .opts4 = 0x2040410,
    .opts5 = 0x2200,
    .opts6 = 0x0000,
    .opts7 = 0x0007,
    .opts8 = 0x0b34,
    .package_details = 0x18000000,
    .config = gal6_acw
},

