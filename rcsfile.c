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
 * $Id: rcsfile.c,v 1.16 2004/09/04 02:30:02 iedowse Exp $
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include "rcsfile.h"
#include "strbuf.h"

static void get_admin(struct parser *pp, struct rcsfile *rcsp);
static void get_deltas(struct parser *pp, struct rcsfile *rcsp);
static void get_desc(struct parser *pp, struct rcsfile *rcsp);
static void get_deltatexts(struct parser *pp, struct rcsfile *rcsp);
static void fixup_deltas(struct rcsfile *rcsp);
static void patch_printop(struct rcspatch_op *opp, char *prefix);
static void reversepatch(struct rcspatch *pp);
static struct rcspatch *makepatch(struct revnode *revp);
static struct rcspatch *patch_create(void);
static void patch_destroy(struct rcspatch *pp);
static void patch_add(struct rcspatch *pp, int op, int line, int nline,
    int len, struct rcstext *textp);
static int id_lookup(struct rcstext *id);
static int optional_tok(struct parser *pp, struct token *tokp, int type);
static void expect_tok(struct parser *pp, struct token *tokp, int type);
static void puttok(struct parser *pp, struct token *tokp);
static int gettok(struct parser *pp, struct token *tokp); 

char *tokname[] = {"NONE", "NUM", "ID", "STRING", "COLON", "SEMI"};

struct rcstext
	id_desc =	{"desc",	4},
	id_head =	{"head",	4},
	id_branch =	{"branch",	6},
	id_access =	{"access",	6},
	id_symbols =	{"symbols",	7},
	id_locks =	{"locks",	5},
	id_strict =	{"strict",	6},
	id_comment =	{"comment",	7},
	id_expand =	{"expand",	6},
	id_date =	{"date",	4},
	id_author =	{"author",	6},
	id_state =	{"state",	5},
	id_branches =	{"branches",	8},
	id_next =	{"next",	4},
	id_log =	{"log",		3},
	id_text =	{"text",	4};


struct rcsfile *
rcsfile_open(const char *filename) {
	int fd;
	struct stat sb;
	struct parser pp;
	struct token tok;
	struct rcsfile *rcsp;
	char *map, *p;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		warn("%s: open", filename);
		return NULL;
	}

	if (fstat(fd, &sb) != 0) {
		warn("%s: fstat", filename);
		close(fd);
		return NULL;
	}
		
	if ((map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) ==
	    MAP_FAILED) {
		warn("%s: mmap", filename);
		close(fd);
		return NULL;
	}

	close(fd);

	pp.pos = map;
	pp.end = map + sb.st_size;	
	pp.saved.type = TOKTYPE_NONE;

	rcsp = calloc(1, sizeof(*rcsp));
	rcsp->mapstart = map;
	rcsp->maplen = sb.st_size;
	rcsp->filename = strdup(filename);

	p = strrchr(rcsp->filename, '/');
	rcsp->shortfname.start = (p == NULL) ? rcsp->filename : p + 1;
	rcsp->shortfname.len = strlen(rcsp->shortfname.start);
	if (rcsp->shortfname.len > 2 && bcmp(",v", rcsp->shortfname.start +
	    rcsp->shortfname.len - 2, 2) == 0)
		rcsp->shortfname.len -= 2;

	rcsp->flags = 0;

	rcsp->access = textlist_create();
	rcsp->symbols = namedobjlist_create();
	rcsp->revtags = namedobjlist_create();
	rcsp->branchhead = namedobjlist_create();
	rcsp->revs = namedobjlist_create();
	rcsp->revsbynum = namedobjlist_create();

	get_admin(&pp, rcsp);
	get_deltas(&pp, rcsp);
	get_desc(&pp, rcsp);
	get_deltatexts(&pp, rcsp);
	fixup_deltas(rcsp);
	if (gettok(&pp, &tok))
		errx(1, "%s: junk at end of rcs file", filename);

	return rcsp;
}

void
rcsfile_free(struct rcsfile *rcsp) {
	Namedobjlist_iter *iter;
	struct revnode *revp;
	struct rcsnum *nump;
	struct textlist *tlp;
	void *name;
	int namelen;

	iter = nol_iter_create(rcsp->revs);
	while ((revp = nol_iter_next(iter, &name, &namelen)) != NULL) {
		namedobjlist_removeitem(rcsp->revs, name, namelen);
		namedobjlist_removeitem(rcsp->revsbynum, revp->rev.num,
		    RCSNUM_BYTES(&revp->rev));

		numfree(&revp->rev);
		numfree(&revp->date);
		if (revp->textlines != NULL)
			textlist_destroy(revp->textlines);
		if (revp->outputlines != NULL)
			textlist_destroy(revp->outputlines);
		textlist_destroy(revp->branchrevs);
		textlist_destroy(revp->branchpoints);
		textlist_destroy(revp->branches);
		textlist_destroy(revp->tags);
		free(revp);

		nol_iter_reset(iter);
	}
	nol_iter_destroy(iter);

	iter = nol_iter_create(rcsp->symbols);
	while ((nump = nol_iter_next(iter, &name, &namelen)) != NULL) {
		namedobjlist_removeitem(rcsp->symbols, name, namelen);
		numfree(nump);
		free(nump);

		nol_iter_reset(iter);
	}
	nol_iter_destroy(iter);

	iter = nol_iter_create(rcsp->revtags);
	while ((tlp = nol_iter_next(iter, &name, &namelen)) != NULL) {
		namedobjlist_removeitem(rcsp->revtags, name, namelen);
		textlist_destroy(tlp);

		nol_iter_reset(iter);
	}
	nol_iter_destroy(iter);

	iter = nol_iter_create(rcsp->branchhead);
	while ((revp = nol_iter_next(iter, &name, &namelen)) != NULL) {
		namedobjlist_removeitem(rcsp->branchhead, name, namelen);

		nol_iter_reset(iter);
	}
	nol_iter_destroy(iter);

	textlist_destroy(rcsp->access);
	namedobjlist_destroy(rcsp->symbols);
	namedobjlist_destroy(rcsp->revtags);
	namedobjlist_destroy(rcsp->branchhead);
	namedobjlist_destroy(rcsp->revs);
	namedobjlist_destroy(rcsp->revsbynum);

	if (munmap(rcsp->mapstart, rcsp->maplen) != 0)
		warn("rcsfile_free: munmap");
	free(rcsp->filename);

	free(rcsp);
}

/*
 * Like rcsfile_open, but try to find the real ,v file if we weren't
 * given one.
 */
struct rcsfile *
rcsfile_smartopen(const char *filename, char **branchp) {
	Strbuf *basename, *dirname, *rcsdir, *ftmp, *buf;
	int len, dlen;
	FILE *fp;
	char *p;
	struct rcsfile *rcsp;

	len = strlen(filename);
	if (len > 2 && strcmp(filename + len - 2, ",v") == 0)
		return rcsfile_open(filename);

	basename = sb_create();
	dirname = sb_create();
	rcsdir = sb_create();
	ftmp = sb_create();
	buf = sb_create();

	sb_printf(dirname, "%s", filename);
	if ((p = strrchr(sb_ptr(dirname), '/')) != NULL) {
		dlen = p - sb_ptr(dirname) + 1;
		sb_truncate(dirname, dlen);
		sb_printf(basename, "%s", filename + dlen);
	} else {
		dlen = 0;
		sb_reset(dirname);
		sb_printf(basename, "%s", filename);
	}

	sb_printf(ftmp, "%sCVS/Root", sb_ptr(dirname));
	if ((fp = fopen(sb_ptr(ftmp), "r")) != NULL) {
		if (sb_getline(fp, buf) == -1)
			errx(1, "%s: no Root info\n", sb_ptr(ftmp));
		if ((p = strchr(sb_ptr(buf), ':')) != NULL)
			p++;
		else
			p = sb_ptr(buf);
		sb_printf(rcsdir, "%s/", p);
		fclose(fp);
		
		sb_printf(ftmp, "%sCVS/Repository", sb_ptr(dirname));
		if ((fp = fopen(sb_ptr(ftmp), "r")) == NULL)
			err(1, "%s", sb_ptr(ftmp));
		if (sb_getline(fp, buf) == -1)
			errx(1, "%s: no Repository info\n", sb_ptr(ftmp));
		sb_appendf(rcsdir, "%s/", sb_ptr(buf));
		fclose(fp);

		sb_printf(ftmp, "%sCVS/Tag", sb_ptr(dirname));
		if (branchp != NULL && *branchp == NULL &&
		    (fp = fopen(sb_ptr(ftmp), "r")) != NULL) {
			sb_getline(fp, buf);
			if (sb_ptr(buf)[0] == 'T')
				*branchp = strdup(sb_ptr(buf) + 1);
			fclose(fp);
		} else if (branchp != NULL && *branchp == NULL)
			*branchp = "MAIN";
	} else
		sb_printf(rcsdir, "%sRCS/", sb_ptr(dirname));

	sb_printf(ftmp, "%s%s,v", sb_ptr(rcsdir), sb_ptr(basename));
	if ((fp = fopen(sb_ptr(ftmp), "r")) != NULL)
		fclose(fp);
	else {
		sb_printf(ftmp, "%sAttic/%s,v", sb_ptr(rcsdir),
		    sb_ptr(basename));
		if ((fp = fopen(sb_ptr(ftmp), "r")) != NULL)
			fclose(fp);
		else {
			/* Revert to the main name for the error message. */
			sb_printf(ftmp, "%s%s,v", sb_ptr(rcsdir),
			    sb_ptr(basename));
		}
	}

	rcsp = rcsfile_open(sb_ptr(ftmp));

	sb_free(basename);
	sb_free(dirname);
	sb_free(rcsdir);
	sb_free(ftmp);
	sb_free(buf);

	return rcsp;
}

void
rcsfile_setflags(struct rcsfile *rcsp, int flags) {
	rcsp->flags = flags;
}


static void
get_admin(struct parser *pp, struct rcsfile *rcsp) {
	struct token tok;
	int id;

	while (optional_tok(pp, &tok, TOKTYPE_ID)) {
		if ((id = id_lookup(&tok.value)) == ID_DESC) {
			puttok(pp, &tok);
			break;
		}
		switch (id) {
		case ID_HEAD:
			if (optional_tok(pp, &tok, TOKTYPE_NUM))
				rcsp->headrev = tok.value;
			break;
		case ID_BRANCH:
			if (optional_tok(pp, &tok, TOKTYPE_NUM))
				rcsp->branch = tok.value;
			break;
		case ID_ACCESS:
			while (optional_tok(pp, &tok, TOKTYPE_ID))
				textlist_add(rcsp->access, &tok.value);
			break;
		case ID_SYMBOLS:
			while (optional_tok(pp, &tok, TOKTYPE_ID)) {
				struct rcsnum *nump;
				struct rcstext symbol = tok.value;
			
				expect_tok(pp, &tok, TOKTYPE_COLON);
				expect_tok(pp, &tok, TOKTYPE_NUM);
#if 0			
				printf("symbol: '%.*s' -> '%.*s'\n",
				    symbol.len, symbol.start,
				    tok.value.len, tok.value.start);
#endif

				if (namedobjlist_lookup(rcsp->symbols,
				    symbol.start, symbol.len) != NULL) {
					warnx("Duplicate symbol '%.*s'",
					    symbol.len, symbol.start);
					continue;
				}

				nump = malloc(sizeof(*nump));
				numinit(nump);
				text2num(&tok.value, nump);
				namedobjlist_additem(rcsp->symbols,
				    symbol.start, symbol.len, nump);
			}
			break;
		case ID_LOCKS:
			while (optional_tok(pp, &tok, TOKTYPE_ID)) {
				expect_tok(pp, &tok, TOKTYPE_COLON);
				expect_tok(pp, &tok, TOKTYPE_NUM);
			}
			expect_tok(pp, &tok, TOKTYPE_SEMI);
			if (!optional_tok(pp, &tok, TOKTYPE_ID))
				continue;
			if (!txtequ(&tok.value, &id_strict)) {
				puttok(pp, &tok);
				continue;
			}
			break;
		case ID_COMMENT:
			if (optional_tok(pp, &tok, TOKTYPE_STRING))
				rcsp->comment = tok.value;
			break;
		case ID_EXPAND:
			if (optional_tok(pp, &tok, TOKTYPE_STRING))
				rcsp->expand = tok.value;
			break;
		default:
			printf("unknown: '%.*s'", tok.value.len,
			    tok.value.start);
			while (gettok(pp, &tok)) {
				if (tok.type != TOKTYPE_ID &&
				    tok.type != TOKTYPE_NUM &&
				    tok.type != TOKTYPE_STRING &&
				    tok.type != TOKTYPE_COLON)
					break;
				printf(" [%s]'%.*s'", tokname[tok.type],
				    tok.value.len, tok.value.start);
			}
			puttok(pp, &tok);
			printf("\n");
			break;
		}
		expect_tok(pp, &tok, TOKTYPE_SEMI);
	}

}

static void
get_deltas(struct parser *pp, struct rcsfile *rcsp) {
	struct token tok;
	struct revnode *revp;
	int id;

	while (optional_tok(pp, &tok, TOKTYPE_NUM)) {
		revp = malloc(sizeof(*revp));
		bzero(revp, sizeof(*revp));
		revp->rcsp = rcsp;
		revp->textlines = NULL;
		revp->outputlines = NULL;
		revp->olrefs = 0;
		revp->branchrevs = textlist_create();
		revp->branchpoints = textlist_create();
		revp->branches = textlist_create();
		revp->tags = textlist_create();

		revp->revtext = tok.value;
		text2num(&revp->revtext, &revp->rev);
		namedobjlist_additem(rcsp->revs, tok.value.start,
		    tok.value.len, revp);
		namedobjlist_additem(rcsp->revsbynum, revp->rev.num,
		    RCSNUM_BYTES(&revp->rev), revp);
		rcsp->nrevs++;

		while (optional_tok(pp, &tok, TOKTYPE_ID)) {
			if ((id = id_lookup(&tok.value)) == ID_DESC) {
				puttok(pp, &tok);
				break;
			}
			switch (id) {
			case ID_DATE:
				expect_tok(pp, &tok, TOKTYPE_NUM);
				text2num(&tok.value, &revp->date);
				if (revp->date.num[0] < 100)
					revp->date.num[0] += 1900;
				break;
			case ID_AUTHOR:
				expect_tok(pp, &tok, TOKTYPE_ID);
				revp->author = tok.value;
				break;
			case ID_STATE:
				if (optional_tok(pp, &tok, TOKTYPE_ID))
					revp->state = tok.value;
				break;
			case ID_BRANCHES:
				while (optional_tok(pp, &tok, TOKTYPE_NUM)) {
					textlist_add(revp->branchrevs,
					    &tok.value);
				}
				break;
			case ID_NEXT:
				if (optional_tok(pp, &tok, TOKTYPE_NUM))
					revp->patchnextrev = tok.value;
				break;
			default:
				printf("delta unknown: '%.*s'", tok.value.len,
				    tok.value.start);
				while (gettok(pp, &tok)) {
					if (tok.type != TOKTYPE_ID &&
					    tok.type != TOKTYPE_NUM &&
					    tok.type != TOKTYPE_STRING &&
					    tok.type != TOKTYPE_COLON)
						break;
					printf(" [%s]'%.*s'", tokname[tok.type],
					    tok.value.len, tok.value.start);
				}
				puttok(pp, &tok);
				printf("\n");
				break;
			}
			expect_tok(pp, &tok, TOKTYPE_SEMI);
		}
	}
}

static void
get_desc(struct parser *pp, struct rcsfile *rcsp) {
	struct token tok;

	expect_tok(pp, &tok, TOKTYPE_ID);
	if (id_lookup(&tok.value) != ID_DESC)
		errx(1, "missing 'desc'\n");
	expect_tok(pp, &tok, TOKTYPE_STRING);
	rcsp->desc = tok.value;
}

static void
get_deltatexts(struct parser *pp, struct rcsfile *rcsp) {
	struct token tok;
	struct revnode *revp;

	while (optional_tok(pp, &tok, TOKTYPE_NUM)) {
		revp = namedobjlist_lookup(rcsp->revs, tok.value.start,
		    tok.value.len);
		if (revp == NULL)
			errx(1, "rev %.*s not found", tok.value.len,
			    tok.value.start);

		while (optional_tok(pp, &tok, TOKTYPE_ID)) {
			switch (id_lookup(&tok.value)) {
			case ID_LOG:
				expect_tok(pp, &tok, TOKTYPE_STRING);
				revp->log = tok.value;
				break;
			case ID_TEXT:
				expect_tok(pp, &tok, TOKTYPE_STRING);
				revp->text = tok.value;
				break;
			default:
				printf("deltatext unknown: '%.*s'",
				    tok.value.len, tok.value.start);
				while (gettok(pp, &tok)) {
					if (tok.type != TOKTYPE_ID &&
					    tok.type != TOKTYPE_NUM &&
					    tok.type != TOKTYPE_STRING &&
					    tok.type != TOKTYPE_COLON)
						break;
					printf(" [%s]'%.*s'", tokname[tok.type],
					    tok.value.len, tok.value.start);
				}
				puttok(pp, &tok);
				printf("\n");
				break;
			}
		}
	}
}

static void
fixup_deltas(struct rcsfile *rcsp) {
	Namedobjlist_iter *iter;
	struct revnode *revp;
	struct rcsnum *nump;
	struct rcstext symb;

	iter = nol_iter_create(rcsp->revs);
	while ((revp = nol_iter_next(iter, NULL, NULL)) != NULL) {
		struct revnode *revp1;
		struct rcstext *textp;

		TEXTLIST_FOREACH(revp->branchrevs, textp) {
			revp1 = namedobjlist_lookup(rcsp->revs, textp->start,
			    textp->len);
			if (revp1 == NULL)
				errx(1,
				    "fixup_deltas: missing '%.*s' at '%.*s'",
				    textp->len, textp->start,
				    revp->revtext.len, revp->revtext.start);

			revp1->patchprev = revp;
		}

		if (revp->patchnextrev.start == NULL)
			continue;

		revp1 = namedobjlist_lookup(rcsp->revs,
		    revp->patchnextrev.start, revp->patchnextrev.len);
		if (revp1 == NULL) {
			printf("fixup_deltas: missing rev '%.*s' at '%.*s'\n",
			    revp->patchnextrev.len, revp->patchnextrev.start,
			    revp->revtext.len, revp->revtext.start);
			continue;
		}

		revp->patchnext = revp1;
		revp1->patchprev = revp;
	}

	nol_iter_reset(iter);
	while ((revp = nol_iter_next(iter, NULL, NULL)) != NULL) {
		if ((revp->patchnext != NULL && numcmp(&revp->patchnext->rev,
		    &revp->rev) < 0) || (revp->patchprev != NULL &&
		    numcmp(&revp->patchprev->rev, &revp->rev) > 0)) {
			revp->next = revp->patchprev;
			revp->prev = revp->patchnext;
		} else {
			revp->next = revp->patchnext;
			revp->prev = revp->patchprev;
		}
	}
	nol_iter_destroy(iter);

	rcsp->head = namedobjlist_lookup(rcsp->revs, rcsp->headrev.start,
	    rcsp->headrev.len);
	if (rcsp->head == NULL)
		errx(1,"head revision '%.*s' not found!\n", rcsp->headrev.len,
		    rcsp->headrev.start);

	iter = nol_iter_create(rcsp->symbols);
	while ((nump = nol_iter_next(iter, (void **)&symb.start,
	    &symb.len)) != NULL) {
		struct rcsnum num;
		struct rcstext *textp;
		struct textlist *tlp;
		int found;

		tlp = namedobjlist_lookup(rcsp->revtags, nump->num,
		     RCSNUM_BYTES(nump));
		if (tlp == NULL) {
			tlp = textlist_create();
			namedobjlist_additem(rcsp->revtags, nump->num,
			    RCSNUM_BYTES(nump), tlp);
		}
		textlist_add(tlp, &symb);

		if (nump->len < 3 || nump->num[nump->len - 2]) {
			/* Not a branch symbol */
			if ((revp = namedobjlist_lookup(rcsp->revsbynum,
			    nump->num, RCSNUM_BYTES(nump))) != NULL)
				textlist_add(revp->tags, &symb);
			continue;
		}

		/* symb is a branch symbol */
		numcpy(nump, &num);
		num.len -= 2;

		revp = namedobjlist_lookup(rcsp->revsbynum, num.num,
		    RCSNUM_BYTES(&num));
		if (revp == NULL)
			errx(1, "base revision for '%.*s' not found",
			    symb.len, symb.start);

		num.num[num.len] = num.num[num.len + 1];
		num.len++;

		textlist_add(revp->branchpoints, &symb);

		found = 0;
		TEXTLIST_FOREACH(revp->branchrevs, textp) {
			struct rcsnum brnum;
	
			numinit(&brnum);
			text2num(textp, &brnum);
			if (brnum.len == num.len +1 && bcmp(brnum.num,
			    num.num, RCSNUM_BYTES(&num)) == 0) {
				numfree(&brnum);
				found = 1;
				break;
			}
			numfree(&brnum);
		}

		if (found) {
			revp = namedobjlist_lookup(rcsp->revs, textp->start,
			    textp->len);
			if (revp == NULL)
				errx(1, "branch '%.*s': rev '%.*s' not found",
				    symb.len, symb.start, textp->len,
				    textp->start);

			while (revp->next != NULL) {
				textlist_add(revp->branches, &symb);
				revp = revp->next;
			}
			textlist_add(revp->branches, &symb);
		}

		namedobjlist_additem(rcsp->branchhead, symb.start, symb.len,
		    revp);
	}
	nol_iter_destroy(iter);
}

struct revnode **
revlist(struct rcsfile *rcsp, char *branch) {
	struct revnode **list, *revp;
	int i = 0;

	list = calloc(rcsp->nrevs + 1, sizeof(*list));

	if (branch == NULL || strcmp(branch, "ALL") == 0) {
		Namedobjlist_iter *iter;

		iter = nol_iter_create(rcsp->revs);
		while ((revp = nol_iter_next(iter, NULL, NULL)) != NULL)
			list[i++] = revp;
		nol_iter_destroy(iter);

		qsort(list, rcsp->nrevs, sizeof(*list), revbydate);
		return list;
	}

	if (strcmp(branch, "MAIN") == 0 || strcmp(branch, "HEAD") == 0)
		revp = rcsp->head;
	else {
		revp = namedobjlist_lookup(rcsp->branchhead, branch,
		    strlen(branch));
		if (revp == NULL) {
			free(list);
			return NULL;
		}
	}

	while (revp != NULL) {
		if (i == rcsp->nrevs)
			abort();
		list[i++] = revp;
		revp = revp->prev;
	}
	
	return list;
}

void
rev_calc(struct revnode *revp) {
	struct rcstext *textp;
	struct rcspatch *pp;
	struct rcspatch_op *opp;
	int i;

	if (revp->outputlines != NULL)
		return;

	if (revp->textlines == NULL)
		revp->textlines = textsplit(&revp->text);
	revp->outputlines = textlist_create();

	if ((pp = makepatch(revp)) == NULL) {
		TEXTLIST_FOREACH(revp->textlines, textp)
			textlist_add(revp->outputlines, textp);
		return;
	}

	for (opp = pp->op; opp < &pp->op[pp->len]; opp++) {
		if (opp->op == RPOP_DEL)
			continue;

		for (i = 0; i < opp->len; i++)
			textlist_add(revp->outputlines, &opp->textp[i]);
	}
	patch_destroy(pp);
}

void
rev_diff(struct revnode *revp, int ctx, int reverse) {
	struct revnode *rp;
	struct rcstext *textp;
	struct rcspatch *pp;
	struct rcspatch_op *opp;
	int chunkend;
	int i;

	if (revp->prev == NULL) {
		if (revp->outputlines == NULL)
			rev_calc(revp);
		TEXTLIST_FOREACH(revp->outputlines, textp)
			textprint(textp);
		return;
	}

	if (revp->patchprev != revp->prev) {
		reverse = !reverse;
		revp = revp->prev;
	}

	rev_calc(revp);

	pp = makepatch(revp);
	if (reverse)
		reversepatch(pp);

	rp = reverse ? revp : revp->patchprev;
	printf("--- %.*s\t%d/%02d/%02d %02d:%02d:%02d\t%.*s\n",
	    rp->rcsp->shortfname.len, rp->rcsp->shortfname.start,
	    rp->date.num[0], rp->date.num[1], rp->date.num[2],
	    rp->date.num[3], rp->date.num[4], rp->date.num[5],
	    rp->revtext.len, rp->revtext.start);
	rp = reverse ? revp->patchprev : revp;
	printf("+++ %.*s\t%d/%02d/%02d %02d:%02d:%02d\t%.*s\n",
	    rp->rcsp->shortfname.len, rp->rcsp->shortfname.start,
	    rp->date.num[0], rp->date.num[1], rp->date.num[2],
	    rp->date.num[3], rp->date.num[4], rp->date.num[5],
	    rp->revtext.len, rp->revtext.start);



	chunkend = 0;
	for (opp = pp->op; opp < &pp->op[pp->len]; opp++) {
		int cstart, ocount, coff;
		struct rcspatch_op *copp;

		/* Deal with the simple cases */
		if (opp->op == RPOP_ADD || opp->op == RPOP_DEL ||
		    opp->line + opp->len < chunkend) {
			patch_printop(opp, opp->op == RPOP_ADD ? "+" :
			    opp->op == RPOP_DEL ? "-" : " ");
			continue;
		}

		/* End the current chunk */
		for (i = 0; i + opp->line < chunkend; i++) {
			fputc(' ', stdout);
			textprint(&opp->textp[i]);
		}

		/* Check for end of patch */
		if (opp - pp->op == pp->len - 1)
			continue;

		/*
		 * Now we know that we need a new chunk. Scan forward
		 * to find its length.
		 */
		coff = (opp->len < ctx) ? 0 : opp->len - ctx;
		cstart = opp->line + coff;
		chunkend = opp->line + opp->len;
		ocount = chunkend - cstart;
		
		for (copp = opp + 1; copp < &pp->op[pp->len]; copp++) {
			if (copp->op == RPOP_ADD || copp->op == RPOP_DEL ||
			    copp->len <= ctx || (copp - pp->op < pp->len - 1 &&
			    copp->len <= ctx * 2)) {
				if (copp->op != RPOP_DEL)
					ocount += copp->len;
				if (copp->op != RPOP_ADD)
					chunkend += copp->len;
				continue;
			}

			chunkend += ctx;
			ocount += ctx;
			break;
		}

		/* Start the new chunk */
		printf("@@ -%d,%d +%d,%d @@\n", cstart + 1, chunkend - cstart,
		    opp->nline + coff + 1, ocount);
		for (i = coff; i < opp->len; i++) {
			fputc(' ', stdout);
			textprint(&opp->textp[i]);
		}
	}
	patch_destroy(pp);
}

void
rev_addref(struct revnode *revp) {
	revp->olrefs++;
}

void
rev_remref(struct revnode *revp) {
	if (--revp->olrefs < 0)
		abort();
	if (!(revp->rcsp->flags & RCSFILE_LOWMEM) || revp->olrefs != 0)
		return;
	if (revp->outputlines == NULL || revp->prev == NULL)
		return;

	/*
	 * Removed the now-unused text, but leave a small
	 * proportion (1/16) of texts so that future lookups
	 * won't have to go right back to the start.
	 */
	if ((((int)revp * 17702227) & 0xf00) == 0)
		return;
	textlist_destroy(revp->outputlines);
	revp->outputlines = NULL;
}

static void
patch_printop(struct rcspatch_op *opp, char *prefix) {
	int i;

	for (i = 0; i < opp->len; i++) {
		printf("%s", prefix);
		textprint(&opp->textp[i]);
	}
}

static void
reversepatch(struct rcspatch *pp) {
	struct rcspatch_op *opp;
	int i, l;

	for (i = 0; i < pp->len; i++) {
		opp = &pp->op[i];
		if (opp->op == RPOP_DEL && i + 1 < pp->len &&
		    opp[1].op == RPOP_ADD &&
		    opp->nline >= opp[1].nline &&
		    opp->nline < opp[1].nline + opp[1].len) {
			struct rcspatch_op ptmp;

			ptmp = opp[0];
			opp[0] = opp[1];
			opp[1] = ptmp;
		}

		switch (opp->op) {
		case RPOP_DEL:
			opp->op = RPOP_ADD;
			break;
		case RPOP_ADD:
			opp->op = RPOP_DEL;
			break;
		default:
			break;
		}
		l = opp->nline;
		opp->nline = opp->line;
		opp->line = l;
	}
}

static struct rcspatch *
makepatch(struct revnode *revp) {
	struct rcstext *textp;
	struct textlist *plist;
	struct rcspatch *pp;
	int nline, oline;

	if (revp->patchprev == NULL)
		return NULL;

	rev_addref(revp->patchprev);
	rev_addref(revp);
	rev_calc(revp->patchprev);
	plist = revp->patchprev->outputlines;
	pp = patch_create();
	pp->oldnode = revp->patchprev;
	pp->newnode = revp;

	oline = 0;
	nline = 0;
	TEXTLIST_FOREACH(revp->textlines, textp) {
		const char *p = textp->start;
		char op;
		int arg1, arg2;

		/* XXX check for parse errors */
		op = *p++;
		arg1 = strtoul(p, (char **)&p, 10) - 1;
		arg2 = strtoul(p, (char **)&p, 10);
		
		/* Convert 'insert-after' semantics to 'insert-before' */
		if (op == 'a' && oline <= arg1)
			arg1++;

		if (oline < arg1) {
			patch_add(pp, RPOP_COPY, oline, nline, arg1 - oline,
			    &plist->list[oline]);
			nline += arg1 - oline;
			oline += arg1 - oline;
			
			if (oline > plist->len || textp >
			    &revp->textlines->list[revp->textlines->len])
				abort();
		}

		/* Always start a patch with a RPOP_COPY section */
		if (oline == 0)
			patch_add(pp, RPOP_COPY, 0, 0, 0, &plist->list[0]);

		switch(op) {
		case 'd':
			patch_add(pp, RPOP_DEL, arg1, nline, arg2,
			    &plist->list[arg1]);
			oline += arg2;
			if (oline > plist->len)
				abort();
			break;
		case 'a':
			patch_add(pp, RPOP_ADD, arg1, nline, arg2, textp + 1);
			textp += arg2;
			nline += arg2;
			if (oline > plist->len || textp >
			    &revp->textlines->list[revp->textlines->len])
				abort();
			break;
		}
	}
	/* Add a final RPOP_COPY section, even if it has zero lines */
	patch_add(pp, RPOP_COPY, oline, nline, plist->len - oline,
	    &plist->list[oline]);
	return pp;
}

static struct rcspatch *
patch_create() {
	struct rcspatch *pp;

	pp = malloc(sizeof(*pp));
	pp->oldnode = pp->newnode = NULL;
	pp->op = NULL;
	pp->len = 0;
	pp->op_len = 0;

	return pp;
}

static void
patch_destroy(struct rcspatch *pp) {
	if (pp->op != NULL)
		free(pp->op);
	if (pp->oldnode != NULL)
		rev_remref(pp->oldnode);
	if (pp->newnode != NULL)
		rev_remref(pp->newnode);
	free(pp);
}

static void
patch_add(struct rcspatch *pp, int op, int line, int nline, int len,
    struct rcstext *textp) {
	struct rcspatch_op *opp;

	if (pp->len == pp->op_len) {
		pp->op_len += pp->op_len + 1;
		pp->op = realloc(pp->op, pp->op_len * sizeof(*pp->op));
	}

	opp = &pp->op[pp->len++];
	opp->op = op;
	opp->line = line;
	opp->nline = nline;
	opp->len = len;
	opp->textp = textp;
}

static int
id_lookup(struct rcstext *id) {
	if (id->start == NULL)
		return ID_NONE;
	switch (*id->start) {
	case 'a':
		if (txtequ(id, &id_access))
			return ID_ACCESS;
		if (txtequ(id, &id_author))
			return ID_AUTHOR;
		break;
	case 'b':
		if (txtequ(id, &id_branch))
			return ID_BRANCH;
		if (txtequ(id, &id_branches))
			return ID_BRANCHES;
		break;
	case 'c':
		if (txtequ(id, &id_comment))
			return ID_COMMENT;
		break;
	case 'd':
		if (txtequ(id, &id_date))
			return ID_DATE;
		if (txtequ(id, &id_desc))
			return ID_DESC;
		break;
	case 'e':
		if (txtequ(id, &id_expand))
			return ID_EXPAND;
		break;
	case 'h':
		if (txtequ(id, &id_head))
			return ID_HEAD;
		break;
	case 'l':
		if (txtequ(id, &id_locks))
			return ID_LOCKS;
		if (txtequ(id, &id_log))
			return ID_LOG;
		break;
	case 'n':
		if (txtequ(id, &id_next))
			return ID_NEXT;
		break;
	case 's':
		if (txtequ(id, &id_state))
			return ID_STATE;
		if (txtequ(id, &id_strict))
			return ID_STRICT;
		if (txtequ(id, &id_symbols))
			return ID_SYMBOLS;
		break;
	case 't':
		if (txtequ(id, &id_text))
			return ID_TEXT;
		break;
	}
	return ID_NONE;
}


static int
optional_tok(struct parser *pp, struct token *tokp, int type) {
	if (!gettok(pp, tokp))
		return 0;
	if (tokp->type != type) {
		puttok(pp, tokp);
		return 0;
	}
	return 1;
}

static void
expect_tok(struct parser *pp, struct token *tokp, int type) {
	if (!gettok(pp, tokp))
		errx(1, "expect_tok(%s): EOF", tokname[type]);
	if (tokp->type != type)
		errx(1, "expect_tok(%s): got %s['%.*s']", tokname[type],
		    tokname[tokp->type], tokp->value.len, tokp->value.start);
}
	
void
puttok(struct parser *pp, struct token *tokp) {
	if (pp->saved.type != TOKTYPE_NONE)
		abort();
	pp->saved = *tokp;
}

static int
gettok(struct parser *pp, struct token *tokp) {
	char *p, *end;
	const char *p1;

	if (pp->saved.type != TOKTYPE_NONE) {
		*tokp = pp->saved;
		pp->saved.type = TOKTYPE_NONE;
		return 1;
	}

	tokp->type = TOKTYPE_NONE;
	p = pp->pos;
	end = pp->end;

	while (p < end) {
		switch (*p) {
		case ' ': case '\b': case '\t': case '\n':
		case '\002': case '\f': case '\r':
			p++;
			continue;

		case '@':
			p++;
			tokp->value.start = p;

			for (;;) {
				if ((p = memchr(p, '@', end - p)) == NULL)
					errx(1, "no matching '@'");
				if (p + 1 < end && p[1] == '@') {
					p += 2;
					continue;
				}
				tokp->value.len = p - tokp->value.start;
				tokp->type = TOKTYPE_STRING;
				p++;
				break;
			}
			goto done;

		case ':':
			tokp->value.start = NULL;
			tokp->type = TOKTYPE_COLON;
			p++;
			goto done;
			
		case ';':
			tokp->value.start = NULL;
			tokp->type = TOKTYPE_SEMI;
			p++;
			goto done;
		}

		tokp->value.start = p;
		while (p < end) {
			switch (*p) {
			default:
				p++;
				continue;

			case ' ': case '\b': case '\t': case '\n':
			case '\002': case '\f': case '\r':
			case ':': case ';':
				break;
			}
			break;
		}
		tokp->value.len = p - tokp->value.start;

		tokp->type = TOKTYPE_NUM;
		for (p1 = tokp->value.start; p1 < p; p1++) {
			if ((*p1 < '0' || *p1 > '9') && *p1 != '.') {
				tokp->type = TOKTYPE_ID;
				break;
			}
		}
		goto done;
	}
		
done:
	pp->pos = p;
	return (tokp->type != TOKTYPE_NONE);
}

int
revbydate(const void *v1, const void *v2) {
	struct revnode *revp1 = *(struct revnode **)v1;
	struct revnode *revp2 = *(struct revnode **)v2;
	int ret;

	ret = -numcmp(&revp1->date, &revp2->date);
	if (ret != 0)
		return ret;
	ret = strcmp(revp1->rcsp->filename, revp2->rcsp->filename);
	if (ret != 0)
		return (ret < 0) ? -1 : 1;
	return 0;
}
