OS=`if [ -e /usr/bin/sw_vers ] ; then echo osx ; else echo linux ; fi`

all: 
	make -C src -f Makefile.$(OS)
	make -C demo -f Makefile.$(OS)

clean:
	make -C src -f Makefile.$(OS) clean
	make -C demo -f Makefile.$(OS) clean

install:
	make -C src -f Makefile.$(OS) install DESTDIR=$(DESTDIR)
