#
#
#   Makefile
#
#

TARGETS=bintail
OBJECTS=bintail.o
BINDIR=/usr/local/bin

CDEBUG=-Wall

.cc.o:
	g++ $(CFLAGS) $(CDEBUG) -c $<

$(TARGETS): $(OBJECTS)
	g++ $(CFLAGS) -o $@ $(OBJECTS)

clean:
	rm -f $(TARGETS) core *.o

strip: $(TARGETS)
	strip --strip-unneeded $(TARGETS)

rebuild: clean all

install: strip
	for i in $(TARGETS) ; do install -o root -g root -m 0755 $$i $(BINDIR) ; done

all: $(TARGETS)

# EOF: Makefile
