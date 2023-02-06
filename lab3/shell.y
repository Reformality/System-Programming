
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE LESS GREAT_GREAT PIPE TWO_GREAT GREAT_AMPERSAND GREAT_GREAT_AMPERSAND AMPERSAND

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"
#include <regex>
#include <sys/types.h>
#include <dirent.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <limits.h>


void yyerror(const char * s);
int yylex();

std::vector<std::string *>entries = std::vector<std::string *>(); //vector storing wildcard
void expandWildcardsIfNecessary(std::string *);
void expand_wildcards(std::string *, std::string *);
bool comparStr(std::string *, std::string *);

%}

%%

goal:
  commands
  ;

commands:
  command
  | commands command
  ;

command: simple_command
       ;

simple_command:	
  pipe_list io_modifier_list background_optional NEWLINE {
    // printf("   Yacc: Execute command\n");
    Shell::_currentCommand.execute();
  }
  | NEWLINE 
  | error NEWLINE { yyerrok; }
  ;

pipe_list:
  pipe_list PIPE command_and_args
  | command_and_args
  ;

command_and_args:
  command_word argument_list {
    Shell::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

argument:
  WORD {
    // Command::_currentSimpleCommand->insertArgument( $1 );
    expandWildcardsIfNecessary($1);
  }
  ;

command_word:
  WORD {
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

io_modifier_list:
  io_modifier_list iomodifier_opt
  | //empty
  ;

iomodifier_opt:
  GREAT WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      yyerror("Ambiguous output redirect.\n");
    }
    // printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._redirector = ">";
  }
  | GREAT_GREAT WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      yyerror("Ambiguous output redirect.\n");
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._redirector = ">>";
  }
  | GREAT_GREAT_AMPERSAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      yyerror("Ambiguous output redirect.\n");
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._redirector = ">>&";
  }
  | GREAT_AMPERSAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      yyerror("Ambiguous output redirect.\n");
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._redirector = ">&";
  }
  | LESS WORD {
    Shell::_currentCommand._inFile = $2;
    Shell::_currentCommand._redirector = "<";
  }
  | TWO_GREAT WORD {
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._redirector = "2>";
  }
  ;

background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | //empty
  ;

%%

using namespace std;

bool comparStr(string * a, string * b) {
  int result = a->compare(*b);
  return result < 0;
}

void expandWildcardsIfNecessary(std::string * argument) {
  //reset wildcard vector if not empty
  while (!entries.empty()) { 
    entries.pop_back();
  }

  string *temp_string = new string(*argument);

  //check for wildcards
  if (temp_string->find("*") == string::npos && temp_string->find("?") == string::npos) {
    Command::_currentSimpleCommand->insertArgument(argument);
    delete temp_string;
  }
  else { //wildcards exist
    string *prefix = new string();
    expand_wildcards(prefix, temp_string);
    delete prefix;
    delete temp_string;

    if (entries.empty()) { //wildcard doesn't match anything, return with wildcard
      Command::_currentSimpleCommand->insertArgument(argument);
    }
    else {
      sort(entries.begin(), entries.end(), comparStr);  //sort wildcard
      //inserting arguments
      for (auto& temp : entries) {
        Command::_currentSimpleCommand->insertArgument(temp);
      } //end for
      delete (argument);
    }
  }
}

void expand_wildcards(string *prefix, string *suffix) {
  //base case: write prefix to entries
  if (suffix == nullptr) {
    entries.push_back(new string(*prefix));
    return;
  }

  //if this is the first time this function is called: (no prefix)
  //and suffix (path) is not a relative path
  //then write in the "/"
  if (prefix->empty() && suffix->at(0) == '/') {
    *prefix = prefix->append("/");
    *suffix = suffix->erase(0, 1);
  }
  //copy suffix content
  string *temp_string = new string(*suffix);
  if (temp_string->find("/") != string::npos) { //there is "/" in suffix
    //substring up to the first "/"
    *temp_string = temp_string->substr(0, temp_string->find("/"));
    //substring content after "/"
    *suffix = suffix->substr(suffix->find("/") + 1, suffix->size() - 1 - suffix->find("/"));
  } 
  else { //no '/' in suffix
    suffix = nullptr;
  }

  //if there is no wildcards in the current temp_string
  if (temp_string->find("*") == string::npos && temp_string->find("?") == string::npos) {
    *prefix = prefix->append(*temp_string);
    *prefix = prefix->append("/");
    if (suffix != nullptr) {  //there are still suffix after "/", run function again
      string *p = new string(*prefix);
      string *s = new string(*suffix);
      expand_wildcards(p, s);
      delete p;
      delete s;
    }
    else {  //not more suffix, return to base case
      string *p = new string(*prefix);
      expand_wildcards(p, nullptr);
      delete p;
    }
  }
  else { //there is wildcards in the current temp_string
    //converting to regex
    string *reg_string = new string(*temp_string);
    bool find_hidden = false;

    //replace all dots
    for (int i = 0; i < reg_string->size(); i++) {
      if (reg_string->at(i) == '.') {
        //check whether to search hidden file
        if (i == 0) {
          find_hidden = true;
        }
        *reg_string = reg_string->replace(i, 1, "\\.");
        i++;
      }
    }
    //inserting ^
    *reg_string = reg_string->insert(0, "^");
    //replace ? to .
    while (reg_string->find("?") != string::npos) {
      *reg_string = reg_string->replace(reg_string->find("?"), 1, ".");
    }
    //replace * to .*
    for (int i = 0; i < reg_string->size(); i++) {
      if (reg_string->at(i) == '*') {
        *reg_string = reg_string->replace(i, 1, ".*");
        i++;
      }
    }
    //inserting $ to the end
    *reg_string = reg_string->append("$");
    //new compiled regex
    regex reg(*reg_string);
    delete reg_string;
    //open directory
    DIR *dir = opendir(prefix->empty() ? "." : prefix->c_str());  //open local or prefix 
    if (dir == nullptr) {
      perror("opendir");
      return;
    }

    //traverse current directory
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
      string *prefix_local = new string(*prefix);
      string *suffix_local = suffix == nullptr ? nullptr :new string(*suffix);
      
      //check if name matches
      if (regex_match(ent->d_name, reg)) {
        //if there are more suffix
        if (suffix_local != nullptr) {
          //only continue if ent is a directory
          if (ent->d_type == DT_DIR) {
            //check if it is the first time reaching here
            if (prefix_local->empty() || *prefix_local == "/") {  //first dir
              *prefix_local = prefix_local->append(ent->d_name);
            } else {
              if ( prefix_local->find_last_of("/") != prefix_local->size() - 1) { //last char is not "/"
                *prefix_local = prefix_local->append("/");
              }
              *prefix_local = prefix_local->append(ent->d_name);
            }
            string *p = new string(*prefix_local);
            string *s = new string(*suffix_local);
            expand_wildcards(p, s); //continue to run function
            delete p;
            delete s;
          }
        }
        else { //suffix is null
          //hidden file check
          if (ent->d_name[0] == '.') {
            if (find_hidden) {
              if ( prefix_local->find_last_of("/") != prefix_local->size() - 1) { //last char is not "/"
                *prefix_local = prefix_local->append("/");
              }
              *prefix_local = prefix_local->append(ent->d_name);
              string *p = new string(*prefix_local);
              expand_wildcards(p, nullptr);
              delete p;
            }
          }
          else {  //not hidden file
            if ( prefix_local->find_last_of("/") != prefix_local->size() - 1) { //last char is not "/"
              *prefix_local = prefix_local->append("/");
            }
            *prefix_local = prefix_local->append(ent->d_name);
            string *p = new string(*prefix_local);
            expand_wildcards(p, nullptr); //return to base case
            delete p;
          }
        }

      } //end regex if
      delete prefix_local;
      delete suffix_local;
    } //end while
    closedir(dir);
  }
  delete (temp_string);
}

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}



#if 0
main()
{
  yyparse();
}
#endif
