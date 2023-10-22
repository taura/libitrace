
libitrace
================

libitrace is a library to extract functions and their source code locations executed by an application.  it will be a useful tool when you examine an application's behavior.

compile
----------------
```
$ cd libitrace/src
$ ls
itrace_ctl.c  itrace_ctl.h  itrace.py*	libitrace.c  Makefile
$ make
gcc -O3 -Wall -o libitrace.so -fPIC -shared libitrace.c
gcc -O3 -Wall -o itrace_ctl itrace_ctl.c -L/home/tau/public_html/lecture/dive_to_oss/projects/libitrace/src -Wl,-R/home/tau/public_html/lecture/dive_to_oss/projects/libitrace/src -litrace
```

test
----------------
```
$ cd libitrace/test
$ ls
f.cc  g.cc  h.cc  Makefile  q.cc
$ make
g++ -o libf.so -O0 -g -finstrument-functions -shared -fPIC f.cc
g++ -o libg.so -O0 -g -finstrument-functions -shared -fPIC g.cc
g++ -o libh.so -O0 -g -finstrument-functions -shared -fPIC h.cc
g++ -o q -O0 -g -finstrument-functions q.cc -L. -Wl,-R. -lf -lg -lh
g++ -o q_static -O0 -g -finstrument-functions q.cc f.cc g.cc h.cc
$ LD_PRELOAD=../src/libitrace.so ./q
warning: ITRACE_LOG not set, defaults to itrace_log_%d.dat
warning: ITRACE_LOG_SZ not set, defaults to 1048576
warning: ITRACE_MAPS not set, defaults to itrace_maps_%d.txt
warning: ITRACE_MAPS_DUMP_AT not set, defaults to 0
warning: ITRACE_INIT_STATUS not set, defaults to 1
pid = 4034
46344850
$ ../src/itrace.py show itrace_log_4034.dat 
path /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/libf.so
 src /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/f.cc
  fun     3   150 f(long, long)
path /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/libg.so
 src /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/g.cc
  fun     3  7500 g(long, long, long)
path /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/libh.so
 src /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/h.cc
  fun     1 375100 h(long, long, long, long)
path /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/q
 src /home/tau/public_html/lecture/dive_to_oss/projects/libitrace/test/q.cc
  fun     5     1 start_prof()
  fun     6     1 stop_prof()
  fun    10     1 main
```

Basic Usage
----------------

 * compile your application with -finstrument-functions
```
 gcc -finstrument-functions ...
 g++ -finstrument-functions ...
 g++ -o your_app ...
   ...
```
 * run your application with LD_PRELOAD environment variable set to the libitrace.so 
```
LD_PRELOAD=/right/path/to/libitrace.so ./your_app
```
 * then you will obtain itrace_log_PID.dat and itrace_maps_PID.txt
  * the former is a binary file containing address of functions executed
  * the latter is a text file describing addresses of shared libraries of the process (it's simply a dump of /proc/self/maps taken by the libitrace.so)
 * after you obtain the two, show the result by itrace.py show command
```
itrace.py show itrace_log_PID.dat
```
NOTE: the command needs itrace_maps_PID.txt too, and its absolute path name is written in itrace_log_PID.dat. So you cannot move the itrace_maps_PID.txt file.

Advanced Usage
----------------

At times you may not want to record functions in the entire execution of the program, but record functions only in a particular interval of the execution.  In that case,

 * you set environment variable ITRACE_INIT_STATUS=0 when you run the application, so that the application starts without recording
 * execute "itrace.py start" when you want to start recording
  * alternatively, you may call itrace_control(1) from within your program
 * execute "itrace.py stop" when you want to stop recording 
  * alternatively, you may call itrace_control(0) from within your program
 
itrace.py method is particularly useful when you don't know where to insert the call to itrace_control. for GUI programs, you may run the application until it starts waiting user inputs, call itrace.py start, input something to run the application a while, call itrace.py stop, and then quit the application.

here is a procedure to examine gnumeric.

```
$ LD_PRELOAD=/right/path/to/libitrace.so ITRACE_INIT_STATUS=0 ./gnumeric-1.12-35 &
# ... wait until it brings up and starts receiving user inputs ...
$ itrace.py start
# ... input something to a cell ...
$ itrace.py stop
# ... exit application normally ...
$ itrace.py show itrace_log_PID.dat
 ...
```
  