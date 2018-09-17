.PHONY : all clean install uninstall
CC = /usr/bin/gcc
RM = /bin/rm
CP= /bin/cp
INSTALL_DIR= /usr/local/lib

all : libcoverage.so

coverage.o : coverage.c
	$(CC) -g -c -fpic $^

libcoverage.so : coverage.o
	$(CC) -g -shared -o $@ $^

install: libcoverage.so
	$(CP) $^ $(INSTALL_DIR)

uninstall: 
	$(RM) -f $(INSTALL_DIR)/libcoverage.so

clean:
	$(RM) -f *.o *.so
