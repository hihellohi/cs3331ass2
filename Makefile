CC=g++
CPPFLAGS=-Wall -Werror -g
OBJS=Lsr.o

all: Lsr

Lsr : $(OBJS)
	$(CC) -o Lsr $(OBJS)

Lsr.o : Lsr.cpp

clean :
	rm -f Lsr $(OBJS)
