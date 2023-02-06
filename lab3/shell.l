
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include "y.tab.hh"
#include <string>

#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <iostream>
#include <sys/wait.h>
#include <regex>
#include <pwd.h>

extern int * returnCode;
extern pid_t * bgPID;
extern std::string * lastArg;
extern char * path_argv;

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

////////////  Start added code ///////////
extern "C" char * read_line();

int mygetc(FILE * f) {
 static char *p;
 char ch;

 if (!isatty(0)) {
 // stdin is not a tty. Call real getc
   return getc(f);
 }

 // stdin is a tty. Call our read_line.
 if (p==NULL || *p == 0) {
   char * s = read_line();
   p = s;
 }

 ch = *p;
 p++;

 return ch;
}

#undef getc
#define getc(f) mygetc(f)

/////////// End added code ///////////

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

"<" {
  return LESS;
}

">>" {
  return GREAT_GREAT;
}

"|" {
  return PIPE;
}

"2>" {
  return TWO_GREAT;
}

">&" {
  return GREAT_AMPERSAND;
}

">>&" {
  return GREAT_GREAT_AMPERSAND;
}

"&" {
  return AMPERSAND;
}

[^ \t\n\|&<>]*\\[^ \n\t]* { //escape old:(([^ \\\t\n<>|&]*\\.[^ \\\t\n<>|&]*)*)
  char * temp = yytext;
  char newStr[500];
  int index = 0;
  int i = 0;

  for (i = 0; i < strlen(temp); i++) {
    if (temp[i] == '\\') {
      newStr[index] = temp[i + 1];
      index++;
      i++;
    } else {
      newStr[index] = temp[i];
      index++;
    }
  }

  newStr[index] = '\0';
  yylval.cpp_string = new std::string(newStr);
  return WORD;
}

\".+\" { //quotes
  std::string temp = yytext;
  int end = temp.size()-2;
  temp = temp.substr(1, end);
  yylval.cpp_string = new std::string(temp);
  return WORD;
}

(`.*`)|(\$\(.*\)) { //subshell
  std::string temp(yytext);
  if (temp.find("`") == 0) {
    temp = temp.substr(1, temp.size()-2);
  } else {
    temp = temp.substr(2, temp.size()-3);
  }
  temp.append("\nexit\n");

  int tmpin = dup(0);
  int tmpout = dup(1);
  int PIN[2];
  pipe(PIN);
  int POUT[2];
  pipe(POUT);

  write(PIN[1], temp.c_str(), temp.size());
  close(PIN[1]);
  dup2(PIN[0],0);
  close(PIN[0]);
  dup2(POUT[1],1);
  close(POUT[1]);

  int ret = fork();
  if (ret == 0) {
    execlp("/proc/self/exe", "/proc/self/exe", NULL);
    perror("");
    exit(-1);
  }

  //restore in/out defaults
  dup2(tmpin, 0);
  dup2(tmpout, 1);
  close(tmpin);
  close(tmpout);

  char bufferString[1024];    //size can be changed
  int bufferLength = read(POUT[0],bufferString, 1024);
  bufferString[bufferLength] = '\0';
  std::string returnString(bufferString);

  //remove "\n"
  while (returnString.find("\n") != -1) {
    int i = returnString.find("\n");
    returnString.replace(i, 1, " ");
  }

  //put back to main shell using myunputc
  const char * putbackString = returnString.c_str();
  int forLen = returnString.size();
  for (int i = forLen - 1; i >= 0; i--) {
    myunputc(putbackString[i]);
  }
}

source[ ][^ \t\n<>|&][^ \t\n<>|&]* { //source BUILD-IN
  char * raw_file = yytext + 7;
  int inFile = open(raw_file, O_RDONLY);
  char bufferTemp[1024];
  int bufferLen = read(inFile, bufferTemp, 1024);
  bufferTemp[bufferLen] = '\0';
  std::string buffer(bufferTemp);

  //remove "\n"
  while (buffer.find("\n") != -1) {
    int i = buffer.find("\n");
    buffer.replace(i, 1, " ");
  }
  //put back to main shell using myunputc
  const char * putbackString = buffer.c_str();
  int forLen = buffer.size();
  for (int i = forLen - 1; i >= 0; i--) {
    myunputc(putbackString[i]);
  }
}

(\$\{\$\}) { //${$} The PID of the shell process
  int pid = getpid();
  std::string pid_str = std::to_string(pid);
  yylval.cpp_string = new std::string(pid_str);
  return WORD;
}

(\$\{\?\}) { //${?} The return code of the last executed simple command
  int num = *returnCode;
  std::string num_str = std::to_string(num);
  yylval.cpp_string = new std::string(num_str);
  return WORD;
}

(\$\{\!\}) { //${!} PID of the last process run in the background
  int pid = *bgPID;
  std::string pid_str = std::to_string(pid);
  yylval.cpp_string = new std::string(pid_str);
  return WORD;
}

(\$\{\_\}) { //${_} The last argument in the fully expanded previous command
  yylval.cpp_string = new std::string(* lastArg);
  return WORD;
}

(\$\{SHELL\}) { //${SHELL} The path of your shell executable.
  char absPath[1024];
  realpath(path_argv, absPath);
  yylval.cpp_string = new std::string(absPath);
  return WORD;
}

([^ \\\t\n<>|&]*\$\{.*\}[^ \\\t\n<>|&]*)* { //env var expansion
  using namespace std;
  string *temp = new string(yytext);
  while (regex_search(*temp, regex("([$][{].*?[}])")) ) {
    smatch sm;
    regex_search(*temp, sm, regex("([$][{].*?[}])"));
    string envName = sm.str(0).substr(2, sm.str(0).size() - 3);
    *temp = regex_replace(*temp, regex("([$][{].*?[}])"), getenv(envName.c_str()), regex_constants::format_first_only);
  }
  yylval.cpp_string = new std::string(*temp);
  return WORD;
}

(\~) { //Tilde: list the home directory
  struct passwd *pws;
  pws = getpwuid(geteuid());
  yylval.cpp_string = new std::string(pws->pw_dir);
  return WORD;
}

(\~[^ \\\t\n<>|&\/]+) { //Tilde: List [george]'s home directory
  std::string temp(yytext);
  std::string user = temp.substr(1, temp.size() - 1);
  struct passwd *pws;
  pws = getpwnam(user.c_str());
  if (pws == NULL) {
    yylval.cpp_string = new std::string(yytext);
    return WORD;
  }
  yylval.cpp_string = new std::string(pws->pw_dir);
  return WORD;
}

(\~[^ \\\t\n<>|&\/]+\/[^ \\\t\n<>|&\/]+) {
  std::string temp(yytext);
  int pos1 = temp.find('/');
  std::string user = temp.substr(1, pos1-1);
  std::string dir = temp.substr(pos1+1, temp.size() - 1);
  struct passwd *pws;
  pws = getpwnam(user.c_str());
  if (pws == NULL) {
    yylval.cpp_string = new std::string(yytext);
    return WORD;
  }
  std::string tempStr(pws->pw_dir);
  tempStr.append("/");
  tempStr.append(dir);
  yylval.cpp_string = new std::string(tempStr);
  return WORD;
}

[^ \t\n<>|&][^ \t\n<>|&]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}
