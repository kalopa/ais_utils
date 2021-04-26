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
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <string.h>

#define BUFFER_SIZE		512

int				src_fd;
int				dst_fd;
char			buffer[BUFFER_SIZE];
unsigned long	msg_count;

void	usage();

/*
 *
 */
int
main(int argc, char *argv[])
{
	int i, src_port, dst_port;
	char *src_host, *dst_host, *cp;
	struct hostent *hp;
	struct sockaddr_in sin;
	in_addr_t addr;

	if (argc != 3)
		usage();
	src_host = argv[1];
	dst_host = argv[2];
	if ((cp = strchr(src_host, ':')) != NULL) {
		*cp++ = '\0';
		src_port = atoi(cp);
	} else
		src_port = 4321;
	if ((cp = strchr(dst_host, ':')) != NULL) {
		*cp++ = '\0';
		dst_port = atoi(cp);
	} else
		dst_port = 2500;
	printf("SRC: %s - %d\n", src_host, src_port);
	printf("DST: %s - %d\n", dst_host, dst_port);
	/*
	 * Open a UDP port for listening...
	 */
	if ((addr = inet_addr(src_host)) == INADDR_NONE) {
		if ((hp = gethostbyname(src_host)) == NULL) {
			fprintf(stderr, "?Error - unresolved hostname: %s\n", src_host);
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
	 * Now open a connection to the destination...
	 */
	if ((addr = inet_addr(dst_host)) == INADDR_NONE) {
		if ((hp = gethostbyname(dst_host)) == NULL) {
			fprintf(stderr, "?Error - unresolved hostname: %s\n", dst_host);
			exit(2);
		}
		memcpy((char *)&addr, hp->h_addr, hp->h_length);
	}
	if ((dst_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("ais_relay (udp_open)");
		exit(1);
	}
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = htons(dst_port);
	if (connect(dst_fd, (const struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("ais_relay (connect)");
		exit(1);
	}
	msg_count = 0L;
	while ((i = read(src_fd, buffer, BUFFER_SIZE)) >= 0) {
		if ((++msg_count % 100L) == 0)
			printf("%ld packets relayed.\n", msg_count);
		if (write(dst_fd, buffer, i) < 0) {
			perror("ais_relay (udp write)");
			exit(1);
		}
	}
	perror("ais_relay (udp read)");
	exit(1);
}

/*
 *
 */
void
udp_write(char *bufp, int nbytes)
{
	if (write(src_fd, bufp, nbytes) != nbytes) {
		perror("ais_relay (udp_write)");
		exit(1);
	}
}

/*
 *
 */
void
usage()
{
	fprintf(stderr, "Usage: ais_relay <src_host> <dst_host>\n");
	exit(2);
}
