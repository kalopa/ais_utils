#!/bin/sh
#
docker container run -itd \
	-p 4321:4321/udp \
	--name ais_relay \
	registry.kalopa.net/ais-relay:1.5
exit 0
