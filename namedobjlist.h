/*
 * Copyright (c) 1999-2000 Ian Dowse <iedowse@maths.tcd.ie>
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
 *
 * $Id: namedobjlist.h,v 1.6 2025/01/01 20:29:42 tom Exp $
 */
#ifndef NAMEDOBJLIST_H
#define NAMEDOBJLIST_H

#include "bsd_queue.h"

typedef struct namedobjlist Namedobjlist;

struct namedobjlist_item {
	void *name;
	int namelen;
	void *data;

	TAILQ_ENTRY(namedobjlist_item) all;
	TAILQ_ENTRY(namedobjlist_item) hash;
};

struct namedobjlist {
	TAILQ_HEAD(all_hashhead, namedobjlist_item) all;
	TAILQ_HEAD(nol_hashhead, namedobjlist_item) *hash;
	int log2hashsize;
	int nitems;
};

typedef struct namedobjlist_iterator Namedobjlist_iter;
struct namedobjlist_iterator {
	Namedobjlist *nol;
	struct namedobjlist_item *nextitem;
};

Namedobjlist *namedobjlist_create(void);
void namedobjlist_destroy(Namedobjlist *self);
void *namedobjlist_lookup(Namedobjlist *self, const void *name, int namelen);
const void *namedobjlist_revlookup(Namedobjlist *self, void *data, int *lenp);
void namedobjlist_additem(Namedobjlist *self, const void *name, int namelen,
    void *data);
void *namedobjlist_removeitem(Namedobjlist *self, const void *name,
    int namelen);

Namedobjlist_iter *nol_iter_create(Namedobjlist *nol);
void nol_iter_reset(Namedobjlist_iter *self);
void *nol_iter_next(Namedobjlist_iter *self, const void **namep, int *namelenp);
void nol_iter_destroy(Namedobjlist_iter *self);

#endif
