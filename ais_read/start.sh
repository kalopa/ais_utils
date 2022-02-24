#!/bin/sh
#
set -e

/app/ais_read -l $SERIAL_DEVICE -s $SERIAL_SPEED -h $REMOTE_HOST -p $REMOTE_PORT -d /data
exit 0
