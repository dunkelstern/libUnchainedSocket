SRC=main.c
OBJS=$(addprefix build/, $(SRC:.c=.o))

TARGET=build/demo

LDFLAGS=-lpthread -L../src/build -lUnchainedSocket
CFLAGS=-g --std=c99 -D_GNU_SOURCE -I../src

all: $(OBJS)
	clang -o $(TARGET) $(OBJS) $(LDFLAGS)

clean:
	rm -rf build

build/%.o: %.c
	@if [ ! -d build ] ; then mkdir -p build ; fi
	clang -c $< -o $@ $(CFLAGS)
