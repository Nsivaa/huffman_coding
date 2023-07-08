CPPFLAGS= -pthread -O3 -Wall -Wno-unused-variable -g -I ./headers -std=c++17 -lstdc++
TARGETS= huf_thread huf_sequential huf_ff

all: $(TARGETS)
	make clean

huf_thread: huf_thread.o
	g++ $(CPPFLAGS) huf_thread.o utimer.cpp -o huf_thread

huf_thread.o: huf_thread.cpp
	g++ $(CPPFLAGS) -o huf_thread.o -c huf_thread.cpp

huf_sequential: huf_sequential.cpp
	g++ $(CPPFLAGS) huf_thread.cpp utimer.cpp -o huf_sequential.o

clean:
	rm *.o
