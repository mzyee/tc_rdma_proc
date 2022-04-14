#include <assert.h>
#include <sys/time.h>
#include <iostream>

int main(int argc, char *argv[])
{
	long deltaT = 0;
	struct timeval t1{0,0};
	gettimeofday(&t1, NULL);
	
	deltaT = t1.tv_sec * 10000000 + t1.tv_usec; // us
	std::cout << "-------mzytest--------time elapse(us):" << deltaT << std::endl;

	return 0;
}