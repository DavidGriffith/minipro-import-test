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

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "database.h"
#include "minipro.h"

device_t infoic_devices[] =
{
#include "infoic_devices.h"
		{ .name = NULL }, };

device_t infoic2plus_devices[] =
{
#include "infoic2plus_devices.h"
		{ .name = NULL }, };

device_t *get_device_table(minipro_handle_t *handle)
{
	if (strcmp(handle->model, "TL866II+") == 0) {
		return &(infoic2plus_devices[0]);
	}

	return &(infoic_devices[0]);
}

device_t *get_device_by_name(minipro_handle_t *handle, const char *name)
{
	device_t *device;

	for (device = get_device_table(handle); device[0].name; device = &(device[1]))
	{
		if (!strcasecmp(name, device->name))
			return (device);
	}
	return NULL;
}

