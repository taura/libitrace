#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void start_prof() { }
void stop_prof() { }

long f(long i, long n);

int main(int argc, char ** argv) {
  long n = (argc > 1 ? atol(argv[1]) : 50);
  printf("pid = %d\n", getpid());
  long s = 0; 
  for (long i = 0; i < n; i++) {
    s += f(i, n - 1);
  }
  start_prof();
  for (long i = 0; i < n; i++) {
    s += f(i, n);
  }
  stop_prof();
  for (long i = 0; i < n; i++) {
    s += f(i, n + 1);
  }
  printf("%ld\n", s);
  return 0;
}
