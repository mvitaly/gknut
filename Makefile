# Makefiile based on Makefile from gkrellweather by Franky Lam <franky@frankylam.com>
# Modified for gkrellmbups by Chris

# Package information
PACKAGE   = gkrellmbups
VERSION   = 0.3
DIST      = $(PACKAGE)-$(VERSION)
DISTFILES = ChangeLog COPYING Doxyfile INSTALL Makefile README \
            gkrellmbups.c gkrellmbups.h ups_connect.c ups_connect.h

# Non-UK users should uncomment the next line
# MAINS_MIN = -DMAINS_MIN=90

# Binaries
INSTALL   = install
MKDIR     = mkdir
RM        = rm -f
RMRF      = rm -rf
TAR       = tar
GZIP      = gzip
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

OBJS = gkrellmbups.o ups_connect.o

grellmbups.so: $(OBJS)
	$(CC) $(OBJS) -o gkrellmbups.so $(LFLAGS) $(LIBS) 

clean:
	$(RMRF) *.o core *.so* *.bak *~ $(DIST) $(DIST).tar $(DIST).tar.gz

ups_connect.o: ups_connect.c ups_connect.h
gkrellmbups.o: gkrellmbups.c gkrellmbups.c ups_connect.h

documentation::
	if [ -e Doxyfile ] ; then \
		doxygen ; \
	fi

install:
	if [ -d /usr/lib/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gkrellmbups.so /usr/lib/gkrellm/plugins/ ; \
	elif [ -d /usr/share/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gkrellmbups.so /usr/share/gkrellm/plugins/ ; \
	elif [ -d /usr/local/lib/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gkrellmbups.so /usr/local/lib/gkrellm/plugins/ ; \
	elif [ -d /usr/lib/gkrellm/plugins/ ] ; then \
		$(INSTALL) -c -s -m 644 gkrellmbups.so /usr/lib/gkrellm/plugins/ ; \
	else \
		$(INSTALL) -D -c -s -m 644 gkrellmbups.so /usr/lib/gkrellm/plugins/gkrellmbups.so ; \
	fi

uninstall:
	rm -f /usr/lib/gkrellm/plugins/gkrellmbups.so
	rm -f /usr/share/gkrellm/plugins/gkrellmbups.so
	rm -f /usr/local/lib/gkrellm/plugins/gkrellmbups.so
	rm -f /usr/lib/gkrellm/plugins/gkrellmbups.so

dist::
	$(RMRF) $(DIST) $(DIST).tar $(DIST).tar.gz
	$(MKDIR) $(DIST)
	for I in $(DISTFILES); do \
	if [ -x $$I ]; then $(INSTALL) -D -m 0775 $$I $(DIST)/$$I; else \
	if [ -f $$I ]; then $(INSTALL) -D -m 0664 $$I $(DIST)/$$I; else \
	if [ -d $$I ]; then $(INSTALL) -D -m 0555 -d $$I $(DIST)/$$I; fi; fi; fi ; \
	done
	$(TAR) -cf $(DIST).tar $(DIST)
	$(GZIP) $(DIST).tar
	$(RMRF) $(DIST)
