#include <cstdio>
#include <unistd.h>

#include "shell.hh"
#include <signal.h>
#include <sys/wait.h>

int yyparse(void);

char * path_argv;
int iniCode = 0;
pid_t iniPID = 0;
std::string iniStr = "";

int * returnCode = &iniCode;
pid_t * bgPID = &iniPID;
std::string * lastArg = &iniStr;

void Shell::prompt() {
  if (isatty(0)) {
    printf("myshell>");
    fflush(stdout);
  }
}

void zombieHandler(int sig) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

extern "C" void ctrlCHandler( int sig )
{
	kill(sig, SIGSTOP);
  // Shell::_currentCommand.clear();
  printf("\n");
  Shell::prompt();
}

int main(int argc, char *argv[]) {
  path_argv = argv[0]; //obtain relative path to the shell
  struct sigaction sa;
  sa.sa_handler = ctrlCHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if(sigaction(SIGINT, &sa, NULL)){
    perror("sigaction");
    exit(2);
  }

  struct sigaction sb;
  sb.sa_handler = zombieHandler;
  sigemptyset(&sb.sa_mask);
  sb.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sb, NULL)) {
    perror("sigaction");
    exit(2);
  }

  Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;
