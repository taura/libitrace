/* 
 * itrace_ctl.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "itrace_ctl.h"


void help(const char * prog) {
  fprintf(stderr,
          "usage:\n"
          "  %s\n",
          prog);
}

int do_cmd_start_stop(int argc, char ** argv, int status) {
  char * prog = argv[0];
  if (optind >= argc) {
    fprintf(stderr, "error: start/stop command requires a filename\n");
    help(prog);
    return 0;                   /* NG */
  }
  char * itrace_log = argv[optind];
  optind++;
  int r = itrace_set_state(itrace_log, status);
  return r;
}

int do_cmd_show(int argc, char ** argv) {
  char * prog = argv[0];
  if (optind >= argc) {
    fprintf(stderr, "error: show command requires a filename\n");
    help(prog);
    return 0;                   /* NG */
  }
  char * itrace_log = argv[optind];
  optind++;
  int r = itrace_show(itrace_log);
  return r;
}

int do_cmd(int argc, char ** argv) {
  char * prog = argv[0];
  while (1) {
    int opt = getopt(argc, argv, "+g:");
    if (opt == -1) break;
    switch (opt) {
    case 'g':
      break;
    default:
      fprintf(stderr, "invalid option -%c\n", opt);
      help(prog);
      return 1;
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "error: requires a command\n");
    help(prog);
    return 1;
  }
  char * cmd = argv[optind];
  optind++;
  int r = 1;
  if (strcmp(cmd, "start") == 0) {
    r = do_cmd_start_stop(argc, argv, 1);
  } else if (strcmp(cmd, "stop") == 0) {
    r = do_cmd_start_stop(argc, argv, 0);
  } else if (strcmp(cmd, "show") == 0) {
    r = do_cmd_show(argc, argv);
  } else {
    fprintf(stderr, "error: invalid command %s\n", cmd);
    help(prog);
    exit(1);
  }
  return r;
}

int main(int argc, char ** argv) {
  int r = do_cmd(argc, argv);
  if (r) {
    return 0;
  } else {
    return 1;
  }
}
