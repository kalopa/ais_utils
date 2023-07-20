/*
 * Copyright (c) 2021, Kalopa Robotics Limited.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      "This product includes software developed by Kalopa Robotics
 *      Limited."
 *
 * 4. The name of Kalopa Robotics must not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KALOPA ROBOTICS LIMITED "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL KALOPA ROBOTICS LIMITED
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ABSTRACT
 * Application to read AIS data from a serial port and send it to AISHub.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <string.h>

#define BUFFER_SIZE		512

struct baud_rate {
	speed_t		sval;
	int 		ival;
} baud_rates[] = {
	{B300, 300},
	{B600, 600},
	{B1200, 1200},
	{B2400, 2400},
	{B9600, 9600},
	{B19200, 19200},
	{B38400, 38400},
	{B57600, 57600},
	{B115200, 115200},
	{0, 0}
};

int		serfd;
int		ufd;
int		rdoffset;
char	rdbuffer[BUFFER_SIZE + 2];
char	*datadir;

void	process();
void	serial_open(char *, speed_t);
void	serial_read();
void	ais_data(char *, int);
void	udp_open(char *, int);
void	udp_write(char *, int);
void	make_path(char *);
void	usage();

/*
 *
 */
int
main(int argc, char *argv[])
{
	int i, speed, port;
	char *device, *host;

	opterr = 0;
	speed = B9600;
	port = 2500;
	device = "/dev/ttyS0";
	host = "data.aishub.net";
	datadir = NULL;
	while ((i = getopt(argc, argv, "l:s:h:p:d:")) != EOF) {
		switch (i) {
		case 'l':
			device = optarg;
			break;

		case 's':
			speed = atoi(optarg);
			for (i = 0; baud_rates[i].ival != 0; i++)
				if (baud_rates[i].ival == speed)
					break;
			if (baud_rates[i].ival == 0) {
				fprintf(stderr, "?Error - invalid baud rate: %d\n", speed);
				exit(2);
			}
			speed = baud_rates[i].sval;
			break;

		case 'h':
			host = optarg;
			break;

		case 'p':
			if ((port = atoi(optarg)) < 100 || port > 65535)
				usage();
			break;

		case 'd':
			datadir = optarg;
			break;

		default:
			usage();
			break;
		}
	}
	serial_open(device, speed);
	udp_open(host, port);
	process();
	exit(0);
}

/*
 *
 */
void
process()
{
	int n, running = 1;
	struct timeval tval;
	fd_set rdfds;

	printf("Processing...\n");
	rdoffset = 0;
	while (running) {
		FD_ZERO(&rdfds);
		FD_SET(serfd, &rdfds);
		tval.tv_sec = 5;
		tval.tv_usec = 0;
		if ((n = select(serfd + 1, &rdfds, NULL, NULL, &tval)) < 0) {
			perror("ais_read (select)");
			exit(1);
		}
		if (n == 0)
			continue;
		if (FD_ISSET(serfd, &rdfds))
			serial_read();
	}
}

/*
 *
 */
void
serial_open(char *dev, speed_t speed)
{
	struct termios term;

	printf("Opening serial device [%s]...\n", dev);
	if ((serfd = open(dev, O_RDONLY)) < 0) {
		fprintf(stderr, "ais_read (serial_open): ");
		perror(dev);
		exit(1);
	}
	if (tcgetattr(serfd, &term) < 0) {
		perror("ais_read (tcgetattr)");
		exit(1);
	}
	term.c_iflag = IGNBRK|ISTRIP;
	term.c_oflag = OPOST;
	term.c_cflag = CS8|CREAD|CLOCAL;
	term.c_lflag = 0;
	cfsetispeed(&term, speed);

	if (tcsetattr(serfd, TCSANOW, &term) < 0) {
		perror("ais_read (tcsetattr)");
		exit(1);
	}
}

/*
 *
 */
void
serial_read()
{
	int nbytes;
	char *cp;

	/*
	 * Set an alarm here, because sometimes the device goes off
	 * into the woods and the best thing to do is exit and allow
	 * the restart to clear things out. The down-side is we don't
	 * want to fail too often or Docker will get annoyed.
	 */
	alarm(60*60);
	if ((nbytes = read(serfd, rdbuffer + rdoffset, BUFFER_SIZE - rdoffset)) < 0) {
		perror("ais_read (process read)");
		exit(1);
	}
	alarm(0);
	rdoffset += nbytes;
	rdbuffer[rdoffset] = '\0';
	if ((cp = strrchr(rdbuffer, '\n')) == NULL || (nbytes = cp - rdbuffer + 1) < 5)
		return;
	*cp = '\0';
	if (strncmp(rdbuffer, "!AIV", 4) == 0)
		ais_data(rdbuffer, nbytes);
	rdoffset -= nbytes;
	if (rdoffset > 0)
		memmove(rdbuffer, &rdbuffer[nbytes], rdoffset);
}

/*
 * Deal with a line of AIS data.
 */
void
ais_data(char *datap, int len)
{
	unsigned int oldch, my_csum, their_csum;
	char *cp, *endcp;
	static FILE *logfp = NULL;
	static int last_hour = 0;

	oldch = 0;
	if ((endcp = strpbrk(datap, "\r\n")) != NULL) {
		oldch = *endcp;
		*endcp = '\0';
	}
	for (my_csum = 0, cp = datap + 1; *cp != '\0' && *cp != '*'; cp++)
		my_csum ^= *cp;
	if (*cp != '*') {
		fprintf(stderr, "?Error - missing checksum in serial data.\n%s\n", datap);
		return;
	}
	*cp = '\0';
	their_csum = (int )strtol(cp + 1, NULL, 16);
	if (my_csum != their_csum) {
		fprintf(stderr, "?Error - invalid checksum in serial data.\n%s\n", datap);
		return;
	}
	if (datadir != NULL) {
		char *fpath;
		struct tm *tmp;
		time_t now;

		time(&now);
		tmp = localtime(&now);
		if (logfp == NULL || last_hour != tmp->tm_hour) {
			if (logfp != NULL)
				fclose(logfp);
			if ((fpath = malloc(strlen(datadir) + 32)) == NULL) {
				perror("malloc");
				exit(1);
			}
			sprintf(fpath, "%s/%04d%02d%02d", datadir,
							tmp->tm_year + 1900,
							tmp->tm_mon + 1,
							tmp->tm_mday);
			make_path(fpath);
			sprintf(fpath, "%s/%04d%02d%02d/ais%02d.log", datadir,
							tmp->tm_year + 1900,
							tmp->tm_mon + 1,
							tmp->tm_mday, tmp->tm_hour);
			if ((logfp = fopen(fpath, "a")) == NULL) {
				perror(fpath);
				exit(1);
			}
			free(fpath);
			last_hour = tmp->tm_hour;
		}
		fprintf(logfp, "%02d:%02d:%02d:%s\n", tmp->tm_hour, tmp->tm_min,
							tmp->tm_sec, datap + 1);
		fflush(logfp);
	}
	*cp = '*';
	if (endcp != NULL)
		*endcp = oldch;
	udp_write(datap, len);
}

/*
 *
 */
void
udp_open(char *host, int port)
{
	struct sockaddr_in sin;
	in_addr_t addr;

	if ((addr = inet_addr(host)) == INADDR_NONE) {
		struct hostent *hp;

		if ((hp = gethostbyname(host)) == NULL) {
			fprintf(stderr, "?Error - unresolved hostname: %s\n", host);
			exit(2);
		}
		memcpy((char *)&addr, hp->h_addr, hp->h_length);
	}
	if ((ufd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("ais_read (udp_open)");
		exit(1);
	}
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = htons(port);
	if (connect(ufd, (const struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("ais_read (connect)");
		exit(1);
	}
}

/*
 *
 */
void
udp_write(char *bufp, int nbytes)
{
	if (write(ufd, bufp, nbytes) != nbytes) {
		perror("ais_read (udp_write)");
		exit(1);
	}
}

/*
 *
 */
void
make_path(char *path)
{
	char *cp;
	struct stat stbuf;

	printf("Make directory [%s] if it doesn't exist...\n", path);
	if (stat(path, &stbuf) >= 0) {
		if ((stbuf.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr, "?Error - path '%s' is not a directory.\n", path);
			exit(1);
		}
		return;
	}
	if ((cp = strrchr(path, '/')) != NULL) {
		*cp = '\0';
		make_path(path);
		*cp = '/';
	}
	if (mkdir(path, 0755) < 0) {
		perror(path);
		exit(1);
	}
}

/*
 *
 */
void
usage()
{
	fprintf(stderr, "Usage: ais_read -l <device> -s <speed> -h <host> -p <port> -d <datadir>\n");
	exit(2);
}
