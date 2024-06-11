/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Base and wieldly used definitions
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

 #include "base.hpp"
 #include "printf.h"
 #include "stdio.h"
 #include <stdarg.h>

static const char* const level_str[]= {
	"FATAL",
	"ERROR",
	"WARN ",
	"INFO ",
	"DEBUG"
};

#include <signal.h>

namespace pb {
Abstract::~Abstract() {}

void log_func(const char* file, int line, int level, const char* fmt, ...) noexcept {
	char buff[512] = {0};
	int n = 0;

	// pre
	n = snprintf_(buff, 511, "[%s:%i]'t%s: ", file, line, level_str[level]);
	if (n > 0) fwrite(buff, 1, n, stderr); // write message
	
	// message
	va_list args;
	va_start(args, fmt);
	n = vsnprintf_(buff, 511, fmt, args);
	va_end(args);
	if (n > 0) fwrite(buff, 1, n, stderr); // write message
	fputc('\n', stderr);
}

[[noreturn]] void terminate() {
	#ifdef PIXELBOX_DEBUG
	raise(SIGTRAP); // debugger trap
	#endif
	throw nullptr; // но всё равно сможем словить, если захотим
}

};