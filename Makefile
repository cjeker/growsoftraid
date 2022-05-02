#	$OpenBSD$

PROG=		growsoftraid
NOMAN=
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=	-Wmissing-declarations -Wredundant-decls
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
LDADD=		-lutil
DPADD=		${LIBUTIL}

.include <bsd.prog.mk>
