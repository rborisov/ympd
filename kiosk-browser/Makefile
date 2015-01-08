CFLAGS=$(shell pkg-config --cflags  webkitgtk-3.0)
CFLAGS+= -Wall -g
LDFLAGS=$(shell pkg-config --libs webkitgtk-3.0)

all: browser

browser: browser3.c
	cc -c $(CFLAGS) browser3.c
	cc -o browser browser3.o $(LDFLAGS)

clean:
	rm -f *~
	rm -f browser
	rm -f browser3.o

install:
	cp browser /usr/bin/
