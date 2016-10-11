CC=g++
CPPFLAGS=-Wall -Werror -g
OBJS=lsr.o

all: Lsr

Lsr : $(OBJS)
	$(CC) -o Lsr $(OBJS)

lsr.o : lsr.cpp

clean :
	rm -f Lsr $(OBJS)
