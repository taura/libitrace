/* 
 * itrace.c
 */
#define BACKTRACE 0
#include <assert.h>
#if BACKTRACE
#include <execinfo.h>
#endif
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "itrace_ctl.h"

typedef enum {
  itrace_status_dormant = 0,
  itrace_status_recording = 1,
} itrace_status_t;

typedef struct {
  void * a;
  long count;
} itrace_call_record_t;

typedef struct {
  itrace_status_t status;
  pid_t pid;
  long n_calls;
  long dump_at;
  /* if maps_len <= PATH_MAX,
     maps is a valid array of chars.
     otherwise, maps_ptr points to 
     a string outside this structure */
  long maps_len;
  const char * maps_ptr;
  char maps[PATH_MAX + 1];
  long n;
  itrace_call_record_t records[1];
} itrace_sym_array_t;

static void itrace_dump_maps(itrace_sym_array_t * sa) {
  FILE * fp = fopen("/proc/self/maps", "rb");
  if (!fp) {
    perror("fopen"); exit(1);
  }
  FILE * wp = fopen(sa->maps_ptr, "wb");
  if (!wp) {
    perror("fopen"); exit(1);
  }
  const int n = 4096;
  char a[n];
  //fprintf(wp, "***** begin maps *****\n");
  while (1) {
    size_t rd = fread(a, 1, n, fp);
    if (ferror(fp)) {
      perror("fread"); exit(1);
    }
    fwrite(a, 1, rd, wp);
    if (rd < n) break;
  }
  //fprintf(wp, "***** end maps *****\n");

  //fprintf(stderr, "absoluterize maps %s\n", sa->maps_ptr);
  char abs_path[PATH_MAX + 1];
  if (realpath(sa->maps_ptr, abs_path)) {
    //fprintf(stderr, "OK %s\n", abs_path);
    strncpy(sa->maps, abs_path, PATH_MAX + 1);
    sa->maps_len = strlen(sa->maps);
    sa->maps_ptr = sa->maps;
  } else {
    perror("realpath");
    fprintf(stderr, "warning: could not get absolute path to maps %s; maps kept relative\n",
            sa->maps_ptr);
  }
}

static long hash_address(void * a, long n) {
  long x = (long)a;
  return (x >> 2) % n;
}

static long itrace_sym_array_record(itrace_sym_array_t * sa, void * a) {
  if (sa->status == itrace_status_recording) {
    long nc = __sync_fetch_and_add(&sa->n_calls, 1);
    if (nc == sa->dump_at) {
      itrace_dump_maps(sa);
    }
    long n = sa->n;
    long h = hash_address(a, n);
    itrace_call_record_t * recs = sa->records;
    for (long i = 0; i < n; i++) {
      long k = (h + i) % n;
      void ** p = &recs[k].a;
      if (*p == 0) {
        __sync_bool_compare_and_swap(p, 0, a);
      }
      if (*p == a) {
        __sync_fetch_and_add(&recs[k].count, 1);
        return i + 1;
      }
    }
    return n + 1;
  } else {
    return 0;                   /* not done */
  }
}

static int itrace_sym_array_set_state(itrace_sym_array_t * sa, int state) {
  sa->status = state;           /* 0 to stop; 1 to start */
  return 1;
}

/* create sym array file of n elements with filename */
static itrace_sym_array_t * itrace_create_sym_array(const char * trace_log, long n,
                                                    const char * maps, pid_t pid,
                                                    itrace_status_t init_status,
                                                    long dump_at) {
  long pathlen = strlen(trace_log) + 50;
  char path[pathlen];
  sprintf(path, trace_log, pid);
  int fd = open(path, O_RDWR|O_CREAT|O_TRUNC|O_EXCL, 0644);
  if (fd == -1) {
    perror("open"); exit(1);
  }
  long length = (long)(&((itrace_sym_array_t*)0)->records[n]);
  if (ftruncate(fd, length) == -1) {
    perror("ftruncate"); exit(1);
  }
  itrace_sym_array_t * sa = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (!sa) {
    perror("mmap"); exit(1);
  }
  sa->status = init_status;

  //fprintf(stderr, "embed pid %d into maps %s\n", pid, maps);
  
  long len = snprintf(sa->maps, PATH_MAX + 1, maps, pid); /* len = would have been written */
  sa->maps_len = len;
  if (len > PATH_MAX) {
    fprintf(stderr, "warning: maps too long (%ld chars); maps invalid\n", len);
    char path[len + 1];
    snprintf(path, len + 1, maps, pid);
    sa->maps_ptr = strdup(path);
  } else {
    //fprintf(stderr, "OK %s\n", sa->maps);
    sa->maps_ptr = sa->maps;
  }
  sa->pid = pid;
  sa->n_calls = 0;
  sa->dump_at = dump_at;
  sa->n = n;
  for (long i = 0; i < n; i++) {
    assert(sa->records[i].a == 0);
    assert(sa->records[i].count == 0);
  }
  __sync_synchronize();
  return sa;
}

long getenv_l(char * var, long default_val) {
  const char * s = getenv(var);
  if (s) {
    char * rest = 0;
    long x = strtol(s, &rest, 10);
    if (*rest) {
      fprintf(stderr, "warning: invalid string (%s) given to %s, defaults to %ld\n",
              s, var, default_val);
      return default_val;
    } else {
      return x;
    }
  } else {
    fprintf(stderr, "warning: %s not set, defaults to %ld\n", var, default_val);
    return default_val;
  }
}

const char * getenv_s(char * var, const char * default_val) {
  const char * s = getenv(var);
  if (s) {
    return s;
  } else {
    fprintf(stderr, "warning: %s not set, defaults to %s\n", var, default_val);
    return default_val;
  }
}

/* create sym array as specified by env vars */
static itrace_sym_array_t * itrace_create_sym_array_from_env_vars() {
  const char * itrace_log  = getenv_s("ITRACE_LOG", "itrace_log_%d.dat");
  long         log_sz      = getenv_l("ITRACE_LOG_SZ", 1024 * 1024);
  const char * maps        = getenv_s("ITRACE_MAPS", "itrace_maps_%d.txt");
  long         dump_at     = getenv_l("ITRACE_MAPS_DUMP_AT", 0);
  long         init_status = getenv_l("ITRACE_INIT_STATUS", 1) != 0;
  itrace_sym_array_t * sa = itrace_create_sym_array(itrace_log, log_sz, maps, getpid(),
                                                    init_status != 0, dump_at);
  return sa;
}

itrace_sym_array_t * itrace_global_sym_array = 0;

/* ensure itrace_global_sym_array points to the global sym array */
static itrace_sym_array_t * itrace_ensure_global_sym_array() {
  itrace_sym_array_t ** p = &itrace_global_sym_array;
  itrace_sym_array_t * q = *p;
  
  if (q == 0) {
    if (__sync_bool_compare_and_swap(p, 0, (itrace_sym_array_t *)1)) {
      /* I am responsible for creating sym_array */
      q = itrace_create_sym_array_from_env_vars();
      *p = q;
    }
  }
  while (q == (itrace_sym_array_t *)1) {
    q = *p;
  }
  return q;
}

static long itrace_global_sym_array_record(void * a) {
  itrace_sym_array_t * sa = itrace_ensure_global_sym_array();
  long c = itrace_sym_array_record(sa, a);
  return c;
}

static int itrace_global_sym_array_set_state(int state) {
  itrace_sym_array_t * sa = itrace_ensure_global_sym_array();
  return itrace_sym_array_set_state(sa, state);
}

int itrace_control(int state) {
  return itrace_global_sym_array_set_state(state);
}

int itrace_show(const char * itrace_log) {
  int fd = open(itrace_log, O_RDONLY);
  if (fd == -1) {
    perror("open"); return 0;   /* NG */
  }
  off_t sz = lseek(fd, 0, SEEK_END);
  itrace_sym_array_t * sa = mmap(0, sz, PROT_READ, MAP_PRIVATE, fd, 0);
  if (!sa) {
    perror("mmap"); return 0;   /* NG */
  }
  printf("status: %d\n", sa->status);
  printf("maps_len: %ld\n", sa->maps_len);
  printf("maps: %s\n", sa->maps);
  printf("pid: %d\n", sa->pid);
  printf("n_calls: %ld\n", sa->n_calls);
  printf("dump_at: %ld\n", sa->dump_at);
  printf("n: %ld\n", sa->n);
  long n = sa->n;
  for (long i = 0; i < n; i++) {
    void * a = sa->records[i].a;
    if (a) {
      printf("[%ld]: a: %p count: %ld\n", i, a, sa->records[i].count);
    }
  }
  return 1;                     /* OK */
}

int itrace_set_state(const char * itrace_log, int status) {
  int fd = open(itrace_log, O_RDWR);
  if (fd == -1) {
    perror("open"); return 0;   /* NG */
  }
  off_t sz = lseek(fd, 0, SEEK_END);
  itrace_sym_array_t * sa = mmap(0, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (!sa) {
    perror("mmap"); return 0;   /* NG */
  }
  sa->status = status;
  munmap(sa, sz);
  close(fd);
  return 1;                     /* OK */
}

void __cyg_profile_func_enter(void * a, void * c) {
#if BACKTRACE
  itrace_sym_array_t * sa = itrace_ensure_global_sym_array();
  if (sa->status) {
    int n = 5;
    void * bt[n];
    int m = backtrace(bt, n);
    for (int i = 0; i < m; i++) {
      itrace_global_sym_array_record(bt[i]);
    }
  }
#else
  itrace_global_sym_array_record(a);
#endif
}

void __cyg_profile_func_exit(void * a, void * c) {
}
