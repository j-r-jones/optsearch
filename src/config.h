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
/**
 * This module provides functions for the reading in of the various files,
 * and initialising of related data structures.
 *
 * There are three config files to read in:
 * One for the various options we need to set.
 *  - This also may give the paths to the other two files
 * One for the params
 * One for the flags
 */
#ifndef H_OPTSEARCH_CONFIG_
#define H_OPTSEARCH_CONFIG_

#include "common.h"
#include <string.h>
#include <assert.h>
#include <yaml.h>

/*
 * TODO Handle flag dependencies/influences; we need to be able to collect
 * together small sets of flags with one that they all depend on being set.
 * Often, this is a bool flag that is required to be 'on' to make the
 * others have some effect.  This is not always the case though.
 *
 * The id integer is there to make the flags easier to compare and say "is
 * this the same one?"
 */

/** Compiler flag types */
typedef enum opt_flag_type_e {
	OPT_NO_FLAG,
	OPT_RANGE_FLAG,
	OPT_LIST_FLAG,
	OPT_ONOFF_FLAG
} opt_flag_type_t;

typedef int opt_flag_uid;

/** The compiler flag structure */
typedef struct {

    /** The flag type */
	opt_flag_type_t type;

	opt_flag_uid uid;

	const char *name;	/* The string passed to the compiler, eg the unroll-loops part of -funroll-loops */
	const char *prefix;	/* eg "--param " (note the inclusion of the space) or "-f" */

	int num_dependencies;
	opt_flag_uid *dependencies;	/* array of IDs of other flags that depend on this one being set */

    /** The flag data
     *
     * Using a union here means that more types can be added later as they are
     * found and hopefully fewer changes will be necessary than with 3
     * distinct types.
     */
	union {

	/** The list parameters (for @c OPT_LIST_FLAG). */
		struct {
			/* range flags have a name, default and max and min values */
			const char *separator;	/* usually either a space or a '=' char */
			int max;
			int min;
			int value;	/* current value */
		} range;

	/** The list parameters (for @c OPT_RANGE_FLAG). */
		struct {
			/* list flags have a name, default and set of values that they
			 * can take.  These are strings */
			const char *separator;	/* usually either a space or a '=' char */
			const char **values;	/* These could be numeric, but we can treat them like strings. They may be strings like 'on', 'off', 'fast' */
			int value;	/* current value as an index into the array that represents the list of values */
			int size;
		} list;

	/** The list parameters (for @c OPT_ONOFF_FLAG). */
		struct {
			/* onoff flags are either on or off */
			const char *neg_prefix;	/* eg the "-fno-" in -fno-unroll-loops */
			bool set;	/* Is it set or unset.  IE do we use the prefix or the neg_prefix? */
		} onoff;

	} data;

} opt_flag_t;

/**
 * The config structure.
 * Contains flag lists as well as other configuration
 * settings.  Eg build script, the two test scripts, the clean script, as
 * well as which compiler we are using.
 */
typedef struct {
	/** Details about the compiler itself.  At present, the compiler version
	 * is not used. */
	char *compiler;
	char *compiler_version;
	int num_flags;
	opt_flag_t **compiler_flags;

	/** Timeout and commands to run for each stage of the build. */
    int timeout;
	char *quit_signal;
	char *clean_script;
	char *build_script;
	char *accuracy_test; /** This test is important to ensure that the optimisation has not caused a numeric instability or other problem. */

    int benchmark_timeout; /** A separate timeout for each run of the benchmark */
    int benchmark_repeats; /** Max number of times to repeat benchmark runs */
	double epsilon; /** Experimental error */
	char *perf_test; /** The benchmark itself */
} opt_config_t;

/**
 * Parse the YAML file supplied and return a populated opt_config_t.
 *
 * @param   yaml_file The file to read in, used to populate the config
 * @param   config    An empty config object
 */
int read_config(const char *yaml_file, opt_config_t * config);

size_t print_node(unsigned char *str, size_t length, FILE * stream);

opt_flag_t *new_list_flag(const char *name, const char *prefix,
			  const char *separator, const char **values,
			  int num_values);

opt_flag_t *new_onoff_flag(const char *name, const char *prefix,
			   const char *neg_prefix);

opt_flag_t *new_range_flag(const char *name, const char *prefix,
			   const char *separator, const int max, const int min,
			   const int def);

void opt_destroy_flag(opt_flag_t * flag);

opt_config_t * opt_new_config(void);

/**
 * Free all the flags, etc malloc'd when read_config was called in config.c,
 * but do not call free on the config itself.  The caller has to do that,
 * since frequently it is not a malloc'd thing.
 */
void opt_clean_config(opt_config_t * config);

void opt_destroy_config(opt_config_t * config);

int opt_bcast_string(int root, char * string);

int opt_share_config(int root, opt_config_t * config);

#endif				/* include guard H_OPTSEARCH_CONFIG_ */
