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
 * $Id: rcsfile.h,v 1.5 2001/05/20 14:51:33 iedowse Exp $
 */
#ifndef RCSFILE_H
#define RCSFILE_H

#include "misc.h"
#include "namedobjlist.h"

struct revnode {
	struct rcsfile *rcsp;

	struct rcstext revtext;
	struct rcsnum rev;
	struct rcstext author;
	struct rcsnum date;
	struct rcstext log;
	struct rcstext text;
	struct rcstext state;
	struct rcstext patchnextrev;

	struct textlist *textlines;
	struct textlist *outputlines;
	int olrefs;

	struct textlist *branchrevs;

	struct textlist *tags;
	struct textlist *branches;
	struct textlist *branchpoints;

	struct revnode *next;
	struct revnode *prev;
	struct revnode *patchnext;
	struct revnode *patchprev;
};


#define RCSFILE_LOWMEM	0x0001	/* Cache less to reduce memory usage */

struct rcsfile {
	char *mapstart;
	int maplen;
	char *filename;
	struct rcstext shortfname;
	int flags;

	struct rcstext headrev;
	struct rcstext branch;
	struct rcstext comment;
	struct rcstext expand;
	struct rcstext desc;

	struct textlist *access;

	struct revnode *head;
	Namedobjlist *symbols;
	Namedobjlist *revtags;
	Namedobjlist *branchhead;
	Namedobjlist *revs;
	Namedobjlist *revsbynum;
	int nrevs;
};

struct token {
	int type;
	struct rcstext value;
};

#define TOKTYPE_NONE	0
#define TOKTYPE_NUM	1
#define TOKTYPE_ID	2
#define TOKTYPE_STRING	3
#define TOKTYPE_COLON	4
#define TOKTYPE_SEMI	5

struct parser {
	char *pos;
	char *end;

	struct token saved;
};

#define ID_NONE		0
#define ID_DESC		1
#define ID_HEAD		2
#define ID_BRANCH	3
#define ID_ACCESS	4
#define ID_SYMBOLS	5
#define ID_LOCKS	6
#define ID_STRICT	7
#define ID_COMMENT	8
#define ID_EXPAND	9
#define ID_DATE		10
#define ID_AUTHOR	11
#define ID_STATE	12
#define ID_BRANCHES	13
#define ID_NEXT		14
#define ID_LOG		15
#define ID_TEXT		16

struct rcspatch_op {
	enum {RPOP_COPY, RPOP_DEL, RPOP_ADD} op;
	int line;
	int nline;
	int len;
	struct rcstext *textp;
};

struct rcspatch {
	struct revnode *oldnode;
	struct revnode *newnode;
	struct rcspatch_op *op;
	int len;
	int op_len;
};

struct rcsfile *rcsfile_open(const char *filename);
struct rcsfile *rcsfile_smartopen(const char *filename, char **branchp);
void rcsfile_free(struct rcsfile *rcsp);
void rcsfile_setflags(struct rcsfile *rcsp, int flags);
struct revnode **revlist(struct rcsfile *rcsp, char *branch);
void rev_calc(struct revnode *revp);
void rev_diff(struct revnode *revp, int ctx, int reverse);
void rev_addref(struct revnode *revp);
void rev_remref(struct revnode *revp);
int revbydate(const void *v1, const void *v2);



#endif
