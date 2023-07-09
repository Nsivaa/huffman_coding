CPPFLAGS= -pthread -O3 -DTRACE_FASTFLOW -Wall -Wno-unused-variable -g -std=c++17 -lstdc++
TARGETS= huf_thread huf_sequential huf_ff
THREADHEADERS= ./headers
FFHEADERS= ./fastflow

all: $(TARGETS)
	make clean

huf_thread: huf_thread.o
	g++ $(CPPFLAGS) -I $(THREADHEADERS) huf_thread.o utimer.cpp -o huf_thread

huf_thread.o: huf_thread.cpp
	g++ $(CPPFLAGS) -c huf_thread.cpp -I $(THREADHEADERS) -o huf_thread.o

huf_sequential: huf_sequential.o
	g++ $(CPPFLAGS) huf_sequential.o utimer.cpp -o huf_sequential

huf_sequential.o: huf_sequential.cpp
	g++ $(CPPFLAGS) -I $(THREADHEADERS) -c huf_sequential.cpp -o huf_sequential.o

huf_ff: huf_ff.o
	g++ $(CPPFLAGS) -I $(THREADHEADERS) -I $(FFHEADERS) huf_ff.o utimer.cpp -o huf_ff

huf_ff.o: huf_ff.cpp
	g++ $(CPPFLAGS) -c huf_ff.cpp -I $(FFHEADERS) -I $(THREADHEADERS) -o huf_ff.o

clean:
	rm *.o
