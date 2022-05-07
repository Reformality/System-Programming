#include <stdlib.h>
#include <unistd.h>

int a = 25;   /*initialized global var*/
int b[45000]; /*uninitialized global var*/
int d[45000]; /*uninitialized global var*/

void donothing();

int main()
{
    char c=25;
    int  *ptr;

    printf("Address of the function donothing() is 0x%p\n", donothing);
    printf("Address of integer a is 0x%p\n", &a);
    printf("Address of array b is 0x%p\n", b);
    printf("Address of array d is 0x%p\n", d);
    printf("Address of character c is 0x%p\n", &c);

    ptr = (int *)malloc(sizeof(int));
    printf("Address of integer pointer ptr is 0x%p\n", &ptr);

    printf("Address of the integer pointed to by ptr is 0x%p\n", ptr);

    donothing();
    sleep(60);
    return 0;
}

void donothing()
{
    int local;

    printf("\nAddress of integer local is 0x%p\n", &local);

    return;
}
