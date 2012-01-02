APXS13=/usr/local/apache/bin/apxs
APXS22=/usr/local/apache2/bin/apxs
MODULE=mod_ipblock.c

ap13:
	sudo $(APXS13) -cia $(MODULE)

ap2:
	sudo $(APXS22) -cia $(MODULE)

all:
	ap13

clean:
	sudo rm -rf .libs *.o *.lo *.so *.la *.a *.slo

