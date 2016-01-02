SRC=queue.c server.c
OBJS=$(addprefix build/, $(SRC:.c=.o))

TARGET=build/libUnchainedSocket.a

LDFLAGS=-lpthread
CFLAGS=-g --std=c99 -D_GNU_SOURCE

all: $(TARGET)

clean:
	rm -rf build

install: $(TARGET)
	@if [ ! -d "$(DESTDIR)/usr/local/lib" ] ; then mkdir -p "$(DESTDIR)/usr/local/lib" ; fi
	@if [ ! -d "$(DESTDIR)/usr/local/include/unchainedSocket" ] ; then mkdir -p "$(DESTDIR)/usr/local/include/unchainedSocket" ; fi
	install -m 0755 $(TARGET) $(DESTDIR)/usr/local/lib
	install -m 0644 *.h $(DESTDIR)/usr/local/include/unchainedSocket

$(TARGET): $(OBJS)
	ar -rc $(TARGET) $(OBJS)

build/%.o: %.c
	@if [ ! -d build ] ; then mkdir -p build ; fi
	clang -c $< -o $@ $(CFLAGS)
