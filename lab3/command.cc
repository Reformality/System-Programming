/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>

#include <unistd.h> //added: fork() 
#include <sys/wait.h> //added: waitpid()
#include <fcntl.h> //added: open() and close()
#include <string.h> //strcpy() and strcmp()
#include <string>
#include <cstring>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>
#include <regex>

#include <iostream>

#include "command.hh"
#include "shell.hh"

extern char **environ;

extern int * returnCode;
extern pid_t * bgPID;
extern std::string * lastArg;

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _redirector = "";
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile && !_redirector.compare(">>&") && !_redirector.compare(">&") ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    // exit the shell if requested
    if ((*_simpleCommands[0]->_arguments[0]).compare("exit") == 0 ) {
        printf( "Bye!\n");
        exit( 1 );
    }

    // Print contents of Command data structure
    // print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec
    
    //save in/out
    int tmpin = dup(0);
    int tmpout = dup(1);
    int tmperr = dup(2);
    //set input redirection
    int fdin;
    if (_inFile) {
        fdin = open((*_inFile).c_str(), O_RDONLY, 0440); //4->read only
    } else {
        fdin = dup(tmpin);
    }
    int ret; //child process
    int fdout;
    //set error redirection
    int fderr;
    if (_errFile) {
        if (!_redirector.compare(">>&")) {
            fderr = open((*_errFile).c_str(), O_CREAT|O_RDWR|O_APPEND, 0660);
        } else {
            fderr = open((*_errFile).c_str(), O_CREAT|O_RDWR|O_TRUNC, 0660);
        }
    } else {
        fderr = dup(tmperr);
    }
    dup2(fderr, 2);
    close(fderr);

    for(int i=0; i < (int)_simpleCommands.size(); i++){
        //redirect input
        dup2(fdin, 0);
        close(fdin);

        //setenv [BUILD-IN]
        if(_simpleCommands[i]->_arguments[0]->compare("setenv") == 0) {
            std::string envname(*_simpleCommands[i]->_arguments[1]);
            std::string envval(*_simpleCommands[i]->_arguments[2]);
            setenv(envname.c_str(), envval.c_str(), 1);
            clear();
			Shell::prompt();
			return;
        }

        //unsetenv [BUILD-IN]
        if(_simpleCommands[i]->_arguments[0]->compare("unsetenv") == 0) {
            std::string name(*_simpleCommands[i]->_arguments[1]);
            unsetenv(name.c_str());
            clear();
			Shell::prompt();
			return;
        }

        //cd [BUILD-IN]
        if(_simpleCommands[i]->_arguments[0]->compare("cd") == 0) {
            if (_simpleCommands[i]->_arguments.size() < 2) {
                struct passwd *pw = getpwuid(getuid());
                chdir(pw->pw_dir);
            } else {
                int err = chdir(_simpleCommands[i]->_arguments[1]->c_str());
                if (err == -1) {
                    fprintf(stderr, "cd: can't cd to %s\n", _simpleCommands[i]->_arguments[1]->c_str());
                }
            }

            *lastArg = *(_simpleCommands[i]->_arguments.back());

            clear();
			Shell::prompt();
			return;
        }

        //setup output
        if (i == (int)_simpleCommands.size()-1){
            //last simple command
            if(_outFile){
                if (!_redirector.compare(">>&") || !_redirector.compare(">>")) {
                    fdout = open( (*_outFile).c_str(), O_CREAT|O_RDWR|O_APPEND, 0660);
                } else {
                    fdout = open( (*_outFile).c_str(), O_CREAT|O_RDWR|O_TRUNC, 0660);
                }
            } else {
                fdout = dup(tmpout);
            }
        } else {
            //not last simple command
            //create pipe
            int fdpipe[2];
            pipe(fdpipe);
            fdout = fdpipe[1];
            fdin = fdpipe[0];
        }
        //redirect output
        dup2(fdout, 1);
        close(fdout);
        //create child process
        ret=fork();
        if(ret == 0){
            //printenv [BUILD-in]
            if (_simpleCommands[i]->_arguments[0]->compare("printenv") == 0) {
                char **temp = environ;
                while (*temp != NULL) {
                    printf("%s\n", *temp);
                    temp++;
                }
                exit(1);
            }

            close(tmpin);
            close(tmpout);
            close(tmperr);

            //execvp arguments
            auto argv = std::vector<char *>();
            for(auto arg:_simpleCommands[i]->_arguments){
                argv.push_back(const_cast<char *>(arg->c_str()));
            }
            argv.push_back(NULL);
            execvp(_simpleCommands[i]->_arguments[0]->c_str(), argv.data());
            perror("execvp");
            exit(1);
        }
    }
    
    //restore in/out defaults
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    dup2(tmperr, 2);
    close(tmpin);
    close(tmpout);
    close(tmperr);

    if (!Command::_background) {
        waitpid(ret, returnCode, 0);
        if (WIFEXITED(*returnCode)) {   //if return normally
            *returnCode = (int) WEXITSTATUS(*returnCode);   //return the exit status of the child
        }
    }
    else { //background
        *bgPID = ret;
    }
    
    *lastArg = *(_simpleCommands[_simpleCommands.size() - 1]->_arguments.back());

    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;
