#include "defines/settings.h"
#include "utils/logger.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void sig_handler(int signo) {
  switch (signo) {
    case SIGSEGV: logger(EMERGENCY, "Caught Signal SIGSEGV"); break;
    default: logger(EMERGENCY, "Caught Signal %d", signo);
  }
  exit(signo);
}

void daemonize() {
  pid_t pid;
  if ((pid = fork()) == -1) {
    logger(ALERT, "fork %m");
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  if (setsid() < 0) {
    exit(EXIT_FAILURE);
  }
  signal(SIGHUP, SIG_IGN);
  if ((pid = fork()) == -1) {
    logger(ALERT, "fork %m");
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
    printf("%s=%d; export %s;\n", OIDC_PID_ENV_NAME, pid, OIDC_PID_ENV_NAME);
    printf("echo Agent pid $%s\n", OIDC_PID_ENV_NAME);
    exit(EXIT_SUCCESS);
  }
  chdir("/");
  umask(0);
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  open("/dev/null", O_RDONLY);
  open("/dev/null", O_RDWR);
  open("/dev/null", O_RDWR);
}
