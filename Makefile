PROG=		torwm

PREFIX?=	/usr/local

SRCS=		client.c config.y desktop.c event.c functions.c group.c main.c screen.c state.c utils.c xutils.c

OBJS=		client.o config.o desktop.o event.o functions.o group.o main.o screen.o state.o utils.o xutils.o
		
PKG_CONFIG?=	pkg-config

CPPFLAGS+=	`$(PKG_CONFIG) --cflags x11 xrandr`

CFLAGS?=	-Wall -O2 -g -D_GNU_SOURCE

LDFLAGS+=	`$(PKG_CONFIG) --libs x11 xrandr`

MANPREFIX?=	$(PREFIX)/share/man

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG) config.c

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(PROG)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $<

install: $(PROG)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(PROG) $(DESTDIR)$(PREFIX)/bin

.PRECIOUS: config.c
