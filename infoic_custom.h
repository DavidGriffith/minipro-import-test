/*
 * infoic_custom.h - Custom devices and overrides.
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
    .protocol_id = 0x31,
    .variant = 0x26,
    .read_buffer_size =  0x200,
    .write_buffer_size = 0x80,
    .code_memory_size = 0x8000,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x200,
    .opts2 = 0x40,
    .opts3 = 0x2710, //increased write page time to 10ms
    .opts4 = 0xc010,
    .opts5 = 0x00,
    .opts6 = 0x00,
    .opts7 = 0x00,
    .package_details = 0x1c000000,
    .config = NULL
},
{
    .name = "AT28C256E@PLCC32",
    .protocol_id = 0x37,
    .variant = 0x80,
    .read_buffer_size =  0x200,
    .write_buffer_size = 0x80,
    .code_memory_size = 0x8000,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x200,
    .opts2 = 0x40,
    .opts3 = 0x2710, //increased write page time to 10ms
    .opts4 = 0xc010,
    .opts5 = 0x00,
    .opts6 = 0x00,
    .opts7 = 0x00,
    .package_details = 0xff000000,
    .config = NULL
},
{
    .name = "AT28C256E@SOIC28",
    .protocol_id = 0x31,
    .variant = 0x26,
    .read_buffer_size =  0x200,
    .write_buffer_size = 0x80,
    .code_memory_size = 0x8000,
    .data_memory_size = 0x00,
    .data_memory2_size = 0x00,
    .chip_id = 0x0000,
    .chip_id_bytes_count = 0x00,
    .opts1 = 0x200,
    .opts2 = 0x40,
    .opts3 = 0x2710, //increased write page time to 10ms
    .opts4 = 0xc010,
    .opts5 = 0x00,
    .opts6 = 0x00,
    .opts7 = 0x00,
    .package_details = 0x9c000000,
    .config = NULL
},
