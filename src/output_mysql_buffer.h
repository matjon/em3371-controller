/*
 *  Copyright (C) 2020-2021 Mateusz Jończyk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <stdbool.h>

#include "emax_em3371.h"

bool init_mysql_buffer(size_t buffer_size);
void shutdown_mysql_buffer();

bool store_in_mysql_buffer(const struct device_sensor_state *state);
bool pop_from_mysql_buffer(struct device_sensor_state *state);
long get_mysql_buffer_count();
