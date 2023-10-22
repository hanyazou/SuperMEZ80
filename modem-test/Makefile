
all: modem-test

modem-test: modem-test.c ymodem.c ymodem.h
	cc -I. -o modem-test modem-test.c ymodem.c

test:: all
	pkill -a modem-test || true
	./modem-test &
	ls -l /tmp/modem-test-*
	sz --ymodem foo.txt bar.txt < /tmp/modem-test-tx > /tmp/modem-test-rx
