/* 
 * This file is part of OptSearch.
 *
 * OptSearch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OptSearch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OptSearch.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2007-2018 Jessica Jones
 */

#ifndef H_OPTSEARCH_LOGGING_
#define H_OPTSEARCH_LOGGING_

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#include <mpi.h>

typedef enum {
	LOG_LEVEL_ALL = 100,	/* Turn on all logging */
	LOG_LEVEL_OFF = 0,	/* Turn off all logging */
	LOG_LEVEL_TRACE = 50,
	LOG_LEVEL_DEBUG = 25,
	LOG_LEVEL_INFO = 17,
	LOG_LEVEL_WARN = 5,
	LOG_LEVEL_ERROR = 3,
	LOG_LEVEL_FATAL = 1
} log_level;

extern const log_level default_log_level;

extern void log_initialise(FILE * output);
extern void log_set_outstream(FILE * out);

extern void turn_off_logging();
extern void turn_on_logging();

extern void set_log_level(log_level level);
extern log_level get_log_level();

extern int log_debug(const char *fmt, ...);
extern int log_error(const char *fmt, ...);
extern int log_fatal(const char *fmt, ...);
extern int log_info(const char *fmt, ...);
extern int log_warn(const char *fmt, ...);
extern int log_trace(const char *fmt, ...);

#endif				/* H_OPTSEARCH_LOGGING_ */
