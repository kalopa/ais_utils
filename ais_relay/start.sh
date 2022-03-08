#!/bin/sh
#
set -e

/app/ais_relay $SRC_HOST aishub.net:2569 ais.vesselfinder.com:5436
exit 0
