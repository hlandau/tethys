/* Tethys, util.c -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_cidr_to_str(u_cidr *cidr, char *s)
{
	struct in_addr in;

	in.s_addr = htonl(cidr->addr);
	u_ntop(&in, s);
	s += strlen(s);
	sprintf(s, "/%d", cidr->netsize);
}

void u_str_to_cidr(char *s, u_cidr *cidr)
{
	struct in_addr in;
	char *p;

	p = strchr(s, '/');
	if (p == NULL) {
		cidr->netsize = 32;
	} else {
		*p++ = '\0';
		cidr->netsize = atoi(p);
		if (cidr->netsize < 0)
			cidr->netsize = 0;
		else if (cidr->netsize > 32)
			cidr->netsize = 32;
	}

	u_aton(s, &in);
	cidr->addr = ntohl(in.s_addr);
}

int u_cidr_match(u_cidr *cidr, char *s)
{
	struct in_addr in;
	ulong mask, addr;

	if (cidr->netsize == 0)
		return 1; /* everything is in /0 */
	/* 8 becomes 0x00ffffff, 21 becomes 0x000007ff, etc */
	mask = (1 << (32 - cidr->netsize)) - 1;

	u_aton(s, &in);
	addr = ntohl(in.s_addr);

	return (addr | mask) == (cidr->addr | mask);
}

unsigned long parse_size(char *s)
{
	unsigned long n;
	char *mul;

	n = strtoul(s, &mul, 0);

	switch (tolower(*mul)) {
	/* mind the fallthrough */
	case 't': n <<= 10;
	case 'g': n <<= 10;
	case 'm': n <<= 10;
	case 'k': n <<= 10;
	default:
		break;
	}

	return n;
}

#define CT_NICK   001
#define CT_IDENT  002
#define CT_SID    004

static uint ctype_map[256];
static char null_casemap[256];
static char rfc1459_casemap[256];
static char ascii_casemap[256];

int matchmap(char *mask, char *string, char *casemap)
{
	char *m = mask, *s = string;
	char *m_bt = m, *s_bt = s;
 
	for (;;) {
		switch (*m) {
		case '\0':
			if (*s == '\0')
				return 1;
		backtrack:
			if (m_bt == mask)
				return 0;
			m = m_bt;
			s = ++s_bt;
			break;
 
		case '*':
			while (*m == '*')
				m++;
			m_bt = m;
			s_bt = s;
			break;
 
		default:
			if (!*s)
				return 0;
			if (casemap[(uchar)*m] != casemap[(uchar)*s]
			    && *m != '?')
				goto backtrack;
			m++;
			s++;
		}
	}
}

int match(char *mask, char *string)
{
	return matchmap(mask, string, null_casemap);
}

int matchirc(char *mask, char *string)
{
	return matchmap(mask, string, rfc1459_casemap);
}

int matchcase(char *mask, char *string)
{
	return matchmap(mask, string, ascii_casemap);
}

int matchhash(char *hash, char *string)
{
	char buf[CRYPTLEN];
	u_crypto_hash(buf, string, hash);
	return streq(buf, hash);
}

int mapcmp(char *s1, char *s2, char *map)
{
	int diff;

	while (*s1 && *s2) {
		diff = map[(ulong)*s1] - map[(ulong)*s2];
		if (diff != 0)
			return diff;
		s1++;
		s2++;
	}

	return map[(ulong)*s1] - map[(ulong)*s2];
}

int casecmp(char *s1, char *s2)
{
	return mapcmp(s1, s2, ascii_casemap);
}

int irccmp(char *s1, char *s2)
{
	return mapcmp(s1, s2, rfc1459_casemap);
}

void u_ntop(struct in_addr *in, char *s)
{
	u_strlcpy(s, inet_ntoa(*in), INET_ADDRSTRLEN);
}

void u_aton(char *s, struct in_addr *in)
{
	in->s_addr = inet_addr(s);
}

char *cut(char **p, char *delim)
{
	char *s = *p;

	if (s == NULL)
		return NULL;

	while (**p && !strchr(delim, **p))
		(*p)++;

	if (!**p) {
		*p = NULL;
	} else {
		*(*p)++ = '\0';
		while (strchr(delim, **p))
			(*p)++;
	}

	return s;
}

void null_canonize(char *s)
{
}

void rfc1459_canonize(char *s)
{
	for (; *s; s++)
		*s = rfc1459_casemap[(uchar)*s];
}

void ascii_canonize(char *s)
{
	for (; *s; s++)
		*s = ascii_casemap[(uchar)*s];
}

char *id_to_name(char *s)
{
	if (s[3]) { /* uid */
		u_user *u = u_user_by_uid(s);
		return u ? u->nick : "*";
	} else { /* sid */
		u_server *sv = u_server_by_sid(s);
		return sv ? sv->name : "*";
	}
}

char *name_to_id(char *s)
{
	if (strchr(s, '.')) { /* server */
		u_server *sv = u_server_by_name(s);
		return sv ? (sv->sid[0] ? sv->sid : NULL) : NULL;
	} else {
		u_user *u = u_user_by_nick(s);
		return u ? u->uid : NULL;
	}
}

char *ref_to_name(char *s)
{
	if (isdigit(s[0]))
		return id_to_name(s);
	return s;
}

char *ref_to_id(char *s)
{
	if (isdigit(s[0]))
		return s;
	return name_to_id(s);
}

bool exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

char *ref_to_ref(u_link *ctx, char *ref)
{
	return ctx->type == LINK_SERVER ? ref_to_id(ref) : ref_to_name(ref);
}

u_link *ref_link(u_link *ctx, char *ref)
{
	if (ctx && ctx->type == LINK_SERVER && isdigit(ref[0])) {
		if (ref[3]) {
			u_user *u = u_user_by_uid(ref);
			return u ? u->link : NULL;
		} else {
			u_server *sv = u_server_by_sid(ref);
			return sv ? sv->link : NULL;
		}
	} else {
		if (!strchr(ref, '.')) {
			u_user *u = u_user_by_nick(ref);
			return u ? u->link : NULL;
		} else {
			u_server *sv = u_server_by_name(ref);
			return sv ? sv->link : NULL;
		}
	}
}

char *link_name(u_link *link)
{
	char *name = NULL;

	switch (link->type) {
	case LINK_USER:
		name = ((u_user*)link->priv)->nick;
		break;
	case LINK_SERVER:
		name = SERVER(link->priv)->name;
		break;
	default:
		break;
	}

	return (name && name[0]) ? name : "*";
}

char *link_id(u_link *link)
{
	char *id = NULL;

	switch (link->type) {
	case LINK_USER:
		id = ((u_user*)link->priv)->uid;
		break;
	case LINK_SERVER:
		id = SERVER(link->priv)->sid;
	default:
		break;
	}

	return (id && id[0]) ? id : NULL;
}

int is_valid_nick(char *s)
{
	if (isdigit(*s) || *s == '-')
		return 0;
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_NICK))
			return 0;
	}
	return 1;
}

int is_valid_ident(char *s)
{
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_IDENT))
			return 0;
	}
	return 1;
}

int is_valid_sid(char *s)
{
	if (strlen(s) != 3)
		return 0;
	if (!isdigit(s[0]))
		return 0;
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_SID))
			return 0;
	}
	return 1;
}

int is_valid_chan(char *s)
{
	if (!s || !strchr(CHANTYPES, *s))
		return 0;
	if (strchr(s, ' '))
		return 0;
	return 1;
}

int init_util(void)
{
	int i;

	for (i=0; i<256; i++) {
		ctype_map[i] = 0;
		if (isalnum(i) || strchr("[]{}|\\^-_`", i))
			ctype_map[i] |= CT_NICK;
		if (isalnum(i) || strchr("[]{}-_", i))
			ctype_map[i] |= CT_IDENT;
		if (isalnum(i))
			ctype_map[i] |= CT_SID;

		null_casemap[i] = i;
		rfc1459_casemap[i] = islower(i) ? toupper(i) : i;
		ascii_casemap[i] = islower(i) ? toupper(i) : i;
	}

	rfc1459_casemap['['] = '{';
	rfc1459_casemap[']'] = '}';
	rfc1459_casemap['\\'] = '|';
	rfc1459_casemap['~'] = '^';

	return 0;
}

