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
 *    derived from this software without specific prior written permission
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
 *
 * $Id: misc.h,v 1.4 2025/10/26 14:09:49 tom Exp $
 */
#ifndef MISC_H
#define MISC_H

struct rcstext {
	const char *start;
	int len;
};

struct rcsnum {
	int *num;
	int len;
};

#define RCSNUM_BYTES(nump) (int)((size_t)(nump)->len * sizeof(*(nump)->num))

struct textlist {
	struct rcstext *list;
	int len;
	int list_len;
};

#define TEXTLIST_FOREACH(listp, p) \
	for ((p) = (listp)->list; \
	    (p) != NULL && (p) < &(listp)->list[(listp)->len]; (p)++)

struct textlist *textlist_create(void);
void textlist_destroy(struct textlist *tlp);
void textlist_add(struct textlist *tlp, struct rcstext *text);
int txtequ(struct rcstext *p1, struct rcstext *p2);

void numinit(struct rcsnum *p);
int numequ(const struct rcsnum *p1, const struct rcsnum *p2);
int numcmp(const struct rcsnum *p1, const struct rcsnum *p2);
void numcpy(const struct rcsnum *p1, struct rcsnum *p2);
void numextend(struct rcsnum *p, int len);
void numfree(struct rcsnum *p);
void text2num(struct rcstext *textp, struct rcsnum *nump);

struct textlist *textsplit(struct rcstext *textp);
void textprint(struct rcstext *textp);

#endif
