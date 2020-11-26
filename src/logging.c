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

#include "logging.h"

/* This is a problem when it is too small */
#define MAX_MSG_SIZE 8192

/* This should be more than enough */
#define MAX_PREFIX_SIZE 22

const log_level default_log_level = LOG_LEVEL_DEBUG;

static log_level current_log_level = LOG_LEVEL_INFO;

int rank;
int namlen;
char hostname[MPI_MAX_PROCESSOR_NAME] = { '\0' };

FILE *outstream;

void turn_off_logging()
{
	set_log_level(LOG_LEVEL_OFF);
}

void turn_on_logging()
{
	set_log_level(LOG_LEVEL_ALL);
}

log_level get_log_level()
{
	return current_log_level;
}

void set_log_level(log_level level)
{
	current_log_level = level;
}

void log_initialise(FILE * out)
{
	if (out == NULL)
		out = stderr;
	outstream = out;

	MPI_Get_processor_name(hostname, &namlen);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
}

/* TODO REFACTOR this is all still terrible.  There is too much repetition for my liking, and a bit more thought would give a more elegant solution. */

void log_set_outstream(FILE * out)
{
	if (out != NULL)
		outstream = out;
}

int log_message(const char *msg)
{
	char timestamp[10];
	int retval = 0;

	if (outstream == NULL)
		log_initialise(NULL);

	strftime(timestamp, sizeof timestamp, "%T", localtime(&(time_t) {
							      time(NULL)}
		 ));
	fprintf(outstream, "%s (%s:%d) ", timestamp, hostname, rank);
	fprintf(outstream, msg);
	/* TODO This is unlikely to be a very useful return value */
	retval = fprintf(outstream, "\n");
	return retval;
}


int log_message_with_prefix(int msg_log_level, const char * prefix, const char *fmt, ...)
{
	char buffer[MAX_MSG_SIZE] = { '\0' };
	int prefix_length = 0;
	char *start = NULL;
	int retval = 0;

	log_level current_log_level = get_log_level();

	if (current_log_level >= msg_log_level) {
		prefix_length = strlen(prefix);
		start = &buffer[prefix_length];
		strncat(buffer, prefix, MAX_MSG_SIZE - prefix_length - 1);
		va_list args;
		va_start(args, fmt);
		vsnprintf(start, MAX_MSG_SIZE-1, fmt, args);
		retval = log_message(buffer);
		va_end(args);
	}
	return retval;
}

int log_debug(const char *fmt, ...)
{
	int retval = 0;
	log_level current_log_level = get_log_level();
	const char * prefix = "[DEBUG] ";
 
	if (current_log_level >= LOG_LEVEL_DEBUG) {
		va_list args;
		va_start(args, fmt);
		retval = log_message_with_prefix(LOG_LEVEL_DEBUG, prefix, fmt, args);
		va_end(args);
	}

	return retval;
}

int log_warn(const char *fmt, ...)
{
	int retval = 0;
	log_level current_log_level = get_log_level();
	const char *prefix = "[WARN] ";
 
	if (current_log_level >= LOG_LEVEL_WARN) {
		va_list args;
		va_start(args, fmt);
		retval = log_message_with_prefix(LOG_LEVEL_WARN, prefix, fmt, args);
		va_end(args);
	}

	return retval;
}

int log_error(const char *fmt, ...)
{
	int retval = 0;
	log_level current_log_level = get_log_level();
	const char *prefix = "[ERROR] ";

	if (current_log_level >= LOG_LEVEL_ERROR) {
		va_list args;
		va_start(args, fmt);
		retval = log_message_with_prefix(LOG_LEVEL_ERROR, prefix, fmt, args);
		va_end(args);
	}
	return retval;
}

int log_fatal(const char *fmt, ...)
{
	int retval = 0;
	log_level current_log_level = get_log_level();
	const char *prefix = "[FATAL] ";

	if (current_log_level >= LOG_LEVEL_FATAL) {
		va_list args;
		va_start(args, fmt);
		retval = log_message_with_prefix(LOG_LEVEL_FATAL, prefix, fmt, args);
		va_end(args);
	}
	return retval;
}

int log_info(const char *fmt, ...)
{
	int retval = 0;
	log_level current_log_level = get_log_level();
	const char *prefix = "[INFO] ";

	if (current_log_level >= LOG_LEVEL_INFO) {
		va_list args;
		va_start(args, fmt);
		retval = log_message_with_prefix(LOG_LEVEL_INFO, prefix, fmt, args);
		va_end(args);
	}
	return retval;
}

int log_trace(const char *fmt, ...)
{
	int retval = 0;
	log_level current_log_level = get_log_level();
	const char *prefix = "[TRACE] ";

	if (current_log_level >= LOG_LEVEL_TRACE) {
		va_list args;
		va_start(args, fmt);
		retval = log_message_with_prefix(LOG_LEVEL_TRACE, prefix, fmt, args);
		va_end(args);
	}
	return retval;
}

