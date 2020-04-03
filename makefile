# change application name here (executable output name)
TARGET=luxpl
# compiler
CC=gcc
# debug
DEBUG=-g
# optimisation
OPT=-O0
# warnings
WARN=-Wall

PTHREAD=-pthread

CCFLAGS=$(DEBUG) $(OPT) $(WARN) $(PTHREAD) -pipe 

GTKLIB=`pkg-config --cflags --libs gtk+-3.0`

# linker
LD=gcc
LDFLAGS=$(PTHREAD) $(GTKLIB) -export-dynamic -lwiringPi -lvlc -lX11

OBJS=    luxpl01.o

all: $(OBJS)
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
    
luxpl01.o: src/luxpl01.c
	$(CC) -c $(CCFLAGS) src/luxpl01.c $(GTKLIB) -o luxpl01.o
    
clean:
	rm -f *.o $(TARGET)
