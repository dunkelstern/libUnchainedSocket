OS=`(sw_vers -productVersion &>/dev/null && echo osx) || echo linux`

all: 
	make -C src -f Makefile.$(OS)
	make -C demo -f Makefile.$(OS)

clean:
	make -C src -f Makefile.$(OS) clean
	make -C demo -f Makefile.$(OS) clean

install:
	make -C src -f Makefile.$(OS) install DESTDIR=$(DESTDIR)