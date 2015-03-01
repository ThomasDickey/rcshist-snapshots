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
 * $Id: rcshist.c,v 1.13 2015/03/01 14:26:14 tom Exp $
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "namedobjlist.h"
#include "rcsfile.h"
#include "misc.h"

void filelist_expand(char ***filelistp, int *nfilesp);
int filelist_ftscmp(const FTSENT * *fe1, const FTSENT * *fe2);
void prrev(struct revnode *revp);
void prlist(const char *prefix, struct textlist *tlp);
void prlog(struct revnode *revp);
void onerev(char *filename, char *revame);
void usage(void);

char *progname;
int mflag;

void
usage() {
	fprintf(stderr,
	    "Usage: %s [-mR] [-r<branch|MAIN|ALL>] <filename> ...\n"
	    "       %s -L<revision> <filename>\n",
	    progname, progname);
	exit(1);
}

int
main(int argc, char **argv) {
	struct rcsfile **rcsp;
	int ch, i, nfiles;
	char *branch = NULL;
	char *revname = NULL;
	char **filelist;
	struct revnode **rlist, **rltmp;
	int Rflag, rnum, rlist_len;

	progname = argv[0];
	Rflag = 0;
	while ((ch = getopt(argc, argv, "L:mr:R")) != -1) {
		switch (ch) {
		case 'L':
			revname = optarg;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'r':
			branch = optarg;
			break;
		case 'R':
			Rflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	nfiles = argc;
	filelist = argv;

	if (revname != NULL) {
		onerev(filelist[0], revname);
		exit(0);
	}

	if (Rflag)
		filelist_expand(&filelist, &nfiles);

	rlist = NULL;
	rnum = 0;
	rlist_len = 0;

	rcsp = malloc((size_t) nfiles * sizeof(*rcsp));
	for (i = 0; i < nfiles; i++) {
		struct revnode **rpp;

		if ((rcsp[i] = rcsfile_smartopen(filelist[i], &branch)) == NULL)
			continue;
		if (mflag)
			rcsfile_setflags(rcsp[i], RCSFILE_LOWMEM);

		if ((rltmp = revlist(rcsp[i], branch)) == NULL) {
			warnx("%s: %s: no such branch\n", filelist[i],
			    argv[2]);
			continue;
		}
		
		for (rpp = rltmp; *rpp != NULL; rpp++) {
			if (rnum == rlist_len) {
				rlist_len += rlist_len + 1;
				rlist = realloc(rlist, (size_t) rlist_len *
				    sizeof(*rlist));
			}
			rlist[rnum++] = *rpp;
		}
		free(rltmp);
	}

	qsort(rlist, (size_t) rnum, sizeof(*rlist), revbydate);
	for (i = 0; i < rnum; i++)
		prrev(rlist[i]);
	free(rlist);

	for (i = 0; i < nfiles; i++)
		if (rcsp[i] != NULL)
			rcsfile_free(rcsp[i]);

	return 0;
}

void
prrev(struct revnode *revp) {

	printf("REV:%-20.*s%-20.*s%d/%02d/%02d %02d:%02d:%02d       %.*s\n",
	    revp->revtext.len, revp->revtext.start,
	    revp->rcsp->shortfname.len, revp->rcsp->shortfname.start,
	    revp->date.num[0], revp->date.num[1], revp->date.num[2],
	    revp->date.num[3], revp->date.num[4], revp->date.num[5],
	    revp->author.len, revp->author.start);

	prlist("branchpoints:", revp->branchpoints);
	prlist("branches:    ", revp->branches);
	prlist("tags:        ", revp->tags);

	printf("\n");
	prlog(revp);
	printf("\n");
	rev_calc(revp);

#if 0
	TEXTLIST_FOREACH(revp->outputlines, textp)
		printf("%.*s", textp->len, textp->start);
#endif
	rev_diff(revp, 3, 0);
}

void
onerev(char *filename, char *revname) {
	struct rcsfile *rcsp;
	struct revnode *revp;

	if ((rcsp = rcsfile_open(filename)) == NULL)
		err(1, "%s: rcsfile_open", filename);

	revp = namedobjlist_lookup(rcsp->revs, revname, (int) strlen(revname));
	if (revp == NULL)
		errx(1, "%s: %s: revision not found", filename, revname);

	prlist("branchpoints:", revp->branchpoints);
	prlist("branches:    ", revp->branches);
	prlist("tags:        ", revp->tags);
	rev_diff(revp, 3, 0);
}

void
prlist(const char *prefix, struct textlist *tlp) {
	struct rcstext *textp;
	int len = 0;
	int prefixlen = (int) strlen(prefix) + 4;

	if (tlp->len == 0)
		return;
	
	TEXTLIST_FOREACH(tlp, textp) {
		if (len == 0 || len + textp->len > 75) {
			if (len == 0)
				printf("%-*s", prefixlen, prefix);
			else
				printf(",\n%-*s", prefixlen, "");
			len = prefixlen;
		}
		if (len > prefixlen) {
			printf(", ");
			len += 2;
		}
		printf("%.*s", textp->len, textp->start);
		len += textp->len;
	}
	printf("\n");
}

void
prlog(struct revnode *revp) {
	struct rcstext *textp;
	struct textlist *tlp = textsplit(&revp->log);

	TEXTLIST_FOREACH(tlp, textp) {
		if (textp->len != 1 || textp->start[0] != '\n')
			printf("   ");
		textprint(textp);
	}

	textlist_destroy(tlp);
}

void
filelist_expand(char ***filelistp, int *nfilesp)
{
	FTS *fts;
	FTSENT *fe;
	char **newlist;
	int newlist_arraysize, nfiles;

	fts = fts_open(*filelistp, FTS_PHYSICAL, filelist_ftscmp);
	if (fts == NULL)
		err(1, "ftsopen");

	newlist = malloc(sizeof(*newlist));
	if (newlist == NULL)
		err(1, "realloc");
	newlist_arraysize = 1;
	nfiles = 0;
	while ((fe = fts_read(fts)) != NULL) {
		switch (fe->fts_info) {
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			warnx("%s: %s", fe->fts_path, strerror(fe->fts_errno));
			continue;
		case FTS_F:
			break;
		default:
			continue;
		}

		if (nfiles + 1 == newlist_arraysize) {
			newlist_arraysize += newlist_arraysize + 1;
			newlist = realloc(newlist, (size_t) newlist_arraysize *
			    sizeof(*newlist));
			if (newlist == NULL)
				err(1, "realloc");
		}
		newlist[nfiles] = strdup(fe->fts_path);
		if (newlist[nfiles] == NULL)
			err(1, "strdup");
		nfiles++;
	}
	if (fts_close(fts) != 0)
		err(1, "fts_close");

	newlist[nfiles] = NULL;
	*filelistp = newlist;
	*nfilesp = nfiles;
}

int
filelist_ftscmp(const FTSENT * *fe1, const FTSENT * *fe2)
{
	return (strcoll((*fe1)->fts_name, (*fe2)->fts_name));
}
