#include <stdio.h>
#include <sys/time.h>
 
int main() {
    struct timeval start, end;
    gettimeofday( &start, NULL );
    printf("hahahaa");
    gettimeofday( &end, NULL );
    int timeuse = 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
    printf("time: %d us\n", timeuse);
    return 0;
}