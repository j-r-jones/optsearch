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

#include "config.h"

/* yaml_char_t is a typedef of unsigned char.  This is a problem if the
 * platform defaults to using signed chars.  This will probably bite us
 * later when we are trying to read the details of individual flags. */
const yaml_char_t CLEAN_SCRIPT[] = "clean-script";
const yaml_char_t BUILD_SCRIPT[] = "build-script";
const yaml_char_t TEST_SCRIPT[] = "accuracy-test";
const yaml_char_t BENCHMARK[] = "performance-test";
const yaml_char_t SIGNAL[] = "quit-signal";

const yaml_char_t COMPILER[] = "compiler";
const yaml_char_t COMPILER_NAME[] = "name";
const yaml_char_t COMPILER_VERSION[] = "version";

const yaml_char_t FLAGS_SECTION[] = "flags";

const yaml_char_t FLAG_NAME[] = "name";
const yaml_char_t FLAG_TYPE[] = "type";
const yaml_char_t FLAG_DESCRIPTION[] = "description";	/* These are just for readability of the YAML */

const yaml_char_t DEFAULT[] = "default";	/* Default value of the flag */
const yaml_char_t DEPENDS_ON[] = "depends-on";	/* Flags that this one depends on or is affected by */
const yaml_char_t DEPENDED_ON_BY[] = "depended-on-by";	/* Flags that depend on or are affected by this flag */

const yaml_char_t ONOFF_FLAG_TYPE[] = "onoff";
const yaml_char_t ON_PREFIX[] = "on-prefix";
const yaml_char_t OFF_PREFIX[] = "off-prefix";

const yaml_char_t LIST_FLAG_TYPE[] = "list";
const yaml_char_t VALUES[] = "values";	/* For list flags */

const yaml_char_t PREFIX[] = "prefix";
const yaml_char_t SEPARATOR[] = "separator";	/* Default separator is a space */

const yaml_char_t RANGE_FLAG_TYPE[] = "range";
const yaml_char_t MIN[] = "min";
const yaml_char_t MAX[] = "max";

static int flag_uid = 0;

/**
 * Copies a yaml_char_t * and returns a pointer to the copy.
 *
 * Anything created via this function will need to be freed later.  Memory
 * allocation is done via malloc.  Note that yaml_char_t is a typedef of
 * unsigned char, and on some platforms char defaults to signed.  To avoid
 * problems due to signedness of char, the string functions are not used here.
 * 
 * @param from the pointer to the value to be copied.
 * @return a copy of the supplied yaml_char_t *
 */
yaml_char_t *copy_yaml_value(const yaml_char_t * from)
{
	size_t size = 0;
	if (from == NULL) {
		log_error("Cannot copy a NULL value.");
		return NULL;
	}
	size = sizeof(*from);
	yaml_char_t *to = (yaml_char_t *) malloc(size);
	memcpy(to, from, size);
	return to;
}

/**
 * Compare two yaml_char_t[].
 *
 * Uses memcmp to compare the two arrays, after first comparing their sizes.
 *
 * @return true if the keys match, false otherwise
 */
bool compare_keys(const yaml_char_t * first, const yaml_char_t * second)
{
	size_t size_first = sizeof(*first);
	size_t size_second = sizeof(*second);

	if (size_first == size_second) {
		return (memcmp(first, second, size_first) == 0);
	}
	return false;
}

int get_next_token(yaml_parser_t * parser, yaml_token_t * token)
{
	if (!yaml_parser_scan(parser, token)) {
		/* TODO handle ERROR more usefully.
		 * Unfortunately all we get is a 0 on error, but the parser should
		 * have an error. */
		log_error("Parser error: %s", parser->problem);
		/* This message tends not to be very helpful */
		/* log_error("Error encountered approximately at byte %d, \n\"%s\"\n", parser->problem_offset, parser->context); */
		return 1;
	}
	return 0;
}

/* This is really only for debugging */
size_t print_token(unsigned char *str, size_t length, FILE * stream)
{
	return fwrite(str, 1, length, stream);
}

opt_flag_t *new_list_flag(const char *name, const char *prefix,
			  const char *separator, const char **values,
			  int num_values)
{
	opt_flag_t *flag = (opt_flag_t *) malloc(sizeof(*flag));
	flag->uid = flag_uid++;
	/* TODO This may need a bit more thought */
	flag->name = name;
	flag->type = OPT_LIST_FLAG;
	flag->prefix = prefix;
	flag->data.list.separator = separator;
	flag->data.list.values = values;
	flag->data.list.size = num_values;
	flag->data.list.value = 0;
	return flag;
}

opt_flag_t *new_onoff_flag(const char *name, const char *prefix,
			   const char *neg_prefix)
{
	opt_flag_t *flag = (opt_flag_t *) malloc(sizeof(*flag));
	flag->uid = flag_uid++;
	flag->name = name;	/* TODO This probably ought to be a strcpy or memcpy or similar */
	flag->type = OPT_ONOFF_FLAG;
	flag->prefix = prefix;
	flag->data.onoff.neg_prefix = neg_prefix;
	return flag;
}

opt_flag_t *new_range_flag(const char *name, const char *prefix,
			   const char *separator, const int max, const int min,
			   const int def)
{
	opt_flag_t *flag = (opt_flag_t *) malloc(sizeof(*flag));
	flag->uid = flag_uid++;
	flag->name = name;
	flag->type = OPT_RANGE_FLAG;
	flag->prefix = prefix;
	flag->data.range.separator = separator;
	if (max <= min) {
		/* TODO We need to run the compiler to get useful values, but that
		 * is probably best done in a totally different script that
		 * generates the YAML config for us
		 */
		if (def > min) {
			/* This is a bit arbitrary.  Who's to say what it should be? */
			flag->data.range.max = 2 * def;
		} else {
			/* This is entirely arbitrary for now */
			flag->data.range.max = 100;
		}
	} else {
		flag->data.range.max = max;
	}
	flag->data.range.min = min;
	flag->data.range.value = min;
	return flag;
}

/*
 * The next bit was originally given as an example by Alaric Snell-Pym, when I
 * was struggling with libyaml.  It has been modified from that original
 * example, but not greatly.
 * It is based on reference code at http://pyyaml.org/wiki/LibYAML
 */
enum parser_state_t {
	S_INITIAL = 'i',
	S_TOP_LEVEL_MAP = 't',
	S_COMPILER_MAP = 'c',
	S_COMPILER_FLAGS_SEQ = 'f',
	S_COMPILER_FLAG_MAP = 'F',
	S_COMPILER_FLAG_MAP_SEQ = 'S',
};

enum option_attr_t {
	OA_NONE = '0',
	OA_VALUES = 'v',
	OA_DEPENDS_ON = 'd',
	OA_DEPENDED_ON_BY = 'D',
};

int is_map(enum parser_state_t state)
{
	return state == S_TOP_LEVEL_MAP ||
	    state == S_COMPILER_MAP || state == S_COMPILER_FLAG_MAP;
}

/*
 * TODO Use the string constants instead of hard-coding values
 * TODO Error handling and input validation -- there is next to none!
 */
int read_config(const char *yaml_file, opt_config_t * config)
{
	yaml_parser_t parser;
	yaml_event_t event;
	enum parser_state_t state = S_INITIAL;

	// Temporary state variables
	char *map_key = NULL;
	char *scalar_value = NULL;
	opt_flag_uid uid_counter = 0;
	opt_flag_t *flag_buffer = NULL;
	int flag_count = 0;
	enum option_attr_t oa = OA_NONE;
	FILE *input = NULL;

	int done = 0;
	/* Some sensible defaults */
    config->epsilon = 5.0;
    config->timeout = 120;
    config->benchmark_timeout = 120;
    config->benchmark_repeats = 20;
	config->num_flags = 0;
	config->compiler_flags = NULL;

	/* Create the Parser object. */
	yaml_parser_initialize(&parser);

	/* Set a file input. */
	input = fopen(yaml_file, "rb");	/* TODO Check this worked */

	yaml_parser_set_input_file(&parser, input);

	/* Read the event sequence. */
	while (!done) {
		/* Get the next event. */
		if (!yaml_parser_parse(&parser, &event))
			goto error;

		//log_trace("config.c: State = %c, map_key = %s", state, map_key);
		switch (event.type) {
		case YAML_NO_EVENT:
			break;

		case YAML_STREAM_START_EVENT:
			break;
		case YAML_STREAM_END_EVENT:
			break;

		case YAML_DOCUMENT_START_EVENT:
			break;
		case YAML_DOCUMENT_END_EVENT:
			break;

		case YAML_ALIAS_EVENT:
			log_error("Aliases aren't supported: %s",
				  event.data.alias.anchor);
			break;

		case YAML_SCALAR_EVENT:
			scalar_value = (char *) event.data.scalar.value;
			log_debug("config.c: Got scalar: %s", scalar_value);
			if (is_map(state)) {
				if (map_key == NULL) {
					map_key = strdup(scalar_value);
				} else {
					switch (state) {
					case S_TOP_LEVEL_MAP:
						if (!strcmp
						    (map_key, "quit-signal")) {
							config->quit_signal = strdup(scalar_value);
							log_debug("Got quit signal: %s", config->quit_signal);
						} else
						    if (!strcmp
							(map_key,
							 "clean-script")) {
							config->clean_script =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key,
							 "build-script")) {
							config->build_script =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key,
							 "accuracy-test")) {
							config->accuracy_test =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key,
							 "performance-test")) {
							config->perf_test =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key, "epsilon")) {
							config->epsilon = atof(scalar_value);	/* TODO Check validity */
						} else if (!strcmp (map_key, "timeout")) {
							config->timeout = atof(scalar_value);	/* TODO Check validity */
						} else if (!strcmp (map_key, "benchmark-timeout")) {
							config->benchmark_timeout = atof(scalar_value);	/* TODO Check validity */
						} else if (!strcmp (map_key, "benchmark-repeats")) {
							config->benchmark_repeats = atoi(scalar_value);	/* TODO Check validity */
						} else {
							log_error
							    ("Encountered invalid map key in top-level of config: %s='%s'",
							     map_key,
							     scalar_value);
						}
						break;
					case S_COMPILER_MAP:
						if (!strcmp(map_key, "name")) {
							config->compiler =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key, "version")) {
							config->
							    compiler_version =
							    strdup
							    (scalar_value);
						} else {
							log_error
							    ("Encountered invalid map key in compiler section of config: %s='%s'",
							     map_key,
							     scalar_value);
						}
						break;
					case S_COMPILER_FLAG_MAP:
						if (!strcmp(map_key, "name")) {
							flag_buffer->name =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key, "type")) {
							if (!strcmp
							    (scalar_value,
							     "on-off")) {
								flag_buffer->
								    type =
								    OPT_ONOFF_FLAG;
								flag_buffer->
								    data.onoff.
								    set = true;
								config->
								    num_flags++;
							} else
							    if (!strcmp
								(scalar_value,
								 "range")) {
								flag_buffer->
								    type =
								    OPT_RANGE_FLAG;
								flag_buffer->
								    data.range.
								    min = 0;
								flag_buffer->
								    data.range.
								    max = 0;
								flag_buffer->
								    data.range.
								    value = 0;
								config->
								    num_flags++;
							} else
							    if (!strcmp
								(scalar_value,
								 "list")) {
								flag_buffer->
								    type =
								    OPT_LIST_FLAG;
								flag_buffer->
								    data.list.
								    size = 0;
								flag_buffer->
								    data.list.
								    value = 0;
								flag_buffer->
								    data.list.
								    values =
								    NULL;
								config->
								    num_flags++;
							} else {
								log_error
								    ("Unknown flag type '%s'\n",
								     scalar_value);
							}
						} else
						    if (!strcmp
							(map_key,
							 "on-prefix")) {
							flag_buffer->prefix =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key,
							 "off-prefix")) {
							// FIXME: Check is an onoff
							flag_buffer->data.onoff.
							    neg_prefix =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key, "prefix")) {
							flag_buffer->prefix =
							    strdup
							    (scalar_value);
						} else
						    if (!strcmp
							(map_key,
							 "separator")) {
							if (flag_buffer->type ==
							    OPT_LIST_FLAG) {
								flag_buffer->
								    data.list.
								    separator =
								    strdup
								    (scalar_value);
							} else if (flag_buffer->
								   type ==
								   OPT_RANGE_FLAG)
							{
								flag_buffer->
								    data.range.
								    separator =
								    strdup
								    (scalar_value);
							} else {
								// FIXME: Else error
								log_error
								    ("Encountered error while processing flags: %s:'%s'",
								     map_key,
								     scalar_value);
							}
						} else
						    if (!strcmp(map_key, "max"))
						{
							// FIXME: Check is a range
							// TODO FIXME: Check is sensible
							flag_buffer->data.range.
							    max =
							    atoi(scalar_value);
						} else
						    if (!strcmp(map_key, "min"))
						{
							// FIXME: Check is a range
							// TODO FIXME: Check is sensible
							flag_buffer->data.range.
							    min =
							    atoi(scalar_value);
						} else {
							log_error
							    ("Encountered invalid compiler flag data: %s:'%s'\n",
							     map_key,
							     scalar_value);
						}
						break;
					}

					// We are a sequence within a map, so clean up the outer
					// map's key that named us.
					free(map_key);
					map_key = NULL;
				}
			} else {
				// Just a scalar in a sequence
				switch (state) {
				case S_COMPILER_FLAG_MAP_SEQ:
					switch (oa) {
					case OA_VALUES:
						flag_buffer->data.list.size++;
						flag_buffer->data.list.values =
						    realloc(flag_buffer->data.
							    list.values,
							    sizeof(char *) *
							    flag_buffer->data.
							    list.size);
						flag_buffer->data.list.
						    values[flag_buffer->data.
							   list.size - 1] =
						    strdup(scalar_value);
						break;
					case OA_DEPENDS_ON:
						log_warn
						    ("Encountered currently unhandled option: '%s' depends on '%s'",
						     flag_buffer->name,
						     scalar_value);
						break;
					case OA_DEPENDED_ON_BY:
						log_warn
						    ("Encountered currently unhandled option: '%s' depends on '%s'",
						     scalar_value,
						     flag_buffer->name);
						break;
					}
					break;
				default:
					log_error("Scalar value in state %c",
						  state);
					break;
				}
			}
			break;

		case YAML_SEQUENCE_START_EVENT:
			log_debug("config.c: Sequence start");
			switch (state) {
			case S_COMPILER_MAP:
				if (!strcmp(map_key, "flags")) {
					state = S_COMPILER_FLAGS_SEQ;
					break;
				} else {
					log_error
					    ("Unknown compiler sequence %s",
					     map_key);
					break;
				}
				break;
			case S_COMPILER_FLAG_MAP:
				// Sequence in flag attribute
				if (!strcmp(map_key, "values")) {
					oa = OA_VALUES;
				} else if (!strcmp(map_key, "depends-on")) {
					oa = OA_DEPENDS_ON;
				} else if (!strcmp(map_key, "depended-on-by")) {
					oa = OA_DEPENDED_ON_BY;
				} else {
					// FIXME: Barf with an error
				}
				state = S_COMPILER_FLAG_MAP_SEQ;
				break;
			default:
				log_error("Sequence in state %c", state);
				break;
			}
			if (map_key) {
				// We are a sequence within a map, so clean up the outer
				// map's key that named us.
				free(map_key);
				map_key = NULL;
			}
			break;
		case YAML_SEQUENCE_END_EVENT:
			log_debug("config.c: Sequence end");
			switch (state) {
			case S_COMPILER_FLAGS_SEQ:
				state = S_COMPILER_MAP;
				break;
			case S_COMPILER_FLAG_MAP_SEQ:
				oa = OA_NONE;
				state = S_COMPILER_FLAG_MAP;
				break;
			default:
				log_error("Sequence end in state %c", state);
				break;
			}
			break;

		case YAML_MAPPING_START_EVENT:
			log_debug("config.c: Mapping start");
			switch (state) {
			case S_INITIAL:
				state = S_TOP_LEVEL_MAP;
				break;
			case S_TOP_LEVEL_MAP:
				if (!strcmp(map_key, "compiler")) {
					state = S_COMPILER_MAP;
				} else {
					log_error("Unknown top-level map %s",
						  map_key);
					break;
				}
				break;
			case S_COMPILER_FLAGS_SEQ:
				state = S_COMPILER_FLAG_MAP;
				flag_count++;
				config->compiler_flags =
				    realloc(config->compiler_flags,
					    flag_count * sizeof(opt_flag_t *));
				// Allocate a new flag
				flag_buffer = calloc(1, sizeof(opt_flag_t));
				config->compiler_flags[flag_count - 1] =
				    flag_buffer;
				// FIXME: Initialise flag_buffer with defaults
				flag_buffer->uid = uid_counter++;
				break;
			default:
				log_error("Map in state %c", state);
				break;
			}
			if (map_key) {
				// We are a map within a map, so clean up the outer
				// map's key that named us.
				free(map_key);
				map_key = NULL;
			}
			break;
		case YAML_MAPPING_END_EVENT:
			log_debug("config.c: Mapping end");
			switch (state) {
			case S_TOP_LEVEL_MAP:
				state = S_INITIAL;
				break;
			case S_COMPILER_MAP:
				state = S_TOP_LEVEL_MAP;

				// Put a final NULL in the compiler_flags array
				flag_count++;
				config->compiler_flags =
				    realloc(config->compiler_flags,
					    flag_count * sizeof(opt_flag_t *));
				config->compiler_flags[flag_count - 1] = NULL;
				break;
			case S_COMPILER_FLAG_MAP:
				state = S_COMPILER_FLAGS_SEQ;
				flag_buffer = NULL;
				break;
			default:
				log_error("Mapping end in state %c", state);
				break;
			}
			break;

		default:
			log_error("Unknown event");
			break;
		}

		log_debug("config.c: State <- %c", state);

		/* Are we finished? */
		done = (event.type == YAML_STREAM_END_EVENT);

		/* The application is responsible for destroying the event object. */
		yaml_event_delete(&event);
	}

	/* Destroy the Parser object. */
	yaml_parser_delete(&parser);
	fclose(input);

	return 1;

 error:
	/* Destroy the Parser object. */
	yaml_parser_delete(&parser);
	fclose(input);

	return 0;
}

opt_config_t * opt_new_config(void)
{
	opt_config_t * config = malloc(sizeof(*config));
	config->num_flags = 0;
	config->compiler_flags = NULL;
	config->compiler = NULL;
	config->compiler_version = NULL;
	config->timeout = 0;
	config->quit_signal = NULL;
	config->clean_script = NULL;
	config->build_script = NULL;
	config->accuracy_test = NULL;
	config->benchmark_timeout = 0;
	config->benchmark_repeats = 0;
	config->epsilon = 0.0;
	config->perf_test = NULL;
	return config;
}

int opt_bcast_string(int root, char * string)
{
	int rc, my_rank, len = 0;

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	if (root == my_rank) {
		if (string == NULL) {
			log_fatal("Cannot send NULL string to workers");
			MPI_Abort(MPI_COMM_WORLD, -1);
		}
		len = strlen(string);
	}
	MPI_Bcast(&len, 1, MPI_INT, root, MPI_COMM_WORLD);
	len = len;
	log_debug("len is %d", len);
	if (MASTER != my_rank) {
		string = realloc(string, (len+1) * sizeof(char));
		string[len] = '\0';
	}
	rc = MPI_Bcast(string, len, MPI_CHAR, root, MPI_COMM_WORLD);
	log_debug("String post-broadcast: %s", string);
	return rc;
}

int opt_share_config(int root, opt_config_t * config)
{
	int my_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	if (config == NULL) {
		log_fatal("Cannot share (or populate) a NULL config");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	/* MPI_Bcast each part of the config in turn, except for the flags
	 * and the compiler name/version, which the workers do not need.  Lots of
	 * small broadcasts like this is inefficient, but we only do it once per
	 * run, so it should not matter.
	 */

	MPI_Bcast(&(config->timeout), 1, MPI_INT, root, MPI_COMM_WORLD);
	if (root != my_rank)
		config->quit_signal = strdup("");
	opt_bcast_string(root, config->quit_signal);
	log_debug("Got quit signal: %s", config->quit_signal);
	if (root != my_rank)
		config->clean_script = strdup("");
	opt_bcast_string(root, config->clean_script);
	log_debug("Got clean script: %s", config->clean_script);
	if (root != my_rank)
		config->build_script = strdup("");
	opt_bcast_string(root, config->build_script);
	log_debug("Got build script: %s", config->build_script);
	if (root != my_rank)
		config->accuracy_test = strdup("");
	opt_bcast_string(root, config->accuracy_test);
	log_debug("Got test script: %s", config->accuracy_test);

	MPI_Bcast(&(config->benchmark_timeout), 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&(config->benchmark_repeats), 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&(config->epsilon), 1, MPI_DOUBLE, root, MPI_COMM_WORLD);
	if (root != my_rank)
		config->perf_test = strdup("");
	opt_bcast_string(root, config->perf_test);
	log_debug("Got benchmark script: %s", config->perf_test);

	if (root != my_rank)
		config->compiler = strdup("");
	opt_bcast_string(root, config->compiler);
	if (root != my_rank)
		config->compiler_version = strdup("");
	opt_bcast_string(root, config->compiler_version);
	/* TODO Some more useful diagnostics would be nice here */
	return 0;
}

void opt_clean_config(opt_config_t * config)
{
	int i;
	opt_flag_t *flag = NULL;
	if (config->compiler_flags != NULL) {
		for (i = 0; i < config->num_flags; i++) {
			flag = config->compiler_flags[i];
			opt_destroy_flag(flag);
		}
		config->compiler_flags = NULL;
		config->num_flags = 0;
	}
	if (config->compiler != NULL) {
		free(config->compiler);
		config->compiler = NULL;
	}
	if (config->compiler_version != NULL) {
		free(config->compiler_version);
		config->compiler_version = NULL;
	}
	if (config->clean_script != NULL) {
		free(config->clean_script);
		config->clean_script = NULL;
	}
	if (config->build_script != NULL) {
		free(config->build_script);
		config->build_script = NULL;
	}
	if (config->quit_signal != NULL) {
		free(config->quit_signal);
		config->quit_signal = NULL;
	}
	if (config->accuracy_test != NULL) {
		free(config->accuracy_test);
		config->accuracy_test = NULL;
	}
	if (config->perf_test != NULL) {
		free(config->perf_test);
		config->perf_test = NULL;
	}
}

void opt_destroy_config(opt_config_t * config)
{
	if (config != NULL) {
		opt_clean_config(config);
		free(config);
		config = NULL;
	}
}

void opt_destroy_flag(opt_flag_t * flag)
{
	int i;

	/* Most of these things are const char *, so do not need to be freed */

	/*
	switch (flag->type) {
	case OPT_RANGE_FLAG:
		free(flag->data.range.separator);
		flag->data.range.separator = NULL;
		break;
	case OPT_LIST_FLAG:
		free(flag->data.list.separator);
		flag->data.list.separator = NULL;
		for (i = 0; i < flag->data.list.size; i++) {
			free(flag->data.list.values[i]);
			flag->data.list.values[i] = NULL;
		}
		free(flag->data.list.values);
		break;
	case OPT_ONOFF_FLAG:
		free(flag->data.onoff.neg_prefix);
		flag->data.onoff.neg_prefix = NULL;
		break;
	default:
		log_error("Unrecognised flag type");
		break;
	}

	free(flag->prefix);
	flag->prefix = NULL;
	free(flag->name);
	flag->name = NULL; */
	free(flag);
}
