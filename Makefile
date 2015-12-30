all: 
	make -C src
	make -C demo

clean:
	make -C src clean
	make -C demo clean

install:
	make -C src install DESTDIR=$(DESTDIR)