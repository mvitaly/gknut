# Makefiile based on Makefile from gkrellmbups by Chris
# Modified for gknut by Vitaly

# Package information
PACKAGE   = gknut
VERSION   = 0.0.2
DIST      = $(PACKAGE)-$(VERSION)
DISTFILES = ChangeLog COPYING Doxyfile INSTALL Makefile README \
            gknut.c gknut.h nut_connect.c nut_connect.h

# Non-UK users should uncomment the next line
# MAINS_MIN = -DMAINS_MIN=90

# Binaries
INSTALL   = install
MKDIR     = mkdir
RM        = rm -f
RMRF      = rm -rf
TAR       = tar
GZIP      = gzip -c
BZIP2     = bzip2 -c
CD        = cd
DOXYGEN   = doxygen

# compile and link arguments
GTK_INCLUDE   = `gtk-config --cflags`
GTK_LIB       = `gtk-config --libs`
IMLIB_INCLUDE = `imlib-config --cflags-gdk`
IMLIB_LIB     = `imlib-config --libs-gdk`
GLIB_INCLUDE  = `glib-config --cflags`
GLIB_LIB      = `glib-config --libs`

# comment the next line and uncomment the one after if you have an athlon/duron...
FLAGS = -O2 -Wall -fPIC $(GTK_INCLUDE) $(IMLIB_INCLUDE) $(GLIB_INCLUDE) $(MAINS_MIN)
#FLAGS = -O2 -Wall -fPIC -ffast-math -mcpu=athlon -march=athlon $(GTK_INCLUDE) $(IMLIB_INCLUDE) $(GLIB_INCLUDE) $(MAINS_MIN)
LIBS = $(GTK_LIB) $(IMLIB_LIB) $(GLIB_LIB) -lpthread
LFLAGS = -shared

CC = gcc $(CFLAGS) $(FLAGS)

OBJS = gknut.o nut_connect.o

grellmbups.so: $(OBJS)
	$(CC) $(OBJS) -o gknut.so $(LFLAGS) $(LIBS) 

clean:
	$(RMRF) *.o core *.so* *.bak *~ $(DIST) $(DIST).tar $(DIST).tar.gz $(DIST).tar.bz2

nut_connect.o: nut_connect.c nut_connect.h
gknut.o: gknut.c gknut.c nut_connect.h

documentation::
	if [ -e Doxyfile ] ; then \
		doxygen ; \
	fi

install:
	if [ -d /usr/lib/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gknut.so /usr/lib/gkrellm/plugins/ ; \
	elif [ -d /usr/share/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gknut.so /usr/share/gkrellm/plugins/ ; \
	elif [ -d /usr/local/lib/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gknut.so /usr/local/lib/gkrellm/plugins/ ; \
	elif [ -d /usr/lib/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gknut.so /usr/lib/gkrellm/plugins/ ; \
	else \
		$(INSTALL) -D -c -s -m 644 gknut.so /usr/lib/gkrellm/plugins/gkrellmbups.so ; \
	fi

uninstall:
	rm -f /usr/lib/gkrellm/plugins/gknut.so
	rm -f /usr/share/gkrellm/plugins/gknut.so
	rm -f /usr/local/lib/gkrellm/plugins/gknut.so
	rm -f /usr/lib/gkrellm/plugins/gknut.so

dist::
	$(RMRF) $(DIST) $(DIST).tar $(DIST).tar.gz
	$(MKDIR) $(DIST)
	for I in $(DISTFILES); do \
	if [ -x $$I ]; then $(INSTALL) -D -m 0775 $$I $(DIST)/$$I; else \
	if [ -f $$I ]; then $(INSTALL) -D -m 0664 $$I $(DIST)/$$I; else \
	if [ -d $$I ]; then $(INSTALL) -D -m 0555 -d $$I $(DIST)/$$I; fi; fi; fi ; \
	done
	$(TAR) -cf $(DIST).tar $(DIST)
	$(GZIP) $(DIST).tar > $(DIST).tar.gz
	$(BZIP2) $(DIST).tar > $(DIST).tar.bz2
	$(RM) $(DIST).tar
	$(RMRF) $(DIST)
