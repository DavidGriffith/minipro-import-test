/*
 * usb.h - Low level USB declarations and definitions.
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

#ifndef USB_H_
#define USB_H_

#include <libusb.h>

int msg_send(libusb_device_handle *handle, uint8_t *buffer, size_t size);
int msg_recv(libusb_device_handle *handle, uint8_t *buffer, size_t size);
int write_payload(libusb_device_handle *handle, uint8_t *buffer, size_t length);
int read_payload(libusb_device_handle *handle, uint8_t *buffer, size_t length);
#endif
