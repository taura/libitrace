long h(long i, long j, long k, long n);

long g(long i, long j, long n) {
  long s = 0;
  for (long k = 0; k < n; k++) {
    s += h(i, j, k, n);
  }
  return s;
}

