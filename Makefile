
TARGET=		rcshist
OBJECTS= 	rcshist.o namedobjlist.o rcsfile.o misc.o strbuf.o
CFLAGS+=	-Wall -g -O2

VERS=		1.0
DISTFILES=	Makefile bsd_queue.h misc.c misc.h namedobjlist.c \
		namedobjlist.h rcsfile.c rcsfile.h rcshist.c strbuf.c \
		strbuf.h rcshist.1
DISTNAME=	rcshist-${VERS}
DISTDIR=	distdir/${DISTNAME}

all: ${TARGET}

${TARGET}: ${OBJECTS}
	${CC} ${CFLAGS} -o ${TARGET} ${LDFLAGS} ${OBJECTS}

clean:
	rm -f ${TARGET} ${OBJECTS} *.core *.o core



dist:
	test ! -e distdir/${DISTNAME}.tgz
	rm -rf ${DISTDIR}
	mkdir -p ${DISTDIR}
	cp ${DISTFILES} ${DISTDIR}/
	(cd ${DISTDIR} && make -s && make -s clean)
	rm -rf ${DISTDIR}
	mkdir -p ${DISTDIR}
	cp ${DISTFILES} ${DISTDIR}/
	(cd distdir && tar zcvf ${DISTNAME}.tgz ${DISTNAME})
	rm -rf ${DISTDIR}

