#!/usr/bin/env bash

echo "Sleepy script called with FLAGS=\"$FLAGS\" on host $HOSTNAME"

# This script should always time out
while true
do
    sleep 20
done

exit 0
