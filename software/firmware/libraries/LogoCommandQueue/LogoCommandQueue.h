#ifndef LogoCommandQueue_h
#define LogoCommandQueue_h

#include <Arduino.h>

class LogoCommandQueue
{
public:
	LogoCommandQueue(int length);
	boolean insert(String s);
	boolean enqueue(String s);
	String dequeue();
	boolean isFull();
	boolean isEmpty();
	int pending();
	void printCommandQ();

private:
	int queueLength;
	COMMAND *cmdQ;
	int qHead;
	int qSize;

};

#endif
