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

#include "data.h"

/* 
 * SQLite data storage and retrieval functions.  We will also need to
 * initialise the database by opening the file and creating the tables the
 * first time around.  If the file already exists, assume we are resuming a
 * previous run (use stat to check it exists).  We may want to verify that the
 * tables exist before proceeding in such cases.
 *
 * Note that only the master rank, which is also the one running SPSO, should
 * access any of these functions.  It is the duty of the caller to ensure that
 * this is so.
 */

sqlite3 *opt_db = NULL;

spso_dimension_t ** db_search_space;
int db_search_space_size;

int opt_db_get_last_insert_rowid(int *id);

/* These are just helper functions so that we can create structs easily when
 * restoring.  Probably these ought to be in spso.c, so they might get moved
 * later.
 */
spso_velocity_t *opt_db_new_velocity(int num_dims, spso_dimension_t ** dims)
{
	int i;
	spso_velocity_t *velocity =
	    (spso_velocity_t *) malloc(sizeof(*velocity));
	velocity->dimension = malloc(sizeof(*velocity->dimension) * num_dims);
	for (i = 0; i < num_dims; i++) {
		velocity->dimension[i] = dims[i]->min;
	}
	return velocity;
}

spso_position_t *opt_db_new_position(int num_dims, spso_dimension_t ** dims)
{
	int i;
	spso_position_t *position =
	    (spso_position_t *) malloc(sizeof(*position));
	position->dimension = malloc(sizeof(*position->dimension) * num_dims);
	for (i = 0; i < num_dims; i++) {
		position->dimension[i] = dims[i]->min;
	}
	return position;
}

spso_particle_t *opt_db_new_particle(int uid, int num_dims,
				     spso_dimension_t ** dims)
{
	spso_particle_t *part = malloc(sizeof(*part));
	part->uid = uid;
	part->velocity = *opt_db_new_velocity(num_dims, dims);
	part->position = *opt_db_new_position(num_dims, dims);
	part->previous_best = *opt_db_new_position(num_dims, dims);
	return part;
}

spso_swarm_t *opt_db_new_swarm(int size, int num_dims, spso_dimension_t ** dims)
{
	int i;
	spso_swarm_t *swarm = malloc(sizeof(*swarm));
	swarm->size = size;
	swarm->particles = malloc(sizeof(*(swarm->particles)) * swarm->size);
	for (i = 0; i < swarm->size; i++) {
		swarm->particles[i] = opt_db_new_particle(i, num_dims, dims);
	}
	return swarm;
}

int batch_exec(sqlite3 * db, int num_stmts, char **statements)
{
	int i, rc;
	char *errmsg = NULL;
	char *stmt;

	log_trace("Batch executing SQL statements.. ");
	for (i = 0; i < num_stmts; i++) {
		stmt = statements[i];
		/* We don't care about the results */
		log_trace("data.c: Executing statement: %s", stmt);
		rc = sqlite3_exec(db, stmt, NULL, NULL, &errmsg);
		if (rc != SQLITE_OK) {
			log_error
			    ("SQL error in batch execution of statement %s\n", errmsg);
			sqlite3_free(errmsg);
		}
	}

	return 0;
}

static int
opt_db_get_real_callback(void *vint, int argc, char **argv, char **azColName)
{
	double *i = (double *)vint;

	if (argc != 1) {
		log_warn
		    ("Got multiple (%d) results back when asking for a single real.  Returning only the first one.",
		     argc);
	}
	if (NULL == argv[0]) {
		log_error("No result returned for single real");
		*i = -1;
	} else {
		*i = atof(argv[0]);
	}
	log_debug("data.c: Got value: %.6e", *i);
	return SQLITE_OK;
}

static int
opt_db_get_int_callback(void *vint, int argc, char **argv, char **azColName)
{
	int *i = (int *)vint;

	if (argc != 1) {
		log_warn
		    ("Got multiple (%d) results back when asking for a single int.  Returning only the first one.",
		     argc);
	}
	if (NULL == argv[0]) {
		log_error("No result returned for single int");
		*i = -1;
	} else {
		*i = atoi(argv[0]);
	}
	log_debug("data.c: Got value: %d", *i);
	return SQLITE_OK;
}

static int
opt_db_get_ulonglong_callback(void *vull, int argc, char **argv, char **azColName)
{
	unsigned long long *i = (unsigned long long *)vull;

	if (argc != 1) {
		log_warn
		    ("Got multiple (%u) results back when asking for a single unsigned long long.  Returning only the first one.",
		     argc);
	}
	if (NULL == argv[0]) {
		log_error("No result returned for single unsigned long long");
		*i = -1;
	} else {
		*i = atoll(argv[0]);
	}
	log_debug("data.c: Got value: %u", *i);
	return SQLITE_OK;
}

int opt_db_get_last_insert_rowid(int *id)
{
	int rc;
	char *errmsg = NULL;
	char *stmt = "SELECT last_insert_rowid();";

	rc = sqlite3_exec(opt_db, stmt, opt_db_get_int_callback, id, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error attempting to find the last insert rowid: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		return 1;
	}
	return 0;

}

char *opt_db_get_velocity_table_create_stmt(int num_dims,
					    spso_dimension_t ** dims)
{
	int i;
	const char *stmt_front =
	    "CREATE TABLE velocity (id INTEGER NOT NULL UNIQUE";
	const char *stmt_join_start = ", '";
	const char *stmt_join_end = "' INTEGER NOT NULL";
	const char *stmt_end = ", count INTEGER NOT NULL, PRIMARY KEY (id));";
	char *stmt = NULL;
	int size = strlen(stmt_front) + strlen(stmt_end) + 1;	/* Starting point */

	for (i = 0; i < num_dims; i++) {
		log_debug("data.c: Dimension name is: %s", dims[i]->name);
		size =
		    size + strlen(dims[i]->name) + strlen(stmt_join_start) +
		    strlen(stmt_join_end);
	}
	stmt = calloc(sizeof(char), size);
	strcat(stmt, stmt_front);
	for (i = 0; i < num_dims; i++) {
		strcat(stmt, stmt_join_start);
		strcat(stmt, dims[i]->name);
		strcat(stmt, stmt_join_end);
	}
	strcat(stmt, stmt_end);
	log_debug("data.c: Created statement: %s", stmt);
	return stmt;
}

char *opt_db_get_position_table_create_stmt(int num_dims,
					    spso_dimension_t ** dims)
{
	int i;
	const char *stmt_front =
	    "CREATE TABLE position (id INTEGER NOT NULL UNIQUE, fitness REAL";
	const char *stmt_join_start = ", '";
	const char *stmt_join_end = "' INTEGER NOT NULL";
	const char *stmt_end = ", visits INTEGER, PRIMARY KEY (id));";
	char *stmt = NULL;
	int size = strlen(stmt_front) + strlen(stmt_end) + 1;	/* Starting point */

	for (i = 0; i < num_dims; i++) {
		log_debug("data.c: Dimension name is: %s", dims[i]->name);
		size =
		    size + strlen(dims[i]->name) + strlen(stmt_join_start) +
		    strlen(stmt_join_end);
	}
	stmt = calloc(sizeof(char), size);
	strcat(stmt, stmt_front);
	for (i = 0; i < num_dims; i++) {
		strcat(stmt, stmt_join_start);
		strcat(stmt, dims[i]->name);
		strcat(stmt, stmt_join_end);
	}
	strcat(stmt, stmt_end);
	log_debug("data.c: Created statement: %s", stmt);
	return stmt;
}

int opt_db_create_schema(int num_dims, spso_dimension_t ** dims)
{
	int num_stmts = 25;
	int rc;
	char *position_table_stmt =
	    opt_db_get_position_table_create_stmt(num_dims, dims);
	char *velocity_table_stmt =
	    opt_db_get_velocity_table_create_stmt(num_dims, dims);
	char *statements[] = {
		"CREATE TABLE flag (id INTEGER NOT NULL UNIQUE, name TEXT NOT NULL, type INTEGER NOT NULL, PRIMARY KEY (id));",
		"CREATE TABLE dimension (id INTEGER NOT NULL UNIQUE, name TEXT NOT NULL, min INTEGER NOT NULL, max INTEGER NOT NULL, PRIMARY KEY (id), FOREIGN KEY (name) REFERENCES flag(name));",
		position_table_stmt,
		velocity_table_stmt,
		"CREATE TABLE particle (id INTEGER NOT NULL UNIQUE, positionID INTEGER NOT NULL, velocityID INTEGER NOT NULL, bestPositionID INTEGER, FOREIGN KEY (positionID) REFERENCES position(id), FOREIGN KEY (velocityID) REFERENCES velocity(id), FOREIGN KEY (bestPositionID) references position(id),  PRIMARY KEY (id));",
		"CREATE TABLE particle_history (timestamp DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, particleID INTEGER NOT NULL, positionID INTEGER NOT NULL, velocityID INTEGER NOT NULL, bestPositionID INTEGER NOT NULL, FOREIGN KEY (particleID) REFERENCES particle(id), FOREIGN KEY (positionID) REFERENCES position(id), FOREIGN KEY (velocityID) REFERENCES velocity(id), FOREIGN KEY (bestPositionID) references position(id));",
		"CREATE TABLE global_best_history (timestamp DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL, positionID INTEGER NOT NULL, FOREIGN KEY (positionID) REFERENCES position(id));",
		/* Singletons */
		"CREATE TABLE singleton (what TEXT NOT NULL, value INTEGER);",
		"INSERT INTO singleton VALUES('PRNG_SEED', NULL);",
		"INSERT INTO singleton VALUES('CONVERGED', 0);",
		"INSERT INTO singleton VALUES('BEST_POS', NULL);",
		"INSERT INTO singleton VALUES('NO_MOVEMENT_COUNTER', 0);",
		"CREATE TABLE real_singleton (what TEXT NOT NULL, value REAL);",
		"INSERT INTO real_singleton VALUES('PREV_PREV_BEST', NULL);",
		"INSERT INTO real_singleton VALUES('PREV_BEST', NULL);",
		"INSERT INTO real_singleton VALUES('CURR_BEST', NULL);",
		/* Foreign-key indices */
		"CREATE INDEX 'dimension_name' ON 'dimension'('name');",
		"CREATE INDEX 'global_best_history_positionID' ON 'global_best_history'('positionID');",
		"CREATE INDEX 'particle_bestPositionID' ON 'particle'('bestPositionID');",
		"CREATE INDEX 'particle_velocityID' ON 'particle'('velocityID');",
		"CREATE INDEX 'particle_positionID' ON 'particle'('positionID');",
		"CREATE INDEX 'particle_history_positionID' ON 'particle_history'('positionID');",
		"CREATE INDEX 'particle_history_particleID' ON 'particle_history'('particleID');",
		"CREATE INDEX 'particle_history_velocityID' ON 'particle_history'('velocityID');",
		"CREATE INDEX 'particle_history_bestPositionID' ON 'particle_history'('bestPositionID');"
	};
	if (opt_db == NULL) {
		log_error("Cannot create scheme with a NULL database");
		return 1;
	}
	rc = batch_exec(opt_db, num_stmts, statements);
	free(position_table_stmt);
	free(velocity_table_stmt);
	return rc;
}

int
opt_db_verify_schema(char *db_name, opt_config_t * config,
		     int db_search_space_size, spso_dimension_t ** db_search_space)
{
	/* TODO
	 * For now, I think we'll just tell people not to be stupid and try to run
	 * with something that is not our SQLite DB for this run.  Ideally, we
	 * would check the schema and that all the stored dimensions match the
	 * db_search_space.
	 */
	return 0;
}

int
opt_db_store_searchspace(int db_search_space_size,
			 spso_dimension_t ** db_search_space)
{
	int i, rc = 0;
	for (i = 0; i < db_search_space_size; i++) {
		rc = opt_db_store_dimension(db_search_space[i]);
	}
	/* This is not such a useful return value */
	return rc;
}

int
opt_db_init(char *db_name, opt_config_t * config, int num_dims,
	    spso_dimension_t ** dimensions)
{
	int rc, ret;
	struct stat filestat;
	char * errmsg;
	const char * stmt = "PRAGMA main.journal_mode=WAL; PRAGMA wal_autocheckpoint=3; PRAGMA synchronous=FULL;";

	log_trace("data.c: Entered opt_db_init in data.c");

	db_search_space = dimensions;
	db_search_space_size = num_dims;

	if (file_exists(db_name)) {
		log_debug("data.c: Opening database file found at %s", db_name);
		stat(db_name, &filestat);
		if ((filestat.st_mode & S_IFMT) == S_IFREG) {
			rc = sqlite3_open(db_name, &opt_db);
			if (rc) {
				log_fatal("Could not open database: %s",
					  sqlite3_errmsg(opt_db));
				sqlite3_close(opt_db);
				opt_db = NULL;
				return OPT_DB_ERROR;
			}
			/* TODO check rc */
			rc = opt_db_verify_schema(db_name, config,
						  db_search_space_size,
						  db_search_space);
			log_debug
			    ("data.c: Found database with expected file name (%s).  Assuming we are resuming from a previous run.",
			     db_name);
			ret = OPT_DB_RESUME;
		} else {
			log_fatal
			    ("File with the same name as our database exists, but is not a regular file.");
			sqlite3_close(opt_db);
			opt_db = NULL;
			return OPT_DB_ERROR;
		}
	} else {
		rc = sqlite3_open(db_name, &opt_db);
		if (rc) {
			log_fatal("Could not open database: %s",
				  sqlite3_errmsg(opt_db));
			sqlite3_close(opt_db);
			opt_db = NULL;
			return OPT_DB_ERROR;
		}
		rc = opt_db_create_schema(db_search_space_size, db_search_space);
		if (rc) {
			log_fatal("Could not create database schema: %s",
				  sqlite3_errmsg(opt_db));
			sqlite3_close(opt_db);
			opt_db = NULL;
			return OPT_DB_ERROR;
		}
		rc = opt_db_store_searchspace(db_search_space_size, db_search_space);
		log_debug("data.c: New database initialised at %s", db_name);
		ret = OPT_DB_NEW;
	}

	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
			("SQL error in batch execution of statement %s\n", errmsg);
		sqlite3_free(errmsg);
	}

	return ret;
}

static int opt_db_check_flag_callback(void *vflag, int argc, char **argv,
				      char **azColName)
{
	int i;
	opt_flag_t *flag = (opt_flag_t *) vflag;
	log_debug("data.c: Flag->uid: %d, flag->name: '%s', flag->type: %d",
		  flag->uid, flag->name, flag->type);
	for (i = 0; i < argc; i++) {
		log_debug("data.c: %s = %s", azColName[i],
			  argv[i] ? argv[i] : "NULL");
		if (!strcmp("id", azColName[i])) {
			if (flag->uid != atoi(argv[i])) {
				log_trace("data.c: UIDs do not match");
				return SQLITE_ERROR;
			} else {
				log_trace("data.c: UIDs match");
			}
		} else if (!strcmp("name", azColName[i])) {
			if (strcmp(flag->name, argv[i])) {
				log_trace("data.c: flag names do not match");
				return SQLITE_ERROR;
			} else {
				log_trace("data.c: flag names match");
			}
		} else if (!strcmp("type", azColName[i])) {
			if (flag->type != atoi(argv[i])) {
				return SQLITE_ERROR;
			} else {
				log_debug("data.c: flag types match");
			}
		} else {
			log_debug("data.c: Unknown column");
			return SQLITE_ERROR;
		}
	}
	return SQLITE_OK;
}

int opt_db_check_flag(int uid, opt_flag_t * flag)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_format = "SELECT * FROM flag WHERE(ID='%d');";
	char *stmt = malloc(sizeof(char) * (strlen(stmt_format) + 11));

	sprintf(stmt, stmt_format, uid);

	rc = sqlite3_exec(opt_db, stmt, opt_db_check_flag_callback, flag,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error("SQL error while looking for flag with UID %d: %s\n",
			  uid, errmsg);
		sqlite3_free(errmsg);
		return 1;
	}
	return 0;
}

int opt_db_store_flag(opt_flag_t * flag)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_format = "INSERT INTO flag (id, name, type) VALUES(%d, '%s', %d);";
	/* 10 is the size of INT_MAX when rendered as a string, the +1 is for \0 */
	char *stmt =
	    malloc(sizeof(char) *
		   (strlen(stmt_format) + strlen(flag->name) + 21));

	sprintf(stmt, stmt_format, flag->uid, flag->name, flag->type);

	/*
	 * If we care about the results, we need to provide a callback function,
	 * which receives as it's first argument the pointer that is the 4th
	 * argument to sqlite3_exec().
	 rc = sqlite3_exec(opt_db, stmt, store_flag_callback, NULL, &errmsg);
	 */
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered while trying to store flag: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		return 1;
	}
	return 0;
}

int opt_db_store_flags(opt_config_t * config)
{
	int rc = 0;
	int i;

	for (i = 0; i < config->num_flags; i++) {
		rc = opt_db_store_flag(config->compiler_flags[i]);
	}

	return rc;
}

int opt_db_store_dimension(spso_dimension_t * dim)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_format =
	    "INSERT INTO dimension (id, name, min, max) VALUES('%d', '%s', '%d', '%d');";
	/* 10 is the size of INT_MAX when rendered as a string, the +1 is for \0 */
	char *stmt =
	    malloc(sizeof(char) *
		   (strlen(stmt_format) + strlen(dim->name) +
		    (CHAR_INT_MAX * 3) + 1));

	sprintf(stmt, stmt_format, dim->uid, dim->name, dim->min, dim->max);

	/*
	 * If we care about the results, we need to provide a callback function,
	 * which receives as it's first argument the pointer that is the 4th
	 * argument to sqlite3_exec().
	 rc = sqlite3_exec(opt_db, stmt, store_flag_callback, NULL, &errmsg);
	 */
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered while trying to store dimension: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		free(stmt);
		return 1;
	}
	free(stmt);
	return 0;
}

int opt_db_find_velocity(int *vel_id, spso_velocity_t * vel)
{
	int rc;
	int i;
	char *errmsg = NULL;
	char *stmt_start = "SELECT id FROM velocity WHERE (\"%s\"=%d)";
	char *stmt_join = " AND (\"%s\"=%d)";
	char *stmt_end = ";";
	char *stmt = NULL;
	char *tmp = NULL;
	int length, size = 0;
	if (vel == NULL) {
		log_error("Cannot look for a NULL velocity.");
		return SQLITE_ERROR;
	}
	if (db_search_space[0]->name == NULL) {
		log_debug
		    ("data.c: WTF db_search_space[0]->name is NULL; using this to build SQL statements will end badly.");
		return SQLITE_ERROR;
	}
	size =
	    strlen(stmt_start) + strlen(db_search_space[0]->name) +
	    CHAR_INT_MAX + 1;
	/* Hopefully no names will be as long as 512 chars.  This is really to
	 * enforce a bit of space between tmp and stmt in memory. If tmp is not
	 * allocated before stmt, what happens is that we overflow and trample on
	 * stmt during the loop. */
	tmp = calloc(sizeof(char), strlen(stmt_join) + CHAR_INT_MAX + 1 + 512);
	stmt = calloc(sizeof(char), size);

	*vel_id = -1;

	sprintf(stmt, stmt_start, db_search_space[0]->name,
		vel->dimension[0]);
	/* TODO Check the ordering of the columns.  They should be in dim->uid
	 * numerically ascending order because of the way they are created. */
	size = (strlen(stmt) + 1) * sizeof(char);
	for (i = 1; i < db_search_space_size; i++) {
		length =
		    sizeof(char) * (strlen(stmt_join) +
				    strlen(db_search_space[i]->name) +
				    CHAR_INT_MAX + 1);
		tmp = realloc(tmp, length);
		snprintf(tmp, length, stmt_join, db_search_space[i]->name,
			 vel->dimension[i]);
		size = size + length + 1;
		stmt = realloc(stmt, size);	/* This fails, possibly due to heap corruption */
		strncat(stmt, tmp, length);
	}
	size = size + (strlen(stmt_end) * sizeof(char) + 1);
	stmt = realloc(stmt, size);
	strcat(stmt, stmt_end);
	log_debug("data.c: Statement is: '%s'", stmt);
	rc = sqlite3_exec(opt_db, stmt, opt_db_get_int_callback, vel_id,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when looking for position: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	free(tmp);
	return rc;
}

int opt_db_find_position(int *pos_id, spso_position_t * pos)
{
	int rc;
	int i;
	char *errmsg = NULL;
	char *stmt_start = "SELECT id FROM position WHERE (\"%s\"=%d)";
	char *stmt_join = " AND (\"%s\"=%d)";
	char *stmt_end = ";";
	char *stmt = NULL;
	char *tmp = NULL;
	int length, size = 0;
	if (pos == NULL || pos->dimension == NULL) {
		log_error("Cannot look for a NULL position.");
		return SQLITE_ERROR;
	}
	if (db_search_space[0]->name == NULL) {
		log_debug
		    ("data.c: WTF pos->coordinates[0].name is NULL; using this to build SQL statements will end badly.");
		return SQLITE_ERROR;
	}
	size =
	    strlen(stmt_start) + strlen(db_search_space[0]->name) +
	    CHAR_INT_MAX + 1;
	/* Hopefully no names will be as long as 512 chars.  This is really to
	 * enforce a bit of space between tmp and stmt in memory. If tmp is not
	 * allocated before stmt, what happens is that we overflow and trample on
	 * stmt during the loop. */
	tmp = calloc(sizeof(char), strlen(stmt_join) + CHAR_INT_MAX + 1 + 512);
	stmt = calloc(sizeof(char), size);

	*pos_id = -1;

	sprintf(stmt, stmt_start, db_search_space[0]->name,
		pos->dimension[0]);
	/* TODO Check the ordering of the columns.  They should be in dim->uid
	 * numerically ascending order because of the way they are created. */
	size = (strlen(stmt) + 1) * sizeof(char);
	for (i = 1; i < db_search_space_size; i++) {
		length =
		    sizeof(char) * (strlen(stmt_join) +
				    strlen(db_search_space[i]->name) +
				    CHAR_INT_MAX + 1);
		tmp = realloc(tmp, length);
		snprintf(tmp, length, stmt_join, db_search_space[i]->name,
			 pos->dimension[i]);
		size = size + length + 1;
		stmt = realloc(stmt, size);	/* This fails, possibly due to heap corruption */
		strncat(stmt, tmp, length);
	}
	size = size + (strlen(stmt_end) * sizeof(char) + 1);
	stmt = realloc(stmt, size);
	strcat(stmt, stmt_end);
	log_debug("data.c: Statement is: '%s'", stmt);
	/* SEGFAULT HERE TODO FIXME XXX */
	rc = sqlite3_exec(opt_db, stmt, opt_db_get_int_callback, pos_id,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when looking for position: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	free(tmp);
	return rc;
}

int opt_db_increment_velocity(int vel_id)
{
	int rc;
	char *stmt_fmt = "UPDATE velocity SET count=count+1 WHERE (id=%d);";
	char *errmsg, *stmt = NULL;
	int size = strlen(stmt_fmt) + CHAR_INT_MAX + 1;

	stmt = calloc(sizeof(char), size);
	sprintf(stmt, stmt_fmt, vel_id);
	log_debug("data.c: SQL to increment velocity counter is: '%s'", stmt);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered attempting to update velocity counter: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_increment_position(int pos_id)
{
	int rc;
	char *stmt_fmt = "UPDATE position SET visits=visits+1 WHERE (id=%d);";
	char *errmsg, *stmt = NULL;
	int size = strlen(stmt_fmt) + CHAR_INT_MAX + 1;

	stmt = calloc(sizeof(char), size);
	sprintf(stmt, stmt_fmt, pos_id);
	log_debug("data.c: SQL to increment position counter is: '%s'", stmt);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered attempting to update position counter: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_store_position(int *pos_id, spso_position_t * pos)
{
	int rc;
	int i;
	char *errmsg = NULL;
	char *stmt_start = "INSERT INTO position VALUES(NULL, NULL, ";
	char *stmt_end = ", 1);";
	char *stmt =
	    calloc(sizeof(char),
		   (strlen(stmt_start) + ((CHAR_INT_MAX + 2) * db_search_space_size) +
		    strlen(stmt_end) + 1));
	char *tmp = calloc(sizeof(char), CHAR_INT_MAX + 3);

	/* Check if position is already in the DB */
	rc = opt_db_find_position(pos_id, pos);
	if (rc != SQLITE_OK) {
		log_warn
		    ("An error occurred and we could not tell if the position was already in SQLite.");
		free(stmt);
		free(tmp);
		return rc;
	} else if (*pos_id >= 0) {
		log_debug
		    ("Position already exists in the DB. Incrementing counter instead.");
		/* All position IDs should be positive */
		free(stmt);
		free(tmp);
		rc = opt_db_increment_position(*pos_id);
		return rc;
	}

	strcat(stmt, stmt_start);
	/* TODO Check the ordering of the columns.  They should be in dim->uid
	 * numerically ascending order because of the way they are created. */
	for (i = 0; i < db_search_space_size; i++) {
		if (i < db_search_space_size - 1)
			sprintf(tmp, "%d, ", pos->dimension[i]);
		else
			sprintf(tmp, "%d", pos->dimension[i]);
		strcat(stmt, tmp);
	}
	strcat(stmt, stmt_end);
	log_debug("data.c: Insert statement is: '%s'", stmt);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error while trying to insert position into database: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	free(tmp);

	rc = opt_db_get_last_insert_rowid(pos_id);
	return rc;
}

int opt_db_store_velocity(int *vel_id, spso_velocity_t * vel)
{
	int rc;
	int i;
	char *errmsg = NULL;
	char *stmt_start = "INSERT INTO velocity VALUES(NULL, ";
	char *stmt_end = ", 0);";
	char *stmt =
	    calloc(sizeof(char),
		   (strlen(stmt_start) + ((CHAR_INT_MAX + 2) * db_search_space_size) +
		    strlen(stmt_end) + 1));
	char *tmp = calloc(sizeof(char), CHAR_INT_MAX + 3);

	/* Check if velocity is already in the DB */
	*vel_id = -1;
	rc = opt_db_find_velocity(vel_id, vel);
	if (rc != SQLITE_OK) {
		log_warn
		    ("An error occurred and we could not tell if the velocity was already in SQLite.");
		free(stmt);
		free(tmp);
		return rc;
	} else if (*vel_id >= 0) {
		log_debug
		    ("Velocity already exists in the DB. Incrementing counter instead.");
		/* All velocity IDs should be positive */
		free(stmt);
		free(tmp);
		rc = opt_db_increment_velocity(*vel_id);
		return rc;
	}

	strcat(stmt, stmt_start);
	/* TODO Check the ordering of the columns.  They should be in dim->uid
	 * numerically ascending order because of the way they are created. */
	for (i = 0; i < db_search_space_size; i++) {
		if (i < db_search_space_size - 1)
			sprintf(tmp, "%d, ", vel->dimension[i]);
		else
			sprintf(tmp, "%d", vel->dimension[i]);
		//log_debug("data.c: i: %d : tmp is: '%s'", i, tmp);
		strcat(stmt, tmp);
	}
	strcat(stmt, stmt_end);
	log_debug("data.c: Insert statement is: '%s'", stmt);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error when trying to insert velocity into database: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);

	rc = opt_db_get_last_insert_rowid(vel_id);
	return rc;
}

static int opt_db_get_dim_callback(void *v, int argc, char **argv,
				   char **azColName)
{
	int i;
	spso_dimension_t *dim = (spso_dimension_t *) v;
	if (dim == NULL) {
		log_error("Cannot populate null dim");
		return SQLITE_ERROR;
	}
	log_debug("data.c: Retrieving dimension from database");
	for (i = 0; i < argc; i++) {
		log_debug("data.c: %s = %s", azColName[i],
			  argv[i] ? argv[i] : "NULL");
		if (!strcmp("id", azColName[i]))
			dim->uid = atoi(argv[i]);
		else if (!strcmp("min", azColName[i]))
			dim->min = atoi(argv[i]);
		else if (!strcmp("max", azColName[i]))
			dim->max = atoi(argv[i]);
		else if (!strcmp("name", azColName[i]))
			dim->name = strdup(argv[i]);
	}
	if (dim->name == NULL) {
		log_debug
		    ("data.c: Retrieved dimension with NULL name from database");
	}
	return SQLITE_OK;
}

spso_dimension_t *opt_db_new_dimension(int id, const char *name)
{
	return spso_new_dimension(id, 0, 0, 0, name);
}

spso_dimension_t *opt_db_get_dimension(int id)
{
	int rc;
	spso_dimension_t *dim = opt_db_new_dimension(id, NULL);
	char *errmsg = NULL;
	char *stmt_format = "SELECT * FROM dimension WHERE(ID=%d);";
	char *stmt = malloc(sizeof(char) * (strlen(stmt_format) + 11));
	sprintf(stmt, stmt_format, id);
	rc = sqlite3_exec(opt_db, stmt, opt_db_get_dim_callback, dim, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when trying to retrieve dimension: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		free(dim);
		dim = NULL;
	}
	free(stmt);
	return dim;
}

static int opt_db_get_pos_callback(void *vpos, int argc, char **argv,
				   char **azColName)
{
	int i, j;
	spso_position_t *pos = (spso_position_t *) vpos;
	if (pos == NULL) {
		log_error("Cannot initialise null position");
		return SQLITE_ERROR;
	}
	for (i = 0; i < argc; i++) {
		log_debug("data.c: %s = %s", azColName[i],
			  argv[i] ? argv[i] : "NULL");
		if (!strcmp("id", azColName[i]))
			continue;
		if (!strcmp("fitness", azColName[i])) {
			//pos->fitness = atof(argv[i]);
			// There is no fitness member of the position struct
			continue;
		} else {
			for (j = 0; j < db_search_space_size; j++) {
				if (!strcmp
				    (db_search_space[j]->name, azColName[i])) {
					pos->dimension[j] = atoi(argv[i]);
					j = db_search_space_size - 1;
				}
			}
		}
	}
	return SQLITE_OK;
}

/* TODO Explicitly give column names to avoid ordering being weird, as
 * matching strings in C is a pain.  This is especially difficult for the
 * position table though, where we only know the column names at runtime.  I
 * am not sure there is a good way to do it. */
int opt_db_get_position(int pos_id, spso_position_t * pos)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_format = "SELECT * FROM position WHERE(ID=%d);";
	char *stmt =
	    malloc(sizeof(char) * (strlen(stmt_format) + CHAR_INT_MAX + 1));

	sprintf(stmt, stmt_format, pos_id);

	rc = sqlite3_exec(opt_db, stmt, opt_db_get_pos_callback, pos, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when trying to retrieve position: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		free(stmt);
		return 1;
	}
	free(stmt);
	return 0;
}

int opt_db_update_position_fitness(int id, spso_fitness_t fitness)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_format = "UPDATE position SET fitness='%lf' WHERE (id=%d);";
	char *stmt =
	    malloc(sizeof(char) *
		   (strlen(stmt_format) + CHAR_INT_MAX + CHAR_DBL_MAX + 1));

	sprintf(stmt, stmt_format, fitness, id);
	log_debug("data.c: Statement is: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when trying to update position fitness: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		return 1;
	}
	return 0;
}

int opt_db_update_particle_fitness(int id, spso_fitness_t fitness)
{
	int rc, pos_id;
	char *errmsg = NULL;
	char *stmt_format = "SELECT positionID FROM particle WHERE (id=%d);";
	char *stmt =
	    malloc(sizeof(char) * (strlen(stmt_format) + CHAR_INT_MAX + 1));
	sprintf(stmt, stmt_format, id);
	rc = sqlite3_exec(opt_db, stmt, opt_db_get_int_callback,
			  &pos_id, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when trying to update particle fitness: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
		return rc;
	}
	rc = opt_db_update_position_fitness(pos_id, fitness);
	return rc;
}

static int opt_db_get_vel_callback(void *v, int argc, char **argv,
				   char **azColName)
{
	int i, j;
	spso_velocity_t *vel = (spso_velocity_t *) v;
	for (i = 0; i < argc; i++) {
		log_debug("data.c: %s = %s", azColName[i],
			  argv[i] ? argv[i] : "NULL");
		if (!strcmp("id", azColName[i])) {
			continue;
		} else {
			for (j = 0; j < db_search_space_size; j++) {
				if (!strcmp
				    (db_search_space[j]->name, azColName[i])) {
					vel->dimension[j] = atoi(argv[i]);
					j = db_search_space_size - 1;
				}
			}
		}
	}
	return SQLITE_OK;
}

int opt_db_get_velocity(int vel_id, spso_velocity_t * vel)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_format = "SELECT * FROM velocity WHERE(ID=%d);";
	char *stmt =
	    malloc(sizeof(char) * (strlen(stmt_format) + CHAR_INT_MAX + 1));

	sprintf(stmt, stmt_format, vel_id);

	rc = sqlite3_exec(opt_db, stmt, opt_db_get_vel_callback, vel, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when trying to retrieve velocity: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

bool opt_db_check_particle_known(int id)
{
	bool ret = false;
	int rc, tmp = -1;
	char *errmsg = NULL;
	char *fmt = "SELECT id FROM particle WHERE id=%d;";
	char *query = calloc(strlen(fmt) + CHAR_INT_MAX + 1, sizeof(char));

	sprintf(query, fmt, id);
	rc = sqlite3_exec(opt_db, query, opt_db_get_int_callback, &tmp,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to check if particle %d was in database: %s\n",
		     id, errmsg);
		sqlite3_free(errmsg);
	} else if (id == tmp) {
		ret = true;
	}
	free(query);
	return ret;
}

int opt_db_store_particle(spso_particle_t * p)
{
	int rc, pos_id, vel_id, prev_id;
	char *errmsg = NULL;
	char *stmt_format = "INSERT INTO particle (id, positionID, velocityID, bestPositionID) VALUES(%d, %d, %d, %d);";
	char *stmt = NULL;

	if (p->uid < 0) {
		log_error
		    ("Refusing to store particle with negative ID as this denotes a temporary particle used during SPSO calculations.");
		return SQLITE_ERROR;
	}
	if (opt_db_check_particle_known(p->uid)) {
		log_error
		    ("Refusing to store particle that is already in the database. Call opt_db_update_particle() instead.");
		return SQLITE_ERROR;
	}

	stmt =
	    malloc(sizeof(char) *
		   (strlen(stmt_format) + (CHAR_INT_MAX * 4) + 1));

	/* TODO This should already have been stored in the table */
	rc = opt_db_store_position(&prev_id, &(p->previous_best));
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}

	rc = opt_db_update_position_fitness(prev_id, p->previous_best_fitness);
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}

	/*
	 * There is really no way to tell if another particle has already been
	 * here, so there may be duplication, but it should not matter.  Actually
	 * that's not true, it shouldn't matter, but we can check by doing a
	 * select on the position table.
	 */
	rc = opt_db_store_position(&pos_id, &(p->position));
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}

	rc = opt_db_store_velocity(&vel_id, &(p->velocity));
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}

	sprintf(stmt, stmt_format, p->uid, pos_id, vel_id, prev_id);
	log_debug("data.c: Statement is: %s", stmt);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered when trying to store particle: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);

	rc = opt_db_update_particle_history(p->uid, pos_id, vel_id, -1);
	return rc;
}

int opt_db_update_particle(spso_particle_t * p)
{
	int rc, pos_id, vel_id, prev_id;
	char *errmsg = NULL;
	char *stmt_format = "UPDATE particle SET \"%s\"=%d WHERE (id=%d);";
	int size = 0;
	char *stmt = NULL;
	if (p == NULL) {
		log_error
		    ("data.c: Cannot update database entry for NULL particle");
		return SQLITE_OK;
	}
	if (!opt_db_check_particle_known(p->uid)) {
		log_warn("This particle does not exist in the database yet.");
		return opt_db_store_particle(p);
	}

	log_trace("data.c: Updating particle %d", p->uid);

	rc = opt_db_store_position(&prev_id, &(p->previous_best));
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}
	log_debug("data.c: particle %d prev_id is now %d", p->uid, prev_id);

	rc = opt_db_update_position_fitness(prev_id, p->previous_best_fitness);
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}

	rc = opt_db_store_position(&pos_id, &(p->position));
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}
	log_debug("data.c: particle %d pos_id is now %d", p->uid, pos_id);

	rc = opt_db_store_velocity(&vel_id, &(p->velocity));
	if (rc != SQLITE_OK) {
		/* Error message should already have been printed */
		free(stmt);
		return rc;
	}
	log_debug("data.c: particle %d vel_id is now %d", p->uid, vel_id);

	size =
	    strlen(stmt_format) + strlen("positionID") + (CHAR_INT_MAX * 2) + 1;
	stmt = realloc(stmt, size);
	sprintf(stmt, stmt_format, "positionID", p->uid, pos_id);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("Could not update particle (ID %d) position: SQL error: %s\n",
		     p->uid, errmsg);
		sqlite3_free(errmsg);
		free(stmt);
		return rc;
	}
	size =
	    strlen(stmt_format) + strlen("velocityID") + (CHAR_INT_MAX * 2) + 1;
	stmt = realloc(stmt, size);
	sprintf(stmt, stmt_format, "velocityID", p->uid, vel_id);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("Could not update particle (ID %d) velocity: SQL error: %s\n",
		     p->uid, errmsg);
		sqlite3_free(errmsg);
		free(stmt);
		return rc;
	}
	size =
	    strlen(stmt_format) + strlen("bestPositionID") +
	    (CHAR_INT_MAX * 2) + 1;
	stmt = realloc(stmt, size);
	sprintf(stmt, stmt_format, "bestPositionID", p->uid, vel_id);
	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("Could not update particle (ID %d) best position: SQL error: %s\n",
		     p->uid, errmsg);
		sqlite3_free(errmsg);
		free(stmt);
		return rc;
	}
	free(stmt);

	/* Update the particle_history table, but only if we actually moved */
	if (pos_id != prev_id) {
		rc = opt_db_update_particle_history(p->uid, pos_id, vel_id, prev_id);
	}

	return rc;
}

static int opt_db_get_part_callback(void *vids, int argc, char **argv,
				    char **azColName)
{
	int i;
	int *ids = (int *)vids;

	for (i = 0; i < argc; i++) {
		log_debug("data.c: %s = %s", azColName[i],
			  argv[i] ? argv[i] : "NULL");
		if (!strcmp("id", azColName[i])) {
			ids[0] = atoi(argv[i]);	/* id of the particle, which we already know */
		} else if (!strcmp("positionID", azColName[i])) {
			ids[1] = atoi(argv[i]);	/* id of the particle's velocity */
		} else if (!strcmp("velocityID", azColName[i])) {
			ids[2] = atoi(argv[i]);	/* id of the particle's position */
		} else if (!strcmp("bestPositionID", azColName[i])) {
			ids[3] = atoi(argv[i]);	/* id of the particle's best found position so far */
		} else {
			log_warn("Got unhandled column from particle table: %s",
				 azColName[i]);
		}
	}
	return SQLITE_OK;
}

static int opt_db_get_fitness_callback(void *vfit, int argc, char **argv,
				       char **azColName)
{
	double *fitness = (double *)vfit;

	if (argc != 1) {
		log_error
		    ("Received %d results when had expected only one for fitness.",
		     argc);
	} else if (argv[0] != NULL) {
		*fitness = atof(argv[0]);
	} else {
		log_warn
		    ("A fitness requested from database, but none recorded for this position");
		*fitness = 0.0;
	}
	return SQLITE_OK;
}

int opt_db_get_position_count(int* count)
{
	int rc;
	char *errmsg = NULL;
	char *query = "SELECT COUNT(id) FROM position;";

	*count = -1;

	rc = sqlite3_exec(opt_db, query, opt_db_get_int_callback, count,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to find number of positions: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}

	return rc;
}

int opt_db_get_position_visits(int id, int *visits)
{
	int rc;
	char *errmsg = NULL;
	char *fmt = "SELECT visits FROM position WHERE (id=%d);";
	char *query = malloc(sizeof(*query) * strlen(fmt) + CHAR_INT_MAX + 1);

	*visits = -1;

	sprintf(query, fmt, id);
	log_debug("data.c: Query statment is: %s", query);
	rc = sqlite3_exec(opt_db, query, opt_db_get_int_callback, visits,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to get number of visits for position: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(query);
	return rc;
}

int opt_db_get_position_fitness(int id, double *fitness)
{
	int rc;
	char *errmsg = NULL;
	char *fmt = "SELECT fitness FROM position WHERE (id=%d);";
	char *query = malloc(sizeof(*query) * strlen(fmt) + CHAR_INT_MAX + 1);

	sprintf(query, fmt, id);
	log_debug("data.c: Query statment is: %s", query);
	rc = sqlite3_exec(opt_db, query, opt_db_get_fitness_callback, fitness,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to get fitness for position: %s\n",
		     errmsg);
		sqlite3_free(errmsg);
	}
	free(query);
	return rc;
}

int opt_db_get_particle(int id, spso_particle_t * particle)
{
	int rc;
	char *errmsg = NULL;
	char *fmt = "SELECT * FROM particle WHERE id=%d;";
	char *query = NULL;
	int ids[4] = { -1 };	/* Ready to store positionID, velocityID and previous best positionID */

	if (particle == NULL) {
		log_error("Cannot retrieve data into null particle");
		return SQLITE_ERROR;
	}

	query = malloc(sizeof(*query) * strlen(fmt) + CHAR_INT_MAX + 1);
	sprintf(query, fmt, id);
	rc = sqlite3_exec(opt_db, query, opt_db_get_part_callback, ids,
			  &errmsg);
	/* This should not be possible, unless the particle is not in the database */
	if (particle->uid != ids[0]) {
		log_error
		    ("IDs don't match up!  Particle probably isn't in the database.");
	}

	rc = opt_db_get_position(ids[1], &(particle->position));
	rc = opt_db_get_velocity(ids[2], &(particle->velocity));
	rc = opt_db_get_position(ids[3], &(particle->previous_best));
	rc = opt_db_get_position_fitness(ids[3],
					 &(particle->previous_best_fitness));
	if (rc != SQLITE_OK) {
		log_error
		    ("Could not retrieve fitness from database for particle %d",
		     id);
		particle->previous_best_fitness = DBL_MAX;
	}
	free(query);
	return rc;
}

static int opt_db_get_count_callback(void *v, int argc, char **argv,
				     char **azColName)
{
	int *count = (int *)v;
	/* TODO handle errors */
	*count = atoi(argv[0]);

	return SQLITE_OK;
}

int opt_db_get_count(int *count, char *tablename)
{
	int rc;
	char *errmsg = NULL;
	char *query_fmt = "SELECT COUNT(*) FROM %s;";
	char *query =
	    malloc(sizeof(*query) * strlen(query_fmt) + strlen(tablename) + 1);

	sprintf(query, query_fmt, tablename);
	rc = sqlite3_exec(opt_db, query, opt_db_get_count_callback, count,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to get count from table %s: %s\n",
		     tablename, errmsg);
		sqlite3_free(errmsg);
	}
	free(query);
	return rc;
}

int opt_db_get_dimension_count(int *num_dims)
{
	return opt_db_get_count(num_dims, "dimension");
}

int opt_db_get_particle_count(int *num_particles)
{
	return opt_db_get_count(num_particles, "particle");
}

int opt_db_get_flag_count(int *num_flags)
{
	return opt_db_get_count(num_flags, "flag");
}

spso_dimension_t **opt_db_get_searchspace(int *s)
{
	int i;
	int size = 0;
	spso_dimension_t **space = NULL;
	/* TODO one giant query is probably a lot more efficient than lots of
	 * small ones, and kinder to our file system */
	opt_db_get_dimension_count(&size);
	*s = size;

	space = malloc(sizeof(*space) * (size));
	/* TODO Handle errors */
	for (i = 0; i < size; i++) {
		space[i] = opt_db_get_dimension(i);
	}
	return space;
}

int opt_db_store_swarm(spso_swarm_t * swarm)
{
	int i;

	if (swarm == NULL) {
		log_error("Cannot store NULL swarm");
		return SQLITE_ERROR;
	}
	for (i = 0; i < swarm->size; i++) {
		opt_db_store_particle(swarm->particles[i]);
	}

	return SQLITE_OK;
}

/**
 * Before calling this you might want to:
 *  spso_dimension_t * dims = NULL;
 *  dims = opt_db_get_searchspace(&num_dims);
 */
spso_swarm_t *opt_db_get_swarm(int num_dims, spso_dimension_t ** dims)
{
	int i, size;
	spso_swarm_t *swarm = NULL;

	opt_db_get_particle_count(&size);

	swarm = opt_db_new_swarm(size, num_dims, dims);
	/* TODO one giant query is probably a lot more efficient than lots of
	 * small ones, and kinder to our file system */
	for (i = 0; i < swarm->size; i++) {
		swarm->particles[i] = opt_db_new_particle(i, num_dims, dims);
		opt_db_get_particle(i, swarm->particles[i]);
	}

	return swarm;
}

int opt_db_store_singleton(const char *name, int value)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_fmt = "UPDATE singleton SET value='%d' where (what=\"%s\");";
	char *stmt = NULL;
	int size;

	if (name == NULL) {
		log_error("Cannot store NULLs in singleton table");
		return SQLITE_ERROR;
	}

	size = strlen(stmt_fmt) + strlen(name) + CHAR_INT_MAX + 1;
	stmt = calloc(size, sizeof(char));
	sprintf(stmt, stmt_fmt, value, name);

	log_debug("data.c: Executing query: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to store singleton %s: %s\n",
		     name, errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_store_real_singleton(const char *name, double value)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_fmt =
	    "UPDATE real_singleton SET value='%lf' where (what=\"%s\");";
	char *stmt = NULL;
	int size;

	if (name == NULL) {
		log_error("Cannot store NULLs in real_singleton table");
		return SQLITE_ERROR;
	}

	size = strlen(stmt_fmt) + strlen(name) + CHAR_DBL_MAX + 1;
	stmt = calloc(size, sizeof(char));
	sprintf(stmt, stmt_fmt, value, name);

	log_debug("data.c: Executing query: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to store real singleton %s: %s\n",
		     name, errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_retrieve_real_singleton(const char *name, double *value)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_fmt =
	    "SELECT value FROM real_singleton where (what=\"%s\");";
	char *stmt = NULL;
	int size;

	if (name == NULL) {
		log_error("Cannot store NULLs in singleton table");
		return SQLITE_ERROR;
	}

	size = strlen(stmt_fmt) + strlen(name) + 1;
	stmt = calloc(sizeof(char), size);
	sprintf(stmt, stmt_fmt, name);

	log_debug("data.c: Executing query: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, opt_db_get_real_callback, value,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to retrieve real singleton %s: %s\n",
		     name, errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_retrieve_singleton(const char *name, int *value)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_fmt = "SELECT value FROM singleton where (what=\"%s\");";
	char *stmt = NULL;
	int size;

	if (name == NULL) {
		log_error("Cannot look up NULLs");
		return SQLITE_ERROR;
	}

	size = strlen(stmt_fmt) + strlen(name) + CHAR_INT_MAX + 1;
	stmt = calloc(sizeof(char), size);
	sprintf(stmt, stmt_fmt, name);

	log_debug("data.c: Executing query: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, opt_db_get_int_callback, value,
			  &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to retrieve singleton %s: %s\n",
		     name, errmsg);
		sqlite3_free(errmsg);
	}
	log_debug("data.c: Got %d for %s in singleton table", *value, name);

	free(stmt);
	return rc;
}

int opt_db_get_prng_seed(unsigned int *seed)
{
	return opt_db_retrieve_singleton("PRNG_SEED", (int *) seed);
}

int opt_db_store_prng_seed(unsigned int seed)
{
	return opt_db_store_singleton("PRNG_SEED", (int) seed);
}

int opt_db_get_no_move_counter(int *counter)
{
	return opt_db_retrieve_singleton("NO_MOVEMENT_COUNTER", counter);
}

int opt_db_store_no_move_counter(int counter)
{
	return opt_db_store_singleton("NO_MOVEMENT_COUNTER", counter);
}

int opt_db_get_converged(int *flag)
{
	return opt_db_retrieve_singleton("CONVERGED", flag);
}

int opt_db_store_converged(int flag)
{
	return opt_db_store_singleton("CONVERGED", flag);
}

int opt_db_store_prev_prev_best(double fitness)
{
	return opt_db_store_real_singleton("PREV_PREV_BEST", fitness);
}

int opt_db_get_prev_prev_best(double *fitness)
{
	double tmp;
	int rc = opt_db_retrieve_real_singleton("PREV_PREV_BEST", &tmp);
	if (tmp <= 0.0)
		*fitness = DBL_MAX;
	else
		*fitness = tmp;
	return rc;
}

int opt_db_store_prev_best(double fitness)
{
	return opt_db_store_real_singleton("PREV_BEST", fitness);
}

int opt_db_get_prev_best(double *fitness)
{
	double tmp;
	int rc = opt_db_retrieve_real_singleton("PREV_BEST", &tmp);
	if (tmp <= 0.0)
		*fitness = DBL_MAX;
	else
		*fitness = tmp;
	return rc;
}

int opt_db_store_curr_best(spso_position_t * position, double fitness)
{
	int rc, id, pid;
	if (position == NULL) {
		log_info("No current best position; Ignoring.");
		return SQLITE_OK;
	}
	rc = opt_db_store_position(&id, position);
	if (rc == SQLITE_OK) {
		log_debug("data.c: Found current best position at ID %d", id);
		rc = opt_db_store_real_singleton("CURR_BEST", fitness);
		rc = opt_db_update_position_fitness(id, fitness);	/* Should be unnecessary */
		/* Check before updating whether we've actually changed anything */
		rc = opt_db_retrieve_singleton("BEST_POS", &pid);
		if (rc == SQLITE_OK && pid != id) {
			log_debug("data.c: Storing current best position with fitness: %.6e");
			rc = opt_db_store_singleton("BEST_POS", id);
			rc = opt_db_update_global_best_history(id);
		}
	} else {
		log_warn
		    ("An error occurred and we could not determine which positionID the current best position corresponds to.");
	}
	return rc;
}

static int opt_db_get_best_callback(void *v, int argc, char **argv,
				    char **azColName)
{
	int *id = (int *)v;
	/* TODO handle errors */
	if (argc != 2) {
		log_error
		    ("Expecting two values when requesting min(fitness), id from position table.");
		return SQLITE_ERROR;
	}
	log_debug("data.c: azColName[1] is %s", azColName[1]);
	if (argv[1] != NULL && !strcmp(azColName[1], "id")) {
		/* TODO Check column names */
		*id = atoi(argv[1]);
	} else {
		log_error("No current best position");
		return SQLITE_ERROR;
	}
	log_debug("data.c: Got position: %d for current best position", *id);
	return SQLITE_OK;
}

int opt_db_get_curr_best(spso_position_t * position, double *fitness)
{
	int rc, id;
	char *errmsg = NULL;
	char *stmt = "SELECT min(fitness),id FROM position;";

	/*
	 * The two combined should be excessive and unnecessary
	 */
	rc = opt_db_retrieve_real_singleton("CURR_BEST", fitness);
	rc = opt_db_retrieve_singleton("BEST_POS", &id);
	if (rc == SQLITE_OK) {
		rc = opt_db_get_position(id, position);
	} else {
		log_warn
		    ("Could not retrieve current best position from singleton table entry.  Disregarding these entries.");

		rc = sqlite3_exec(opt_db, stmt, opt_db_get_best_callback, &id,
				  &errmsg);
		if (rc != SQLITE_OK) {
			log_error
			    ("SQL error encountered attempting to retrieve current best fitness from database: %s\n",
			     errmsg);
			sqlite3_free(errmsg);
			return rc;
		}
		rc = opt_db_get_position(id, position);
		rc = opt_db_get_position_fitness(id, fitness);
	}
	log_debug
	    ("data.c: Got position id %d with fitness %.6e for global current best",
	     id, *fitness);

	if (*fitness <= 0.0) {
		log_warn("No fitness value recovered for global current best");
		*fitness = DBL_MAX;
	}
	return rc;
}


int opt_db_update_particle_history(int particle_id, int new_position_id, int new_velocity_id, int best_position_id)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_fmt = "INSERT INTO particle_history (particleID, positionID, velocityID, bestPositionID) VALUES(%d, %d, %d, %d);";
	char *stmt = NULL;
	int size;

	size = strlen(stmt_fmt) + (4*CHAR_INT_MAX) + 1;
	stmt = calloc(size, sizeof(char));
	sprintf(stmt, stmt_fmt, particle_id, new_position_id, new_velocity_id, best_position_id);

	log_debug("data.c: Executing query: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to store recent history for particle %d: %s\n", particle_id, errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_update_global_best_history(int new_position_id)
{
	int rc;
	char *errmsg = NULL;
	char *stmt_fmt = "INSERT INTO global_best_history (positionID) VALUES(%d);";
	char *stmt = NULL;
	int size;

	size = strlen(stmt_fmt) + CHAR_INT_MAX + 1;
	stmt = calloc(size, sizeof(char));
	sprintf(stmt, stmt_fmt, new_position_id);

	log_debug("data.c: Executing query: %s", stmt);

	rc = sqlite3_exec(opt_db, stmt, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		log_error
		    ("SQL error encountered trying to store recent history for global current best position: %s\n", errmsg);
		sqlite3_free(errmsg);
	}
	free(stmt);
	return rc;
}

int opt_db_finalise(void)
{
	/* TODO Still allocated memory */
	if (opt_db != NULL)
		return sqlite3_close(opt_db);
	log_error("Attempted to close a NULL database");
	return 1;
}
