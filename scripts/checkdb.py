#!/usr/bin/env python3

import sqlite3
import argparse
import os
from urllib.request import pathname2url

# I wrote this quick and dirty script because Isambard2 does not have GNU
# readline headers available and I did not fancy building it from source.
# That meant I couldn't build the sqlite3 executable and check by hand.
# Of course, if we also had NumPy it would be a lot more useful doing things
# this way, as we could also plot results using the SQLite output.

# Unfortunately, the flags table does not get filled in.  It should really
# contain things like the type of flag, so that we can reconstruct the
# flag/parameter choices more accurately with just the database.  As it is,
# you will also need the YAML config file if you want to do that.

def main(args):

    dbfilename = args.file
    # TODO check the file exists first

    if not os.path.isfile(dbfilename):
        print("ERROR: File ", dbfilename, " does not exist.")
        sys.exit(-1)

    if args.vacuum:
        try:
            conn = sqlite3.connect(dbfilename)
            cur = conn.cursor()
            print("Vacuuming database ..")
            cur.execute('vacuum;')
            conn.commit()
        except sqlite3.Error as e:
            print("An error was encountered trying to vacuum the database:", e.args[0])

    else:
        # TODO there are a lot more things that it might be useful to know.
        # These just give a tiny hint.  Having NumPy would really help,
        # but it is not installed on Isambard2 at present.

        try:
            dburi = 'file:%s?mode=ro' % pathname2url(dbfilename)
            conn = sqlite3.connect(dburi, uri=True)
        except sqlite3.OperationalError as e:
            print('ERROR: Could not connect to databse: ', e)


        try:
            cur = conn.cursor()

            # Warning: If the database is empty, these will fail.
            # That should only happen if something went really wrong

            print('\nNumber of positions searched: ',)
            cur.execute('select count(id) from position;')
            print(cur.fetchone()[0])

            print('\nTotal number of evaluations (all visits) is: ',)
            cur.execute('SELECT SUM(visits) FROM position;')
            print(cur.fetchone()[0])

        
            print("\nMaximum number of visits to any single position is: ",)
            cur.execute("SELECT MAX(visits) FROM position;")
            print(cur.fetchone()[0])

            print("\nAverage number of visits per position is: ",)
            cur.execute("SELECT AVG(visits) FROM position;")
            print(cur.fetchone()[0])

            # This one doesn't produce anything useful
            #print("Top 10 positions by fitness (fitness,visits,id):")
            #cur.execute("SELECT fitness,visits,id FROM position;")
            #rows = cur.fetchall()
            #sorted_rows = sorted(rows, key=lambda fitness: fitness[0])

            print("\nTop 10 most visited positions:")
            cur.execute("SELECT id,fitness,visits FROM position;")
            rows = cur.fetchall()
            sorted_rows = sorted(rows, key=lambda visits: visits[2])
            count = 0
            print('Id, Fitness, Visits:')
            for row in sorted_rows:
                count = count + 1
                print(row)
                if count == 10:
                    break
                
            print('\nAll best positions found:')
            print('Timestamp, PositionID:')
            for row in cur.execute('select * from global_best_history;'):
                print(row)

            # If all our fitnesses are UINT_MAX, then most likely every
            # attempt to assess fitness failed.  That means that something
            # failed at all positions visited.  Compiling may have failed,
            # execution of a wrapper script, benchmark or test may have failed,
            # etc.  OptSearch expects quite a few failures to be encountered,
            # but is likely to think the search converged if we never "move"
            # (ie fitness does not change).

            # Did the fitnesses of the positions change much?
            cur.execute('select fitness from position;')
            rows = cur.fetchall()  # This gives us a list of tuples
            print('\nFitnesses found vary between minimum: ', min(rows)[0], ' and maximum: ', max(rows)[0], '\n')

        except sqlite3.Error as e:
            print("An error was encountered trying to query the database:", e.args[0])

    conn.close()


if __name__ == "__main__" :
    parser = argparse.ArgumentParser(
                description='Do some basic sanity checking of SQLite files generated by OptSearch')
    parser.add_argument('-f', '--file', help='The sqlite file to interrogate', dest='file', required=True)
    parser.add_argument('-v', '--vacuum', help='Vacuum database and exit', action='store_true')
    args = parser.parse_args()
    main(args)

