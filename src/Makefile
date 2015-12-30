SRC=queue.c server.c
OBJS=$(addprefix build/, $(SRC:.c=.o))

TARGET=build/libUnchainedSocket.a

LDFLAGS=-lpthread
CFLAGS=-g --std=c99 -D_GNU_SOURCE

all: $(TARGET)

clean:
	rm -rf build

install: $(TARGET)
	install -m 0755 -t $(DESTDIR)/usr/local/lib $(TARGET)
	install -m 0644 -t $(DESTDIR)/usr/local/include/unchainedSocket src/*.h

$(TARGET): $(OBJS)
	ar -rc $(TARGET) $(OBJS)

build/%.o: %.c
	@if [ ! -d build ] ; then mkdir -p build ; fi
	clang -c $< -o $@ $(CFLAGS)
