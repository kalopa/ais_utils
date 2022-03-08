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
#include <sys/time.h>
#include <time.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <string.h>

#define BUFFER_SIZE		512

struct ais_dest {
	struct ais_dest	*next;
	int				fd;
	char			*host;
	int				port;
};

int				src_fd;
char			buffer[BUFFER_SIZE];
unsigned long	msg_count;
struct ais_dest	*dlist;

void	usage();

/*
 *
 */
int
main(int argc, char *argv[])
{
	int i, src_port;
	char *src_host, *cp;
	struct hostent *hp;
	struct sockaddr_in sin;
	in_addr_t addr;
	struct ais_dest *adp, *dtail;
	time_t now;
	struct tm *tmp;

	if (argc < 3)
		usage();
	dlist = dtail = NULL;
	src_host = argv[1];
	if ((cp = strchr(src_host, ':')) != NULL) {
		*cp++ = '\0';
		src_port = atoi(cp);
	} else
		src_port = 4321;
	printf("SRC: %s - %d\n", src_host, src_port);
	/*
	 * Open a UDP port for listening...
	 */
	if ((addr = inet_addr(src_host)) == INADDR_NONE) {
		/*
		 * Deal with some weird DNS issues.
		 */
		for (i = 0; i < 5; i++)
			if ((hp = gethostbyname(src_host)) != NULL)
				break;
		if (hp == NULL) {
			fprintf(stderr, "?Error - unresolved hostname: '%s'\n", src_host);
			exit(2);
		}
		memcpy((char *)&addr, hp->h_addr, hp->h_length);
	}
	if ((src_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("ais_relay (udp_open)");
		exit(1);
	}
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = htons(src_port);
	if (bind(src_fd, (const struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("ais_relay (bind)");
		exit(1);
	}
	/*
	 * Now create all of the destinations...
	 */
	for (i = 2; i < argc; i++) {
		if ((adp = (struct ais_dest *)malloc(sizeof(*adp))) == NULL) {
			perror("ais_relay: malloc");
			exit(1);
		}
		adp->next = NULL;
		if (dlist == NULL)
			dlist = adp;
		else
			dtail->next = adp;
		dtail = adp;
		adp->host = strdup(argv[i]);
		if ((cp = strchr(adp->host, ':')) != NULL) {
			*cp++ = '\0';
			adp->port = atoi(cp);
		} else
			adp->port = 2500;
		printf("DSTn: %s - %d\n", adp->host, adp->port);
		/*
		 * Open a connection to the destination...
		 */
		if ((addr = inet_addr(adp->host)) == INADDR_NONE) {
			if ((hp = gethostbyname(adp->host)) == NULL) {
				fprintf(stderr, "?Error - unresolved hostname: %s\n", adp->host);
				exit(2);
			}
			memcpy((char *)&addr, hp->h_addr, hp->h_length);
		}
		if ((adp->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("ais_relay (udp_open)");
			exit(1);
		}
		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = addr;
		sin.sin_port = htons(adp->port);
		if (connect(adp->fd, (const struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
			fprintf(stderr, "ais_relay: %s (port %d): ", adp->host, adp->port);
			perror("connect");
			exit(1);
		}
	}
	msg_count = 0L;
	while ((i = read(src_fd, buffer, BUFFER_SIZE)) >= 0) {
		if ((++msg_count % 10L) == 0) {
			time(&now);
			tmp = localtime(&now);
			printf("%04d-%02d-%02d %02d:%02d:%02d: %ld packets relayed.\n",
							tmp->tm_year + 1900, tmp->tm_mon + 1,
							tmp->tm_mday, tmp->tm_hour, tmp->tm_min,
							tmp->tm_sec, msg_count);
		}
		for (adp = dlist; adp != NULL; adp = adp->next) {
			if (write(adp->fd, buffer, i) < 0) {
				fprintf(stderr, "ais_relay: %s (port %d): ", adp->host, adp->port);
				perror("udp write");
				exit(1);
			}
		}
	}
	perror("ais_relay (udp read)");
	exit(1);
}

/*
 *
 */
void
usage()
{
	fprintf(stderr, "Usage: ais_relay <src_host> <dst_host1> ...\n");
	exit(2);
}
