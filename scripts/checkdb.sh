#! /usr/bin/env bash

DB=$1

if [ "x$1" == "x" ]
then
	echo "Please supply the database name/path."
	exit 1
fi

echo -n "Total number of evaluations (all visits) is: "
../optsearch/build/sqlite3 "$DB" \
	-batch "SELECT SUM(visits) FROM position;"

echo -n "Maximum number of visits is: "
../optsearch/build/sqlite3 "$DB" \
	-batch "SELECT MAX(visits) FROM position;"

echo -n "Average number of visits is: "
../optsearch/build/sqlite3 "$DB" \
	-batch "SELECT AVG(visits) FROM position;"

# This one doesn't produce anything useful
#echo "Top 10 positions by fitness and visits (fitness,visits,id):"
#../optsearch/build/sqlite3 "$DB" \
#	-batch "SELECT fitness,visits,id FROM position;" | sort -n | head

echo "Top 10 most visited positions:"
../optsearch/build/sqlite3 "$DB" \
	-batch "SELECT visits,fitness,id FROM position;" | sort -n | tail

