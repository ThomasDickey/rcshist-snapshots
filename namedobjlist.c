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
 *
 * $Id: namedobjlist.c,v 1.3 2001/05/20 14:51:33 iedowse Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "namedobjlist.h"

static int nol_hash(Namedobjlist *self, const void *str, int strlen);
static void nol_rehash(Namedobjlist *self, int log2hs);
static struct namedobjlist_item *namedobjlist_find(Namedobjlist *self,
    const void *name, int namelen);


static int
nol_hash(Namedobjlist *self, const void *vstr, int strlen) {
	int hash = 0;
	const unsigned char *str = vstr;
	const unsigned char *send = str + strlen;

	while (str < send)
		hash += (*str + 1) << ((hash^*str++) & 15);

	return hash & ((1<<self->log2hashsize) - 1);
}

static void
nol_rehash(Namedobjlist *self, int log2hs) {
	struct namedobjlist_item *itemp;
	int i;

	self->log2hashsize = log2hs;
	self->hash = realloc(self->hash, (1<<log2hs) * sizeof(*self->hash));

	for (i = 0; i < (1<<log2hs); i++)
		TAILQ_INIT(&self->hash[i]);

	TAILQ_FOREACH(itemp, &self->all, all) {
		struct nol_hashhead *hash;
	
		hash = &self->hash[nol_hash(self, itemp->name, itemp->namelen)];
		TAILQ_INSERT_TAIL(hash, itemp, hash);
	}
}
	
Namedobjlist *
namedobjlist_create() {
	Namedobjlist *nol = malloc(sizeof(*nol));

	TAILQ_INIT(&nol->all);
	nol->nitems = 0;
	nol->hash = NULL;
	nol_rehash(nol, 2);
	
	return nol;
}

void
namedobjlist_destroy(Namedobjlist *self) {
	if (self->nitems != 0) {
		/*
		 * Since we know nothing about the list contents, there
		 * is no way we can figure out how to destroy them.
		 */
		fprintf(stderr, "namedobjlist_destroy: list not empty\n");
		abort();
	}
	if (self->hash != NULL)
		free(self->hash);
	free(self);
}

static struct namedobjlist_item *
namedobjlist_find(Namedobjlist *self, const void *name, int namelen) {
	struct namedobjlist_item *itemp;

	struct nol_hashhead *hash = &self->hash[nol_hash(self, name, namelen)];

	TAILQ_FOREACH(itemp, hash, hash)
		if (itemp->namelen == namelen && bcmp(name, itemp->name,
		    namelen) == 0)
			return itemp;
	return NULL;
}

void *
namedobjlist_lookup(Namedobjlist *self, const void *name, int namelen) {
	struct namedobjlist_item *itemp;

	itemp = namedobjlist_find(self, name, namelen);
	return itemp ? itemp->data : NULL;
}

const void *
namedobjlist_revlookup(Namedobjlist *self, void *data, int *lenp) {
	struct namedobjlist_item *itemp;

	TAILQ_FOREACH(itemp, &self->all, all)
		if (data == itemp->data) {
			if (lenp != NULL)
				*lenp = itemp->namelen;
			return itemp->name;
		}
	return NULL;
}

void
namedobjlist_additem(Namedobjlist *self, const void *name, int namelen,
    void *data) {
	struct namedobjlist_item *itemp;
	struct nol_hashhead *hash;

	if (namedobjlist_find(self, name, namelen) != NULL) {
		fprintf(stderr, "namedobjlist_additem: '%.*s' exists!\n",
		    namelen, (char *)name); /* XXX strvisx this */
		abort();
	}

	itemp = malloc(sizeof(*itemp));
	hash = &self->hash[nol_hash(self, name, namelen)];

	itemp->name = malloc(namelen + 1);
	bcopy(name, itemp->name, namelen);
	((char *)itemp->name)[namelen] = '\0';
	itemp->namelen = namelen;
	itemp->data = data;

	TAILQ_INSERT_TAIL(&self->all, itemp, all);
	TAILQ_INSERT_TAIL(hash, itemp, hash);
	if (++self->nitems > 2*(1<<self->log2hashsize))
		nol_rehash(self, self->log2hashsize + 1);
}

void *
namedobjlist_removeitem(Namedobjlist *self, const void *name, int namelen) {
	struct namedobjlist_item *itemp;
	struct nol_hashhead *hash;
	void *ret = NULL;

	itemp = namedobjlist_find(self, name, namelen);
	hash = &self->hash[nol_hash(self, name, namelen)];

	if (itemp) {
		ret = itemp->data;
		TAILQ_REMOVE(&self->all, itemp, all);
		TAILQ_REMOVE(hash, itemp, hash);
		free(itemp->name);
		free(itemp);
		self->nitems--;
	}
	return ret;
}

Namedobjlist_iter *
nol_iter_create(Namedobjlist *nol) {
	Namedobjlist_iter *self = malloc(sizeof(*self));
	
	self->nol = nol;
	self->nextitem = TAILQ_FIRST(&nol->all);
	
	return self;
}

void
nol_iter_reset(Namedobjlist_iter *self) {
	self->nextitem = TAILQ_FIRST(&self->nol->all);
}

void *
nol_iter_next(Namedobjlist_iter *self, void **namep, int *namelenp) {
	struct namedobjlist_item *item;

	if ((item = self->nextitem) == NULL)
		return NULL;

	if (namep != NULL)
		*namep = item->name;
	if (namelenp != NULL)
		*namelenp = item->namelen;
	self->nextitem = TAILQ_NEXT(item, all);

	return item->data;
}

void
nol_iter_destroy(Namedobjlist_iter *self) {
	free(self);
}

