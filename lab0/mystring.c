#include <sys/types.h>
#include <stdlib.h>

/*
 * Implement the following string procedures.
 *
 * Type "man strstr" to find what each of the functions should do.
 *
 * For example, mystrcpy should do the same as strcpy.
 *
 * IMPORTANT: DO NOT use predefined string functions.
 */

char *mystrcpy(char * s1, const char * s2)
{
  char *temp1 = s1;
  
  while (*s2 != '\0') {
    *s1 = *s2;
    s1++;
    s2++;
  }
  
  *s1 = '\0';
  return temp1;
}

size_t mystrlen(const char *s)
{
  int count = 0;
  while (*s != '\0') {
    count++;
    s++;
  }
  return count;
}

char *mystrdup(const char *s1)
{
  size_t length;
  length = mystrlen(s1);
  char *str;
  str = (char*) malloc(sizeof(*str) * (length + 1));
  char *temp1 = str;

  while(*s1 != '\0') {
    *str = *s1;
    str++;
    s1++;
  }
  *str = '\0';
  return temp1;
}

char *mystrcat(char * s1, const char * s2)
{
  size_t s1Length = mystrlen(s1);
  size_t s2Length = mystrlen(s2);
  size_t i;
  for (i = 0; i < s2Length; i++) {
    s1[s1Length + i] = s2[i];
  }
  s1[s1Length + i] = '\0';
  return s1;
}

char *mystrstr(char * s1, const char * s2)
{
  if (*s2 == '\0') return s1;
  for (int i = 0; i < mystrlen(s1); i++) {
    if (*(s1 + i) == *s2) {
      char *temp = mystrstr(s1 + i + 1, s2 + 1);
      if (temp) {
        return temp - 1;
      } else {
        return 0;
      }
    }
  }
  return 0;
}

int mystrcmp(const char *s1, const char *s2) {
  while (*s1) {
    if (*s1 != *s2) break;
    s1++;
    s2++;
  }
  return *s1 - *s2;
}

