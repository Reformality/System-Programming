#include "mysort.h"
#include <alloca.h>
#include <assert.h>
#include <string.h>
#include <cstdlib>

//
// Sort an array of element of any type
// it uses "compFunc" to sort the elements.
// The elements are sorted such as:
//
// if ascending != 0
//   compFunc( array[ i ], array[ i+1 ] ) <= 0
// else
//   compFunc( array[ i ], array[ i+1 ] ) >= 0
//
// See test_sort to see how to use mysort.
//
void mysort( int n,                      // Number of elements
	     int elementSize,            // Size of each element
	     void * array,               // Pointer to an array
	     int ascending,              // 0 -> descending; 1 -> ascending
	     CompareFunction compFunc )  // Comparison function.
{
  // Add your code here. Use any sorting algorithm you want.
  if (ascending) {
    for (int i = 1; i < n; i++){
	  int j = i - 1;
	  while (j >= 0 && (*compFunc)(((char*) array)+j*elementSize, ((char*) array)+(j+1)*elementSize) > 0) {
	    char *s1 = (char*) malloc((size_t) elementSize);
		char *s2 = (char*) malloc((size_t) elementSize);
		memcpy(s1, ((char*) array)+j*elementSize, (size_t) elementSize);
		memcpy(s2, ((char*) array)+(j+1)*elementSize, (size_t) elementSize);
		memcpy(((char*) array)+j*elementSize, s2, (size_t) elementSize);
		memcpy(((char*) array)+(j+1)*elementSize, s1, (size_t) elementSize);
		j--;
	  }
	}
  } else {
    for (int i = 1; i < n; i++){
	  int j = i - 1;
	  while (j >= 0 && (*compFunc)(((char*) array)+j*elementSize, ((char*) array)+(j+1)*elementSize) < 0) {
	    char *s1 = (char*) malloc((size_t) elementSize);
		char *s2 = (char*) malloc((size_t) elementSize);
		memcpy(s1, ((char*) array)+j*elementSize, (size_t) elementSize);
		memcpy(s2, ((char*) array)+(j+1)*elementSize, (size_t) elementSize);
		memcpy(((char*) array)+j*elementSize, s2, (size_t) elementSize);
		memcpy(((char*) array)+(j+1)*elementSize, s1, (size_t) elementSize);
		j--;
	  }
	}
  }
}
