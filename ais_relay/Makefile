#
#
#
CFLAGS=	-O -Wall

all:	ais_read ais_relay nmea_parse

clean:
	rm -f ais_read ais_relay nmea_parse *.o

ais_read: ais_read.o
	$(CC) -o ais_read ais_read.o

ais_relay: ais_relay.o
	$(CC) -o ais_relay ais_relay.o

nmea_parse: nmea_parse.o
	$(CC) -o nmea_parse nmea_parse.o
