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
 * $Id: strbuf.c,v 1.3 2004/05/30 11:21:40 iedowse Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "strbuf.h"

/* Code for handling variable length strings.
 */

#define STRBUF_MIN 10
static void sb_pullupto(struct strbuf *sb, int len);

struct strbuf *sb_create() {
	struct strbuf *sb = malloc(sizeof(*sb));

	sb->buf = NULL;
	sb->pos = 0;
	sb->buflen = 0;

	return sb;
}

/* Reset the string to zero length (don't actually free anything) */
void sb_reset(struct strbuf *sb) {
	sb->pos = 0;
}

void sb_free(struct strbuf *sb) {
	if (sb->buf)
		free(sb->buf);
	free(sb);
}

/*
 * Return a pointer to the string for printing/copying etc.
 */
char *sb_ptr(struct strbuf *sb) {
	return sb->pos == 0 ? "" : sb->buf;
}

int sb_len(struct strbuf *sb) {
	return sb->pos;
}

/*
 * Detach the internal malloc'd string from the strbuf, and return it.
 * If the string was empty return a malloc'd copy of "".
 */
char *sb_detach(struct strbuf *sb) {
	char *ret;

	/*
	 * If the string length is 0 we just return a strdup'd ""  and
	 * leave everything else intact. 
	 */
	if (sb->pos == 0)
		return strdup("");

	ret = sb->buf;
	sb->buf = NULL;
	sb->pos = 0;
	sb->buflen = 0;

	return ret;
}

/* Extend the buffer in sb to be at least len bytes */
static void 
sb_pullupto(struct strbuf *sb, int len) {
	if (len <= sb->buflen)
		return;

	if (sb->buf)
		/* Expand to 2*orig + extra */
		sb->buflen += len;
	else
		sb->buflen = STRBUF_MIN + len;

	sb->buf = realloc(sb->buf, sb->buflen);
}

void sb_appendstr(struct strbuf *sb, const char *string) {
	int len = strlen(string);

	sb_pullupto(sb, len + sb->pos + 1);
	bcopy(string, &sb->buf[sb->pos], len + 1);
	sb->pos += len;
}

void sb_appendbytes(struct strbuf *sb, const char *str, int len) {
	sb_pullupto(sb, len + sb->pos + 1);
	bcopy(str, &sb->buf[sb->pos], len);
	sb->pos += len;
	sb->buf[sb->pos] = '\0';
}

void sb_appendchar(struct strbuf *sb, char c) {
	sb_pullupto(sb, sb->pos + 2);
	sb->buf[sb->pos++] = c;
	sb->buf[sb->pos] = '\0';
}

/* Move the string from src to dest */
void sb_move(struct strbuf *src, struct strbuf *dest) {
	if (dest->buf)
		free(dest->buf);
	dest->buf = src->buf;
	dest->pos = src->pos;
	dest->buflen = src->buflen;
	src->buf = NULL;
	src->pos = src->buflen = 0;
}

/* Truncate the string to len characters */
void sb_truncate(struct strbuf *sb, int len) {
	if (len < 0 || len > sb->pos)
		abort();
	sb->pos = len;
	if (sb->buf)
		sb->buf[sb->pos] = '\0';
}

/* Read a line from the given stream into sb.
 * Returns the number of bytes read from the stream, or -1 on error.
 * sb will not contain the last '\n', but it will be counted in the return
 * value.
 */
int sb_getline(FILE *fp, Strbuf *sb) {
	int cnt = 0;
	int c;

	sb_reset(sb);

	while ((c = getc(fp)) != EOF && c != '\n')
		sb_appendchar(sb, c);

	cnt = sb_len(sb);
	/* Count the '\n', but don't add it to the string */
	if (c == '\n')
		cnt++;
	else if (ferror(fp))
		return -1;

	return cnt;
}

/* 
 * sb_{v,}{print,append}f functions. Write (or append) to a Strbuf
 * using printf-style format + arg lists.
 */

#define FBUF_LEN 60

#define FLG_ALT		0x0001
#define FLG_LADJ	0x0002
#define FLG_ZPAD	0x0004
#define FLG_SHORT	0x0008
#define FLG_LONG	0x0010
#define FLG_HEX		0x0020
#define FLG_NUM		0x0040

#define ch2digit(c) ((c) - '0')
#define digit2ch(d) ((d) + '0')

int sb_printf(Strbuf *buf, const char *fmt, ...) {
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = sb_vprintf(buf, fmt, ap);
	va_end(ap);

	return retval;
}
	
int sb_appendf(Strbuf *buf, const char *fmt, ...) {
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = sb_vappendf(buf, fmt, ap);
	va_end(ap);

	return retval;
}

int sb_vprintf(Strbuf *buf, const char *fmt, va_list args) {
	sb_reset(buf);
	return sb_vappendf(buf, fmt, args);
}
	
/* #define PRINTF_CHECKUP */
int sb_vappendf(Strbuf *buf, const char *fmt, va_list args) {

	va_list ap;
#ifdef PRINTF_CHECKUP
	char *oldfmt = fmt;
#endif
	char *p;
	int i, n;
	int base;
	int flags;
	int width;
	int prec;
	char sign;
	char *fieldp;
	int flen;
	int oldlen;
	char tmpbuf[FBUF_LEN + 1];
	unsigned long ul_arg = 0;

	tmpbuf[FBUF_LEN] = '\0';

	va_copy(ap, args);
	if (fmt == NULL) {
		fprintf(stderr, "sb_vappendf: NULL format arg!\n");
		abort();
	}
	
	if (buf == NULL) {
		Strbuf *tmpbuf;
		fprintf(stderr, "sb_vappendf: NULL buf arg!\n");
		tmpbuf = sb_create();
		if (tmpbuf != NULL) {
			sb_vappendf(tmpbuf, fmt, args);
			fprintf(stderr, "Output was '%s'\n", sb_ptr(tmpbuf));
			sb_free(tmpbuf);
		}
		abort();
	}

	oldlen = sb_len(buf);
	for (; *fmt; fmt++) {

		if ((p = index(fmt, '%'))) {
			sb_appendbytes(buf, fmt, p - fmt);
			fmt += p - fmt;
			if (fmt[1])
				fmt++;
		} else {
			sb_appendstr(buf, fmt);
			fmt += strlen(fmt);
			break;
		}

		sign = '\0';
		flags = width = 0;
		prec = flen = -1;

		for (;; fmt++) {
			switch (*fmt) {
			case '#':
				flags |= FLG_ALT;
				continue;
			case '-':
				flags |= FLG_LADJ;
				continue;
			case '+':
				sign = '+';
				continue;
			case ' ':
				if (!sign)
					sign = ' ';
				continue;
			case '0':
				flags |= FLG_ZPAD;
				continue;
			}
			break;
		}
		if (*fmt == '*') {
			fmt++;
			i = va_arg(ap, int);
			width = abs(i);
			if (i<0)
				flags |= FLG_LADJ;
		} else
			for (; isdigit(*fmt); fmt++)
				width = 10 * width + ch2digit(*fmt);

		if (*fmt == '.') {
			if (*++fmt == '*') {
				fmt++;
				i = va_arg(ap, int);
				prec = i >= 0 ? i : -1;
			} else
				for (prec = 0, p++; isdigit(*fmt); fmt++)
					prec = 10 * prec + ch2digit(*fmt);
		}

		for (;; fmt++) {	
			switch (*fmt) {
			case 'h':
				flags |= FLG_SHORT;
				continue;
			case 'l':
				flags |= FLG_LONG;
				continue;
			}
			break;
		}

		base = 10;
		fieldp = tmpbuf + FBUF_LEN;

		switch (*fmt) {
		case 'c':
			*--fieldp = va_arg(ap, int);
			break;

		case 's':
			fieldp = va_arg(ap, char *);
			if (!fieldp)
				fieldp = "(null)";
			if (prec >= 0)
				flen = (p = memchr(fieldp, '\0', prec)) ?
				    p - fieldp : prec;
			else
				flen = strlen(fieldp);
			break;
		
		case 'p':
			flags |= FLG_HEX;
		case 'x':
		case 'X':
			base = 16;
			goto unum;
			
		case 'd':
		case 'i':
			if (flags & FLG_LONG)
				ul_arg = va_arg(ap, long);
			else if (flags & FLG_SHORT)
				ul_arg = (short)va_arg(ap, int);
			else 
				ul_arg = va_arg(ap, int);
			if ((long)ul_arg < 0) {
				sign = '-';
				ul_arg = -ul_arg;
			}

			goto donum;

		case 'o':
			base = 8;
			/* FALLTHROUGH */
		case 'u':
unum:
			if (flags & FLG_LONG)
				ul_arg = va_arg(ap, unsigned long);
			else if (flags & FLG_SHORT)
				ul_arg = (unsigned short)va_arg(ap,unsigned);
			else
				ul_arg = va_arg(ap, unsigned);

			sign = '\0';
			/* FALLTHROUGH */

donum:
			flags |= FLG_NUM;
			switch (base) {
			case 8:
				do {	
					*--fieldp = digit2ch(ul_arg & 7);
					ul_arg = ul_arg >> 3;
				} while(ul_arg);
				if (flags & FLG_ALT && *fieldp != '0')
					*--fieldp = '0';
				break;
			case 10:
				do {
					*--fieldp = digit2ch(ul_arg % 10);
					ul_arg = ul_arg / 10;
				} while(ul_arg);
				break;
			case 16:
				if (flags & FLG_ALT)
					flags |= FLG_HEX;
				i = (*fmt == 'p' ? 'x' : *fmt) - 'X' + 'A' - 10;
				do {
					*--fieldp = ((ul_arg & 15) < 10) ? 
					    digit2ch(ul_arg & 15) :
					    ((ul_arg & 15) + i);
					ul_arg = ul_arg >> 4;
				} while(ul_arg);
				break;
			}
			break;

		case '%':
			*--fieldp = '%';
			break;

		default:
			/* Not standard behaviour, but we want to know when
			 * this happens */
			fprintf(stdout, "Unknown format char '%c'\n", *fmt);
			abort();
		}
		if (flen == -1)
			flen = tmpbuf + FBUF_LEN - fieldp;
		n = (sign ? 1 : 0) + ((flags & FLG_HEX) ? 2 : 0) +
		    (prec > flen ? prec : flen);

		if ((flags & FLG_NUM) && prec >= 0)
			flags &= ~FLG_ZPAD;

		if (!(flags & (FLG_LADJ | FLG_ZPAD)))
			for (i = 0; i < width - n; i++)
				sb_appendchar(buf, ' ');

		if (sign)
			sb_appendchar(buf, sign);
		else if (flags & FLG_HEX)
			sb_appendstr(buf, *fmt == 'X' ? "OX" : "0x");


		if ((flags & (FLG_LADJ | FLG_ZPAD)) == FLG_ZPAD)
			for (i = 0; i < width - n; i++)
				sb_appendchar(buf, '0');

		if (flags & FLG_NUM)
			for (i = 0; i < prec - flen; i++)
				sb_appendchar(buf, '0');

		sb_appendbytes(buf, fieldp, flen);

		if (flags & FLG_LADJ)
			for (i = 0; i < width - n; i++)
				sb_appendchar(buf, ' ');

	}

#ifdef PRINTF_CHECKUP
	if (sb_len(buf) - oldlen != vasprintf(&p, oldfmt, args) ||
	    strcmp(sb_ptr(buf) + oldlen, p)) {
		fprintf(stdout, "PRINTF_CHECKUP: '%s' should be '%s'\n",
		    sb_ptr(buf) + oldlen, p);
		abort();
	}
	free(p);
#endif
	
	return sb_len(buf) - oldlen;
}
