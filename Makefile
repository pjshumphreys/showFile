
## Created by Anjuta

CC = gcc
CFLAGS = -g -Wall
SOURCES = term.c
OBJECTS = $(SOURCES:%.c=%.o)
INCFLAGS = -I/home/user/Projects/querycsv
LDFLAGS = -Wl,-rpath,/usr/local/lib
LIBS =

WATCOM = ~/.wine/drive_c/WATCOM

all: querycsv

querycsv: $(OBJECTS)
	$(CC) -o querycsv $(OBJECTS) $(LDFLAGS) $(LIBS)

.SUFFIXES:
.SUFFIXES: .c .cc .C .cpp .o

.c.o :
	$(CC) -o $@ -c $(CFLAGS) $< $(INCFLAGS)

term.arm: $(SOURCES)
	rm -f $(OBJECTS) term
	/opt/cross/bin/arm-linux-musleabi-gcc -o term -static -lncursesw -Os -O2 -fmax-errors=20 -Wl,-O,-s,--gc-sections $(SOURCES) /opt/cross/arm-linux-musleabi/lib/libncursesw.a
	/opt/cross/bin/arm-linux-musleabi-strip --remove-section=.eh_frame --remove-section=.comment ./term
	upx term
	mv term term.arm

term.x64: $(SOURCES)
	rm -f $(OBJECTS) term
	/usr/local/musl/bin/musl-gcc -o term -static -Os -O2 -fmax-errors=20 -Wl,-O,-s,--gc-sections $(SOURCES) /usr/local/musl/lib/libncursesw.a
	strip --remove-section=.eh_frame --remove-section=.comment ./term
	upx term
	mv term term.x64

term.386: $(SOURCES)
	rm -f $(OBJECTS) term term.i386
	export WATCOM=$(WATCOM); export PATH=$(WATCOM)/binl:$(PATH); export INCLUDE=$(WATCOM)/lh; wcl386 -os $(SOURCES) ncurses.lib
	xxd -l128 -s 84 term | xxd -r -s -32 - term
	echo "000002c: 02" | xxd -r - term
	strip term
	upx term --force-execve
	mv term term.386

count:
	wc *.c *.cc *.C *.cpp *.h *.hpp

clean:
	rm -f $(OBJECTS) querycsv querycsv.i386

.PHONY: all
.PHONY: count
.PHONY: clean
