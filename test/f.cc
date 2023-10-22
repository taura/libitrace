long g(long i, long j, long n);

long f(long i, long n) {
  long s = 0;
  for (long j = 0; j < n; j++) {
    s += g(i, j, n);
  }
  return s;
}
