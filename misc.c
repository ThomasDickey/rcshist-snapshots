/*
 * Copyright (c) 2001 Ian Dowse <iedowse@maths.tcd.ie>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "misc.h"

struct textlist *
textlist_create(void) {
	struct textlist *tlp;

	tlp = malloc(sizeof(*tlp));
	tlp->list = NULL;
	tlp->len = 0;
	tlp->list_len = 0;

	return tlp;
}

void
textlist_destroy(struct textlist *tlp) {
	if (tlp->list != NULL)
		free(tlp->list);
	free(tlp);
}

void
textlist_add(struct textlist *tlp, struct rcstext *text) {
	if (tlp->list_len == tlp->len) {
		tlp->list_len += tlp->list_len + 1;
		tlp->list = realloc(tlp->list, (size_t)tlp->list_len *
		    sizeof(*tlp->list));
	}

	tlp->list[tlp->len++] = *text;
}

int
txtequ(struct rcstext *p1, struct rcstext *p2) {
	return p1->len == p2->len && bcmp(p1->start, p2->start, (size_t)p1->len) == 0;
}

void
numinit(struct rcsnum *p) {
	p->num = NULL;
	p->len = 0;
}

int
numequ(const struct rcsnum *p1, const struct rcsnum *p2) {
	return p1->len == p2->len && bcmp(p1->num, p2->num,
	    (size_t)RCSNUM_BYTES(p1)) == 0;
}

int
numcmp(const struct rcsnum *p1, const struct rcsnum *p2) {
	int i;

	for (i = 0; i < p1->len && i < p2->len; i++)
		if (p1->num[i] != p2->num[i])
			return p1->num[i] < p2->num[i] ? -1 : 1;

	return p1->len == p2->len ? 0 : p1->len < p2->len ? -1 : 1;
}

void
numcpy(const struct rcsnum *p1, struct rcsnum *p2) {
	p2->len = p1->len;
	p2->num = malloc((size_t)RCSNUM_BYTES(p1));
	bcopy(p1->num, p2->num, (size_t)RCSNUM_BYTES(p2));
}

void
numextend(struct rcsnum *p, int len) {
	p->len = len;
	p->num = realloc(p->num, (size_t)RCSNUM_BYTES(p));
}


void
numfree(struct rcsnum *p) {
	free(p->num);
	p->num = 0;
}

void
text2num(struct rcstext *textp, struct rcsnum *nump) {
	int i;
	const char *p, *endp;

	if (nump->num != NULL) {
		printf("text2num: non-NULL num ptr\n");
		abort();
	}

	i = 0;
	p = textp->start;
	endp = textp->start + textp->len;
	while (p < endp && (p = memchr(p, '.', (size_t)(endp - p))) != NULL) {
		i++;
		p++;
	}

	nump->len = i + 1;
	nump->num = malloc((size_t)RCSNUM_BYTES(nump));
	
	p = textp->start;
	for (i = 0; i < nump->len; i++) {
		int n;

		if (*p < '0' || *p > '9') {
			printf("text2num: parse failed '%.*s'\n",
			    textp->len, textp->start);
			abort();
		}
	
		n = 0;
		while (p < endp && *p >= '0' && *p <= '9')
			n = (n * 10) + (*p++ - '0');

		if (p < endp && *p++ != '.') {
			printf("text2num: parse failed '%.*s'\n",
			    textp->len, textp->start);
			abort();
		}

		nump->num[i] = n;
	}
}

void
textprint(struct rcstext *textp) {
	const char *p1;
	const char *p = textp->start;
	const char *end = textp->start + textp->len;
	
	while (p < end && (p1 = memchr(p, '@', (size_t)(end - p))) != NULL) {
		fwrite(p, (size_t)(p1 - p + 1), 1, stdout);
		p = p1 + 2;
	}
	if (p < end)
		fwrite(p, (size_t)(end - p), 1, stdout);
}

struct textlist *
textsplit(struct rcstext *textp) {
	struct rcstext line;
	const char *p, *end;
	struct textlist *tlp = textlist_create();

	p = textp->start;
	end = textp->start + textp->len;

	while (p != NULL && p < end) {
		line.start = p;

		if ((p = memchr(p, '\n', (size_t)(end - p))) == NULL)
			p = end;
		else
			p++;
		line.len = (int)(p - line.start);

		textlist_add(tlp, &line);
	}

	return tlp;
}
