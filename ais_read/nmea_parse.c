/*
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXLINELEN			512
#define MAXARGS				32
#define MESSAGE_LEN			64

struct ais_msg	{
	int		chan;
	int		type;
	int		msg_id;
	int		nfrags;
	int		frag_no;
	int		msg_len;
	char	message[MESSAGE_LEN];
	int		msg_offset;
	int		bit_reg;
	int		bit_count;
	char	*raw_src;
};

#define MSGTYPE_VDM			0
#define MSGTYPE_VDO			1

#define MSG_POSREP_A			1
#define MSG_POSREP_A_ASSIGNED	2
#define MSG_POSREP_A_RESPONSE	3
#define MSG_BASE_STN_REPORT		4
#define MSG_STATIC_VOYAGE_DATA	5
#define MSG_BINARY_ADDRMSG		6
#define MSG_BINARY_ACK			7
#define MSG_BINARY_BCAST		8
#define MSG_SAR_POSREP			9
#define MSG_UTC_DATE_INQ		10
#define MSG_UTC_DATE_RESP		11
#define MSG_SAFETY_MSG			12
#define MSG_SAFETY_ACK			13
#define MSG_SAFETY_BCAST		14
#define MSG_INTERROGATION		15
#define MSG_ASSIGNMENT_MODE		16
#define MSG_DGNSS_BCAST			17
#define MSG_POSREP_B_CS			18
#define MSG_POSREP_B_EQUIP		19
#define MSG_LINK_MGMT			20
#define MSG_AID_TO_NAV			21
#define MSG_CHANNEL_MGMT		22
#define MSG_GROUP_ASSIGN		23
#define MSG_STATIC_DATA			24
#define MSG_SINGLE_SLOT			25
#define MSG_MULTI_SLOT			26
#define MSG_POSREP_LONGRANGE	27

#define NAV_AT_ANCHOR		1
#define NAV_NOT_UNDER_CMD	2
#define NAV_RESTRICTED		3
#define NAV_CONSTRAINED		4
#define NAV_MOORED			5
#define NAV_AGROUND			6
#define NAV_FISHING			7
#define NAV_UW_SAILING		8
#define NAV_AIS_SART		14
#define NAV_UNDEFINED		15

char	input[MAXLINELEN+2];

struct ais_msg		*ais_alloc();
void				ais_free();
int					_get_bits(struct ais_msg *, int);
int					process(char *);

/*
 * It kicks off, here.
 */
int
main(int argc, char *argv[])
{
	char *cp;
	FILE *fp;

	if (argc != 2) {
		fprintf(stderr, "nmea-parse <datafile>\n");
		exit(2);
	}
	if ((fp = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}
	while (fgets(input, MAXLINELEN, fp) != NULL) {
		if ((cp = strpbrk(input, "\r\n")) != NULL)
			*cp = '\0';
		process(input);
	}
	fclose(fp);
	exit(0);
}

/*
 *
 */
struct ais_msg *
ais_alloc()
{
	struct ais_msg *ap;

	if ((ap = (struct ais_msg *)malloc(sizeof(struct ais_msg))) == NULL) {
		perror("ais_alloc");
		exit(1);
	}
	ap->raw_src = NULL;
	ap->msg_offset = ap->bit_reg = ap->bit_count = 0;
	return(ap);
}

/*
 *
 */
void
ais_free(struct ais_msg *ap)
{
	if (ap->raw_src != NULL)
		free(ap->raw_src);
	free(ap);
}

/*
 *
 */
int
_get_bits(struct ais_msg *ap, int nbits)
{
	int bval;

	while (ap->bit_count < nbits) {
		if (ap->msg_offset >= ap->msg_len) {
			printf("FAIL:[%s]\n", ap->raw_src);
			return(-1);
		}
		ap->bit_reg = ap->bit_reg << 8 | (ap->message[ap->msg_offset++] & 0xff);
		ap->bit_count += 8;
	}
	bval = (ap->bit_reg >> (ap->bit_count - nbits)) & ((1 << nbits) - 1);
	ap->bit_count -= nbits;
	if (bval & (1 << (nbits - 1)))
		bval = bval | (0xffffffffffffffff & ~((1 << nbits) - 1));
	return(bval);
}

/*
 *
 */
void
parse_ais(struct ais_msg *ap)
{
	int i, n, type, value;

	printf(">nf:%d,fr:%d,id:%d,ch:%d,len:%d\n", ap->nfrags, ap->frag_no, ap->msg_id, ap->chan, ap->msg_len);
	for (i = 0; i < ap->msg_len; i++) {
		printf(" %02x", ap->message[i] & 0xff);
	}
	putchar('\n');
	type = _get_bits(ap, 6);
	printf("TYPE:%d\n", type);
	switch (type) {
	case MSG_POSREP_A:
	case MSG_POSREP_A_ASSIGNED:
	case MSG_POSREP_A_RESPONSE:
		printf("POSREP A\n");
		printf("Repeat indicator: %d\n", _get_bits(ap, 2));
		printf("MMSI: %d\n", _get_bits(ap, 30));
		printf("Navigation Status: %d\n", _get_bits(ap, 4));
		printf("Rate of Turn: %d\n", _get_bits(ap, 8));
		printf("SOG: %d\n", _get_bits(ap, 10));
		printf("Accuracy: %d\n", _get_bits(ap, 1));
		value = _get_bits(ap, 28);
		printf("VAL:%d/%x\n", value, value);
		printf("Longitude: %d\n", value);
		printf("Latitude: %d\n", _get_bits(ap, 27));
		printf("COG: %d\n", _get_bits(ap, 12));
		printf("Heading: %d\n", _get_bits(ap, 9));
		printf("TimeStamp: %d\n", _get_bits(ap, 6));
		printf("Maneuver: %d\n", _get_bits(ap, 2));
		printf("Spare: %d\n", _get_bits(ap, 3));
		printf("RAIM: %d\n", _get_bits(ap, 1));
		printf("Radio: %d\n", _get_bits(ap, 19));
		break;

	default:
		printf("?Undefined...\n");
		break;
	}
}

/*
 *
 */
int
crack(char *strp, char *argv[], int maxargs)
{
	int i;

	for (i = 0; i < maxargs; i++) {
		while (strp != NULL && isspace(*strp))
			strp++;
		argv[i] = strp;
		if (strp == NULL || *strp == '\0')
			break;
		while (*strp != '\0' && *strp != ',')
			strp++;
		if (*strp == '\0')
			break;
		*strp++ = '\0';
	}
	return(i + 1);
}

/*
 *
 */
int
to_int(char *strp, int base)
{
	int val, ch;

	for (val = 0; isxdigit(*strp);) {
		ch = *strp++;
		if (isdigit(ch))
			ch -= '0';
		else {
			if (ch >= 'A' && ch <= 'F')
				ch = (ch - 'A') + 10;
			else {
				if (ch >= 'a' && ch <= 'f')
					ch = (ch - 'a') + 10;
				else
					return(-1);
			}
		}
		if (ch >= base)
			return(-1);
		val = (val * base) | ch;
	}
	return(val);
}

/*
 *
 */
int
process(char *strp)
{
	int i, n, csum;
	char *argv[MAXARGS], *cp, *xp;
	struct ais_msg *ap;

	/*
	 * Compute and verify the checksum.
	 */
	ap = ais_alloc();
	ap->raw_src = strdup(strp);
	if (*strp++ != '!') {
		ais_free(ap);
		return(-1);
	}
	for (csum = 0, cp = strp; *cp != '*' && *cp != '\0';)
		csum ^= *cp++;
	if (*cp != '*') {
		ais_free(ap);
		return(-1);
	}
	*cp++ = '\0';
	if (to_int(cp, 16) != csum) {
		fprintf(stderr, "Bad csum: [%s]/%x\n", strp, csum);
		ais_free(ap);
		return(-1);
	}
	/*
	 * Quick check that it's an AIS NMEA string.
	 */
	if (strncmp(strp, "AB", 2) != 0 &&
				strncmp(strp, "AD", 2) != 0 &&
				strncmp(strp, "AI", 2) != 0 &&
				strncmp(strp, "AN", 2) != 0 &&
				strncmp(strp, "AR", 2) != 0 &&
				strncmp(strp, "AS", 2) != 0 &&
				strncmp(strp, "AT", 2) != 0 &&
				strncmp(strp, "AX", 2) != 0 &&
				strncmp(strp, "BS", 2) != 0 &&
				strncmp(strp, "SA", 2) != 0) {
		ais_free(ap);
		return(-1);
	}
	strp += 2;
	/*
	 * Look to see if it's ..VDM or ..VDO - don't care about anything else.
	 */
	if (strncmp(strp, "VDM", 3) == 0)
		ap->type = MSGTYPE_VDM;
	else {
		if (strncmp(strp, "VDO", 3) == 0)
			ap->type = MSGTYPE_VDO;
		else {
			ais_free(ap);
			return(-1);
		}
	}
	strp += 3;
	if (*strp++ != ',') {
		ais_free(ap);
		return(-1);
	}
	/*
	 * Now, process the remaining arguments by cracking apart the
	 * comma-separated values and processing them.
	 */
	printf("Proc:[%s]\n", strp);
	n = crack(strp, argv, MAXARGS);
	if (n != 6 ||
				(ap->nfrags = to_int(argv[0], 10)) < 0 ||
				(ap->frag_no = to_int(argv[1], 10)) < 0 ||
				(ap->msg_id = to_int(argv[2], 10)) < 0) {
		ais_free(ap);
		return(-1);
	}
	if (*argv[3] == 'B' || *argv[3] == '2')
		ap->chan = 1;
	else
		ap->chan = 0;
	/*
	 * Convert the message from sixbit back to binary.
	 */
	ap->msg_len = 0;
	for (i = 0, n = 0, cp = argv[4], xp = ap->message; *cp != '\0';) {
		if ((csum = *cp++ - '0') < 0)
			return(-1);
		if (csum > 39) {
			if (csum < 48) {
				ais_free(ap);
				return(-1);
			}
			csum -= 8;
			if (csum > 63) {
				ais_free(ap);
				return(-1);
			}
		}
		i = (i << 6) | csum;
		if ((n += 6) >= 8) {
			n -= 8;
			*xp++ = (i >> n) & 0xff;
			if (++ap->msg_len >= MESSAGE_LEN) {
				ais_free(ap);
				return(-1);
			}
		}
	}
	if ((n = to_int(argv[5], 10)) < 0) {
		ais_free(ap);
		return(-1);
	}
	if (n > 0) {
		i <<= (8 - n);
		*xp++ = i & 0xff;
		if (++ap->msg_len >= MESSAGE_LEN) {
			ais_free(ap);
			return(-1);
		}
	}
	parse_ais(ap);
	ais_free(ap);
	return(0);
}
