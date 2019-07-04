VBCC = /opt/vbcc
CC = export VBCC=$(VBCC); export PATH=$(VBCC)/bin:$(PATH); vc
CFLAGS =
SOURCES = amiga2.c
OBJECTS = $(SOURCES:%.c=%.o)
INCFLAGS = -I$(VBCC)/NDK_3.9/Include/include_h
LDFLAGS = -L$(VBCC)/targets/m68k-kick13/lib/ -DAMIGA
LIBS = +kick13 -lamiga -lm13

all: Term

Term: $(SOURCES)
	$(CC) $(INCFLAGS) $(LDFLAGS) $(LIBS) -o Term $(SOURCES)

clean:
	rm -f Term

.PHONY: all
.PHONY: clean
