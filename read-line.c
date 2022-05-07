/*
 * CS252: Systems Programming
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <sys/types.h>
#include <regex.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>


#define MAX_BUFFER_LINE 2048

extern void tty_raw_mode(void);

// Buffer where line is stored
int line_length = 0;  //length of line
int line_index = 0;   //光标 location
char line_buffer[MAX_BUFFER_LINE];

//history array
int history_index = 0;  //newest history location in the array
char * history [20];  //0->19:old->new
int history_length = sizeof(history)/sizeof(char *);
int history_shift = 0;  //for up and down arrow

void insert(char ch);
void backspace(char ch);
void home();
void end();
void deleteKey(char ch);

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {
  struct termios orig_attr;
  tcgetattr(0,&orig_attr);
  // Set terminal in raw mode
  tty_raw_mode();
  //reset parameters
  line_length = 0;
  line_index = 0;
  for (int i = 0; i < MAX_BUFFER_LINE; i++) {
      line_buffer[i] = '\0';
  }

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch>=32) {
      insert(ch);
      // If max number of character reached return.
      if (line_length >= MAX_BUFFER_LINE - 1) {
        // Add eol and null char at the end of string
        line_buffer[line_length] = 10;
        line_length++;
        line_buffer[line_length] = 0;
        break;
      }
    }
    else if (ch==10) {
      // <Enter> was typed. Return line
      // Print newline
      write(1, &ch, 1);
      
      //store the line to history
      if (history_index == history_length) { //command buffer full, free the oldest history
          free(history[0]);
          for (int i = 0; i < history_length - 1; i++) { //move up commands
              history[i] = history[i + 1];
          }
          history_index--;
      }

      int length = strlen(line_buffer);
      char * command = (char *) calloc(length, sizeof(char));
      strcpy(command, line_buffer);
      history[history_index] = command;
      history_index++;

      history_shift = 0;

      // Add eol and null char at the end of string
      line_buffer[line_length] = 10;
      line_length++;
      line_buffer[line_length] = 0;
      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if (ch == 1) { //home 
      home();
    }
    else if (ch == 5) { //end
      end();
    }
    else if (ch == 4) { //delete ctrl-D
      deleteKey(ch);
    }
    else if (ch == 8) { //backspace ctrl-H
      backspace(ch);
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1; 
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && ch2==65) {
        // Up arrow. Print next line in history.
        if (history_shift == history_index){
          break;
        }
        // Erase old line
        // Print backspaces
        int i = 0;
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }	

        // Copy line from history
        history_shift++;
        strcpy(line_buffer, history[history_index - history_shift]);
        line_length = strlen(line_buffer);

        // echo line
        write(1, line_buffer, line_length);
        line_index = line_length;
      }
      else if (ch1==91 && ch2==66) {  //down arrow
        if (history_shift <= 1) {
          break;
        }
        // Erase old line
        //Print backspaces
        int i = 0;
        for (i = 0; i < line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }
        // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }
        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }
        // Copy line from history
        history_shift--;
        strcpy(line_buffer, history[history_index - history_shift]);
        line_length = strlen(line_buffer);

        // echo line
        write(1, line_buffer, line_length);
        line_index = line_length;
      }
      else if (ch1 == 91 && ch2 == 68) { //left arrow
        if (line_index > 0) {
          char bs = 8;
          write(1, &bs, 1);
          line_index--;
        }
      }
      else if (ch1 == 91 && ch2 == 67) { //right arrow
        if (line_index < line_length) {
          char ca = 27;
          char cb = 91;
          char cc = 67;
          write(1, &ca, 1);
          write(1, &cb, 1);
          write(1, &cc, 1);
          line_index++;
        }
      }
    }
  }
  tcsetattr(0, TCSANOW, &orig_attr);
  return line_buffer;
}

void insert(char ch) {
  if (line_length == 0) { //if the length is zero, reset line_index
      write(1, &ch, 1);
      line_buffer[0] = ch;
      line_length = 1;
      line_index = 1;
      line_buffer[1] = '\0';
      return;
    }

    if (line_index == line_length) { //at the end of line
      write(1, &ch, 1);

      line_buffer[line_index] = ch;
      line_index++;
      line_length++;
      line_buffer[line_index + 1] = '\0';
    }
    else { //inserting character
      //add the rest if any
      char * suffix = (char *) calloc(line_length + 1, sizeof(char)); //one space for \0
      int counter = 0;
      for (int i = line_index; i < line_length; i++) {  
          suffix[counter] = line_buffer[i];
          counter++;
      }
      suffix[counter] = '\0';

      write(1, &ch, 1);
      line_buffer[line_index] = ch;
      line_length++;
      line_index++;
      line_buffer[line_length] = '\0';
      
      //fill in the rest to buffer
      for (int i = line_index, j = 0; i < line_length; i++, j++) { 
          line_buffer[i] = suffix[j];
          char ca = suffix[j];
          write(1, &ca, 1);
      }
      free(suffix);

      for (int i = 0; i < counter; i++) { //move back to cursor position
          char b = 8;
          write(1, &b, 1);
      }
    }
}

void backspace(char ch) {
    if (line_length == 0 || line_index == 0) { //return if no more can be backspaced
        return;
    }

    if (line_index == line_length) { //at the end of line
        // Go back one character
        ch = 8;
        write(1,&ch,1);
        // Write a space to erase the last character read
        ch = ' ';
        write(1,&ch,1);
        // Go back one character
        ch = 8;
        write(1,&ch,1);
        // Remove one character from buffer
        line_buffer[line_index - 1] = '\0';
        line_length--;
        line_index--;
    }
    else { //in the middle
        char * suffix = (char *) calloc(line_length + 1, sizeof(char)); //add one space for \0
        int counter = 0;
        for (int i = line_index; i < line_length; i++) {
            suffix[counter] = line_buffer[i];
            counter++;
        }
        suffix[counter] = '\0';

        for (int i = 0; i < counter; ++i) {
            char inCh = ' ';
            write(1, &inCh, 1);
        }
        for (int i = 0; i < counter; ++i) {
            char inCh = 8;
            write(1, &inCh, 1);
        }

        char bs = 8;
        write(1, &bs, 1);

        line_length--;
        line_index--;
        line_buffer[line_length] = '\0';

        for (int i = line_index, j = 0; i < line_length; i++, j++) {
            line_buffer[i] = suffix[j];
            char inCh = suffix[j];
            write(1, &inCh, 1);
        }
        free(suffix);

        for (int i = 0; i < counter; i++) {
            char b = 8;
            write(1, &b, 1);
        }
    }
}

void home() {
  int count = line_index;
  if (count > 0) {
    for (int i = 0; i < count; i++) {
      char bs = 8;
      write(1, &bs, 1);
    }
  }
  line_index = 0;
}

void end() {
  int count = line_length - line_index;
  if (count > 0) {
    for (int i = 0; i < count; i++) {
      char ca = 27;
      char cb = 91;
      char cc = 67;
      write(1, &ca, 1);
      write(1, &cb, 1);
      write(1, &cc, 1);
    }
    line_index = line_length;
  }
}

void deleteKey(char ch) {
    if (line_length == 0 || line_index == line_length) {
        return;
    }

    if (line_index == line_length - 1) { //at the second last end
        // Write a space to erase the last character read
        ch = ' ';
        write(1,&ch,1);

        // Go back one character
        ch = 8;
        write(1,&ch,1);

        // Remove one character from buffer
        line_buffer[line_index] = '\0';
        line_length--;
    }
    else { //in the middle
        char * suffix = (char *) calloc(line_length + 1, sizeof(char));
        int counter = 0;
        for (int i = line_index + 1; i < line_length; i++) {
            suffix[counter] = line_buffer[i];
            counter++;
        }
        suffix[counter] = '\0';

        for (int i = 0; i < counter + 1; i++) {
            char cc = ' ';
            write(1, &cc, 1);
        }
        for (int i = 0; i < counter + 1; i++) {
            char cc = 8;
            write(1, &cc, 1);
        }

        line_length--;
        line_buffer[line_length] = '\0';

        for (int i = line_index, j = 0; i < line_length; i++, j++) {
            line_buffer[i] = suffix[j];
            char ca = suffix[j];
            write(1, &ca, 1);
        }
        free(suffix);

        for (int i = 0; i < counter; i++) { //move cursor back
            char b = 8;
            write(1, &b, 1);
        }
    }
}