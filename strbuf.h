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
 * $Id: strbuf.h,v 1.1 2001/05/20 14:51:33 iedowse Exp $
 */
#ifndef STRBUF_H
#define STRBUF_H

#include <stdarg.h>
#include <stdio.h>

struct strbuf {
	char *buf;
	int pos;	/* index of '\0' at end of buf */
	int buflen;
};
typedef struct strbuf Strbuf;
int sb_getline(FILE *fp, Strbuf *sb);


int sb_printf(Strbuf *buf, const char *fmt, ...);
int sb_vprintf(Strbuf *buf, const char *fmt, va_list args);
int sb_appendf(Strbuf *buf, const char *fmt, ...);
int sb_vappendf(Strbuf *buf, const char *fmt, va_list args);

Strbuf *sb_create(void);
void sb_reset(Strbuf *sb);
void sb_free(Strbuf *sb);
char *sb_ptr(Strbuf *sb);
char *sb_detach(Strbuf *sb);
int sb_len(Strbuf *sb);
void sb_appendstr(Strbuf *sb, const char *string);
void sb_appendbytes(struct strbuf *sb, const char *str, int len);
void sb_appendchar(Strbuf *sb, char c);
void sb_move(struct strbuf *src, struct strbuf *dest);
void sb_truncate(struct strbuf *sb, int len);


#endif
