#
#
#
CFLAGS=	-O -Wall

all:	ais_read

clean:
	rm -f ais_read nmea_parse *.o

ais_read: main.o
	$(CC) -o ais_read main.o

nmea_parse: nmea_parse.o
	$(CC) -o nmea_parse nmea_parse.o
