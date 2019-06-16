
## Created by Anjuta

CC = gcc
CFLAGS = -g -Wall
INCFLAGS = -I/home/user/Projects/querycsv
LDFLAGS = -Wl,-rpath,/usr/local/lib
LIBS =

WATCOM = ~/.wine/drive_c/WATCOM

all: term.arm term.x64 term.386

term16.exe: dos.c
	del term16.exe
	wcl /ml /zt1900 /dMICROSOFT=1 /fpc /0 /os /k3000 /fe=term16 dos.c
	c:\upx\upx --8086 .\term16.exe

term.exe: win32.c
	del term.exe
	wcl386 -bcl=nt /os /fe=term win32.c
	c:\upx\upx .\term.exe

term.arm: posix.c
	rm -f term.arm
	/opt/cross/bin/arm-linux-musleabi-gcc -o term -static -lncursesw -Os -O2 -fmax-errors=20 -Wl,-O,-s,--gc-sections posix.c /opt/cross/arm-linux-musleabi/lib/libncursesw.a
	/opt/cross/bin/arm-linux-musleabi-strip --remove-section=.eh_frame --remove-section=.comment ./term
	upx term
	mv term term.arm

term.x64: posix.c
	rm -f term.x64
	/usr/local/musl/bin/musl-gcc -o term -static -Os -O2 -fmax-errors=20 -Wl,-O,-s,--gc-sections posix.c /usr/local/musl/lib/libncursesw.a
	strip --remove-section=.eh_frame --remove-section=.comment ./term
	upx term
	mv term term.x64

term.386: posix.c
	rm -f term.i386
	export WATCOM=$(WATCOM); export PATH=$(WATCOM)/binl:$(PATH); export INCLUDE=$(WATCOM)/lh; wcl386 -os -fe=posix getch.c ncurses.lib
	xxd -l128 -s 84 posix | xxd -r -s -32 - posix
	echo "000002c: 02" | xxd -r - posix
	strip posix
	upx posix --force-execve
	mv posix term.386

count:
	wc *.c *.cc *.C *.cpp *.h *.hpp

clean:
	rm -f term.arm term.x64 term.386 term16.exe
