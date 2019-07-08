VBCC = /opt/vbcc
CC = export VBCC=$(VBCC); export PATH=$(VBCC)/bin:$(PATH); vc
CFLAGS =
SOURCES = atarist2.c
OBJECTS = $(SOURCES:%.c=%.o)
INCFLAGS =
LDFLAGS = -DATARI
LIBS = +tos -lgem -lm

all: test.ttp

test.ttp: $(SOURCES)
	$(CC) $(INCFLAGS) $(LDFLAGS) $(LIBS) -o test.ttp $(SOURCES)

clean:
	rm -f test.ttp

.PHONY: all
.PHONY: clean
