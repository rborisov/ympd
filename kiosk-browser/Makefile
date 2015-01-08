CFLAGS=$(shell pkg-config --cflags  webkit2gtk-3.0)
CFLAGS+= -Wall -g
LDFLAGS=$(shell pkg-config --libs webkit2gtk-3.0)

all: browser

browser: browser3.c
	cc -c $(CFLAGS) browser3.c
	cc -o browser browser3.o $(LDFLAGS)

clean:
	rm -f *~
	rm -f browser
	rm -f browser3.o

install:
	cp -f browser /usr/bin/
