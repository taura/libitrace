CC:=gcc
CFLAGS:=-O3 -Wall

all : libitrace.so itrace_ctl

libitrace.so : libitrace.c
	$(CC) $(CFLAGS) -o $@ -fPIC -shared $<

itrace_ctl : itrace_ctl.c
	$(CC) $(CFLAGS) -o $@ $< -L$(shell pwd) -Wl,-R$(shell pwd) -litrace

clean :
	rm -f libitrace.so itrace_ctl
