SRC=queue.c server.c
OBJS=$(addprefix build/, $(SRC:.c=.o))

TARGET=build/libUnchainedSocket.a

LDFLAGS=-lpthread -lBlocksRuntime
CFLAGS=-g --std=c99 -D_GNU_SOURCE -fblocks

all: $(TARGET)

clean:
	rm -rf build

install: $(TARGET)
	@if [ ! -d "$(DESTDIR)/usr/local/lib" ] ; then mkdir -p "$(DESTDIR)/usr/local/lib" ; fi
	@if [ ! -d "$(DESTDIR)/usr/local/include/unchainedSocket" ] ; then mkdir -p "$(DESTDIR)/usr/local/include/unchainedSocket" ; fi
	install -m 0755 -t $(DESTDIR)/usr/local/lib $(TARGET)
	install -m 0644 -t $(DESTDIR)/usr/local/include/unchainedSocket *.h

$(TARGET): $(OBJS)
	ar -rc $(TARGET) $(OBJS)

build/%.o: %.c
	@if [ ! -d build ] ; then mkdir -p build ; fi
	clang -c $< -o $@ $(CFLAGS)
