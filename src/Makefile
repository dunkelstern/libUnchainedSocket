SRC=queue.c server.c
OBJS=$(addprefix build/, $(SRC:.c=.o))

TARGET=build/libUnchainedSocket.a

LDFLAGS=-lpthread
CFLAGS=-g --std=c99 -D_GNU_SOURCE

all: $(OBJS)
	ar -rc $(TARGET) $(OBJS)

clean:
	rm -rf build

build/%.o: %.c
	@if [ ! -d build ] ; then mkdir -p build ; fi
	clang -c $< -o $@ $(CFLAGS)
